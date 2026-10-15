#include "Interrupts.h"
#include <doctest/doctest.h>
#include <ranges>
#include "Gameboy.h"
#include "TestUtilities.h"

TEST_CASE("interrupts: simultaneous sources retain independent propagation times") {
    Interrupts interrupts;
    interrupts.interruptFlag = 0;
    const auto vblank = [&] { interrupts.SetAfter(InterruptType::VBlank, 8, true, 4); };
    const auto stat = [&] { interrupts.SetAfter(InterruptType::LCDStat, 2); };
    SUBCASE("VBlank scheduled first") {
        vblank();
        stat();
    }
    SUBCASE("STAT scheduled first") {
        stat();
        vblank();
    }

    interrupts.Tick();
    CHECK(interrupts.interruptFlag == 0);
    interrupts.Tick();
    CHECK(interrupts.IsSet(InterruptType::LCDStat));
    CHECK_FALSE(interrupts.IsSet(InterruptType::VBlank));
    interrupts.Tick();
    CHECK(interrupts.interruptVisiblePending == 0);
    interrupts.Tick();
    CHECK(interrupts.interruptVisiblePending == 1);
    CHECK_FALSE(interrupts.IsSet(InterruptType::VBlank));
    for (auto _ : std::views::indices(4))
        interrupts.Tick();
    CHECK(interrupts.IsSet(InterruptType::VBlank));
    CHECK(interrupts.IsSet(InterruptType::LCDStat));
    CHECK(interrupts.interruptVisiblePending == 0);
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
        interrupts.interruptEnable = interrupts.interruptFlag = 1;
        interrupts.interruptMasterEnable = true;
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
        CHECK(interrupts.IsSet(InterruptType::VBlank));
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK(cpu.sp() == 0xD000);
        CHECK(gameboy.DebugPeek(0xD000) == 0x23);
        CHECK(cpu.pc() == 0x40);
        CHECK_FALSE(interrupts.IsSet(InterruptType::VBlank));
        static_cast<void>(gameboy.AdvanceTicks(8, policy));
        CHECK(cpu.pc() == 0x40);
        CHECK(cpu.InstructionsCompleted() == 0);
    }
}
