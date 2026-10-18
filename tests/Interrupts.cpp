#include "../frontends/sdl/GameboyFiles.h"
#include "Interrupts.h"
#include <doctest/doctest.h>
#include <ranges>
#include "Gameboy.h"
#include "TestUtilities.h"

TEST_CASE("interrupts: simultaneous sources retain independent propagation times") {
    Interrupts interrupts;
    interrupts.interrupt_flag = 0;
    const auto vblank = [&] { interrupts.set_after(InterruptType::VBlank, 8, true, 4); };
    const auto stat = [&] { interrupts.set_after(InterruptType::LCDStat, 2); };
    SUBCASE("VBlank scheduled first") {
        vblank();
        stat();
    }
    SUBCASE("STAT scheduled first") {
        stat();
        vblank();
    }

    interrupts.tick();
    CHECK(interrupts.interrupt_flag == 0);
    interrupts.tick();
    CHECK(interrupts.is_set(InterruptType::LCDStat));
    CHECK_FALSE(interrupts.is_set(InterruptType::VBlank));
    interrupts.tick();
    CHECK(interrupts.interrupt_visible_pending == 0);
    interrupts.tick();
    CHECK(interrupts.interrupt_visible_pending == 1);
    CHECK_FALSE(interrupts.is_set(InterruptType::VBlank));
    for (auto _ : std::views::indices(4))
        interrupts.tick();
    CHECK(interrupts.is_set(InterruptType::VBlank));
    CHECK(interrupts.is_set(InterruptType::LCDStat));
    CHECK(interrupts.interrupt_visible_pending == 0);
}

TEST_CASE("interrupts: entry writes the stack in the third and fourth machine cycles") {
    constexpr std::array models = {Model::DMGB, Model::CGBC};
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    for (const auto [model, policy] : std::views::cartesian_product(models, policies)) {
        CAPTURE(model);
        CAPTURE(policy);
        auto gameboy = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}});
        auto &cpu = gameboy.[:get_member<Gameboy>("cpu_"):];
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        auto &interrupts = gameboy.[:get_member<Gameboy>("interrupts_"):];
        bus.WriteByte(0xFF40, 0, ComponentSource::CPU);
        bus.WriteByte(0xD000, 0xAA, ComponentSource::CPU);
        bus.WriteByte(0xD001, 0xBB, ComponentSource::CPU);
        cpu.pc(0xC124);
        cpu.sp(0xD002);
        cpu.currentInstruction = 0;
        interrupts.interrupt_enable = interrupts.interrupt_flag = 1;
        interrupts.interrupt_master_enable = true;
        for (auto _ : std::views::indices(2)) {
            static_cast<void>(gameboy.AdvanceTicks(8, policy));
            CHECK(cpu.sp() == 0xD002);
            CHECK(gameboy.DebugPeek(0xD000) == 0xAA);
            CHECK(gameboy.DebugPeek(0xD001) == 0xBB);
        }
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK(cpu.sp() == 0xD001);
        CHECK(gameboy.DebugPeek(0xD001) == 0xC1);
        CHECK(gameboy.DebugPeek(0xD000) == 0xAA);
        CHECK(interrupts.is_set(InterruptType::VBlank));
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK(cpu.sp() == 0xD000);
        CHECK(gameboy.DebugPeek(0xD000) == 0x23);
        CHECK(cpu.pc() == 0x40);
        CHECK_FALSE(interrupts.is_set(InterruptType::VBlank));
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK(cpu.pc() == 0x40);
        CHECK(cpu.InstructionsCompleted() == 0);
    }
}

TEST_CASE("interrupts: serial acknowledgment holds only the selected request through its model-specific window") {
    for (const bool cgb : {false, true}) {
        CAPTURE(cgb);
        Interrupts interrupts;
        interrupts.interrupt_flag = 0x0C;
        interrupts.acknowledge(0x08, cgb);
        for (auto _ : std::views::indices(3 + cgb)) {
            interrupts.set(InterruptType::Serial, false);
            CHECK_FALSE(interrupts.is_set(InterruptType::Serial));
            CHECK(interrupts.is_set(InterruptType::Timer));
            interrupts.tick_cpu();
        }
        interrupts.set(InterruptType::Serial, false);
        CHECK(interrupts.is_set(InterruptType::Serial));
    }
}

