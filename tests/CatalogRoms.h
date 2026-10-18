#pragma once
#include "../frontends/sdl/GameboyFiles.h"

#include <format>
#include "GbMicrotestDiagnostics.h"
#include "TestRoms.h"

enum class CatalogRunner { AgeScreen, AgeMarker, MicroProtocol, MicroDiagnostic, RomScreen, RomStopScreen, RomRegisters };

struct CatalogCase {
    const char *id;
    const char *name;
    const char *suite;
    const char *rom;
    const char *bios;
    const char *expected;
    const char *startup;
    const char *variant;
    const char *subsystem;
    Model model;
    CatalogRunner runner;
    unsigned intervals;
    bool grouped;
};

static constexpr CatalogCase catalogCases[] = {

#include "CatalogCases.inc"
};

static diagnostics::Result runCatalogTest(const CatalogCase &test) {
    using enum CatalogRunner;
    ThreadPermit permit;
    TestObservationSession observations;
    const unsigned limit = romFrames ? romFrames : test.intervals;
    diagnostics::Result result{.test = {.id = test.id,
                                        .suite = test.suite,
                                        .rom = test.rom,
                                        .model = std::string(ModelName(test.model)),
                                        .startup = test.startup,
                                        .expectationVariant = test.variant},
                               .budgetTicks = limit * diagnostics::RunFrameTicks,
                               .expected = *test.expected ? test.expected : std::string(model_name(test.runner)),
                               .details = test.name};
    std::unique_ptr<Gameboy> gameboy;
    std::vector<uint32_t> expected;
    diagnostics::TerminalState terminal;
    auto save = std::filesystem::path(test.rom);
    save.replace_extension(".sav");
    const bool hasSave = std::filesystem::exists(save);
    try {
        result.inputs.cartridgeSave = hasSave ? diagnostics::FileHash(save) : "absent";
        result.inputs.rom = diagnostics::FileHash(test.rom);
        result.inputs.bootrom = *test.bios ? diagnostics::FileHash(test.bios) : "embedded:" + std::string(Gameboy::CoreBuildIdentity());
        std::ostringstream contract;
        contract << test.variant;
        std::optional<MicrotestDiagnosticCheck> diagnostic;
        bool diagnosticScreen{};
        if (test.runner == MicroDiagnostic) {
            const auto entry = std::ranges::find_if(microtestDiagnostics, [&](const auto &candidate) {
                return std::string_view(test.id).contains("/" + std::string(candidate.name) + ".gb|");
            });
            if (entry == std::end(microtestDiagnostics))
                throw std::invalid_argument("Missing microtest diagnostic contract");
            diagnostics::WriteJson(contract, *entry);
            diagnosticScreen = entry->output == MicrotestOutput::Screen;
            result.inputs.reference = *test.expected ? diagnostics::FileHash(test.expected) : diagnostics::TextHash(contract.str());
            diagnostic.emplace(*entry, test.expected);
        }
        result.inputs.reference = *test.expected ? diagnostics::FileHash(test.expected) : diagnostics::TextHash(contract.str());
        result.inputs.configuration =
                diagnostics::TextHash(result.test.model + "|" + test.startup + "|" + contract.str() + "|virtual-rtc|color-correction|" +
                                      std::to_string(limit) + (romFrames ? "|fixed|" : "|until|") + "catalog-v1");
        const bool graphical = test.runner == AgeScreen || test.runner == RomScreen || test.runner == RomStopScreen || diagnosticScreen;
        if (graphical)
            expected = readBinaryFile(test.expected);
        const host::LaunchSettings launch_settings{
                .rom_path = test.rom, .bios_path = test.bios, .emulation = {.model = test.model, .execution_policy = testExecutionPolicy}};
        gameboy = host::init_gameboy(launch_settings);
        if (!*test.bios)
            result.inputs.bootrom = diagnostics::Sha256(std::as_bytes(gameboy->BootromBytes()));
        observations.Attach(*gameboy, launch_settings);
        bool passed{}, completed{}, stable{};
        unsigned matching{}, intervals{};
        std::array<uint8_t, 3> previous{};
        for (; intervals < limit; ++intervals) {
            gameboy->RunFrame();
            const bool boot = gameboy->[:testGameboyMember("bus_"):].bootromRunning;
            if ((test.runner == AgeMarker || test.runner == AgeScreen) && boot)
                continue;
            if (test.runner == MicroProtocol) {
                const std::array current{gameboy->DebugPeek(0xFF80), gameboy->DebugPeek(0xFF81), gameboy->DebugPeek(0xFF82)};
                completed = !boot && (current[2] == 1 || current[2] == 255);
                passed = completed && current[2] == 1;
                stable = completed && current == previous;
                previous = current;
            } else if (test.runner == AgeMarker) {
                const auto pc = gameboy->InspectCpu().pc;
                completed = gameboy->InspectRegister(0xFFFF).cpuVisible == 0 && pc >= 8 && gameboy->InspectCpuRead(pc - 4) == 0xFB &&
                            gameboy->InspectCpuRead(pc - 3) == 0 && gameboy->InspectCpuRead(pc - 2) == 0x76 && gameboy->InspectCpuRead(pc - 1) == 0;
                passed = completed && mooneyePassed(*gameboy);
                stable = completed;
            } else {
                if (diagnostic) {
                    std::ostringstream details;
                    passed = diagnostic->passed(*gameboy, details);
                    result.actual = details.str();
                } else if (test.runner == RomRegisters) {
                    passed = !boot && mooneyePassed(*gameboy);
                } else {
                    const uint32_t mask = test.runner == RomStopScreen ? 0xFFFFFF : 0xFFFFFFFF;
                    passed = !boot && (test.runner != RomStopScreen || gameboy->InspectCpu().stopped) &&
                             std::ranges::equal(std::span(gameboy->GetScreenData(), expected.size()), expected,
                                                [mask](const auto one, const auto two) { return (one & mask) == (two & mask); });
                }
                matching = passed ? matching + 1 : 0;
                stable = matching >= 2;
                if (test.runner == AgeScreen)
                    passed = stable;
                completed = passed;
            }
            if (!romFrames && stable) {
                ++intervals;
                break;
            }
        }
        terminal = terminalState(*gameboy);
        result.position = gameboy->ExecutionPosition();
        const bool sampledAfterBoot = romFrames && !gameboy->[:testGameboyMember("bus_"):].bootromRunning;
        result.outcome = passed                          ? diagnostics::Outcome::Pass
                         : completed || sampledAfterBoot ? diagnostics::Outcome::AssertionFailure
                                                         : diagnostics::Outcome::Timeout;
        result.completion = romFrames ? diagnostics::Completion::FixedInterval
                            : stable  ? (test.runner == AgeMarker ? diagnostics::Completion::CompletionMarker : diagnostics::Completion::StableResult)
                                      : diagnostics::Completion::Deadline;
        if (graphical) {
            result.actual = diagnostics::Sha256(std::as_bytes(std::span(gameboy->GetScreenData(), 160 * 144)));
        } else if (test.runner == MicroProtocol) {
            result.actual = std::format("FF80={:02X} FF81={:02X} FF82={:02X}", previous[0], previous[1], previous[2]);
        } else if (!diagnostic) {
            result.actual = std::format("BC={:04X} DE={:04X} HL={:04X}", terminal.bc, terminal.de, terminal.hl);
        }
        result.details += " after " + std::to_string(intervals) + " RunFrame intervals: " + result.actual;
    } catch (const std::exception &error) {
        result.outcome = diagnostics::Outcome::InfrastructureError;
        result.completion = diagnostics::Completion::Exception;
        result.details += ": " + std::string(error.what());
    }
    if (gameboy) {
        terminal = terminalState(*gameboy);
        result.position = gameboy->ExecutionPosition();
    }
    if (!result.Passed() && !failureDirectory.empty()) {
        diagnostics::WriteFailureBundle(failureDirectory / diagnostics::TextHash(result.test.id), result,
                                        {.test = result.test,
                                         .inputs = result.inputs,
                                         .rom = test.rom,
                                         .bootrom = test.bios,
                                         .reference = test.expected,
                                         .cartridgeSave = hasSave ? save.string() : "",
                                         .expectation = result.expected,
                                         .intervals = limit,
                                         .normalization = test.runner == RomStopScreen ? "rgb24" : "rgba32",
                                         .completion = std::string(model_name(test.runner)),
                                         .executableHash = testExecutableHash,
                                         .fixedIntervals = romFrames != 0},
                                        terminal, gameboy ? std::span(gameboy->GetScreenData(), 160 * 144) : std::span<const uint32_t>{}, expected,
                                        test.runner == RomStopScreen ? 0xFFFFFF : 0xFFFFFFFF);
        observations.Export(result.artifacts.front(), result);
    }
    return result;
}

