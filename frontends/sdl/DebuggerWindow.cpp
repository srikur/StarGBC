#include "../DebuggerWindow.h"

#include <format>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

namespace {
    bool hex_input(const char *label, uint16_t &value) {
        ImGui::SetNextItemWidth(85);
        return ImGui::InputScalar(label, ImGuiDataType_U16, &value, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
    }

    ImVec4 rgba(const uint32_t color) {
        return {float(color & 255) / 255, float((color >> 8) & 255) / 255, float((color >> 16) & 255) / 255, float(color >> 24) / 255};
    }

    void text(const std::string &value) { ImGui::TextUnformatted(value.c_str()); }

    ImVec4 source_color(const CodeSource source) {
        switch (source) {
            case CodeSource::Bios:
                return {1, 0.75f, 0.3f, 1};
            case CodeSource::Cartridge:
                return {0.4f, 0.85f, 1, 1};
            case CodeSource::Mapped:
                return {0.7f, 0.7f, 0.7f, 1};
        }
        return {1, 1, 1, 1};
    }
}

DebuggerWindow::DebuggerWindow(EmulationSession &session, SDL_Window *window, SDL_Renderer *renderer, std::function<void(uint8_t)> save_state,
                               std::function<void(uint8_t)> load_state) :
    session_(session), machine_(session.Machine()), debugger_(session), code_listing_(machine_, debugger_), save_state_(std::move(save_state)),
    load_state_(std::move(load_state)), window_(window), renderer_(renderer) {
    session_.FocusLost();
    SDL_GetWindowSize(window_, &game_width_, &game_height_);
    SDL_SetWindowSize(window_, std::max(1400, game_width_), std::max(900, game_height_));
    SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    auto &io{ImGui::GetIO()};
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    platform_ready_ = ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
    renderer_ready_ = ImGui_ImplSDLRenderer3_Init(renderer_);
    const auto texture = [&](const int width, const int height) {
        auto *result{SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height)};
        if (result) {
            SDL_SetTextureScaleMode(result, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(result, SDL_BLENDMODE_BLEND);
        }
        return result;
    };
    screen_ = texture(SCREEN_WIDTH, SCREEN_HEIGHT);
    tiles_ = texture(128, 192);
    map_ = texture(256, 256);
    sprites_ = texture(80, 64);
    if (!platform_ready_ || !renderer_ready_ || !screen_ || !tiles_ || !map_ || !sprites_) {
        const auto error{"Could not initialize debugger graphics: " + std::string(SDL_GetError())};
        release_resources();
        SDL_SetWindowSize(window_, game_width_, game_height_);
        SDL_SetRenderLogicalPresentation(renderer_, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        throw std::runtime_error(error);
    }
}

DebuggerWindow::~DebuggerWindow() { release_resources(); }

void DebuggerWindow::release_resources() {
    if (context_) {
        ImGui::SetCurrentContext(context_);
        if (renderer_ready_)
            ImGui_ImplSDLRenderer3_Shutdown();
        if (platform_ready_)
            ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(context_);
    }
    SDL_DestroyTexture(screen_);
    SDL_DestroyTexture(tiles_);
    SDL_DestroyTexture(map_);
    SDL_DestroyTexture(sprites_);
}

void DebuggerWindow::action(const std::function<void()> &run) {
    try {
        run();
        error_.clear();
    } catch (const std::exception &error) {
        debugger_.pause();
        error_ = error.what();
    }
}

void DebuggerWindow::toggle() {
    ImGui::SetCurrentContext(context_);
    ImGui::GetIO().ClearInputKeys();
    ImGui::GetIO().ClearInputMouse();
    session_.FocusLost();
    set_game_input(false);
    visible_ = !visible_;
    if (visible_) {
        SDL_GetWindowSize(window_, &game_width_, &game_height_);
        SDL_SetWindowSize(window_, std::max(1400, game_width_), std::max(900, game_height_));
        SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        debugger_.pause();
    } else {
        SDL_SetWindowSize(window_, game_width_, game_height_);
        SDL_SetRenderLogicalPresentation(renderer_, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    }
}

bool DebuggerWindow::handle_shortcut(const SDL_KeyboardEvent &key) {
    if (key.repeat)
        return false;
    if (key.key == SDLK_F5 && !(key.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL)))
        action([&] { debugger_.paused() ? debugger_.resume() : debugger_.pause(); });
    else if (key.key == SDLK_F10)
        action([&] { debugger_.step_over(); });
    else if (key.key == SDLK_F9)
        action([&] { key.mod & SDL_KMOD_SHIFT ? debugger_.step_out() : debugger_.step_instruction(); });
    else if (key.key == SDLK_F6)
        action([&] { debugger_.step_frame(); });
    else if (key.key == SDLK_F7)
        action([&] { debugger_.step_scanline(); });
    else if (key.key == SDLK_F8)
        action([&] { reset(); });
    else
        return false;
    return true;
}

void DebuggerWindow::reset() {
    debugger_.reset();
    follow_instruction_ = true;
    followed_instruction_.reset();
    code_scroll_ = true;
    set_game_input(false);
}

void DebuggerWindow::set_game_input(const bool active) {
    if (game_input_ == active)
        return;
    game_input_ = active;
    session_.FocusLost();
    auto &io{ImGui::GetIO()};
    io.ClearInputKeys();
    if (active) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::SetWindowFocus(nullptr);
    } else {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }
}

void DebuggerWindow::show_code(const CodeSource source, const uint32_t offset) {
    code_source_ = source;
    code_offset_ = offset;
    follow_instruction_ = false;
    code_scroll_ = true;
}

bool DebuggerWindow::handle_event(const SDL_Event &event) {
    if (!visible_ || SDL_GetWindowFromEvent(&event) != window_)
        return false;
    ImGui::SetCurrentContext(context_);
    auto &io{ImGui::GetIO()};
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        set_game_input(false);
    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_F12 && !event.key.repeat)
            return toggle(), true;
        if ((game_input_ || !io.WantTextInput) && handle_shortcut(event.key))
            return true;
        if ((game_input_ || !io.WantTextInput) && event.key.key == SDLK_F5 && (event.key.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL)))
            return false;
    }
    const auto keyboard_event{event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP};
    if (game_input_ && (keyboard_event || event.type == SDL_EVENT_TEXT_INPUT))
        return event.type == SDL_EVENT_TEXT_INPUT;
    if (platform_ready_)
        ImGui_ImplSDL3_ProcessEvent(&event);
    return event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_TEXT_INPUT;
}