TEST_CASE("interrupts: acknowledging a visible request retires its delayed dispatch copy") {
    Interrupts interrupts;
    interrupts.interrupt_flag = 0;
    interrupts.set_after(InterruptType::LCDStat, 4, true);
    interrupts.set_after(InterruptType::Timer, 8, true);
    interrupts.acknowledge(2, true);
    CHECK(interrupts.interrupt_visible_pending == 4);
    CHECK(interrupts.interrupt_flag_delayed == 4);
    for (auto dot{0u}; dot < 8; ++dot) {
        interrupts.tick_cpu();
        interrupts.tick();
        CHECK_FALSE(interrupts.is_set(InterruptType::LCDStat));
    }
    CHECK(interrupts.is_set(InterruptType::Timer));
    CHECK_FALSE(interrupts.delays_pending);

    interrupts.set_after(InterruptType::LCDStat, 4);
    interrupts.acknowledge(2, true);
    for (auto dot{0u}; dot < 4; ++dot) {
        interrupts.tick_cpu();
        interrupts.tick();
    }
    CHECK(interrupts.is_set(InterruptType::LCDStat));
}

TEST_CASE("interrupts: vector selection sees higher-priority IF before dispatch propagation completes") {
    constexpr std::array models = {Model::DMGB, Model::CGBC};
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    for (const auto [model, policy] : std::views::cartesian_product(models, policies)) {
        CAPTURE(model);
        CAPTURE(policy);
        auto gameboy = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}});
        auto &cpu = gameboy.[:get_member<Gameboy>("cpu_"):];
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        auto &interrupts = gameboy.[:get_member<Gameboy>("interrupts_"):];
        bus.WriteByte(LCD_CONTROL_BIT, 0, ComponentSource::CPU);
        cpu.pc(0xC124);
        cpu.sp(0xD002);
        cpu.currentInstruction = 0;
        interrupts.interrupt_enable = 6;
        interrupts.interrupt_flag = 4;
        interrupts.interrupt_master_enable = true;
        static_cast<void>(gameboy.AdvanceTicks(24, policy));
        interrupts.set_after(InterruptType::LCDStat, 8, true);
        CHECK(bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) == 0xE6);
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK(cpu.pc() == 0x48);
        CHECK(bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) == 0xE4);
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK(bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) == 0xE4);
    }
}

TEST_CASE("interrupts: timer acknowledgment suppresses requests during its clear interval") {
    constexpr std::array cgb_values = {false, true};
    for (const auto cgb : cgb_values) {
        CAPTURE(cgb);
        Interrupts interrupts;
        interrupts.interrupt_flag = 6;
        interrupts.acknowledge(4, cgb);
        for (auto tick{0u}; tick < 3u + cgb; ++tick) {
            if (cgb)
                interrupts.set(InterruptType::Timer, false);
            else
                interrupts.set_after(InterruptType::Timer, 4, true);
            CHECK(((interrupts.interrupt_flag | interrupts.interrupt_visible_pending) & 4) == 0);
            CHECK(interrupts.is_set(InterruptType::LCDStat));
            interrupts.tick_cpu();
            interrupts.tick();
        }
        interrupts.set(InterruptType::Timer, false);
        CHECK(interrupts.is_set(InterruptType::Timer));
    }
}

TEST_CASE("interrupts: the STAT clear interval applies when a queued request becomes IF-visible") {
    constexpr std::array<uint8_t, 3> visibility_delay_values = {1, 2, 3};
    for (const auto visibility_delay : visibility_delay_values) {
        CAPTURE(visibility_delay);
        Interrupts interrupts;
        interrupts.interrupt_flag = 6;
        interrupts.set_after(InterruptType::LCDStat, 6, true, visibility_delay);
        interrupts.acknowledge(2, true);
        for (auto tick{1u}; tick <= 6; ++tick) {
            interrupts.tick_cpu();
            interrupts.tick();
            const bool visible = ((interrupts.interrupt_flag | interrupts.interrupt_visible_pending) & 2) != 0;
            CHECK(visible == (visibility_delay == 3 && tick >= 3));
            CHECK(interrupts.is_set(InterruptType::Timer));
        }
        CHECK_FALSE(interrupts.delays_pending);
    }
}