static auto &catalogFutures() {
    static auto futures = [] {
        std::vector<std::shared_future<diagnostics::Result>> results;
        for (const auto &test : catalogCases)
            results.push_back(selectedTestNames.contains(test.name) ? testWorkers->Submit([test] { return runCatalogTest(test); })
                                                                    : std::shared_future<diagnostics::Result>{});
        return results;
    }();
    return futures;
}

static void checkCatalogCase() {
    const auto &result = catalogFutures()[currentTestCase->m_line].get();
    if (testReport)
        testReport->Write(result);
    CHECK_MESSAGE(result.Passed(), result.details);
}

[[maybe_unused]] static const bool catalogCasesRegistered = [] {
    for (size_t i = 0; i < std::size(catalogCases); ++i) {
        const auto &test = catalogCases[i];
        if (!test.grouped)
            doctest::detail::regTest(doctest::detail::TestCase(checkCatalogCase, "tests/catalog", static_cast<unsigned>(i),
                                                               doctest::detail::TestSuite() * test.suite) *
                                     test.name);
    }
    return true;
}();

TEST_CASE("model ROMs: Mooneye startup and HALT, SameSuite early CGB audio" * doctest::test_suite("rom")) {
    for (size_t i = 0; i < std::size(catalogCases); ++i) {
        if (!catalogCases[i].grouped)
            continue;
        const auto &result = catalogFutures()[i].get();
        if (testReport)
            testReport->Write(result);
        CHECK_MESSAGE(result.Passed(), result.details);
    }
}