void DebuggerWindow::draw_image(SDL_Texture *texture, const uint32_t *pixels, const int width, const int height, const float scale) {
    if (!texture)
        return;
    SDL_UpdateTexture(texture, nullptr, pixels, width * sizeof(uint32_t));
    ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture)), {width * scale, height * scale});
}

bool DebuggerWindow::begin_panel(const DebugPanel panel, const char *label) {
    const auto selected{requested_panel_ == panel};
    if (selected)
        requested_panel_.reset();
    return ImGui::BeginTabItem(label, nullptr, selected ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None);
}

void DebuggerWindow::render() {
    if (!visible_ || !platform_ready_ || !renderer_ready_)
        return;
    ImGui::SetCurrentContext(context_);
    debugger_.synchronize();
    ImGui_ImplSDL3_NewFrame();
    auto &io{ImGui::GetIO()};
    const auto font_density{std::max(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y)};
    if (font_density_ != font_density) {
        ImGui_ImplSDLRenderer3_DestroyFontsTexture();
        io.Fonts->Clear();
        ImFontConfig font_config;
        font_config.OversampleH = font_config.OversampleV = 1;
        font_config.PixelSnapH = true;
        font_config.RasterizerDensity = font_density;
        io.Fonts->AddFontDefault(&font_config);
        font_density_ = font_density;
    }
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui::NewFrame();
    const auto *viewport{ImGui::GetMainViewport()};
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Debugger", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::Button(debugger_.paused() ? "Run (F5)" : "Pause (F5)"))
        action([&] { debugger_.paused() ? debugger_.resume() : debugger_.pause(); });
    ImGui::SameLine();
    if (ImGui::Button("Into (F9)"))
        action([&] { debugger_.step_instruction(); });
    ImGui::SameLine();
    if (ImGui::Button("Over (F10)"))
        action([&] { debugger_.step_over(); });
    ImGui::SameLine();
    if (ImGui::Button("Out (Shift+F9)"))
        action([&] { debugger_.step_out(); });
    ImGui::SameLine();
    if (ImGui::Button("T-cycle"))
        action([&] { debugger_.step_ticks(machine_.Inspect().speed == Speed::Double ? 1 : 2); });
    ImGui::SameLine();
    if (ImGui::Button("Line (F7)"))
        action([&] { debugger_.step_scanline(); });
    ImGui::SameLine();
    if (ImGui::Button("Frame (F6)"))
        action([&] { debugger_.step_frame(); });
    ImGui::SameLine();
    if (ImGui::Button("Reset ROM (F8)"))
        action([&] { reset(); });
    text(debugger_.status());
    if (!error_.empty())
        ImGui::TextColored({1, 0.5f, 0.4f, 1}, "%s", error_.c_str());
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("State slot", &save_slot_, 1, 7);
    ImGui::SameLine();
    if (ImGui::Button("Save state"))
        action([&] { save_state_(save_slot_); });
    ImGui::SameLine();
    if (ImGui::Button("Load state"))
        action([&] {
            load_state_(save_slot_);
            debugger_.synchronize();
        });
    ImGui::SameLine();
    ImGui::TextDisabled("Hex addresses and values | F12: game/debugger layout");
    ImGui::Separator();
    const auto state{machine_.Inspect()};
    if (ImGui::BeginTable("workspace", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 490);
        ImGui::TableSetupColumn("Inspectors", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        ImGui::BeginChild("cpu", {0, 0});
        draw_cpu(state);
        ImGui::EndChild();
        ImGui::TableNextColumn();
        draw_image(screen_, machine_.GetScreenData(), SCREEN_WIDTH, SCREEN_HEIGHT, 2);
        if (ImGui::IsItemClicked())
            set_game_input(true);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted("Live game view");
        ImGui::TextUnformatted(game_input_ ? "Joypad input active" : "Click the game view for joypad input");
        ImGui::TextUnformatted("Z / X: A / B\nEnter / Backspace: Start / Select\nArrow keys: D-pad");
        ImGui::EndGroup();
        ImGui::Separator();
        ImGui::BeginChild("inspector-content");
        if (ImGui::BeginTabBar("inspectors", ImGuiTabBarFlags_FittingPolicyScroll)) {
            if (begin_panel(DebugPanel::Memory, "Memory")) {
                draw_memory();
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Breakpoints, "Breakpoints")) {
                draw_breakpoints();
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Screen, "Screen")) {
                ImGui::TextWrapped("Live framebuffer. While stepping, completed pixels and rows update as the PPU runs.");
                draw_image(screen_, machine_.GetScreenData(), SCREEN_WIDTH, SCREEN_HEIGHT, 3);
                if (ImGui::IsItemClicked())
                    set_game_input(true);
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Tiles, "Tiles")) {
                draw_graphics(machine_.inspect_graphics());
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Maps, "Maps")) {
                draw_maps(machine_.inspect_graphics());
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Sprites, "Sprites")) {
                draw_sprites(machine_.inspect_graphics());
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Palettes, "Palettes")) {
                draw_palettes(machine_.inspect_graphics());
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Hardware, "Hardware")) {
                draw_hardware(state);
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Trace, "Trace")) {
                draw_trace();
                ImGui::EndTabItem();
            }
            if (begin_panel(DebugPanel::Help, "Help")) {
                draw_help();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
    ImGui::End();
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer_, 20, 22, 28, 255);
    SDL_RenderClear(renderer_);
    float render_scale_x{}, render_scale_y{};
    SDL_GetRenderScale(renderer_, &render_scale_x, &render_scale_y);
    SDL_SetRenderScale(renderer_, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_SetRenderScale(renderer_, render_scale_x, render_scale_y);
    SDL_RenderPresent(renderer_);
}

