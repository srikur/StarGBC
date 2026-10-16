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
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = model, .noBootrom = true});
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
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = model, .noBootrom = true});
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
