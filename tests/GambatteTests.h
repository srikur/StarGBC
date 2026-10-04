#ifndef STARGBC_GAMBATTETESTS_H
#define STARGBC_GAMBATTETESTS_H

#include <algorithm>
#include <span>
#include "TestRoms.h"

// NOTE: All Gambatte test roms finish after 15 lcd frames (1053360 clock cycles)

static constexpr unsigned GAMBATTE_FRAME_LIMIT = 15;

struct GambatteCase {
    std::string rom;
    std::string expected;
    Model model;
};

struct GambatteResult {
    bool passed;
    std::string details;
};

#define STRING_REPLACE_GAMBATTE_CASES

static const std::vector<GambatteCase> gambatte_roms = {STRING_REPLACE_GAMBATTE_CASES};

#undef STRING_REPLACE_GAMBATTE_CASES

static GambatteResult runGambatteTest(const GambatteCase &gambatte_case) {
    ThreadPermit _permit;
    const std::string label = gambatte_case.rom + " [" + std::string(ModelName(gambatte_case.model)) + "]";
    try {
        const std::vector<uint32_t> expected_result = readBinaryFile(gambatte_case.expected);
        const auto gameboy = Gameboy::init({
                .romName = gambatte_case.rom,
                .model = gambatte_case.model,
                .noBootrom = true,
        });

        for (unsigned frame = 0; frame < GAMBATTE_FRAME_LIMIT; ++frame) {
            gameboy->RunFrame();
        }

        if (gameboy->[:testGameboyMember("bus_"):].bootromRunning)
            return {.passed = false, .details = label + ": bootrom still running after " + std::to_string(GAMBATTE_FRAME_LIMIT) + " frames"};
        if (!std::ranges::equal(std::span(gameboy->GetScreenData(), expected_result.size()), expected_result))
            return {.passed = false, .details = label + ": screen does not match " + gambatte_case.expected};
        return {.passed = true, .details = label};
    } catch (const std::exception &e) {
        return {.passed = false, .details = label + ": " + e.what()};
    }
}

static auto &gambatteFutures() {
    using SF = std::shared_future<GambatteResult>;
    static std::vector<SF> futures = [] {
        std::vector<SF> tmp;
        tmp.reserve(gambatte_roms.size());

        for (const auto &rom : gambatte_roms) {
            tmp.emplace_back(
                    std::async(parallelRomTests ? std::launch::async : std::launch::deferred, [rom] { return runGambatteTest(rom); }).share());
        }
        return tmp;
    }();
    return futures;
}

#define GAMBATTE_TEST(IDX, ROM_STR)                                                                                                                  \
    TEST_CASE("gambatte: " ROM_STR) {                                                                                                                \
        const auto &result = gambatteFutures()[IDX].get();                                                                                           \
        CHECK_MESSAGE(result.passed, result.details);                                                                                                \
    }

#define GAMBATTE_EXPANSION_CASES

#undef GAMBATTE_TEST

#endif // STARGBC_GAMBATTETESTS_H
