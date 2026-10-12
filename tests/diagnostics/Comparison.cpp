#include <doctest/doctest.h>
#include <fstream>
#include <sstream>
#include "diagnostics/Comparison.h"
#include "diagnostics/Json.h"

namespace {
    template<class T>
    std::string comparisonJson(const T &value) {
        std::ostringstream output;
        diagnostics::WriteJson(output, value);
        return output.str();
    }
}

TEST_CASE (
"diagnostics: optimized execution agrees with ordinary hardware and sampled outputs"
)
 {
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        CAPTURE(model);
        const GameboySettings settings{.romName = "roms/acid/cgb-acid2.gbc", .model = model, .noBootrom = true};
        Gameboy optimized(settings), ordinary(settings);
        for (unsigned interval = 0; interval < 30; ++interval) {
            CAPTURE(interval);
            const auto ticks = interval % 2 ? 140'449 : 140'447;
            static_cast<void>(optimized.AdvanceTicks(ticks, ExecutionPolicy::Optimized));
            static_cast<void>(ordinary.AdvanceTicks(ticks, ExecutionPolicy::Ordinary));
            const auto left = diagnostics::CaptureObservation(optimized), right = diagnostics::CaptureObservation(ordinary);
            CHECK(left.hardwareHash == right.hardwareHash);
            CHECK(left.memoryHash == right.memoryHash);
            CHECK(left.framebufferHash == right.framebufferHash);
            CHECK(left.audioHash == right.audioHash);
            CHECK(left.audioSamples == right.audioSamples);
            CHECK(comparisonJson(left.machine) == comparisonJson(right.machine));
        }
    }
}

TEST_CASE (
"diagnostics: ordered observation capture bounds events and rejects incomplete plans"
)
 {
    const auto directory = std::filesystem::temp_directory_path() / ("stargbc-comparison-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const GameboySettings settings{.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true};
    Gameboy machine(settings);
    const auto initial = diagnostics::IdentifyInitialInputs(machine, settings);
    CHECK_THROWS_AS(diagnostics::WriteObservations(directory / "invalid", machine, nullptr, initial, {}, {.endTick = 17, .sampleTicks = 0}), std::invalid_argument);
    REQUIRE(machine.TimelineTicks() == 0);
    diagnostics::WriteObservations(directory / "complete", machine, nullptr, initial, {}, {.beginTick = 7, .endTick = 49, .sampleTicks = 11, .events = true, .eventDetails = true});
    CHECK(machine.TimelineTicks() == 49);
    CHECK(machine.Observations() == nullptr);
    std::ifstream input(directory / "complete");
    const std::string contents{std::istreambuf_iterator<char>(input), {}};
    CHECK(contents.contains("\"record\":\"complete\",\"tick\":49,\"samples\":5"));
    CHECK(contents.contains("\"kind\":\"BusRead\""));
    Gameboy overflow(settings);
    CHECK_THROWS_AS(diagnostics::WriteObservations(directory / "overflow", overflow, nullptr, initial, {}, {.endTick = 100, .events = true, .eventCapacity = 1}), std::runtime_error);
    CHECK(overflow.Observations() == nullptr);
    std::filesystem::remove_all(directory);
}