void DebuggerWindow::draw_cpu(const MachineInspection &state) {
    const auto &cpu{state.cpu};
    text(std::format("{} | {} | {} speed", ModelName(state.model), state.cgbMode ? "CGB" : "DMG",
                     state.speed == Speed::Double ? "Double" : "Normal"));
    text(std::format("Tick {} | Instructions {}", state.tick, machine_.InstructionsCompleted()));
    text(std::format("AF {:04X}   BC {:04X}   DE {:04X}   HL {:04X}", cpu.af, cpu.bc, cpu.de, cpu.hl));
    text(std::format("SP {:04X}   PC {:04X}   IME {}   IE {:02X}   IF {:02X}", cpu.sp, cpu.pc, state.interrupts.masterEnable, state.interrupts.enable,
                     state.interrupts.cpuVisibleFlags));
    text(std::format("Z {}  N {}  H {}  C {} | {}{}{}", bool(cpu.af & 0x80), bool(cpu.af & 0x40), bool(cpu.af & 0x20), bool(cpu.af & 0x10),
                     cpu.halted ? "HALT " : "", cpu.stopped ? "STOP " : "", cpu.locked ? "LOCK " : ""));
    ImGui::TextColored(source_color(code_listing_.execution_source()), "%s", CodeListing::source_label(code_listing_.execution_source()).data());
    ImGui::SameLine();
    text(std::format("{:04X}:{:04X} | opcode {:04X}", machine_.debug_instruction_bank(), machine_.debug_instruction_address(), cpu.instruction));
    text(std::format("{} | M-cycle {} | instruction cycle {}", machine_.debug_instruction_boundary() ? "Boundary" : "In progress", cpu.machineCycle,
                     cpu.instructionCycle));
    text(std::format("PPU mode {} | LY {} | dot {} | pixels {}", static_cast<int>(state.ppu.mode), state.ppu.ly, state.ppu.dot,
                     state.ppu.pixelsDrawn));
    draw_code();
    ImGui::SeparatorText("Stack (raw words)");
    for (auto row{0u}; row < 4; ++row) {
        const auto address{static_cast<uint16_t>(cpu.sp + row * 2)};
        text(std::format("${:04X}: {:04X}", address,
                         debugger_.read_memory(address) | debugger_.read_memory(static_cast<uint16_t>(address + 1)) << 8));
    }
}

