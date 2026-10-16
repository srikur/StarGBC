#define DOCTEST_CONFIG_IMPLEMENT
#include <ranges>
#include "CatalogRoms.h"
#include "GambatteTests.gen.h"
#include "TestRoms.h"

static int runSelectedCatalogCases() {
    std::vector<std::function<diagnostics::Result()>> selected;
    auto unmatched = catalogSelection.ids;
    const auto accept = [&](const auto &test, const std::string_view suite) {
        unmatched.erase(test.id);
        return (catalogSelection.ids.empty() || catalogSelection.ids.contains(test.id)) &&
               (!catalogSelection.model || test.model == *catalogSelection.model) &&
               (catalogSelection.subsystem.empty() ||
                std::ranges::any_of(std::string_view(test.subsystem) | std::views::split(','),
                                    [&](const auto tag) { return std::ranges::equal(tag, catalogSelection.subsystem); })) &&
               (catalogSelection.suites.empty() || catalogSelection.suites.contains(std::string(suite)));
    };
    for (const auto &test : gambatteCases)
        if (accept(test, "gambatte"))
            selected.emplace_back([test] { return runGambatteTest(test); });
    for (const auto &test : catalogCases)
        if (accept(test, test.suite))
            selected.emplace_back([test] { return runCatalogTest(test); });
    if (!unmatched.empty())
        throw std::invalid_argument("Unknown case ID: " + *unmatched.begin());
    if (selected.empty())
        throw std::invalid_argument("No catalog cases matched the selection");
    std::vector<std::shared_future<diagnostics::Result>> results;
    for (auto &run : selected)
        results.push_back(testWorkers->Submit(std::move(run)));
    unsigned failed{};
    for (auto &future : results) {
        const auto &result = future.get();
        testReport->Write(result);
        failed += !result.Passed();
    }
    testReport->Write(diagnostics::RunCompletion{.cases = static_cast<unsigned>(results.size()), .failed = failed});
    std::cout << results.size() << " selected cases, " << failed << " failed\n";
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(const int argc, char **argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--reproduce-catalog") {
        try {
            if (argc != 14)
                throw std::invalid_argument("Expected catalog reproduction inputs, interval contract and capture settings");
            const auto number = [](const char *argument) {
                const std::string text(argument);
                size_t consumed{};
                const auto value = std::stoull(text, &consumed);
                if (text.empty() || text.front() == '-' || consumed != text.size())
                    throw std::invalid_argument("Invalid reproduction number");
                return value;
            };
            const auto found = std::ranges::find(catalogCases, std::string_view(argv[5]), &CatalogCase::id);
            if (found == std::end(catalogCases))
                throw std::invalid_argument("Unknown catalog reproduction case");
            auto test = *found;
            const auto intervals = number(argv[8]);
            if (!intervals || intervals > 1'000'000 || number(argv[9]) > 1)
                throw std::invalid_argument("Invalid reproduction interval contract");
            test.rom = argv[2];
            test.bios = argv[3];
            test.expected = argv[4];
            test.intervals = intervals;
            romFrames = number(argv[9]) ? intervals : 0;
            testTraceCapacity = number(argv[10]);
            testTraceFilter.ticks = {number(argv[11]), number(argv[12])};
            if (testTraceCapacity > 1'000'000 || testTraceFilter.ticks.first > testTraceFilter.ticks.last)
                throw std::invalid_argument("Invalid reproduction capture bounds");
            if (testTraceCapacity)
                testObservationMode = TestObservationMode::Capture;
            if (std::string_view(argv[13]) != "none") {
                const auto line = number(argv[13]);
                if (line > 143)
                    throw std::invalid_argument("Invalid provenance line");
                testProvenanceLine = line;
            }
            threadSemaphore = std::make_shared<std::counting_semaphore<>>(1);
            const std::filesystem::path path = argv[6];
            failureDirectory = path.parent_path() / "failures";
            testExecutableHash = diagnostics::FileHash(argv[0]);
            testReport = std::make_unique<diagnostics::Report>(
                    path, diagnostics::RunIdentity{.executable = argv[0],
                                                   .executableHash = testExecutableHash,
                                                   .compiler = __VERSION__,
                                                   .arguments = std::vector<std::string>(argv + 1, argv + argc)});
            auto save = std::filesystem::path(test.rom);
            save.replace_extension(".sav");
            if ((std::filesystem::exists(save) ? diagnostics::FileHash(save) : "absent") != argv[7])
                throw std::invalid_argument("Reproduction cartridge save changed");
            const auto result = runCatalogTest(test);
            testReport->Write(result);
            testReport->Write(diagnostics::RunCompletion{.cases = 1, .failed = result.Passed() ? 0u : 1u});
            return result.Passed() ? EXIT_SUCCESS : EXIT_FAILURE;
        } catch (const std::exception &error) {
            std::cerr << "Reproduction infrastructure error: " << error.what() << '\n';
            return 2;
        }
    }
    if (argc > 1 && std::string_view(argv[1]) == "--reproduce-gambatte") {
        int input_count = argc;
        auto palette_sampling = DmgPaletteSampling::BitwiseOr;
        bool force_cgb_mode{};
        constexpr std::string_view palette_option = "--dmg-palette-sampling=";
        while (input_count > 2) {
            const auto option = std::string_view(argv[input_count - 1]);
            if (option == "--force-cgb-mode") {
                force_cgb_mode = true;
            } else if (option.starts_with(palette_option)) {
                const auto value = option.substr(palette_option.size());
                if (value != "BitwiseOr" && value != "NewValue") {
                    std::cerr << "Invalid DMG palette sampling setting\n";
                    return 2;
                }
                palette_sampling = value == "NewValue" ? DmgPaletteSampling::NewValue : DmgPaletteSampling::BitwiseOr;
            } else {
                break;
            }
            --input_count;
        }
        if (input_count != 8 && input_count != 11 && input_count != 12) {
            std::cerr << "Expected --reproduce-gambatte ROM MODEL EXPECTATION CASE_ID RESULTS INPUT_SAVE_HASH [TRACE_CAPACITY TRACE_BEGIN TRACE_END "
                         "[PROVENANCE_LINE]]\n";
            return EXIT_FAILURE;
        }
        try {
            if (input_count >= 11) {
                const auto number = [](const char *argument) {
                    const std::string text(argument);
                    size_t consumed{};
                    const auto value = std::stoull(text, &consumed);
                    if (text.empty() || text.front() == '-' || consumed != text.size())
                        throw std::invalid_argument("Invalid trace capture number");
                    return value;
                };
                testTraceCapacity = number(argv[8]);
                testTraceFilter.ticks = {number(argv[9]), number(argv[10])};
                if (!testTraceCapacity || testTraceCapacity > 1'000'000 || testTraceFilter.ticks.first > testTraceFilter.ticks.last)
                    throw std::invalid_argument("Invalid reproduction trace capacity or interval");
                testObservationMode = TestObservationMode::Capture;
                if (input_count == 12) {
                    const auto line = number(argv[11]);
                    if (line > 143)
                        throw std::invalid_argument("Provenance requires a display line from 0 through 143");
                    testProvenanceLine = line;
                }
            }
            const auto model = ModelFromName(argv[3]);
            if (!model || *model == Model::Auto)
                throw std::invalid_argument("Reproduction requires a specific model");
            threadSemaphore = std::make_shared<std::counting_semaphore<>>(1);
            const std::filesystem::path path = argv[6];
            failureDirectory = path.parent_path() / "failures";
            testExecutableHash = diagnostics::FileHash(argv[0]);
            testReport = std::make_unique<diagnostics::Report>(
                    path, diagnostics::RunIdentity{.executable = argv[0],
                                                   .executableHash = testExecutableHash,
                                                   .compiler = __VERSION__,
                                                   .arguments = std::vector<std::string>(argv + 1, argv + argc)});
            auto save = std::filesystem::path(argv[2]);
            save.replace_extension(".sav");
            const auto saveHash = std::filesystem::exists(save) ? diagnostics::FileHash(save) : "absent";
            if (saveHash != argv[7])
                throw std::invalid_argument("Reproduction cartridge save changed");
            const auto result = runGambatteTest({argv[5], argv[2], argv[4], *model, "reproduction", 1, "reproduction"}, palette_sampling, force_cgb_mode);
            testReport->Write(result);
            testReport->Write(diagnostics::RunCompletion{.cases = 1, .failed = result.Passed() ? 0u : 1u});
            std::cout << result.details << '\n';
            return result.Passed() ? EXIT_SUCCESS : EXIT_FAILURE;
        } catch (const std::exception &error) {
            std::cerr << "Reproduction infrastructure error: " << error.what() << '\n';
            return 2;
        }
    }
    runCatalogSelection = runSelectedCatalogCases;
    return ExecuteTestRoms(argc, argv);
}
