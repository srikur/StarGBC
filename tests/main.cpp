#define DOCTEST_CONFIG_IMPLEMENT
#include "AgeTestRoms.h"
#include "GambatteTests.gen.h"
#include "GbMicrotest.gen.h"
#include "GraphicsRoms.h"
#include "TestRoms.h"

int main(const int argc, char **argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--reproduce-gambatte") {
        if (argc != 8) {
            std::cerr << "Expected --reproduce-gambatte ROM MODEL EXPECTATION CASE_ID RESULTS INPUT_SAVE_HASH\n";
            return EXIT_FAILURE;
        }
        try {
            const auto model = ModelFromName(argv[3]);
            if (!model || *model == Model::Auto)
                throw std::invalid_argument("Reproduction requires a specific model");
            threadSemaphore = std::make_shared<std::counting_semaphore<>>(1);
            const std::filesystem::path path = argv[6];
            failureDirectory = path.parent_path() / "failures";
            testExecutableHash = diagnostics::FileHash(argv[0]);
            testReport = std::make_unique<diagnostics::Report>(path, diagnostics::RunIdentity{
                    .executable = argv[0], .executableHash = testExecutableHash, .compiler = __VERSION__,
                    .arguments = std::vector<std::string>(argv + 1, argv + argc)});
            auto save = std::filesystem::path(argv[2]);
            save.replace_extension(".sav");
            const auto saveHash = std::filesystem::exists(save) ? diagnostics::FileHash(save) : "absent";
            if (saveHash != argv[7])
                throw std::invalid_argument("Reproduction cartridge save changed");
            const auto result = runGambatteTest({argv[5], argv[2], argv[4], *model, "reproduction", 1, "reproduction"});
            testReport->Write(result);
            testReport->Write(diagnostics::RunCompletion{.cases = 1, .failed = result.Passed() ? 0u : 1u});
            std::cout << result.details << '\n';
            return result.Passed() ? EXIT_SUCCESS : EXIT_FAILURE;
        } catch (const std::exception &error) {
            std::cerr << "Reproduction infrastructure error: " << error.what() << '\n';
            return 2;
        }
    }
    runCatalogSelection = runSelectedGambatteCases;
    return ExecuteTestRoms(argc, argv);
}
