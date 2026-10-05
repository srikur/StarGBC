#ifndef STARGBC_GBMICROTEST_H
#define STARGBC_GBMICROTEST_H

#include <array>
#include <sstream>
#include "GbMicrotestDiagnostics.h"
#include "TestRoms.h"

// https://github.com/aappleby/GBMicrotest
//   0xFF80 = actual value, 0xFF81 = expected value,
//   0xFF82 = 0x01 on pass, 0xFF on fail

struct GbMicrotestResult {
    bool passed;
    std::string details;
};

static GbMicrotestResult runGbMicrotest(const std::string &rom) {
    ThreadPermit _permit;
    try {
        const auto gameboy = Gameboy::init({
                .romName = rom,
                .biosPath = bootroms.dmgBootrom,
                .model = Model::DMGB,
        });

        const auto &hram = gameboy->[:testGameboyMember("memory_"):].hram_;
        const auto &bus = gameboy->[:testGameboyMember("bus_"):];
        std::optional<MicrotestDiagnosticCheck> diagnostic;
        for (const auto &test : microtestDiagnostics) {
            if (rom == "roms/gbmicrotest/" + std::string(test.name) + ".gb") {
                diagnostic.emplace(test);
                break;
            }
        }
        const auto result = [&](const unsigned frames) -> GbMicrotestResult {
            if (diagnostic) {
                std::ostringstream details;
                details << rom << " [dmgb] after " << frames << " frames";
                const auto passed = diagnostic->passed(*gameboy, details);
                return {.passed = passed, .details = details.str()};
            }
            const bool completed = !bus.bootromRunning && (hram[2] == 0x01 || hram[2] == 0xFF);
            std::ostringstream details;
            details << rom << " [dmgb] " << (completed ? (hram[2] == 0x01 ? "passed" : "failed") : "no result") << " after " << frames << " frames"
                    << std::hex << " (FF80=" << +hram[0] << " FF81=" << +hram[1] << " FF82=" << +hram[2] << ")";
            return {completed && hram[2] == 0x01, details.str()};
        };

        const unsigned frameLimit = romFrames ? romFrames : 600;
        std::array<uint8_t, 3> previous{};
        unsigned matchingFrames = 0;
        for (unsigned frames = 1; frames <= frameLimit; ++frames) {
            gameboy->RunFrame();
            if (diagnostic) {
                const auto current = result(frames);
                matchingFrames = current.passed ? matchingFrames + 1 : 0;
                if (!romFrames && matchingFrames >= 2)
                    return current;
                continue;
            }
            const std::array current{hram[0], hram[1], hram[2]};
            if (!romFrames && !bus.bootromRunning && current == previous && (current[2] == 0x01 || current[2] == 0xFF)) {
                return result(frames);
            }
            previous = current;
        }
        return result(frameLimit);
    } catch (const std::exception &e) {
        return {.passed = false, .details = rom + " [dmgb]: " + e.what()};
    }
}

#define STRING_REPLACE_GBMICROTEST_ROMS

static const std::vector<std::string> gbMicrotestRoms = {STRING_REPLACE_GBMICROTEST_ROMS};

#undef STRING_REPLACE_GBMICROTEST_ROMS

static auto &gbMicrotestFutures() {
    using SF = std::shared_future<GbMicrotestResult>;
    static std::vector<SF> futures = [] {
        std::vector<SF> tmp;
        tmp.reserve(gbMicrotestRoms.size());

        for (const auto &rom : gbMicrotestRoms) {
            tmp.emplace_back(
                    std::async(parallelRomTests ? std::launch::async : std::launch::deferred, [rom] { return runGbMicrotest(rom); }).share());
        }
        return tmp;
    }();
    return futures;
}

#define GBMICRO_TEST(IDX, ROM_STR)                                                                                                                   \
    TEST_CASE("gbmicrotest: " ROM_STR *doctest::test_suite("gbmicrotest")) {                                                                         \
        const auto &result = gbMicrotestFutures()[IDX].get();                                                                                        \
        CHECK_MESSAGE(result.passed, result.details);                                                                                                \
    }

#define GBMICROTEST_EXPANSION_CASES

#undef GBMICRO_TEST

#endif // STARGBC_GBMICROTEST_H
