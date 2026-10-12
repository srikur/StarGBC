#include <chrono>
#include <doctest/doctest.h>
#include "diagnostics/Experiments.h"

namespace {
    class ExperimentDirectory {
        std::filesystem::path directory_{
            std::filesystem::temp_directory_path() /
            ("stargbc-experiment-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))
        };
        unsigned index_{};

    public:
        ~ExperimentDirectory() { std::filesystem::remove_all(directory_); }
        std::filesystem::path Next() { return directory_ / std::to_string(index_++); }
    };
}

TEST_CASE (
"diagnostics: timer phase experiments preserve the reload latch across both speeds"
)
 {
    ExperimentDirectory directory;
    for (const bool doubleSpeed : {false, true}) {
        std::array<uint32_t, 3> values{};
        for (int phase = 0; phase <= 2; ++phase) {
            const auto result = diagnostics::RunExperiment({.family = diagnostics::ExperimentFamily::TimerReload, .model = Model::CGBC,
                                                            .doubleSpeed = doubleSpeed, .phase = phase}, directory.Next());
            REQUIRE(result.outcome == diagnostics::Outcome::Pass);
            values[phase] = result.probeValue;
        }
        CHECK(values[0] == values[2]);
        CHECK(values[1] == 37);
        CHECK(values[0] != values[1]);
    }
}

TEST_CASE (
"diagnostics: overlapping transparent sprites have repeatable CPU and component experiments"
)
 {
    ExperimentDirectory directory;
    for (const auto mode : {diagnostics::ExperimentMode::Component, diagnostics::ExperimentMode::WholeMachine}) {
        const diagnostics::Scenario scenario{.mode = mode, .family = diagnostics::ExperimentFamily::Sprites, .model = Model::CGBC};
        const auto one = diagnostics::RunExperiment(scenario, directory.Next());
        const auto two = diagnostics::RunExperiment(scenario, directory.Next());
        REQUIRE(one.outcome == diagnostics::Outcome::Pass);
        REQUIRE(two.outcome == diagnostics::Outcome::Pass);
        CHECK(one.eventHash == two.eventHash);
        CHECK(one.framebufferHash == two.framebufferHash);
        CHECK(one.completedTick == two.completedTick);
    }
}

TEST_CASE (
"diagnostics: window phases and clipping remain observable in both experiment modes"
)
 {
    ExperimentDirectory directory;
    for (const auto mode : {diagnostics::ExperimentMode::Component, diagnostics::ExperimentMode::WholeMachine})
        for (const uint8_t wx : {0, 5, 7}) {
            const auto result = diagnostics::RunExperiment({.mode = mode, .family = diagnostics::ExperimentFamily::Window,
                                                            .model = Model::CGBE, .phase = -1, .wx = wx}, directory.Next());
            CHECK(result.outcome == diagnostics::Outcome::Pass);
        }
}

TEST_CASE (
"diagnostics: STAT experiments separate peripheral requests from CPU acceptance"
)
 {
    ExperimentDirectory directory;
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        const auto result = diagnostics::RunExperiment({.mode = diagnostics::ExperimentMode::WholeMachine,
                .family = diagnostics::ExperimentFamily::Stat, .model = model, .expectedValue = 1,
                .basisEvidence = "A requested STAT interrupt reaches the CPU and executes the acknowledgment handler"}, directory.Next());
        CHECK(result.outcome == diagnostics::Outcome::Pass);
        CHECK(result.probeValue == 1);
    }
}

TEST_CASE (
"diagnostics: DMA experiments execute timed reads from HRAM and copy all source bytes"
)
 {
    ExperimentDirectory directory;
    for (const auto mode : {diagnostics::ExperimentMode::Component, diagnostics::ExperimentMode::WholeMachine})
        for (const bool doubleSpeed : {false, true}) {
            const auto result = diagnostics::RunExperiment({.mode = mode, .family = diagnostics::ExperimentFamily::Dma,
                                                            .model = Model::CGBC, .doubleSpeed = doubleSpeed}, directory.Next());
            CHECK(result.outcome == diagnostics::Outcome::Pass);
        }
    const auto timeout = diagnostics::RunExperiment({.mode = diagnostics::ExperimentMode::WholeMachine,
            .family = diagnostics::ExperimentFamily::Dma, .budgetTicks = 1}, directory.Next());
    CHECK(timeout.outcome == diagnostics::Outcome::Timeout);
    CHECK(timeout.failureSignature.empty());
}