TEST_CASE("interrupts: clearing an IF-visible CGB LYC request prevents delayed reassertion") {
    constexpr std::array speeds = {false, true};
    constexpr std::array line_values = {1, 153};
    constexpr std::array clear_values = {false, true};
    for (const auto [double_speed, line, clear] : std::views::cartesian_product(speeds, line_values, clear_values)) {
        CAPTURE(double_speed);
        CAPTURE(line);
        CAPTURE(clear);
        auto gameboy = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        auto &gpu = bus.gpu_;
        auto &interrupts = gameboy.[:get_member<Gameboy>("interrupts_"):];
        if (double_speed) {
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
        }
        static_cast<void>(gameboy.AdvanceTicks(8, ExecutionPolicy::Ordinary));
        gpu.InvalidateIdle();
        const auto update = [&] {
            interrupts.tick_cpu();
            if (double_speed)
                interrupts.tick_cpu();
            gpu.Update();
        };
        gpu.currentLine = line;
        gpu.scanlineCounter = line == 153 ? 8 : 0;
        gpu.lyc = line == 153 ? 0 : 1;
        gpu.lycInterruptLine_ = false;
        gpu.stat.enable_ly_interrupt = true;
        gpu.stat.enable_m0_interrupt = gpu.stat.enable_m1_interrupt = gpu.stat.enable_m2_interrupt = false;
        gpu.stat.mode = line == 153 ? GPUMode::MODE_1 : GPUMode::MODE_2;
        interrupts.interrupt_flag = 0;
        update();
        if (!double_speed) {
            CHECK((bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) & 2) == 0);
            update();
        }
        CHECK((bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) & 2) == 2);
        CHECK_FALSE(interrupts.is_set(InterruptType::LCDStat));
        if (clear)
            bus.WriteByte(INTERRUPT_FLAG, 0, ComponentSource::CPU);
        for (auto dot{0u}; dot < 8; ++dot)
            update();
        CHECK((bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) & 2) == (clear ? 0 : 2));
        CHECK(interrupts.is_set(InterruptType::LCDStat) == !clear);
    }
}

TEST_CASE("interrupts: a CGB LYC request on the final CPU tick waits for the next dispatch boundary") {
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    for (const auto policy : policies) {
        CAPTURE(policy);
        auto gameboy = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
        auto &cpu = gameboy.[:get_member<Gameboy>("cpu_"):];
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        auto &gpu = bus.gpu_;
        auto &interrupts = gameboy.[:get_member<Gameboy>("interrupts_"):];
        cpu.pc(0xC001);
        cpu.currentInstruction = 0;
        gameboy.DebugPoke(0xC001, 0);
        gameboy.DebugPoke(0xC002, 0);
        gpu.InvalidateIdle();
        gpu.currentLine = 10;
        gpu.scanlineCounter = 453;
        gpu.stat.mode = GPUMode::MODE_0;
        gpu.lyc = 11;
        gpu.lycInterruptLine_ = false;
        gpu.stat.enable_ly_interrupt = true;
        gpu.stat.enable_m0_interrupt = gpu.stat.enable_m1_interrupt = gpu.stat.enable_m2_interrupt = false;
        interrupts.interrupt_flag = 0;
        interrupts.interrupt_enable = 2;
        interrupts.interrupt_master_enable = true;
        static_cast<void>(gameboy.AdvanceTicks(16, policy));
        CHECK((bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) & 2) == 2);
        CHECK_FALSE(interrupts.is_set(InterruptType::LCDStat));
        CHECK(interrupts.interrupt_master_enable);
        CHECK(cpu.InstructionsCompleted() == 2);
        CHECK(cpu.pc() == 0xC003);
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK_FALSE(interrupts.interrupt_master_enable);
        CHECK(cpu.InstructionsCompleted() == 2);
        CHECK(cpu.pc() == 0xC002);
    }
}

TEST_CASE("interrupts: a STAT edge during acknowledgment cannot reappear after its visibility delay") {
    constexpr std::array models = {Model::DMGB, Model::CGBC};
    constexpr std::array acknowledge_values = {false, true};
    for (const auto [model, acknowledge] : std::views::cartesian_product(models, acknowledge_values)) {
        CAPTURE(model);

        CAPTURE(acknowledge);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.lcdc = 0x91;
        gpu.currentLine = 143;
        gpu.scanlineCounter = IsCgb(model) ? 455 : 451;
        gpu.stat.mode = GPUMode::MODE_0;
        gpu.stat.enable_m1_interrupt = true;
        interrupts.interrupt_flag = 2;
        gpu.Update();
        if (acknowledge)
            interrupts.acknowledge(2, IsCgb(model));
        else
            interrupts.interrupt_flag = 0;
        for (auto dot{0u}; dot < 10; ++dot) {
            interrupts.tick_cpu();
            gpu.Update();
        }
        CHECK(interrupts.is_set(InterruptType::VBlank));
        CHECK(interrupts.is_set(InterruptType::LCDStat) == !acknowledge);
        CHECK_FALSE(interrupts.delays_pending);
    }
}


