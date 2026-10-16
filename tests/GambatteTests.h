#ifndef STARGBC_GAMBATTETESTS_H
#define STARGBC_GAMBATTETESTS_H

#include <algorithm>
#include <array>
#include <span>
#include "TestRoms.h"

static constexpr unsigned GAMBATTE_FRAME_LIMIT = 15;

struct GambatteCase {
    const char *id;
    const char *rom;
    const char *expected;
    Model model;
    const char *name;
    unsigned line;
    const char *subsystem;
};

using GambatteResult = diagnostics::Result;

#define GAMBATTE_CASE(ID, ROM, EXPECTED, MODEL, NAME, LINE, SUBSYSTEM) {ID, ROM, EXPECTED, Model::MODEL, "gambatte: " NAME, LINE, SUBSYSTEM},

static constexpr GambatteCase gambatteCases[] = {
#define GAMBATTE_EXPANSION_CASES
};

#undef GAMBATTE_CASE

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

static GambatteResult runGambatteTest(const GambatteCase &gambatte_case,
                                      const DmgPaletteSampling palette_sampling = DmgPaletteSampling::NewValue,
                                      const bool native_color_startup = true) {
    ThreadPermit _permit;
    TestObservationSession observations;
    const std::string label = std::string(gambatte_case.rom) + " [" + std::string(ModelName(gambatte_case.model)) + "]";
    const std::string_view expected = gambatte_case.expected;
    const bool force_cgb_mode = native_color_startup && IsCgb(gambatte_case.model);
    const std::string variant = expected.starts_with("audio:") ? "audio" : expected.starts_with("out:") ? "digits" : "screen";
    GambatteResult result{.test = {.id = gambatte_case.id,
                                   .suite = "gambatte",
                                   .rom = gambatte_case.rom,
                                   .model = std::string(ModelName(gambatte_case.model)),
                                   .startup = "skip-bootrom",
                                   .expectationVariant = variant},
                          .budgetTicks = GAMBATTE_FRAME_LIMIT * diagnostics::RunFrameTicks,
                          .expected = gambatte_case.expected,
                          .details = label};
    std::unique_ptr<Gameboy> gameboy;
    std::vector<uint32_t> expectedPixels;
    diagnostics::TerminalState terminal;
    auto savePath = std::filesystem::path(gambatte_case.rom);
    savePath.replace_extension(".sav");
    const bool hasSave = std::filesystem::exists(savePath);
    const auto evaluate = [&] {
        result.inputs.cartridgeSave = hasSave ? diagnostics::FileHash(savePath) : "absent";
        result.inputs.rom = diagnostics::FileHash(gambatte_case.rom);
        result.inputs.bootrom = "none";
        result.inputs.reference = variant == "screen" ? diagnostics::FileHash(gambatte_case.expected) : diagnostics::TextHash(expected);
        result.inputs.configuration =
                diagnostics::TextHash(result.test.model + "|skip-bootrom|virtual-rtc|color-correction-off|15-intervals|" +
                                      (palette_sampling == DmgPaletteSampling::NewValue ? "dmg-lcd-new-value|gambatte-v2" : "gambatte-v1") +
                                      (force_cgb_mode ? "|force-cgb-mode" : ""));
        gameboy = Gameboy::init({
                .romName = gambatte_case.rom,
                .model = gambatte_case.model,
                .noBootrom = true,
                .colorCorrection = false,
                .executionPolicy = testExecutionPolicy,
                .dmg_palette_sampling = palette_sampling,
                .force_cgb_mode = force_cgb_mode,
        });
        observations.Attach(*gameboy);
        for (unsigned frame = 0; frame + 1 < GAMBATTE_FRAME_LIMIT; ++frame)
            gameboy->RunFrame();
        auto &audio = gameboy->[:testGameboyMember("audio_"):];
        const uint64_t mixChangesBefore = audio.GetMixLevelChanges();
        gameboy->RunFrame();
        terminal = terminalState(*gameboy);
        terminal.audioBeginTick = (GAMBATTE_FRAME_LIMIT - 1) * diagnostics::RunFrameTicks;
        terminal.audioEndTick = GAMBATTE_FRAME_LIMIT * diagnostics::RunFrameTicks;
        terminal.mixerLevelChanges = audio.GetMixLevelChanges() - mixChangesBefore;
        const bool audible = terminal.mixerLevelChanges != 0;
        result.position = gameboy->ExecutionPosition();
        result.completion = diagnostics::Completion::FixedInterval;
        result.outcome = diagnostics::Outcome::AssertionFailure;
        if (gameboy->[:testGameboyMember("bus_"):].bootromRunning) {
            result.outcome = diagnostics::Outcome::Timeout;
            result.completion = diagnostics::Completion::Deadline;
            result.details += ": bootrom still running after 15 intervals";
            return;
        }
        if (expected.starts_with("audio:")) {
            const bool wantAudible = expected.back() == '1';
            result.actual = audible ? "audio:1" : "audio:0";
            if (audible != wantAudible) {
                result.details += wantAudible ? ": expected audio, but output was silent" : ": expected silence, but output was audible";
                return;
            }
        } else if (expected.starts_with("out:")) {
            const auto digits = expected.substr(4);
            if (digits.empty() || digits.size() > 20)
                throw std::invalid_argument("Gambatte output must occupy 1..20 tiles");
            expectedPixels.assign(gameboy->GetScreenData(), gameboy->GetScreenData() + SCREEN_WIDTH * SCREEN_HEIGHT);
            for (size_t i = 0; i < digits.size(); ++i) {
                terminal.decodedDigits += readGambatteHexTile(gameboy->GetScreenData(), i);
                const auto digit = std::string_view("0123456789ABCDEF").find(digits[i]);
                if (digit == std::string_view::npos)
                    throw std::invalid_argument("Invalid Gambatte hex digit");
                for (size_t y = 0; y < 8; ++y)
                    for (size_t x = 0; x < 8; ++x)
                        expectedPixels[y * SCREEN_WIDTH + i * 8 + x] = (GAMBATTE_HEX_TILES[digit][y] & (0x80 >> x)) ? 0 : 0xFFFFFF;
            }
            result.actual = "out:" + terminal.decodedDigits;
            if (terminal.decodedDigits != digits) {
                result.details += ": expected " + std::string(digits) + ", screen shows " + terminal.decodedDigits;
                return;
            }
        } else {
            expectedPixels = readBinaryFile(gambatte_case.expected);
            result.actual = diagnostics::Sha256(std::as_bytes(std::span(gameboy->GetScreenData(), expectedPixels.size())));
            if (!std::ranges::equal(std::span(gameboy->GetScreenData(), expectedPixels.size()), expectedPixels)) {
                result.details += ": screen does not match " + std::string(gambatte_case.expected);
                return;
            }
        }
        result.outcome = diagnostics::Outcome::Pass;
    };
    try {
        evaluate();
    } catch (const std::exception &error) {
        result.outcome = diagnostics::Outcome::InfrastructureError;
        result.completion = diagnostics::Completion::Exception;
        result.details += ": " + std::string(error.what());
    }
    if (!result.Passed() && !failureDirectory.empty()) {
        const diagnostics::Reproduction reproduction{.test = result.test,
                                                     .inputs = result.inputs,
                                                     .rom = gambatte_case.rom,
                                                     .reference = variant == "screen" ? gambatte_case.expected : "",
                                                     .cartridgeSave = hasSave ? savePath.string() : "",
                                                     .expectation = gambatte_case.expected,
                                                     .intervals = GAMBATTE_FRAME_LIMIT,
                                                     .noBootrom = true,
                                                     .colorCorrection = false,
                                                     .normalization = variant == "digits" ? "gambatte-digits-f8-mask-compared-tiles-only-v1"
                                                                                          : "gambatte-rgb5-to-stargbc-uncorrected-v1",
                                                     .completion = "FixedInterval",
                                                     .executableHash = testExecutableHash,
                                                     .dmg_palette_sampling = std::string(model_name(palette_sampling)),
                                                     .force_cgb_mode = force_cgb_mode};
        diagnostics::WriteFailureBundle(failureDirectory / diagnostics::TextHash(result.test.id), result, reproduction, terminal,
                                        gameboy ? std::span(gameboy->GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT) : std::span<const uint32_t>{},
                                        expectedPixels, variant == "digits" ? 0xF8F8F8 : 0xFFFFFFFF);
        observations.Export(result.artifacts.front(), result);
    }
    return result;
}

static auto &gambatteFutures() {
    using SF = std::shared_future<GambatteResult>;
    static std::vector<SF> futures = [] {
        std::vector<SF> tmp;
        tmp.reserve(std::size(gambatteCases));

        for (const auto &rom : gambatteCases) {
            tmp.push_back(selectedTestNames.contains(rom.name) ? testWorkers->Submit([rom] { return runGambatteTest(rom); }) : SF{});
        }
        return tmp;
    }();
    return futures;
}

static void checkGambatteCase(const size_t idx) {
    const auto &result = gambatteFutures()[idx].get();
    if (testReport)
        testReport->Write(result);
    CHECK_MESSAGE(result.Passed(), result.details);
}

[[maybe_unused]] static const bool gambatteCasesRegistered =
        registerTableCases<gambatteCases, checkGambatteCase>("tests/catalog/gambatte.json", "gambatte");


#endif // STARGBC_GAMBATTETESTS_H
