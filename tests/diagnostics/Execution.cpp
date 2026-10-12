#include <doctest/doctest.h>
#include <limits>
#include "Gameboy.h"

namespace {
    template<class T>
    consteval std::meta::info executionTestMember(const std::string_view name) {
        for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::identifier_of(member) == name)
                return member;
        throw "Unknown execution test member";
    }

    void checkMachineAgreement(const Gameboy &left, const Gameboy &right) {
        CHECK(left.SaveState() == right.SaveState());
        CHECK(std::ranges::equal(std::span(left.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT),
                                 std::span(right.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT)));
        CHECK(left.GetAudioSamplesAvailable() == right.GetAudioSamplesAvailable());
    }
}

TEST_CASE (
"diagnostics: arbitrary bounded stepping matches uninterrupted execution at both speeds"
)
 {
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        for (const bool doubleSpeed : {false, true}) {
            if (doubleSpeed && IsDmg(model))
                continue;
            const GameboySettings settings{.romName = "roms/acid/cgb-acid2.gbc", .model = model, .noBootrom = true};
            Gameboy uninterrupted(settings);
            Gameboy stepped(settings);
            if (doubleSpeed) {
                for (auto *machine : {&uninterrupted, &stepped}) {
                    auto &bus = machine->[:executionTestMember<Gameboy>("bus_"):];
                    bus.prepareSpeedShift = true;
                    bus.ChangeSpeed();
                    REQUIRE(machine->Inspect().speed == Speed::Double);
                }
            }
            uint32_t seed = 0x53A7;
            for (unsigned interval = 0; interval < 3; ++interval) {
                uninterrupted.RunFrame();
                uint64_t remaining = Gameboy::FRAME_CYCLES_DMG * 2;
                while (remaining) {
                    seed = seed * 1664525 + 1013904223;
                    const auto amount = std::min<uint64_t>(remaining, 1 + seed % 509);
                    const auto result = stepped.AdvanceTicks(amount);
                    REQUIRE(result.stopTick - result.beginTick == amount);
                    remaining -= amount;
                }
                checkMachineAgreement(uninterrupted, stepped);
            }
        }
    }
}

TEST_CASE (
"diagnostics: instruction stepping treats CB opcodes as one instruction and bounds idle CPUs"
)
 {
    Gameboy gameboy({.romName = "roms/acid/dmg-acid2.gb", .model = Model::DMGB, .noBootrom = true});
    auto &cpu = gameboy.[:executionTestMember<Gameboy>("cpu_"):];
    cpu.pc(0xC001);
    cpu.currentInstruction = 0;
    gameboy.DebugPoke(0xC001, 0xCB);
    gameboy.DebugPoke(0xC002, 0);
    gameboy.DebugPoke(0xC003, 0x76);
    gameboy.DebugPoke(0xC004, 0);
    const auto nop = gameboy.StepInstruction(32);
    CHECK(nop.reason == ExecutionStopReason::InstructionBoundary);
    CHECK(nop.instructions == 1);
    const auto prefixed = gameboy.StepInstruction(32);
    CHECK(prefixed.reason == ExecutionStopReason::InstructionBoundary);
    CHECK(prefixed.instructions == 1);
    CHECK(prefixed.stopTick - prefixed.beginTick == 16);
    CHECK_FALSE(gameboy.InspectCpu().prefixed);
    static_cast<void>(gameboy.StepInstruction(32));
    CHECK(gameboy.StepInstruction(32).reason == ExecutionStopReason::Halted);
    cpu.halted(false);
    cpu.stopped(true);
    CHECK(gameboy.StepInstruction(32).reason == ExecutionStopReason::Stopped);
    cpu.stopped(false);
    cpu.Lock();
    CHECK(gameboy.StepInstruction(32).reason == ExecutionStopReason::Locked);
    const auto begin = gameboy.TimelineTicks();
    CHECK(gameboy.AdvanceTicks(17).stopTick == begin + 17);
}

TEST_CASE (
"diagnostics: timeline survives scheduler wrap and rejects overflowing deadlines"
)
 {
    Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
    gameboy.[:executionTestMember<Gameboy>("masterCycles"):] = Gameboy::CGB_CYCLES_PER_SECOND - 2;
    gameboy.[:executionTestMember<Gameboy>("timelineTicks_"):] = uint64_t{1} << 32;
    CHECK(gameboy.AdvanceTicks(19).stopTick == (uint64_t{1} << 32) + 19);
    const auto state = gameboy.SaveState();
    CHECK_THROWS_AS(static_cast<void>(gameboy.AdvanceTicks(std::numeric_limits<uint64_t>::max())), std::out_of_range);
    CHECK(gameboy.SaveState() == state);
    CHECK(gameboy.AdvanceTicks(0).stopTick == gameboy.TimelineTicks());
}