TEST_CASE("interrupts: CGB VBlank dispatch distinguishes normal and double speed visibility") {
    constexpr std::array speeds = {false, true};
    constexpr std::array halted_values = {false, true};
    constexpr std::array clear_values = {false, true};
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    for (const auto [double_speed, halted, clear, policy] : std::views::cartesian_product(speeds, halted_values, clear_values, policies)) {
        CAPTURE(halted);

        CAPTURE(double_speed);
        CAPTURE(clear);
        CAPTURE(policy);
        auto gameboy = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
        auto &cpu = gameboy.[:get_member<Gameboy>("cpu_"):];
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        auto &interrupts = gameboy.[:get_member<Gameboy>("interrupts_"):];
        bus.WriteByte(LCD_CONTROL_BIT, 0, ComponentSource::CPU);
        if (double_speed) {
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
        }
        cpu.pc(0xC001);
        cpu.currentInstruction = 0;
        cpu.halted(halted);
        gameboy.DebugPoke(0xC001, 0);
        gameboy.DebugPoke(0xC002, 0);
        interrupts.interrupt_flag = 0;
        interrupts.interrupt_enable = 1;
        interrupts.interrupt_master_enable = true;
        interrupts.set_after(InterruptType::VBlank, double_speed ? 4 : 8, true, double_speed ? 1 : 2);
        static_cast<void>(gameboy.AdvanceTicks(double_speed ? 4 : 8, policy));
        CHECK((bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) & 1) == 1);
        CHECK_FALSE(interrupts.is_set(InterruptType::VBlank));
        CHECK_FALSE(cpu.IsHalted());
        CHECK(interrupts.interrupt_master_enable == (halted || !double_speed));
        CHECK(cpu.InstructionsCompleted() == (halted || double_speed ? 0u : 1u));
        if (clear)
            bus.WriteByte(INTERRUPT_FLAG, 0, ComponentSource::CPU);
        static_cast<void>(gameboy.AdvanceTicks(double_speed ? 4 : 8, policy));
        CHECK(interrupts.interrupt_master_enable == (clear && (halted || !double_speed)));
        CHECK(cpu.InstructionsCompleted() == (halted ? (clear ? 1u : 0u) : double_speed ? 0u : clear ? 2u : 1u));
    }
}


TEST_CASE("interrupts: VBlank visibility precedes dispatch and acknowledgment preserves STAT") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    constexpr std::array speeds = {false, true};
    constexpr std::array optimizations = {false, true};
    constexpr std::array acknowledge_values = {false, true};
    for (const auto [model, double_speed, optimized, acknowledge] : std::views::cartesian_product(models, speeds, optimizations, acknowledge_values)) {
        if (!IsCgb(model) && double_speed)
            continue;

        CAPTURE(model);
        CAPTURE(double_speed);
        CAPTURE(optimized);
        CAPTURE(acknowledge);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.doubleSpeed = double_speed;
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x91;
        gpu.currentLine = 143;
        gpu.scanlineCounter = 450;
        gpu.stat.mode = GPUMode::MODE_0;
        gpu.stat.enable_m1_interrupt = true;
        interrupts.interrupt_flag = 0;
        const unsigned visible_at = !IsCgb(model) ? 3 : double_speed ? 6 : 7;
        const unsigned dispatch_at = !IsCgb(model) ? 5 : double_speed ? 9 : 13;
        for (auto dot{0u}; dot < 20; ++dot) {
            for (auto tick{0u}; tick < (double_speed ? 2u : 1u); ++tick)
                interrupts.tick_cpu();
            gpu.Update();
            CHECK(((interrupts.interrupt_flag | interrupts.interrupt_visible_pending) & 1) ==
                  (!acknowledge && dot >= visible_at ? 1 : 0));
            CHECK(interrupts.is_set(InterruptType::VBlank) == (!acknowledge && dot >= dispatch_at));
            if (acknowledge && dot + 1 == visible_at)
                interrupts.acknowledge(1, IsCgb(model));
        }
        CHECK(interrupts.is_set(InterruptType::LCDStat));
        CHECK_FALSE(interrupts.delays_pending);
    }
}

