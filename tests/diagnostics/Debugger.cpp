#include "../../frontends/sdl/GameboyFiles.h"
#include "Debugger.h"
#include "CodeListing.h"
#include <doctest/doctest.h>
#include "DebugGraphics.h"

namespace {
    template<class T>
    consteval std::meta::info debug_member(const std::string_view name) {
        for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::identifier_of(member) == name)
                return member;
        throw "Unknown debugger test member";
    }

    constexpr std::array debug_models{Model::DMGB, Model::CGBC, Model::CGBE};

    void install_program(Gameboy &machine, const std::span<const uint8_t> bytes) {
        for (auto index{0u}; index < bytes.size(); ++index)
            machine.DebugPoke(WRAM_BEGIN + index, bytes[index]);
        auto &cpu{machine.[:debug_member<Gameboy>("cpu_"):]};
        cpu.pc(WRAM_BEGIN + 1);
        cpu.sp(0xD000);
        cpu.currentInstruction = bytes[0];
        cpu.[:debug_member<CPU<Bus>>("instructionAddress_"):] = WRAM_BEGIN;
        cpu.[:debug_member<CPU<Bus>>("instructionBank_"):] = 0;
    }

    void install_calls(Gameboy &machine) {
        constexpr std::array program{0xCD_u8, 0x10_u8, 0xC0_u8, 0x76_u8};
        install_program(machine, program);
        machine.DebugPoke(0xC010, 0xCD);
        machine.DebugPoke(0xC011, 0x20);
        machine.DebugPoke(0xC012, 0xC0);
        machine.DebugPoke(0xC013, 0xC9);
        machine.DebugPoke(0xC020, 0x04);
        machine.DebugPoke(0xC021, 0xC9);
    }
}

TEST_CASE("debugger: disassembly decodes immediates signed branches prefixes calls and illegal opcodes") {
    struct Example {
        std::array<uint8_t, 3> bytes;
        uint16_t address;
        unsigned size;
        std::string_view text;
        bool call{};
    };
    constexpr std::array examples{Example{{0x00, 0, 0}, 0, 1, "NOP"},
                                  Example{{0x01, 0x34, 0x12}, 0, 3, "LD BC, $1234"},
                                  Example{{0x08, 0xFE, 0xFF}, 0, 3, "LD ($FFFE), SP"},
                                  Example{{0x10, 0, 0}, 0, 2, "STOP"},
                                  Example{{0x18, 0xFD, 0}, 0xC001, 2, "JR $C000"},
                                  Example{{0x20, 0xFE, 0}, 0xFFFE, 2, "JR NZ, $FFFE"},
                                  Example{{0x36, 0x42, 0}, 0, 2, "LD (HL), $42"},
                                  Example{{0x76, 0, 0}, 0, 1, "HALT"},
                                  Example{{0xCB, 0x11, 0}, 0, 2, "RL C"},
                                  Example{{0xCB, 0x7E, 0}, 0, 2, "BIT 7, (HL)"},
                                  Example{{0xCB, 0x86, 0}, 0, 2, "RES 0, (HL)"},
                                  Example{{0xCB, 0xFF, 0}, 0, 2, "SET 7, A"},
                                  Example{{0xCC, 0x10, 0xC0}, 0, 3, "CALL Z, $C010", true},
                                  Example{{0xCD, 0x10, 0xC0}, 0, 3, "CALL $C010", true},
                                  Example{{0xD9, 0, 0}, 0, 1, "RETI"},
                                  Example{{0xE8, 0x80, 0}, 0, 2, "ADD SP, -128"},
                                  Example{{0xE9, 0, 0}, 0, 1, "JP HL"},
                                  Example{{0xF8, 0x7F, 0}, 0, 2, "LD HL, SP+127"},
                                  Example{{0xFF, 0, 0}, 0, 1, "RST $38", true},
                                  Example{{0xD3, 0, 0}, 0, 1, "LOCK $D3"}};
    for (const auto &example : examples) {
        const auto decoded{disassemble(example.address, example.bytes)};
        CHECK(decoded.text == example.text);
        CHECK(decoded.size == example.size);
        CHECK(decoded.call == example.call);
    }
    for (auto opcode{0u}; opcode < 256; ++opcode) {
        const auto decoded{disassemble(0, {static_cast<uint8_t>(opcode), 0, 0})};
        CHECK_FALSE(decoded.text.empty());
        CHECK(decoded.size >= 1);
        CHECK(decoded.size <= 3);
        CHECK(disassemble(0, {0xCB, static_cast<uint8_t>(opcode), 0}).size == 2);
    }
}