void DebuggerWindow::draw_code() {
    ImGui::SeparatorText("Disassembly");
    if (ImGui::Checkbox("Follow instruction", &follow_instruction_)) {
        followed_instruction_.reset();
        code_scroll_ = true;
    }
    ImGui::SameLine();
    auto selected_source{static_cast<int>(code_source_)};
    ImGui::SetNextItemWidth(180);
    if (ImGui::Combo("##code-source", &selected_source, "Mapped memory\0Cartridge ROM\0BIOS\0"))
        show_code(static_cast<CodeSource>(selected_source), 0);
    if (follow_instruction_) {
        const auto identity{uint64_t{machine_.debug_instruction_bank()} << 16 | machine_.debug_instruction_address()};
        const auto source{code_listing_.execution_source()};
        code_scroll_ |= followed_instruction_ != identity || code_source_ != source;
        followed_instruction_ = identity;
        code_source_ = source;
        code_offset_ = code_listing_.execution_offset();
    }
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputScalar(code_source_ == CodeSource::Cartridge ? "File offset" : "Address", ImGuiDataType_U32, &code_offset_, nullptr, nullptr,
                           "%06X", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
        show_code(code_source_, code_offset_);
    if (code_source_ == CodeSource::Cartridge) {
        ImGui::SameLine();
        auto bank{static_cast<uint16_t>(code_offset_ / ROM_BANK_SIZE)};
        if (hex_input("Bank##code", bank))
            show_code(CodeSource::Cartridge, uint32_t{bank} * ROM_BANK_SIZE);
        ImGui::TextDisabled("%zu bytes | %zu banks | all cartridge banks", machine_.rom_bytes().size(),
                            (machine_.rom_bytes().size() + ROM_BANK_SIZE - 1) / ROM_BANK_SIZE);
    } else
        ImGui::TextDisabled(code_source_ == CodeSource::Bios
                                ? "BIOS instructions (amber) | cartridge instructions (blue)"
                                : "Current CPU mapping | BIOS amber, cartridge blue, RAM gray");
    code_listing_.update(code_source_, code_offset_);
    if (!code_listing_.size()) {
        ImGui::TextDisabled("No BIOS loaded. Restart with the BIOS enabled to inspect it.");
        return;
    }
    const auto scroll_row{static_cast<int>(code_listing_.find_row(code_offset_))};
    ImGui::TextDisabled("Click a row for breakpoint / run-to / memory");
    const auto height{std::max(100.0f, ImGui::GetContentRegionAvail().y - 120)};
    if (ImGui::BeginTable("code", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, {0, height})) {
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Bank:Address", ImGuiTableColumnFlags_WidthFixed, 105);
        ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 75);
        ImGui::TableSetupColumn("Instruction");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        if (ImGui::IsWindowHovered() && (ImGui::GetIO().MouseWheel != 0 || ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
            follow_instruction_ = false;
            code_scroll_ = false;
        }
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(code_listing_.size()));
        if (code_scroll_)
            clipper.IncludeItemByIndex(scroll_row);
        while (clipper.Step()) {
            for (auto row{clipper.DisplayStart}; row < clipper.DisplayEnd; ++row) {
                const auto entry{code_listing_.entry(row)};
                const auto &decoded{entry.instruction};
                const auto address{decoded.address};
                const auto bank{entry.bank};
                const auto current{code_listing_.is_current(entry)};
                const auto has_breakpoint{std::ranges::any_of(debugger_.breakpoints, [&](const DebugBreakpoint &point) {
                    return point.enabled && point.kind == DebugStopKind::Execute && point.first <= address && point.last >= address &&
                           (!point.bank || *point.bank == bank);
                })};
                ImGui::PushID(row);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(source_color(entry.source), "%s", CodeListing::source_label(entry.source).data());
                ImGui::TableNextColumn();
                const auto label{std::format("{}{:04X}:{:04X}", has_breakpoint ? "*" : " ", bank, address)};
                if (ImGui::Selectable(label.c_str(), current, ImGuiSelectableFlags_SpanAllColumns))
                    ImGui::OpenPopup("instruction");
                if (ImGui::IsItemHovered() && code_source_ == CodeSource::Cartridge)
                    ImGui::SetTooltip("ROM file offset: %06X", entry.offset);
                if (code_scroll_ && row == scroll_row)
                    ImGui::SetScrollHereY(0.3f);
                if (ImGui::BeginPopup("instruction")) {
                    text(decoded.text);
                    if (ImGui::MenuItem("Add execution breakpoint"))
                        debugger_.breakpoints.push_back({DebugStopKind::Execute, address, address, bank});
                    if (ImGui::MenuItem("Run to here"))
                        action([&] { debugger_.run_to(address, bank); });
                    if (ImGui::MenuItem("Select in memory", nullptr, false, bank == machine_.debug_bank(address))) {
                        memory_address_ = address & 0xFFF0;
                        memory_source_ = 0;
                        memory_follow_pc_ = false;
                        show_panel(DebugPanel::Memory);
                    }
                    ImGui::EndPopup();
                }
                ImGui::TableNextColumn();
                std::string bytes;
                for (const auto value : std::span(decoded.bytes).first(decoded.size))
                    bytes += std::format("{:02X} ", value);
                text(bytes);
                ImGui::TableNextColumn();
                text(decoded.text);
                ImGui::PopID();
            }
        }
        code_scroll_ = false;
        ImGui::EndTable();
    }
}

