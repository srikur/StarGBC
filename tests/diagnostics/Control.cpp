#include <doctest/doctest.h>
#include "Gameboy.h"
#include "diagnostics/Watchpoints.h"

TEST_CASE (
"diagnostics: watchpoint stops retain trigger and safe positions and resume without changing hardware"
)
 {
    const GameboySettings settings{.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true};
    Gameboy controlled(settings);
    Gameboy continuous(settings);
    diagnostics::TraceRecorder recorder(32);
    diagnostics::Watchpoints watchpoints(recorder.Context());
    watchpoints.Add({.events = {.components = uint64_t{1} << static_cast<unsigned>(diagnostics::Component::Bus),
                               .kinds = uint64_t{1} << static_cast<unsigned>(diagnostics::EventKind::BusWrite)}});
    controlled.ObserveWith(&recorder.Context());
    const auto first = controlled.RunUntil({}, 140448);
    REQUIRE(first.execution.reason == ExecutionStopReason::Condition);
    REQUIRE(first.trigger);
    CHECK(first.trigger->position.tick < first.stopped.tick);
    CHECK(first.stopped.tick - first.trigger->position.tick == 1);
    static_cast<void>(continuous.AdvanceTicks(first.execution.stopTick));
    CHECK(controlled.SaveState() == continuous.SaveState());
    const auto second = controlled.RunUntil({}, 140448);
    REQUIRE(second.trigger);
    CHECK(second.trigger->sequence > first.trigger->sequence);
    CHECK(second.execution.stopTick > first.execution.stopTick);
    static_cast<void>(continuous.AdvanceTicks(second.execution.stopTick - first.execution.stopTick));
    CHECK(controlled.SaveState() == continuous.SaveState());
    static_cast<void>(controlled.AdvanceTicks(2001));
    static_cast<void>(continuous.AdvanceTicks(2001));
    CHECK(controlled.SaveState() == continuous.SaveState());
    CHECK(std::ranges::equal(std::span(controlled.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT),
                             std::span(continuous.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT)));
}

TEST_CASE (
"diagnostics: bank aware PC breakpoints and instruction deadlines remain bounded"
)
 {
    const GameboySettings settings{.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true};
    for (const uint16_t bank : {0, 1}) {
        Gameboy machine(settings);
        diagnostics::TraceRecorder recorder(32);
        diagnostics::Watchpoints watchpoints(recorder.Context());
        watchpoints.BreakAt(0x100, bank);
        machine.ObserveWith(&recorder.Context());
        const auto result = machine.RunUntil({}, 8);
        if (bank == 0) {
            REQUIRE(result.trigger);
            CHECK(result.trigger->pc == 0x100);
            CHECK(result.trigger->pcBank == 0);
            CHECK(result.trigger->kind == diagnostics::EventKind::InstructionBegin);
        } else {
            CHECK_FALSE(result.trigger);
            CHECK(result.execution.reason == ExecutionStopReason::TickBudget);
            CHECK(result.execution.stopTick == 8);
        }
    }
}

TEST_CASE (
"diagnostics: declarative conditions inspect PPU and register changes without overshooting deadlines"
)
 {
    Gameboy machine({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
    const std::array unchanged{RunCondition{ConditionKind::RegisterChange, 0xFF04}};
    const auto result = machine.RunUntil(unchanged, 1024);
    CHECK(result.execution.reason == ExecutionStopReason::Condition);
    CHECK(result.conditionIndex == 0);
    CHECK(result.execution.stopTick <= 512);
    const auto state = machine.SaveState();
    const std::array never{RunCondition{ConditionKind::Line, 0, 200}};
    const auto immediate = machine.RunUntil(never, 0);
    CHECK(immediate.execution.reason == ExecutionStopReason::TickBudget);
    CHECK(machine.SaveState() == state);
    const auto bounded = machine.RunUntil(never, 17);
    CHECK(bounded.execution.stopTick - bounded.execution.beginTick == 17);
    const std::array current{RunCondition{ConditionKind::Dot, 0, machine.ExecutionPosition().dot}};
    CHECK(machine.RunUntil(current, 10).execution.stopTick == machine.TimelineTicks());
    std::array<RunCondition, 17> excessive{};
    CHECK_THROWS_AS(static_cast<void>(machine.RunUntil(excessive, 0)), std::invalid_argument);
    const std::array invalid{RunCondition{ConditionKind::RegisterValue, 0x1234}};
    CHECK_THROWS_AS(static_cast<void>(machine.RunUntil(invalid, 1)), std::invalid_argument);
}