TEST_CASE("debugger: execution breakpoints stop before prefetched instructions and resume once") {
    for (const auto model : debug_models) {
        auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}});
        constexpr std::array program{0x00_u8, 0x18_u8, 0xFD_u8};
        install_program(machine, program);
        EmulationSession session(machine);
        Debugger debugger(session);
        debugger.breakpoints.push_back({DebugStopKind::Execute, WRAM_BEGIN, WRAM_BEGIN, 0});
        const auto before{machine.SaveState()};
        debugger.resume();
        debugger.pump(100);
        CHECK(debugger.paused());
        CHECK(machine.SaveState() == before);
        CHECK(machine.InspectCpu().pc == WRAM_BEGIN + 1);
        debugger.resume();
        debugger.pump(100);
        CHECK(debugger.paused());
        CHECK(machine.debug_instruction_address() == WRAM_BEGIN);
        CHECK(machine.InstructionsCompleted() == 2);
        CHECK(machine.TimelineTicks() > 0);
        debugger.breakpoints[0].bank = 1;
        debugger.resume();
        const auto tick{machine.TimelineTicks()};
        debugger.pump(100);
        CHECK_FALSE(debugger.paused());
        CHECK(machine.TimelineTicks() == tick + 100);
    }
}

TEST_CASE("debugger: watchpoint stops and resumes are equivalent to continuous execution") {
    constexpr std::array speeds{false, true};
    for (const auto [model, double_speed] : std::views::cartesian_product(debug_models, speeds)) {
        if (IsDmg(model) && double_speed)
            continue;
        const host::LaunchSettings settings{.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}};
        auto machine = host::load_gameboy(settings);
        auto continuous = host::load_gameboy(settings);
        for (auto *target : std::array{&machine, &continuous}) {
            constexpr std::array program{0x3E_u8, 0x42_u8, 0xEA_u8, 0x00_u8, 0xC1_u8, 0x18_u8, 0xF9_u8};
            install_program(*target, program);
            if (double_speed) {
                auto &bus{target->[:debug_member<Gameboy>("bus_"):]};
                bus.prepareSpeedShift = true;
                bus.ChangeSpeed();
            }
        }
        EmulationSession session(machine);
        Debugger debugger(session);
        debugger.breakpoints.push_back({DebugStopKind::Write, 0xC100, 0xC100, {}});
        debugger.resume();
        debugger.pump(1000);
        REQUIRE(debugger.paused());
        CHECK(machine.DebugPeek(0xC100) == 0x42);
        CHECK(machine.InspectCpu().instructionRunning);
        static_cast<void>(continuous.AdvanceTicks(machine.TimelineTicks()));
        CHECK(machine.SaveState() == continuous.SaveState());
        const auto instructions{machine.InstructionsCompleted()};
        debugger.step_instruction();
        CHECK(machine.InstructionsCompleted() == instructions + 1);
        debugger.resume();
        debugger.pump(1000);
        REQUIRE(debugger.paused());
        static_cast<void>(continuous.AdvanceTicks(machine.TimelineTicks() - continuous.TimelineTicks()));
        CHECK(machine.SaveState() == continuous.SaveState());
    }
}

TEST_CASE("debugger: step over and out respect nested calls and stack depth") {
    constexpr std::array step_out{false, true};
    for (const auto [model, out] : std::views::cartesian_product(debug_models, step_out)) {
        auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}});
        install_calls(machine);
        EmulationSession session(machine);
        Debugger debugger(session);
        if (out) {
            debugger.step_instruction();
            REQUIRE(machine.debug_instruction_address() == 0xC010);
            debugger.step_out();
        } else
            debugger.step_over();
        REQUIRE_FALSE(debugger.paused());
        debugger.pump(1000);
        CHECK(debugger.paused());
        CHECK(machine.debug_instruction_address() == 0xC003);
        CHECK(machine.InspectCpu().sp == 0xD000);
        CHECK(machine.InstructionsCompleted() == 5);
    }
}

