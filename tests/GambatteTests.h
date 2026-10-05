#ifndef STARGBC_GAMBATTETESTS_H
#define STARGBC_GAMBATTETESTS_H

#include <algorithm>
#include <array>
#include <span>
#include "TestRoms.h"

// NOTE: All Gambatte test roms finish after 15 lcd frames (1053360 clock cycles)

static constexpr unsigned GAMBATTE_FRAME_LIMIT = 15;

struct GambatteCase {
    std::string rom;
    // .screen path, "out:<hex>" for a result printed as hex tiles, or "audio:<0|1>" for silent/audible
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

static constexpr std::array<std::array<uint8_t, 8>, 16> GAMBATTE_HEX_TILES{{
        {0x00, 0x7F, 0x41, 0x41, 0x41, 0x41, 0x41, 0x7F}, // 0
        {0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, // 1
        {0x00, 0x7F, 0x01, 0x01, 0x7F, 0x40, 0x40, 0x7F}, // 2
        {0x00, 0x7F, 0x01, 0x01, 0x3F, 0x01, 0x01, 0x7F}, // 3
        {0x00, 0x41, 0x41, 0x41, 0x7F, 0x01, 0x01, 0x01}, // 4
        {0x00, 0x7F, 0x40, 0x40, 0x7E, 0x01, 0x01, 0x7E}, // 5
        {0x00, 0x7F, 0x40, 0x40, 0x7F, 0x41, 0x41, 0x7F}, // 6
        {0x00, 0x7F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}, // 7
        {0x00, 0x3E, 0x41, 0x41, 0x3E, 0x41, 0x41, 0x3E}, // 8
        {0x00, 0x7F, 0x41, 0x41, 0x7F, 0x01, 0x01, 0x7F}, // 9
        {0x00, 0x08, 0x22, 0x41, 0x7F, 0x41, 0x41, 0x41}, // A
        {0x00, 0x7E, 0x41, 0x41, 0x7E, 0x41, 0x41, 0x7E}, // B
        {0x00, 0x3E, 0x41, 0x40, 0x40, 0x40, 0x41, 0x3E}, // C
        {0x00, 0x7E, 0x41, 0x41, 0x41, 0x41, 0x41, 0x7E}, // D
        {0x00, 0x7F, 0x40, 0x40, 0x7F, 0x40, 0x40, 0x7F}, // E
        {0x00, 0x7F, 0x40, 0x40, 0x7F, 0x40, 0x40, 0x40}, // F
}};

static char readGambatteHexTile(const uint32_t *screen, const size_t index) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    for (size_t digit = 0; digit < GAMBATTE_HEX_TILES.size(); ++digit) {
        bool matches = true;
        for (size_t y = 0; y < 8 && matches; ++y) {
            for (size_t x = 0; x < 8 && matches; ++x) {
                const bool black = (GAMBATTE_HEX_TILES[digit][y] & (0x80 >> x)) != 0;
                matches = (screen[y * 160 + index * 8 + x] & 0xF8F8F8) == (black ? 0u : 0xF8F8F8u);
            }
        }
        if (matches)
            return kDigits[digit];
    }
    return '?';
}

static GambatteResult runGambatteTest(const GambatteCase &gambatte_case) {
    ThreadPermit _permit;
    const std::string label = gambatte_case.rom + " [" + std::string(ModelName(gambatte_case.model)) + "]";
    try {
        const auto gameboy = Gameboy::init({
                .romName = gambatte_case.rom,
                .model = gambatte_case.model,
                .noBootrom = true,
                .colorCorrection = false,
        });

        for (unsigned frame = 0; frame + 1 < GAMBATTE_FRAME_LIMIT; ++frame) {
            gameboy->RunFrame();
        }
        auto &audio = gameboy->[:testGameboyMember("audio_"):];
        const uint64_t mixChangesBefore = audio.GetMixLevelChanges();
        gameboy->RunFrame();
        const bool audible = audio.GetMixLevelChanges() != mixChangesBefore;

        if (gameboy->[:testGameboyMember("bus_"):].bootromRunning)
            return {.passed = false, .details = label + ": bootrom still running after " + std::to_string(GAMBATTE_FRAME_LIMIT) + " frames"};

        const std::string_view expected = gambatte_case.expected;
        if (expected.starts_with("audio:")) {
            const bool wantAudible = expected.back() == '1';
            if (audible != wantAudible)
                return {.passed = false,
                        .details = label + (wantAudible ? ": expected audio, but output was silent" : ": expected silence, but output was audible")};
        } else if (expected.starts_with("out:")) {
            const std::string_view digits = expected.substr(4);
            std::string read;
            for (size_t i = 0; i < digits.size(); ++i) {
                read += readGambatteHexTile(gameboy->GetScreenData(), i);
            }
            if (read != digits)
                return {.passed = false, .details = label + ": expected " + std::string(digits) + ", screen shows " + read};
        } else {
            const std::vector<uint32_t> expected_result = readBinaryFile(gambatte_case.expected);
            if (!std::ranges::equal(std::span(gameboy->GetScreenData(), expected_result.size()), expected_result))
                return {.passed = false, .details = label + ": screen does not match " + gambatte_case.expected};
        }
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
    TEST_CASE("gambatte: " ROM_STR *doctest::test_suite("gambatte")) {                                                                               \
        const auto &result = gambatteFutures()[IDX].get();                                                                                           \
        CHECK_MESSAGE(result.passed, result.details);                                                                                                \
    }

#define GAMBATTE_EXPANSION_CASES

#undef GAMBATTE_TEST

#endif // STARGBC_GAMBATTETESTS_H
