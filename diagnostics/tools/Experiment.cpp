#include <iostream>
#include <print>
#include <starparse/starparse.hpp>
#include "diagnostics/Experiments.h"
#include "diagnostics/Json.h"

namespace {
    using namespace StarParse;
    struct[[=Program{"StarGBC_Experiment", "Reproducible component and CPU-executed timing experiments", "1"}]] Arguments {
        [[=Opt{"Component isolation or CPU-executed ROM"}]] diagnostics::ExperimentMode mode{diagnostics::ExperimentMode::Component};
        [[=Opt{"Timing family"}]] diagnostics::ExperimentFamily family{diagnostics::ExperimentFamily::TimerReload};
        [[=Opt{"Hardware revision"}]] Model model{Model::DMGB};
        [[=Opt{"Use CGB double speed"}]] bool double_speed{};
        [[=Opt{"Deterministic data seed"}]] uint64_t seed{1};
        [[=Opt{"Phase relative to the family's boundary"}]] int phase{};
        [[=Opt{"Initial fine scroll"}]] uint8_t scx{3};
        [[=Opt{"Initial window X"}]] uint8_t wx{5};
        [[=Opt{"Overlapping sprite X"}]] uint8_t sprite_x{16};
        [[=Opt{"Second sprite offset from the first"}]] int sprite_offset{};
        [[=Opt{"Use coordinate sprite priority"}]] bool coordinate_priority{};
        [[=Opt{"Additional setup values, repeatable and reducible"}]] std::vector<uint8_t> noise;
        [[=Opt{"Master-tick budget; zero uses the bounded family default"}]] uint64_t ticks{};
        [[=Opt{"Expected sampled value"}]] std::optional<uint8_t> expected_value;
        [[=Opt{"Basis for a proposed expectation"}]] diagnostics::ExperimentBasis basis{diagnostics::ExperimentBasis::InternalInvariant};
        [[=Opt{"Evidence supporting the expectation"}]] std::string evidence{"Platform invariants checked against captured hardware events"};
        [[=Opt{"New artifact directory"}, =Required]] std::string output;
    };
} // namespace

int main(const int argc, char **argv) {
    const auto arguments = StarParse::parse_or_exit<Arguments>(argc, argv);
    if (arguments.help_requested()) {
        std::print("{}", arguments.help());
        return 0;
    }
    if (arguments.version_requested()) {
        std::print("{}", arguments.version());
        return 0;
    }
    try {
        const auto result = diagnostics::RunExperiment({.mode = arguments->mode,
                                                        .family = arguments->family,
                                                        .model = arguments->model,
                                                        .doubleSpeed = arguments->double_speed,
                                                        .seed = arguments->seed,
                                                        .phase = arguments->phase,
                                                        .scx = arguments->scx,
                                                        .wx = arguments->wx,
                                                        .spriteX = arguments->sprite_x,
                                                        .spriteOffset = arguments->sprite_offset,
                                                        .coordinatePriority = arguments->coordinate_priority,
                                                        .noise = arguments->noise,
                                                        .budgetTicks = arguments->ticks,
                                                        .expectedValue = arguments->expected_value,
                                                        .basis = arguments->basis,
                                                        .basisEvidence = arguments->evidence},
                                                       arguments->output);
        diagnostics::WriteJson(std::cout, result);
        std::cout << '\n';
        return result.outcome == diagnostics::Outcome::Pass ? 0 : result.outcome == diagnostics::Outcome::AssertionFailure ? 1 : 2;
    } catch (const std::exception &error) {
        std::cerr << "Experiment infrastructure error: " << error.what() << '\n';
        return 2;
    }
}