TEST_CASE("debugger: step over can be interrupted by a user breakpoint") {
    auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
    install_calls(machine);
    EmulationSession session(machine);
    Debugger debugger(session);
    debugger.breakpoints.push_back({DebugStopKind::Execute, 0xC020, 0xC020, {}});
    debugger.step_over();
    debugger.pump(1000);
    CHECK(debugger.paused());
    CHECK(machine.debug_instruction_address() == 0xC020);
    CHECK(machine.InstructionsCompleted() == 2);
}

TEST_CASE("debugger: an interrupt vector breakpoint stops before the handler executes") {
    for (const auto model : debug_models) {
        auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}});
        auto &cpu{machine.[:debug_member<Gameboy>("cpu_"):]};
        cpu.pc(0x40);
        cpu.[:debug_member<CPU<Bus>>("interruptState"):] = InterruptState::M6;
        EmulationSession session(machine);
        Debugger debugger(session);
        debugger.breakpoints.push_back({DebugStopKind::Execute, 0x40, 0x40, 0});
        const auto state{machine.SaveState()};
        debugger.resume();
        debugger.pump(100);
        REQUIRE(debugger.paused());
        CHECK(machine.SaveState() == state);
        CHECK(machine.debug_instruction_address() == 0x40);
        debugger.step_instruction();
        CHECK(machine.InstructionsCompleted() == 1);
        CHECK(machine.TimelineTicks() > 0);
    }
}

TEST_CASE("debugger: CB stepping and idle CPU controls have bounded progress") {
    auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
    constexpr std::array program{0xCB_u8, 0x00_u8, 0x76_u8, 0x00_u8};
    install_program(machine, program);
    EmulationSession session(machine);
    Debugger debugger(session);
    debugger.step_instruction();
    CHECK(machine.InstructionsCompleted() == 1);
    CHECK(machine.debug_instruction_address() == WRAM_BEGIN + 2);
    debugger.step_instruction();
    REQUIRE(machine.InspectCpu().halted);
    const auto tick{machine.TimelineTicks()};
    debugger.step_instruction();
    CHECK(machine.TimelineTicks() == tick);
    CHECK(debugger.status().find("halted") != std::string::npos);
    debugger.step_scanline();
    CHECK(machine.TimelineTicks() > tick);
    CHECK(machine.TimelineTicks() <= tick + SCANLINE_CYCLES * 2);
    debugger.step_frame();
    CHECK(machine.TimelineTicks() == Gameboy::FRAME_CYCLES_DMG * 2);
}

TEST_CASE("debugger: partial frame stepping preserves frame callbacks inputs and machine state") {
    const host::LaunchSettings settings{.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}};
    auto machine = host::load_gameboy(settings);
    auto continuous = host::load_gameboy(settings);
    EmulationSession session(machine);
    EmulationSession other_session(continuous);
    unsigned before_frames{};
    unsigned after_frames{};
    session.SetEventHandler([&](const SessionEvent event, uint64_t) {
        if (event == SessionEvent::BeforeFrame) {
            ++before_frames;
            session.SetOverrides(0xFF, 0x11);
        } else if (event == SessionEvent::AfterFrame)
            ++after_frames;
    });
    other_session.SetEventHandler([&](const SessionEvent event, uint64_t) {
        if (event == SessionEvent::BeforeFrame)
            other_session.SetOverrides(0xFF, 0x11);
    });
    Debugger debugger(session);
    for (auto step{0u}; step < 20; ++step)
        debugger.step_instruction();
    CHECK(before_frames == 1);
    CHECK(after_frames == 0);
    debugger.step_frame();
    REQUIRE(other_session.RunFrame());
    CHECK(before_frames == 1);
    CHECK(after_frames == 1);
    CHECK(session.FrameCount() == 1);
    CHECK(machine.SaveState() == continuous.SaveState());
    CHECK(std::ranges::equal(std::span(machine.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT),
                             std::span(continuous.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT)));
    const auto snapshot{machine.SaveState()};
    debugger.step_ticks(17);
    REQUIRE(session.LoadState(snapshot));
    debugger.synchronize();
    CHECK(debugger.paused());
    CHECK(debugger.trace_size() == 0);
    debugger.step_frame();
    REQUIRE(other_session.RunFrame());
    CHECK(before_frames == 3);
    CHECK(after_frames == 2);
    CHECK(machine.SaveState() == continuous.SaveState());
}