TEST_CASE("interrupts: DMG LYC source precedes coincidence visibility and dispatch") {
    constexpr std::array line153_values = {false, true};
    constexpr std::array optimizations = {false, true};
    constexpr std::array acknowledge_values = {false, true};
    for (const auto [line153, optimized, acknowledge] : std::views::cartesian_product(line153_values, optimizations, acknowledge_values)) {
        CAPTURE(line153);
        CAPTURE(optimized);
        CAPTURE(acknowledge);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::DMGB);
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x91;
        gpu.currentLine = line153 ? 153 : 41;
        gpu.scanlineCounter = line153 ? 2 : 450;
        gpu.stat.mode = line153 ? GPUMode::MODE_1 : GPUMode::MODE_0;
        gpu.lyc = line153 ? 0 : 42;
        gpu.stat.enable_ly_interrupt = true;
        interrupts.interrupt_flag = 1;
        for (auto dot{0u}; dot < 12; ++dot) {
            CAPTURE(dot);
            interrupts.tick_cpu();
            if (acknowledge && dot == 2)
                interrupts.acknowledge(2, false);
            gpu.Update();
            CHECK(((interrupts.interrupt_flag | interrupts.interrupt_visible_pending) & 2) ==
                  (!acknowledge && dot >= 3 ? 2 : 0));
            CHECK(interrupts.is_set(InterruptType::LCDStat) == (!acknowledge && dot >= (line153 ? 6u : 5u)));
            CHECK(gpu.stat.coincidence_flag == (dot >= 5));
            CHECK(gpu.lycInterruptLine_ == (dot >= 3));
            CHECK(interrupts.is_set(InterruptType::VBlank));
        }
    }
}

TEST_CASE("interrupts: IF writes clear arriving LCD requests without changing other sources") {
    constexpr std::array speeds = {false, true};
    constexpr std::array sources = {InterruptType::VBlank, InterruptType::LCDStat};
    constexpr std::array<uint8_t, 4> values = {0, 1, 2, 3};
    constexpr std::array<unsigned, 4> arrival_values = {0, 1, 2, 3};
    for (const auto [double_speed, source, value, arrival] : std::views::cartesian_product(speeds, sources, values, arrival_values)) {
        CAPTURE(double_speed);
        CAPTURE(source);
        CAPTURE(value);
        CAPTURE(arrival);
        Interrupts interrupts;
        const uint8_t mask = 1u << static_cast<uint8_t>(source);
        if (arrival == 0)
            interrupts.set_after(source, 4, true, 1);
        interrupts.write_flag(value, double_speed);
        for (auto clock{1u}; clock <= 16; ++clock) {
            interrupts.tick_cpu();
            if (!double_speed || clock % 2 == 0)
                interrupts.tick();
            if (clock == arrival)
                interrupts.set_after(source, 4, true);
            if (clock == 1)
                interrupts.set(InterruptType::Serial, false);
        }
        const uint8_t expected = value | 8 | (arrival > 1u + double_speed ? mask : 0);
        CHECK(interrupts.interrupt_flag == expected);
        CHECK(interrupts.interrupt_visible_pending == 0);
        CHECK(interrupts.interrupt_flag_delayed == 0);
        CHECK_FALSE(interrupts.delays_pending);
    }
}

TEST_CASE("interrupts: software CGB LYC requests are readable before normal-speed dispatch") {
    constexpr std::array models = {Model::CGBC, Model::CGBE};
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    constexpr std::array clear_values = {false, true};
    for (const auto [model, policy, clear] : std::views::cartesian_product(models, policies, clear_values)) {
        CAPTURE(model);
        CAPTURE(policy);
        CAPTURE(clear);
        auto gameboy = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}});
        gameboy.[:get_member<Gameboy>("cpu_"):].Lock();
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        auto &gpu = bus.gpu_;
        auto &interrupts = gameboy.[:get_member<Gameboy>("interrupts_"):];
        gpu.InvalidateIdle();
        gpu.currentLine = 150;
        gpu.scanlineCounter = 100;
        gpu.lyc = 0xFF;
        gpu.stat.enable_ly_interrupt = true;
        gpu.stat.enable_m0_interrupt = gpu.stat.enable_m1_interrupt = gpu.stat.enable_m2_interrupt = false;
        interrupts.interrupt_flag = 0;
        bus.WriteByte(SCANLINE_COMPARE, 150, ComponentSource::CPU);
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK((bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) & 2) == 0);
        CHECK_FALSE(interrupts.is_set(InterruptType::LCDStat));
        static_cast<void>(gameboy.AdvanceTicks(2, policy));
        CHECK((bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) & 2) == 2);
        CHECK_FALSE(interrupts.is_set(InterruptType::LCDStat));
        if (clear)
            bus.WriteByte(INTERRUPT_FLAG, 0, ComponentSource::CPU);
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK((bus.ReadByte(INTERRUPT_FLAG, ComponentSource::CPU) & 2) == (clear ? 0 : 2));
        CHECK(interrupts.is_set(InterruptType::LCDStat) == !clear);
    }
}
