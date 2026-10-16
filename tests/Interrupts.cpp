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
        for (auto _ : std::views::indices(3)) {
            interrupts.set(InterruptType::Serial, false);
            CHECK_FALSE(interrupts.is_set(InterruptType::Serial));
            CHECK(interrupts.is_set(InterruptType::Timer));
            interrupts.tick_cpu();
        }
        interrupts.set(InterruptType::Serial, false);
        CHECK(interrupts.is_set(InterruptType::Serial));
    }
}