TEST_CASE("debugger: memory graphics and trace inspection never change machine state") {
    for (const auto model : debug_models) {
        auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}});
        EmulationSession session(machine);
        Debugger debugger(session);
        debugger.step_frame();
        const auto snapshot{machine.SaveDebugState()};
        for (auto address{0u}; address < 0x10000; ++address) {
            static_cast<void>(debugger.read_memory(address));
            static_cast<void>(debugger.instruction(address));
        }
        const auto graphics{machine.inspect_graphics()};
        static_cast<void>(debug_tile_map(graphics, false));
        static_cast<void>(debug_tile_map(graphics, true));
        for (auto index{0u}; index < OBJ_TOTAL_SPRITES; ++index)
            static_cast<void>(debug_sprite(graphics, index));
        for (auto index{0u}; index < debugger.trace_size(); ++index)
            static_cast<void>(debugger.trace_entry(index));
        CHECK(machine.SaveDebugState() == snapshot);
        CHECK_THROWS_AS(debugger.write_memory(0, 0), std::invalid_argument);
        CHECK_THROWS_AS(debugger.write_memory(LCD_CONTROL_BIT, 0), std::invalid_argument);
        debugger.write_memory(WRAM_BEGIN, 0x42);
        CHECK(machine.DebugPeek(WRAM_BEGIN) == 0x42);
        debugger.resume();
        CHECK_THROWS_AS(debugger.write_memory(WRAM_BEGIN, 0), std::logic_error);
    }
}

TEST_CASE("debugger: graphics decoding handles signed tile addressing banks flips and sprite transparency") {
    GraphicsInspection graphics{};
    graphics.cgb_mode = true;
    graphics.background_colors[2 * 4 + 1] = 0xFF112233;
    graphics.vram[BACKGROUND_MAP_LOW - VRAM_BEGIN] = 0xFF;
    graphics.vram[0x2000 + BACKGROUND_MAP_LOW - VRAM_BEGIN] = 0x6A;
    graphics.vram[0x2000 + 255 * 16 + 14] = 1;
    CHECK(debug_tile_map(graphics, false)[0] == 0xFF112233);
    CHECK(debug_tile_map(graphics, true)[0] == 0);
    graphics.lcd_control = 1 << LCDC_OBJ_SIZE;
    graphics.oam[2] = 3;
    graphics.oam[3] = 0x69;
    graphics.object_colors[1 * 4 + 2] = 0xFF445566;
    graphics.vram[0x2000 + 3 * 16 + 15] = 1;
    const auto sprite{debug_sprite(graphics, 0)};
    CHECK(sprite[0] == 0xFF445566);
    CHECK(sprite[1] == 0);
    CHECK(sprite[15 * 8] == 0);
    CHECK_THROWS_AS(static_cast<void>(debug_sprite(graphics, 40)), std::out_of_range);
    CHECK_THROWS_AS(static_cast<void>(debug_tile_pixel(graphics, 2, 0, 0, 0)), std::out_of_range);
}

