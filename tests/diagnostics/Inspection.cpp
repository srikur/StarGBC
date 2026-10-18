#include "GameboyFiles.h"
#include <doctest/doctest.h>
#include "stargbc/Gameboy.h"

namespace {
    template<class T>
    consteval std::meta::info inspectionTestMember(const std::string_view name) {
        for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::identifier_of(member) == name)
                return member;
        throw "Unknown inspection test member";
    }
} // namespace

TEST_CASE("diagnostics: repeated inspection preserves deferred audio and subsequent execution") {
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        const host::LaunchSettings settings{.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}};
        auto observed = host::load_gameboy(settings);
        auto control = host::load_gameboy(settings);
        for (auto *machine : {&observed, &control}) {
            auto &audio = machine->[:inspectionTestMember<Gameboy>("audio_"):];
            for (unsigned tick = 0; tick < 7; ++tick)
                audio.Tick();
        }
        const auto before = observed.SaveState();
        for (unsigned sample = 0; sample < 16; ++sample) {
            const auto view = observed.Inspect();
            CHECK(view.audio.deferredTicks == 7);
            CHECK(view.audio.effectiveTicks == view.audio.processedTicks + 7);
            for (const uint16_t address : {0xFF04, 0xFF0F, 0xFF26, 0xFF30, 0xFF41, 0xFF44, 0xFF4B, 0xFF76, 0xFFFF}) {
                static_cast<void>(observed.InspectRegister(address));
                static_cast<void>(observed.InspectCpuRead(address));
            }
            for (const uint16_t address : {0x100, 0x8000, 0xA000, 0xC000, 0xFE00, 0xFEFF, 0xFF00, 0xFF03, 0xFF80})
                static_cast<void>(observed.InspectCpuRead(address));
            CHECK(observed.SaveState() == before);
        }
        for (unsigned interval = 0; interval < 3; ++interval) {
            observed.RunFrame();
            control.RunFrame();
            CHECK(observed.SaveState() == control.SaveState());
            CHECK(std::ranges::equal(std::span(observed.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT),
                                     std::span(control.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT)));
        }
    }
}

TEST_CASE("diagnostics: register inspection distinguishes stored pending and CPU-visible values") {
    auto gameboy = host::load_gameboy({.rom_path = "roms/acid/dmg-acid2.gb", .emulation = {.model = Model::DMGB, .no_bootrom = true}});
    auto &gpu = gameboy.[:inspectionTestMember<Gameboy>("gpu_"):];
    gpu.windowX = 3;
    gpu.wxPending = 9;
    gpu.wxWriteStage = 2;
    const auto before = gameboy.SaveState();
    const auto window = gameboy.InspectRegister(0xFF4B);
    CHECK(window.stored == 3);
    CHECK(window.cpuVisible == 9);
    REQUIRE(window.pending);
    CHECK(*window.pending == 9);
    CHECK(window.pendingStage == 2);
    CHECK(gameboy.SaveState() == before);
    gpu.currentLine = 153;
    const auto ly = gameboy.InspectRegister(0xFF44);
    CHECK(ly.stored == 153);
    CHECK(ly.cpuVisible == 0);
}