void DebuggerWindow::draw_memory() {
    ImGui::SetNextItemWidth(180);
    ImGui::Combo("Source", &memory_source_, "Mapped address space\0VRAM bank\0WRAM bank\0");
    ImGui::SameLine();
    if (memory_source_) {
        const auto max_bank{memory_source_ == 1 ? int(IsCgb(machine_.GetModel())) : machine_.IsInCgbMode() ? 7 : 1};
        memory_bank_ = std::clamp(memory_bank_, 0, max_bank);
        ImGui::SetNextItemWidth(100);
        ImGui::SliderInt("Bank", &memory_bank_, 0, max_bank);
    } else
        ImGui::Checkbox("CPU-visible bus", &cpu_memory_);
    if (hex_input(memory_source_ ? "Offset" : "Address", memory_address_))
        memory_follow_pc_ = false;
    ImGui::SameLine();
    ImGui::Checkbox("Follow PC", &memory_follow_pc_);
    if (memory_follow_pc_) {
        memory_source_ = 0;
        memory_address_ = machine_.debug_instruction_address() & 0xFFF0;
    }
    ImGui::SameLine();
    if (ImGui::Button("< page")) {
        memory_address_ -= 256;
        memory_follow_pc_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("page >")) {
        memory_address_ += 256;
        memory_follow_pc_ = false;
    }
    ImGui::TextWrapped(memory_source_
                           ? "Physical bank bytes, independent of the active bank and PPU locks. Click a byte to inspect its address."
                           : "Mapped RAM bypasses bus locks; CPU-visible shows what a CPU read would return. Reads have no side effects.");
    const auto graphics{machine_.inspect_graphics()};
    const auto limit{memory_source_ == 1 ? 0x2000u : memory_source_ == 2 ? 0x1000u : 0x10000u};
    memory_address_ = (memory_address_ & 0xFFF0) % limit;
    if (ImGui::BeginTable("memory", 18, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
                          {0, ImGui::GetTextLineHeightWithSpacing() * 18})) {
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 75);
        for (auto column{0u}; column < 16; ++column)
            ImGui::TableSetupColumn(std::format("{:X}", column).c_str(), ImGuiTableColumnFlags_WidthFixed, 23);
        ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableHeadersRow();
        for (auto row{0u}; row < 16; ++row) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const auto start{static_cast<uint16_t>((memory_address_ + row * 16) % limit)};
            text(std::format("{:04X}", start));
            std::string ascii;
            for (auto column{0u}; column < 16; ++column) {
                const auto address{static_cast<uint16_t>(start + column)};
                const auto value{memory_source_ == 1
                                     ? graphics.vram[memory_bank_ * 0x2000 + address]
                                     : memory_source_ == 2
                                     ? machine_.DebugPeekWram(memory_bank_, address)
                                     : debugger_.read_memory(address, cpu_memory_)};
                ascii += value >= 32 && value < 127 ? static_cast<char>(value) : '.';
                ImGui::TableNextColumn();
                ImGui::PushID(address);
                if (ImGui::Selectable(std::format("{:02X}", value).c_str(), !memory_source_ && edit_address_ == address)) {
                    if (!memory_source_) {
                        edit_address_ = address;
                        edit_value_ = value;
                    }
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%04X : %02X (%u)", address, value, value);
                ImGui::PopID();
            }
            ImGui::TableNextColumn();
            text(ascii);
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Edit mapped RAM");
    hex_input("Address##edit", edit_address_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::InputScalar("Value", ImGuiDataType_U8, &edit_value_, nullptr, nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::BeginDisabled(!debugger_.paused() || memory_source_ != 0);
    if (ImGui::Button("Write byte"))
        action([&] { debugger_.write_memory(edit_address_, edit_value_); });
    ImGui::EndDisabled();
    ImGui::TextWrapped("Writes require a paused CPU and mapped RAM. ROM and I/O writes are rejected. A prefetched opcode is not changed by editing "
            "its RAM byte.");
    if (!memory_source_) {
        text(std::format("Selected ${:04X}, bank {:04X}", edit_address_, machine_.debug_bank(edit_address_)));
        if (ImGui::Button("Watch reads"))
            debugger_.breakpoints.push_back({DebugStopKind::Read, edit_address_, edit_address_, {}});
        ImGui::SameLine();
        if (ImGui::Button("Watch writes"))
            debugger_.breakpoints.push_back({DebugStopKind::Write, edit_address_, edit_address_, {}});
    }
}

void DebuggerWindow::draw_breakpoints() {
    ImGui::SetNextItemWidth(160);
    ImGui::Combo("Type", &breakpoint_kind_, "Execute\0CPU read\0CPU write\0CPU read or write\0");
    hex_input("Start", breakpoint_address_);
    ImGui::SameLine();
    ImGui::Checkbox("Range", &breakpoint_range_);
    if (breakpoint_range_) {
        ImGui::SameLine();
        hex_input("End", breakpoint_end_);
    }
    ImGui::Checkbox("Specific bank", &breakpoint_banked_);
    if (breakpoint_banked_) {
        ImGui::SameLine();
        hex_input("Bank (FFFF = boot ROM)", breakpoint_bank_);
    }
    if (ImGui::Button("Add breakpoint"))
        action([&] {
            const auto end{breakpoint_range_ ? breakpoint_end_ : breakpoint_address_};
            if (end < breakpoint_address_)
                throw std::invalid_argument("Range end must be at or after its start");
            debugger_.breakpoints.push_back({static_cast<DebugStopKind>(breakpoint_kind_), breakpoint_address_, end,
                                             breakpoint_banked_ ? std::optional(breakpoint_bank_) : std::nullopt});
        });
    ImGui::TextWrapped("Execution stops before the selected instruction. Watchpoints stop after the triggering master tick, including blocked CPU "
            "accesses. Resume skips the current execution breakpoint once.");
    ImGui::BeginChild("points");
    constexpr std::array names{"Execute", "Read", "Write", "Read/write"};
    for (auto index{0u}; index < debugger_.breakpoints.size();) {
        auto &point{debugger_.breakpoints[index]};
        ImGui::PushID(index);
        ImGui::Checkbox("##enabled", &point.enabled);
        ImGui::SameLine();
        text(std::format("{} ${:04X}-${:04X} bank {}", names[static_cast<int>(point.kind)], point.first, point.last,
                         point.bank ? std::format("{:04X}", *point.bank) : "any"));
        ImGui::SameLine();
        const auto remove{ImGui::SmallButton("Remove")};
        ImGui::PopID();
        if (remove)
            debugger_.breakpoints.erase(debugger_.breakpoints.begin() + index);
        else
            ++index;
    }
    ImGui::EndChild();
}

void DebuggerWindow::draw_graphics(const GraphicsInspection &graphics) {
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("VRAM bank", &tile_bank_, 0, IsCgb(machine_.GetModel()) ? 1 : 0);
    ImGui::SameLine();
    ImGui::Checkbox("Object palettes", &tile_objects_);
    ImGui::SetNextItemWidth(150);
    ImGui::SliderInt("Palette", &tile_palette_, 0, graphics.cgb_mode ? 7 : tile_objects_ ? 1 : 0);
    tile_palette_ = std::clamp(tile_palette_, 0, graphics.cgb_mode ? 7 : tile_objects_ ? 1 : 0);
    std::array<uint32_t, 128 * 192> pixels{};
    for (auto index{0u}; index < pixels.size(); ++index) {
        const auto x{index % 128};
        const auto y{index / 128};
        const auto color{debug_tile_pixel(graphics, tile_bank_, y / 8 * 16 + x / 8, x % 8, y % 8)};
        pixels[index] = (tile_objects_ ? graphics.object_colors : graphics.background_colors)[tile_palette_ * 4 + color];
    }
    draw_image(tiles_, pixels.data(), 128, 192, 3);
    if (ImGui::IsItemHovered()) {
        const auto origin{ImGui::GetItemRectMin()};
        const auto mouse{ImGui::GetMousePos()};
        const auto tile{int((mouse.y - origin.y) / 24) * 16 + int((mouse.x - origin.x) / 24)};
        ImGui::SetTooltip("Tile %03X | bank %d | $%04X", tile, tile_bank_, VRAM_BEGIN + tile * 16);
    }
    ImGui::TextWrapped(
            "All 384 physical tiles. Hover for tile index and address. Colors use the emulator's current palette mapping and color correction.");
}

void DebuggerWindow::draw_maps(const GraphicsInspection &graphics) {
    ImGui::Checkbox("$9C00 map (otherwise $9800)", &high_map_);
    ImGui::SameLine();
    ImGui::Checkbox("Viewport", &map_overlay_);
    const auto pixels{debug_tile_map(graphics, high_map_)};
    draw_image(map_, pixels.data(), 256, 256, 2);
    const auto origin{ImGui::GetItemRectMin()};
    auto *draw{ImGui::GetWindowDrawList()};
    if (map_overlay_) {
        constexpr std::array wraps{0, -256};
        for (const auto [dx, dy] : std::views::cartesian_product(wraps, wraps)) {
            const auto x{int(graphics.scroll_x) + dx};
            const auto y{int(graphics.scroll_y) + dy};
            draw->PushClipRect(origin, {origin.x + 512, origin.y + 512}, true);
            draw->AddRect({origin.x + x * 2, origin.y + y * 2}, {origin.x + (x + SCREEN_WIDTH) * 2, origin.y + (y + SCREEN_HEIGHT) * 2},
                          IM_COL32(255, 90, 80, 255), 0, 0, 2);
            draw->PopClipRect();
        }
    }
    if (ImGui::IsItemHovered()) {
        const auto mouse{ImGui::GetMousePos()};
        const auto x{static_cast<unsigned>((mouse.x - origin.x) / 16)};
        const auto y{static_cast<unsigned>((mouse.y - origin.y) / 16)};
        const auto offset{(high_map_ ? BACKGROUND_MAP_HIGH - VRAM_BEGIN : BACKGROUND_MAP_LOW - VRAM_BEGIN) + y * 32 + x};
        ImGui::SetTooltip("Map (%u,%u) | $%04X | tile %02X | attributes %02X", x, y, VRAM_BEGIN + offset, graphics.vram[offset],
                          graphics.cgb_mode ? graphics.vram[0x2000 + offset] : 0);
    }
    text(std::format("BG map ${:04X} | Window map ${:04X} | SCX {} SCY {} | WX {} WY {}",
                     Bit<LCDC_BG_TILE_MAP_AREA>(graphics.lcd_control) ? BACKGROUND_MAP_HIGH : BACKGROUND_MAP_LOW,
                     Bit<LCDC_WINDOW_TILE_MAP_AREA>(graphics.lcd_control) ? BACKGROUND_MAP_HIGH : BACKGROUND_MAP_LOW, graphics.scroll_x,
                     graphics.scroll_y, graphics.window_x, graphics.window_y));
    ImGui::TextWrapped("Decoded from current VRAM and LCDC, including signed tile indices and CGB attributes. Red rectangle shows the scrolled BG "
            "viewport. Mid-scanline writes are visible in the live screen, not reconstructed here.");
}

void DebuggerWindow::draw_sprites(const GraphicsInspection &graphics) {
    std::array<uint32_t, 80 * 64> pixels{};
    for (auto index{0u}; index < pixels.size(); ++index)
        pixels[index] = ((index % 80 / 4 + index / 80 / 4) & 1) ? 0xFF555555 : 0xFF333333;
    for (auto sprite{0u}; sprite < OBJ_TOTAL_SPRITES; ++sprite) {
        const auto image{debug_sprite(graphics, sprite)};
        for (auto index{0u}; index < image.size(); ++index) {
            if (image[index] >> 24)
                pixels[(sprite / 10 * 16 + index / 8) * 80 + sprite % 10 * 8 + index % 8] = image[index];
        }
    }
    draw_image(sprites_, pixels.data(), 80, 64, 5);
    if (ImGui::IsItemHovered()) {
        const auto mouse{ImGui::GetMousePos()};
        const auto origin{ImGui::GetItemRectMin()};
        const auto index{int((mouse.y - origin.y) / 80) * 10 + int((mouse.x - origin.x) / 40)};
        ImGui::SetTooltip("OAM sprite %d", index);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            selected_sprite_ = index;
    }
    ImGui::TextWrapped("All 40 OAM entries in OAM order, including offscreen sprites. Checkerboard is transparent color 0. Size, tile bank, flips "
            "and palette are applied.");
    if (ImGui::BeginTable("oam", 7, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders, {0, 280})) {
        constexpr std::array headers{"OAM", "X", "Y", "Tile", "Bank", "Palette", "Flags"};
        for (const auto *header : headers)
            ImGui::TableSetupColumn(header);
        ImGui::TableHeadersRow();
        for (auto index{0u}; index < OBJ_TOTAL_SPRITES; ++index) {
            const auto flags{graphics.oam[index * 4 + 3]};
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(std::format("{:02}", index).c_str(), selected_sprite_ == int(index), ImGuiSelectableFlags_SpanAllColumns))
                selected_sprite_ = index;
            ImGui::TableNextColumn();
            text(std::format("{}", int(graphics.oam[index * 4 + 1]) - 8));
            ImGui::TableNextColumn();
            text(std::format("{}", int(graphics.oam[index * 4]) - 16));
            ImGui::TableNextColumn();
            text(std::format("{:02X}", graphics.oam[index * 4 + 2]));
            ImGui::TableNextColumn();
            text(std::format("{}", graphics.cgb_mode ? (flags >> 3) & 1 : 0));
            ImGui::TableNextColumn();
            text(std::format("{}", graphics.cgb_mode ? flags & 7 : (flags >> 4) & 1));
            ImGui::TableNextColumn();
            text(std::format("{:02X} {}{}{}", flags, flags & 0x80 ? "behind BG " : "", flags & 0x40 ? "Y " : "", flags & 0x20 ? "X" : ""));
        }
        ImGui::EndTable();
    }
    text(std::format("Selected OAM ${:04X}; sprite height {}", OAM_BEGIN + selected_sprite_ * 4, Bit<LCDC_OBJ_SIZE>(graphics.lcd_control) ? 16 : 8));
    if (ImGui::Button("Watch selected OAM writes"))
        debugger_.breakpoints.push_back({DebugStopKind::Write,
                                         static_cast<uint16_t>(OAM_BEGIN + selected_sprite_ * 4),
                                         static_cast<uint16_t>(OAM_BEGIN + selected_sprite_ * 4 + 3),
                                         {}});
}

void DebuggerWindow::draw_palettes(const GraphicsInspection &graphics) {
    ImGui::TextWrapped("Effective rendered colors. Hover a swatch for its raw CGB RGB15 value. Object color 0 is transparent in sprite rendering.");
    constexpr std::array objects{false, true};
    constexpr std::array palettes{0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
    for (const auto [object, palette] : std::views::cartesian_product(objects, palettes)) {
        if (!graphics.cgb_mode && palette >= (object ? 2u : 1u))
            continue;
        ImGui::PushID(object * 8 + palette);
        text(std::format("{} {}", object ? "OBJ" : "BG ", palette));
        for (auto color{0u}; color < 4; ++color) {
            ImGui::SameLine();
            const auto index{palette * 4 + color};
            const auto value{(object ? graphics.object_colors : graphics.background_colors)[index]};
            ImGui::ColorButton(std::format("##{}", color).c_str(), rgba(value), ImGuiColorEditFlags_NoTooltip, {45, 28});
            if (ImGui::IsItemHovered()) {
                const auto raw{(object ? graphics.object_rgb15 : graphics.background_rgb15)[index]};
                ImGui::SetTooltip("Color %u | RGB15 %04X | R %u G %u B %u%s", color, raw, raw & 31, (raw >> 5) & 31, (raw >> 10) & 31,
                                  object && !color ? " | transparent" : "");
            }
        }
        ImGui::PopID();
    }
    text(std::format("BGP {:02X}   OBP0 {:02X}   OBP1 {:02X}", machine_.InspectRegister(DMG_BACKGROUND_PALETTE).stored,
                     machine_.InspectRegister(DMG_OBJ_PALETTE_0).stored, machine_.InspectRegister(DMG_OBJ_PALETTE_1).stored));
}

void DebuggerWindow::draw_hardware(const MachineInspection &state) {
    ImGui::BeginChild("hardware");
    ImGui::SeparatorText("PPU pipeline");
    text(std::format("Mode {} | LY {} dot {} | fetcher {} address ${:04X}", static_cast<int>(state.ppu.mode), state.ppu.ly, state.ppu.dot,
                     static_cast<int>(state.ppu.fetcherState), state.ppu.fetchAddress));
    text(std::format("BG FIFO {} | OBJ FIFO {} | tile {:02X} row {} | planes {:02X}/{:02X}", state.ppu.backgroundQueueSize, state.ppu.spriteQueueSize,
                     state.ppu.tile, state.ppu.tileRow, state.ppu.lowPlane, state.ppu.highPlane));
    text(std::format("Discard {} | window line {} triggered {} fetching {} | sprite fetch {} | STAT IRQ {}", state.ppu.scrollDiscard,
                     state.ppu.windowLine, state.ppu.windowTriggered, state.ppu.fetchingWindow, state.ppu.fetchingSprite, state.ppu.statRequestLine));
    if (ImGui::BeginTable("registers", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        constexpr std::array headings{"Address", "Stored", "CPU read", "Pending", "Stage"};
        for (const auto *heading : headings)
            ImGui::TableSetupColumn(heading);
        ImGui::TableHeadersRow();
        for (const auto &reg : state.ppu.registers) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            text(std::format("{:04X}", reg.address));
            ImGui::TableNextColumn();
            text(std::format("{:02X}", reg.stored));
            ImGui::TableNextColumn();
            text(std::format("{:02X}", reg.cpuVisible));
            ImGui::TableNextColumn();
            text(reg.pending ? std::format("{:02X}", *reg.pending) : "-");
            ImGui::TableNextColumn();
            text(std::format("{}", reg.pendingStage));
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Timer and interrupts");
    text(std::format("DIV {:04X} | TIMA {:02X} TMA {:02X} TAC {:02X} | reload delay {} pending {} active {}", state.timer.divider, state.timer.tima,
                     state.timer.tma, state.timer.tac, state.timer.reloadDelay, state.timer.overflowPending, state.timer.reloadActive));
    text(std::format("IE {:02X} | IF stored {:02X} visible {:02X} | delayed {:02X} | IME {} pending {}", state.interrupts.enable,
                     state.interrupts.storedFlags, state.interrupts.cpuVisibleFlags, state.interrupts.delayedRequests, state.interrupts.masterEnable,
                     state.interrupts.enablePending));
    ImGui::SeparatorText("DMA");
    text(std::format("OAM DMA active {} | source ${:04X} offset {:02X} | latch {:02X} | restart {}", state.dma.active, state.dma.source,
                     state.dma.destinationOffset, state.dma.busLatch, state.dma.restartPending));
    text(std::format("HDMA {} | CPU halted {} | control {:02X}", state.dma.hdma.hdmaActive, state.dma.hdma.ShouldHaltCPU(),
                     machine_.InspectRegister(HDMA_CONTROL).cpuVisible));
    ImGui::SeparatorText("Audio");
    text(std::format("NR52 {:02X} | sequencer {} | PCM {:02X}/{:02X} | deferred ticks {}", state.audio.cpuVisibleControl,
                     state.audio.frameSequencerStep, state.audio.pcm12, state.audio.pcm34, state.audio.deferredTicks));
    for (auto index{0u}; index < state.audio.effectiveChannels.size(); ++index) {
        const auto &channel{state.audio.effectiveChannels[index]};
        text(std::format("Channel {}: enabled {} DAC {} | digital {:X} | analog {:.3f}", index + 1, channel.enabled, channel.dacEnabled,
                         channel.digitalOutput, channel.analogOutput));
    }
    ImGui::SeparatorText("Serial / joypad");
    text(std::format("SB {:02X} SC {:02X} | buttons {:02X}", machine_.InspectCpuRead(SERIAL_TRANSFER_DATA),
                     machine_.InspectCpuRead(SERIAL_TRANSFER_CONTROL), machine_.PressedButtons()));
    ImGui::EndChild();
}

void DebuggerWindow::draw_trace() {
    if (ImGui::Button("Clear trace"))
        debugger_.clear_trace();
    ImGui::SameLine();
    ImGui::TextDisabled("Last 512 instruction starts / interrupt vectors; newest first");
    ImGui::TextWrapped("Opcodes are latched CPU values; operand bytes are sampled from mapped memory at instruction start.");
    ImGui::BeginChild("trace");
    for (auto index{debugger_.trace_size()}; index > 0; --index) {
        const auto &entry{debugger_.trace_entry(index - 1)};
        const auto &event{entry.event};
        const auto description{event.kind == diagnostics::EventKind::InstructionBegin
                                   ? disassemble(event.pc, entry.bytes).text
                                   : std::format("Interrupt vector ${:04X}", event.address)};
        text(std::format("{:12}  {:04X}:{:04X}  {}", event.position.tick, event.pcBank, event.pc, description));
    }
    ImGui::EndChild();
}

void DebuggerWindow::draw_help() {
    ImGui::TextWrapped("F5: run/pause. F9: step into. F10: step over CALL/RST. Shift+F9: step out using the return address at SP. F7: next "
            "scanline. F6: finish the current emulation frame. F8: reset the ROM and pause at startup. F12: switch between game and "
            "debugger layouts.");
    ImGui::Separator();
    ImGui::TextWrapped("Lua scripts: Shift+F5 starts/stops the selected script. Ctrl+F5 reloads an active script while debugging. In the normal game "
            "layout, F5 retains its script-reload shortcut.");
    ImGui::TextWrapped("Click the live game view for joypad input. Clicking debugger controls returns keyboard focus to the debugger. Debugger "
            "typing never reaches the game. Execution breakpoints match the prefetched instruction address and bank; the PC register can "
            "already point to an operand. Disassembly bytes can differ from a previously prefetched opcode.");
    ImGui::TextWrapped("Cartridge ROM shows every bank, including banks that are not currently mapped. Jump by bank or ROM file offset, or scroll "
            "through the entire file. BIOS instructions are amber and labeled BIOS; cartridge instructions are blue and labeled ROM. "
            "Follow instruction selects the executing source automatically. Scrolling or choosing a source disables following. "
            "Data is also decoded as instructions; jump to an exact offset to decode from that byte.");
    ImGui::TextWrapped(
            "Reset restarts with the original model and BIOS settings, preserves cartridge RAM, RTC and breakpoints, and clears the "
            "trace and held buttons. Lua receives its state-load callback. F8 also resets in the game layout, preserving run/pause state.");
    ImGui::Separator();
    ImGui::TextWrapped("Into completes the current instruction, including CB prefixes. HALT/STOP/LOCK are reported instead of spinning. Use clock, "
            "line, frame or Run to advance peripherals. Read/write watchpoints may stop midway through an instruction; Into finishes it. "
            "Over and Out run asynchronously and can be interrupted with Pause or any breakpoint.");
    ImGui::Separator();
    ImGui::TextWrapped("Step out assumes SP points to the return address; after a function pushes locals, run to its epilogue first. Bank FFFF "
            "denotes the boot ROM. Frame stepping follows the session's 70224-dot frame interval, including when the LCD is disabled.");
    ImGui::Separator();
    ImGui::TextWrapped("Inspectors read snapshots without advancing or modifying the machine. Raw tile/map/sprite views decode current graphics "
            "memory; the Screen tab shows actual composited pixels. Lua frame callbacks run once per session frame even when that frame "
            "is split into many debugger steps. Save states use the same slots as the game window.");
}