TEST_CASE("debugger: read watchpoints honor ranges and banks and inspection never triggers them") {
    auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
    constexpr std::array program{0xFA_u8, 0x00_u8, 0xD1_u8, 0x18_u8, 0xFB_u8};
    install_program(machine, program);
    machine.DebugPoke(0xD100, 0x42);
    EmulationSession session(machine);
    Debugger debugger(session);
    debugger.breakpoints.push_back({DebugStopKind::Read, 0xD100, 0xD10F, 2});
    debugger.resume();
    debugger.pump(100);
    CHECK_FALSE(debugger.paused());
    debugger.breakpoints[0].bank = 1;
    CHECK(debugger.read_memory(0xD100, true) == 0x42);
    CHECK_FALSE(debugger.paused());
    debugger.pump(100);
    REQUIRE(debugger.paused());
    CHECK(debugger.status().find("read $D100 = $42") != std::string::npos);
    CHECK(machine.InspectCpu().instructionRunning);
    debugger.step_instruction();
    CHECK((machine.InspectCpu().af >> 8) == 0x42);
}

TEST_CASE("debugger: STOP LOCK and an unreachable run target remain interruptible") {
    constexpr std::array states{ExecutionStopReason::Stopped, ExecutionStopReason::Locked};
    for (const auto state : states) {
        auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
        auto &cpu{machine.[:debug_member<Gameboy>("cpu_"):]};
        if (state == ExecutionStopReason::Stopped)
            cpu.stopped(true);
        else
            cpu.Lock();
        EmulationSession session(machine);
        Debugger debugger(session);
        const auto snapshot{machine.SaveState()};
        debugger.step_instruction();
        CHECK(machine.SaveState() == snapshot);
        CHECK(debugger.paused());
        debugger.run_to(0x1234);
        debugger.pump(101);
        CHECK(machine.TimelineTicks() == 101);
        CHECK_FALSE(debugger.paused());
        debugger.pause();
        debugger.pump(1000);
        CHECK(machine.TimelineTicks() == 101);
    }
}

TEST_CASE("debugger: reset restores startup hardware and BIOS while retaining cartridge RAM RTC and breakpoints") {
    constexpr std::array boot_modes{false, true};
    constexpr std::array rtc_modes{false, true};
    for (const auto [model, skip_boot, real_clock] : std::views::cartesian_product(debug_models, boot_modes, rtc_modes)) {
        const host::LaunchSettings settings{.rom_path = "roms/acid/cgb-acid2.gbc",
                                            .emulation = {.model = model, .no_bootrom = skip_boot, .real_rtc = real_clock}};
        auto machine = host::load_gameboy(settings);
        auto fresh = host::load_gameboy(settings);
        auto &cartridge{machine.[:debug_member<Gameboy>("cartridge"):]};
        auto &fresh_cartridge{fresh.[:debug_member<Gameboy>("cartridge"):]};
        cartridge.[:debug_member<Cartridge>("game_ram_size"):] = Cartridge::MAX_RAM_SIZE;
        fresh_cartridge.[:debug_member<Cartridge>("game_ram_size"):] = Cartridge::MAX_RAM_SIZE;
        EmulationSession session(machine);
        Debugger debugger(session);
        const auto startup_address{machine.debug_instruction_address()};
        const auto startup_bank{machine.debug_instruction_bank()};
        debugger.breakpoints.push_back({DebugStopKind::Execute, startup_address, startup_address, startup_bank});
        debugger.step_frame();
        debugger.step_ticks(23);
        REQUIRE(machine.TimelineTicks() > 0);
        REQUIRE(debugger.trace_size() > 0);
        machine.DebugPoke(WRAM_BEGIN, 0x42);
        cartridge.[:debug_member<Cartridge>("game_ram"):].front() = 0xAB;
        cartridge.[:debug_member<Cartridge>("game_ram"):].back() = 0xCD;
        auto &rtc{machine.[:debug_member<Gameboy>("rtc_"):]};
        rtc.write_rtc(0x08, 37);
        rtc.write_rtc(0x09, 12);
        rtc.counter = 12345;
        const auto clock{rtc};
        fresh.[:debug_member<Gameboy>("rtc_"):] = clock;
        fresh_cartridge.restore_ram(cartridge.ram_bytes());
        const auto expected{fresh.SaveState()};
        session.SetPhysicalButton(Keys::A, true);
        session.SetOverrides(0xFF, 0x80);
        session.QueueState(machine.SaveState());
        unsigned loads{};
        unsigned before_frames{};
        session.SetEventHandler([&](const SessionEvent event, const uint64_t frame) {
            if (event == SessionEvent::StateLoaded) {
                ++loads;
                CHECK(frame == 0);
            }
            if (event == SessionEvent::BeforeFrame)
                ++before_frames;
        });
        debugger.run_to(0x1234);
        debugger.reset();
        CHECK(machine.SaveState() == expected);
        CHECK(machine.TimelineTicks() == 0);
        CHECK(machine.InstructionsCompleted() == 0);
        CHECK(machine.debug_instruction_address() == startup_address);
        CHECK(machine.debug_instruction_bank() == startup_bank);
        CHECK(machine.GetModel() == model);
        CHECK(machine.UsesRealRTC() == real_clock);
        CHECK(rtc.[:debug_member<RealTimeClock>("zero_time"):] == clock.[:debug_member<RealTimeClock>("zero_time"):]);
        CHECK(machine.PressedButtons() == 0);
        CHECK(session.PhysicalButtons() == 0);
        CHECK(session.FrameCount() == 0);
        CHECK(session.StateGeneration() == 1);
        CHECK(session.frame_ticks_remaining() == Gameboy::FRAME_CYCLES_DMG * 2);
        CHECK(debugger.paused());
        CHECK(debugger.trace_size() == 0);
        REQUIRE(debugger.breakpoints.size() == 1);
        CHECK(debugger.breakpoints[0].bank == startup_bank);
        CHECK(loads == 1);
        CHECK(machine.ConsumeFrame());
        std::array<float, 16> audio{};
        CHECK(machine.ReadAudioSamples(audio.data(), audio.size()) == 0);
        debugger.resume();
        debugger.pump(100);
        REQUIRE(debugger.paused());
        CHECK(machine.TimelineTicks() == 0);
        CHECK(before_frames == 1);
        debugger.step_instruction();
        CHECK(machine.InstructionsCompleted() == 1);
        CHECK(machine.TimelineTicks() > 0);
        CHECK(before_frames == 1);
        CHECK(machine.PressedButtons() == 0);
    }
}

