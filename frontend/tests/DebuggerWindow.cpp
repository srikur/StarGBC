#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "DebuggerWindow.h"
#include <doctest/doctest.h>
#include <filesystem>
#include <imgui.h>

namespace {
    template<class T>
    consteval std::meta::info debug_member(const std::string_view name) {
        for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::identifier_of(member) == name)
                return member;
        throw "Unknown debugger test member";
    }
}

TEST_CASE("frontend: every inspector renders without advancing the machine and input stays isolated") {
    REQUIRE(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"));
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    struct Cleanup {
        ~Cleanup() { SDL_Quit(); }
    } cleanup;
    SDL_Window *raw_window{};
    SDL_Renderer *raw_renderer{};
    REQUIRE(SDL_CreateWindowAndRenderer("Debugger test", 480, 432, 0, &raw_window, &raw_renderer));
    const std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(raw_window, SDL_DestroyWindow);
    const std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(raw_renderer, SDL_DestroyRenderer);
    Gameboy machine({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
    machine.[:debug_member<Gameboy>("cartridge_"):].[:debug_member<Cartridge>("gameRom_"):].resize(512 * ROM_BANK_SIZE);
    EmulationSession session(machine);
    DebuggerWindow debugger(session, window.get(), renderer.get(), [](uint8_t) {}, [](uint8_t) {});
    const auto snapshot{machine.SaveDebugState()};
    constexpr std::array panels{DebugPanel::Memory,  DebugPanel::Breakpoints, DebugPanel::Screen,   DebugPanel::Tiles, DebugPanel::Maps,
                                DebugPanel::Sprites, DebugPanel::Palettes,    DebugPanel::Hardware, DebugPanel::Trace, DebugPanel::Help};
    for (const auto panel : panels) {
        debugger.show_panel(panel);
        debugger.render();
        debugger.render();
        REQUIRE(ImGui::GetDrawData());
        CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);
        CHECK(machine.SaveDebugState() == snapshot);
    }
    constexpr std::array code_sources{CodeSource::Cartridge, CodeSource::Bios, CodeSource::Mapped};
    constexpr std::array offsets{0u, 0x4000u, 0x7FFFu, 0x7FFFFFu, 0xFFFFFFFFu};
    for (const auto [source, offset] : std::views::cartesian_product(code_sources, offsets)) {
        debugger.show_code(source, offset);
        debugger.render();
        debugger.render();
        REQUIRE(ImGui::GetDrawData());
        CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);
        CHECK(ImGui::GetDrawData()->TotalVtxCount < 200000);
        CHECK(machine.SaveDebugState() == snapshot);
    }
    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.windowID = SDL_GetWindowID(window.get());
    key.key.key = SDLK_Z;
    CHECK(debugger.handle_event(key));
    CHECK(machine.PressedButtons() == 0);
    key.key.key = SDLK_F11;
    ImGui::GetIO().WantTextInput = true;
    CHECK(debugger.handle_event(key));
    CHECK(machine.InstructionsCompleted() == 0);
    ImGui::GetIO().WantTextInput = false;
    CHECK(debugger.handle_event(key));
    CHECK(machine.InstructionsCompleted() == 1);
    CHECK(session.Paused());
    key.key.repeat = true;
    CHECK(debugger.handle_event(key));
    CHECK(machine.InstructionsCompleted() == 1);
    key.key.repeat = false;
    key.key.key = SDLK_F5;
    CHECK(debugger.handle_event(key));
    CHECK_FALSE(session.Paused());
    debugger.controller().pump(32);
    CHECK(debugger.handle_event(key));
    CHECK(session.Paused());
    key.key.key = SDLK_F8;
    CHECK(debugger.handle_event(key));
    CHECK(session.Paused());
    CHECK(session.StateGeneration() == 1);
    CHECK(machine.InstructionsCompleted() == 0);
    CHECK(machine.TimelineTicks() == 0);
    CHECK(debugger.controller().trace_size() == 0);
    debugger.render();
    debugger.render();
    debugger.toggle();
    CHECK_FALSE(debugger.visible());
    int width{}, height{};
    REQUIRE(SDL_GetWindowSize(window.get(), &width, &height));
    CHECK(width == 480);
    CHECK(height == 432);
    debugger.toggle();
    CHECK(debugger.visible());
    CHECK(session.Paused());
    key.key.key = SDLK_F5;
    key.key.mod = SDL_KMOD_CTRL;
    CHECK_FALSE(debugger.handle_event(key));
    key.key.mod = SDL_KMOD_SHIFT;
    CHECK_FALSE(debugger.handle_event(key));
    CHECK(session.Paused());
    debugger.show_panel(DebugPanel::Memory);
    debugger.render();
    debugger.render();
    if (const auto *capture{SDL_getenv("STARGBC_DEBUGGER_CAPTURE")}) {
        debugger.controller().resume();
        debugger.controller().pump(60ULL * Gameboy::FRAME_CYCLES_DMG * 2);
        debugger.controller().pause();
        debugger.render();
        const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface);
        REQUIRE(surface);
        REQUIRE(SDL_SaveBMP(surface.get(), capture));
    }
}