TEST_CASE("debugger: session reset preserves playback state and rejects resets during frame callbacks") {
    constexpr std::array paused_states{false, true};
    for (const auto paused : paused_states) {
        auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
        EmulationSession session(machine);
        session.SetEventHandler([&](const SessionEvent event, uint64_t) {
            if (event == SessionEvent::BeforeFrame)
                CHECK_THROWS_AS(session.reset(), std::logic_error);
        });
        REQUIRE(session.RunFrame());
        session.SetPaused(paused);
        session.reset();
        CHECK(session.Paused() == paused);
        CHECK(machine.TimelineTicks() == 0);
        CHECK(session.FrameCount() == 0);
    }
}

TEST_CASE("debugger: code listing covers all physical ROM banks and handles truncated boundary instructions") {
    constexpr std::array bank_counts{4u, 512u};
    for (const auto bank_count : bank_counts) {
        auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
        auto &cartridge{machine.[:debug_member<Gameboy>("cartridge"):]};
        auto &rom{cartridge.[:debug_member<Cartridge>("game_rom"):]};
        rom.assign(bank_count * ROM_BANK_SIZE, 0);
        rom[0x100] = 0x01;
        rom[0x101] = 0x01;
        rom[0x103] = 0x3E;
        rom[ROM_BANK_SIZE - 1] = 0xCD;
        rom[2 * ROM_BANK_SIZE] = 0x3E;
        rom[2 * ROM_BANK_SIZE + 1] = 0x42;
        rom.back() = 0xCB;
        EmulationSession session(machine);
        Debugger debugger(session);
        CodeListing listing(machine, debugger);
        const auto snapshot{machine.SaveDebugState()};
        listing.update(CodeSource::Cartridge);
        CHECK(listing.size() > 64);
        CHECK(listing.entry(0).offset == 0);
        const auto end_of_bank{listing.entry(listing.find_row(ROM_BANK_SIZE - 1))};
        CHECK(end_of_bank.instruction.text == "DB $CD");
        CHECK(end_of_bank.instruction.size == 1);
        const auto inactive_bank{listing.entry(listing.find_row(2 * ROM_BANK_SIZE))};
        CHECK(inactive_bank.bank == 2);
        CHECK(inactive_bank.source == CodeSource::Cartridge);
        CHECK(inactive_bank.instruction.address == ROM_BANK_SIZE);
        CHECK(inactive_bank.instruction.text == "LD A, $42");
        CHECK_FALSE(listing.is_current(inactive_bank));
        const auto last{listing.entry(listing.size() - 1)};
        CHECK(last.offset == rom.size() - 1);
        CHECK(last.bank == bank_count - 1);
        CHECK(last.instruction.address == VRAM_BEGIN - 1);
        CHECK(last.instruction.text == "DB $CB");
        CHECK(listing.find_row(0xFFFFFFFF) == listing.size() - 1);
        CHECK_THROWS_AS(static_cast<void>(listing.entry(listing.size())), std::out_of_range);
        listing.update(CodeSource::Cartridge, 2 * ROM_BANK_SIZE + 1);
        CHECK(listing.entry(listing.find_row(2 * ROM_BANK_SIZE + 1)).instruction.text == "LD B, D");
        listing.update(CodeSource::Cartridge, 0x101);
        const auto anchored_row{listing.find_row(0x101)};
        CHECK(listing.entry(anchored_row).instruction.text == "LD BC, $3E00");
        CHECK(listing.entry(anchored_row + 1).offset == 0x104);
        CHECK(listing.entry(anchored_row + 2).offset == 0x105);
        CHECK(machine.SaveDebugState() == snapshot);
    }
}

TEST_CASE("debugger: BIOS listing remains inspectable after unmapping and distinguishes cartridge overlay gaps") {
    for (const auto model : debug_models) {
        auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model}});
        EmulationSession session(machine);
        Debugger debugger(session);
        CodeListing listing(machine, debugger);
        const auto snapshot{machine.SaveDebugState()};
        REQUIRE_FALSE(machine.BootromBytes().empty());
        REQUIRE(listing.execution_source() == CodeSource::Bios);
        listing.update(CodeSource::Bios, listing.execution_offset());
        const auto current{listing.entry(listing.find_row(listing.execution_offset()))};
        CHECK(listing.is_current(current));
        CHECK(current.bank == BOOT_ROM_BANK);
        CHECK(current.instruction.bytes[0] == machine.BootromBytes()[current.offset]);
        for (auto row{0u}; row < listing.size(); ++row) {
            const auto entry{listing.entry(row)};
            CHECK((entry.offset < BOOT_ROM_LOW_END || entry.offset >= BOOT_ROM_HIGH_BEGIN));
            CHECK(entry.source == CodeSource::Bios);
        }
        listing.update(CodeSource::Mapped, BOOT_ROM_LOW_END);
        CHECK(listing.entry(0).source == CodeSource::Bios);
        CHECK(listing.entry(listing.find_row(BOOT_ROM_LOW_END)).source == CodeSource::Cartridge);
        if (IsCgb(model))
            CHECK(listing.entry(listing.find_row(BOOT_ROM_HIGH_BEGIN)).source == CodeSource::Bios);
        listing.update(CodeSource::Cartridge, 0);
        CHECK(listing.entry(0).source == CodeSource::Cartridge);
        CHECK(listing.entry(0).instruction.bytes[0] == machine.rom_bytes()[0]);
        CHECK_FALSE(listing.is_current(listing.entry(0)));
        CHECK(machine.SaveDebugState() == snapshot);
        auto &bus{machine.[:debug_member<Gameboy>("bus_"):]};
        bus.bootromRunning = false;
        listing.update(CodeSource::Bios, 0);
        CHECK(listing.entry(0).instruction.bytes[0] == machine.BootromBytes()[0]);
        listing.update(CodeSource::Mapped, 0);
        CHECK(listing.entry(0).source == CodeSource::Cartridge);
        debugger.reset();
        CHECK(listing.execution_source() == CodeSource::Bios);
    }
}
