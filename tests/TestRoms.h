#ifndef STARGBC_TESTROMS_H
#define STARGBC_TESTROMS_H

#include <Gameboy.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <semaphore>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include "ThreadContext.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Report.h"
#include "diagnostics/Artifacts.h"
#include "diagnostics/Trace.h"
#include "diagnostics/Checkpoint.h"
#include "diagnostics/Replay.h"
#include "diagnostics/Provenance.h"

struct Bootroms {
    const char *dmg0Bootrom = "roms/dmg0_boot.bin";
    const char *dmgBootrom = "roms/dmg_boot.bin";
    const char *mgbBootrom = "roms/mgb_boot.bin";
    const char *cgb0Bootrom = "roms/cgb0_boot.bin";
    const char *cgbEBootrom = "roms/cgbE_boot.bin";
    const char *cgbBootrom = "roms/cgb_boot.bin";
    const char *cgbAGBBootrom = "roms/cgb_agb_boot.bin";
    const char *cgbAGB0Bootrom = "roms/cgb_agb0_boot.bin";
    const char *sgbBootrom = "roms/sgb_boot.bin";
    const char *sgb2Bootrom = "roms/sgb2_boot.bin";
};

static constexpr Bootroms bootroms{};
static unsigned romFrames = 0;
static ExecutionPolicy testExecutionPolicy{ExecutionPolicy::Optimized};
static std::unique_ptr<diagnostics::Report> testReport;
static std::filesystem::path failureDirectory;
static std::string testExecutableHash;

enum class TestObservationMode { Disabled, Observe, Capture };
static TestObservationMode testObservationMode{TestObservationMode::Disabled};
static size_t testTraceCapacity{65536};
static diagnostics::EventFilter testTraceFilter;
static bool testCheckpointFailures{};
static std::optional<uint8_t> testProvenanceLine;

class TestObservationSession {
    std::optional<diagnostics::TraceRecorder> recorder_;
    Gameboy *machine_{};
    diagnostics::InputFingerprint checkpointInputs_;
    std::optional<diagnostics::ProvenanceRecorder> provenance_;

public:
    void Attach(Gameboy &gameboy) {
        machine_ = &gameboy;
        if (testCheckpointFailures)
            checkpointInputs_ = diagnostics::IdentifyInitialInputs(gameboy, gameboy.InitialSettings());
        if (testProvenanceLine) {
            provenance_.emplace(diagnostics::PixelRegion{.y = {*testProvenanceLine, *testProvenanceLine}});
            gameboy.RecordPixelsWith(&provenance_->Context());
        }
        if (testObservationMode == TestObservationMode::Disabled)
            return;
        auto filter = testTraceFilter;
        if (testProvenanceLine && !filter.lines)
            filter.lines = {*testProvenanceLine, *testProvenanceLine};
        recorder_.emplace(testTraceCapacity, filter);
        recorder_->SetCaptureEnabled(testObservationMode == TestObservationMode::Capture);
        gameboy.ObserveWith(&recorder_->Context());
    }

    void Export(const std::filesystem::path &directory, const diagnostics::Result &result) {
        if (provenance_) {
            provenance_->Identify(std::string(Gameboy::CoreBuildIdentity()), result.inputs.rom);
            provenance_->WriteJson(directory / "provenance.json");
        }
        if (testCheckpointFailures && machine_)
            diagnostics::WriteCheckpoint(directory / "terminal.checkpoint", *machine_,
                                         {.build = std::string(Gameboy::CoreBuildIdentity()), .inputs = checkpointInputs_});
        if (recorder_ && testObservationMode == TestObservationMode::Capture) {
            recorder_->Identify(testExecutableHash, result.inputs.rom, result.inputs.configuration);
            recorder_->WriteJsonLines(directory / "events.jsonl");
            recorder_->WriteBinary(directory / "events.trace");
        }
    }
};

struct CatalogSelection {
    std::set<std::string, std::less<>> ids;
    std::optional<Model> model;
    std::string subsystem;
    std::set<std::string> suites;

    [[nodiscard]] bool Active() const { return !ids.empty() || model.has_value() || !subsystem.empty(); }
};

static CatalogSelection catalogSelection;
static std::function<int()> runCatalogSelection;

static consteval std::meta::info testGameboyMember(const std::string_view name) {
    for (const auto m : std::meta::nonstatic_data_members_of(^^Gameboy, std::meta::access_context::unchecked())) {
        if (std::meta::identifier_of(m) == name)
            return m;
    }
    throw "Gameboy member not found";
}

static bool mooneyePassed(const Gameboy &gameboy) {
    const auto &regs = gameboy.[:testGameboyMember("registers_"):];
    return regs.GetBC() == 0x0305 && regs.GetDE() == 0x080D && regs.GetHL() == 0x1522;
}

static diagnostics::TerminalState terminalState(Gameboy &gameboy) {
    const auto state = gameboy.Inspect();
    return {.pc = state.cpu.pc, .sp = state.cpu.sp, .af = state.cpu.af, .bc = state.cpu.bc,
            .de = state.cpu.de, .hl = state.cpu.hl, .interruptEnable = state.interrupts.enable,
            .interruptFlag = state.interrupts.storedFlags, .lcdc = gameboy.InspectRegister(0xFF40).stored, .ly = state.ppu.ly,
            .dot = static_cast<uint16_t>(state.ppu.dot), .halted = state.cpu.halted, .stopped = state.cpu.stopped, .locked = state.cpu.locked};
}

static const doctest::TestCaseData *currentTestCase = nullptr;

struct CurrentTestCaseListener : doctest::IReporter {
    unsigned cases_{};
    unsigned failed_{};
    explicit CurrentTestCaseListener(const doctest::ContextOptions &) {}
    void test_case_start(const doctest::TestCaseData &tc) override { currentTestCase = &tc; }
    void report_query(const doctest::QueryData &) override {}
    void test_run_start() override {}
    void test_run_end(const doctest::TestRunStats &) override {
        if (testReport)
            testReport->Write(diagnostics::RunCompletion{.cases = cases_, .failed = failed_});
    }
    void test_case_reenter(const doctest::TestCaseData &) override {}
    void test_case_end(const doctest::CurrentTestCaseStats &stats) override {
        ++cases_;
        failed_ += !stats.testCaseSuccess;
        if (testReport)
            testReport->Write(diagnostics::AssertionResult{
                    .name = currentTestCase->m_name,
                    .suite = currentTestCase->m_test_suite,
                    .outcome = stats.testCaseSuccess ? diagnostics::Outcome::Pass : diagnostics::Outcome::AssertionFailure,
                    .assertions = static_cast<unsigned>(stats.numAssertsCurrentTest),
                    .failures = static_cast<unsigned>(stats.numAssertsFailedCurrentTest)});
    }
    void test_case_exception(const doctest::TestCaseException &) override {}
    void subcase_start(const doctest::SubcaseSignature &) override {}
    void subcase_end() override {}
    void log_assert(const doctest::AssertData &) override {}
    void log_message(const doctest::MessageData &) override {}
    void test_case_skipped(const doctest::TestCaseData &) override {}
};

DOCTEST_REGISTER_LISTENER("current-test-case", 0, CurrentTestCaseListener);

struct SelectedCasesReporter : CurrentTestCaseListener {
    using CurrentTestCaseListener::CurrentTestCaseListener;

    void report_query(const doctest::QueryData &query) override {
        for (unsigned i = 0; i < query.num_data; ++i)
            selectedTestNames.emplace(query.data[i]->m_name);
    }
};

DOCTEST_REGISTER_REPORTER("diagnostics-selection", 0, SelectedCasesReporter);

template<const auto &Cases, auto Check>
static bool registerTableCases(const char *file, const char *suite) {
    const auto run = [] {
        const auto entry = std::ranges::find(Cases, currentTestCase->m_line, [](const auto &tc) { return tc.line; });
        Check(static_cast<size_t>(entry - std::ranges::begin(Cases)));
    };
    for (const auto &tc : Cases) {
        doctest::detail::regTest(doctest::detail::TestCase(run, file, tc.line, doctest::detail::TestSuite() * suite) * tc.name);
    }
    return true;
}

static std::vector<uint32_t> readBinaryFile(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open file " + path);
    }
    constexpr int SCREEN_SIZE = 160 * 144;
    std::vector<uint32_t> result(SCREEN_SIZE);
    ifs.read(reinterpret_cast<char *>(result.data()), SCREEN_SIZE * sizeof(uint32_t));
    if (!ifs)
        throw std::runtime_error("Incomplete screen file " + path);
    return result;
}

static std::optional<Model> ModelFromName(const std::string_view name) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Model))) {
        if (name == std::meta::identifier_of(e))
            return [:e:];
    }
    return std::nullopt;
}

// Headless throughput benchmark: no screen compares, no doctest context. The
// audio sample ring fills and takes the same drain-and-drop path the ROM tests
// exercise, so benchmark and test builds run identical code.
static int runBenchmark(const std::string &rom, const Model model, const unsigned frames) {
    try {
        TestObservationSession observations;
        const auto gameboy = Gameboy::init({
                .romName = rom,
                .biosPath = "",
                .model = model,
                .executionPolicy = testExecutionPolicy,
        });
        observations.Attach(*gameboy);
        constexpr double kGbFrameHz = 4194304.0 / 70224.0;
        const auto start = std::chrono::steady_clock::now();
        for (unsigned f = 0; f < frames; ++f) {
            gameboy->RunFrame();
        }
        const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
        const double fps = frames * 1000.0 / elapsed.count();
        std::cout << rom << " [" << ModelName(model) << "] " << frames << " frames in " << elapsed.count() << " ms | " << fps << " fps | "
                  << fps / kGbFrameHz << "x realtime" << std::endl;
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cerr << "Benchmark failed for " << rom << ": " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

inline int ExecuteTestRoms(const int argc, char **argv) {
    std::vector<char *> doctest_args;
    doctest_args.reserve(argc);
    doctest_args.push_back(argv[0]);

    std::string benchRom;
    Model benchModel = Model::Auto;
    std::filesystem::path reportPath;

    static constexpr std::array<std::pair<std::string_view, std::string_view>, 4> kSuiteFlags{{
            {"--roms", "rom"},
            {"--age", "age"},
            {"--gbmicrotest", "gbmicrotest"},
            {"--gambatte", "gambatte"},
    }};
    std::string suites;
    bool suiteFilterGiven = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (const auto suite = std::ranges::find(kSuiteFlags, arg, &std::pair<std::string_view, std::string_view>::first);
            suite != kSuiteFlags.end()) {
            if (!suites.empty()) {
                suites += ',';
            }
            suites += suite->second;
        } else if (arg.starts_with("--results=")) {
            reportPath = arg.substr(std::string_view("--results=").size());
            if (reportPath.empty()) {
                std::cerr << "--results requires a file path\n";
                return EXIT_FAILURE;
            }
        } else if (arg.starts_with("--execution=")) {
            const auto policy = arg.substr(std::string_view("--execution=").size());
            if (policy != "ordinary" && policy != "optimized") {
                std::cerr << "--execution requires ordinary or optimized\n";
                return EXIT_FAILURE;
            }
            testExecutionPolicy = policy == "ordinary" ? ExecutionPolicy::Ordinary : ExecutionPolicy::Optimized;
        } else if (arg.starts_with("--observations=")) {
            const auto mode = arg.substr(std::string_view("--observations=").size());
            if (mode != "disabled" && mode != "observe" && mode != "capture") {
                std::cerr << "--observations requires disabled, observe or capture\n";
                return EXIT_FAILURE;
            }
            testObservationMode = mode == "disabled" ? TestObservationMode::Disabled
                                                    : mode == "observe" ? TestObservationMode::Observe : TestObservationMode::Capture;
        } else if (arg.starts_with("--trace-capacity=")) {
            try {
                constexpr auto prefix = std::string_view("--trace-capacity=");
                size_t consumed{};
                testTraceCapacity = std::stoul(std::string(arg.substr(prefix.size())), &consumed);
                if (consumed != arg.size() - prefix.size() || !testTraceCapacity || testTraceCapacity > 1'000'000)
                    throw std::invalid_argument("Trace capacity out of range");
            } catch (...) {
                std::cerr << "--trace-capacity requires 1..1000000\n";
                return EXIT_FAILURE;
            }
        } else if (arg.starts_with("--case-id=")) {
            const auto id = arg.substr(std::string_view("--case-id=").size());
            if (id.empty()) {
                std::cerr << "--case-id requires an exact catalog ID\n";
                return EXIT_FAILURE;
            }
            catalogSelection.ids.emplace(id);
        } else if (arg.starts_with("--case-file=")) {
            std::ifstream file(std::string(arg.substr(std::string_view("--case-file=").size())));
            if (!file) {
                std::cerr << "Cannot open case ID file\n";
                return EXIT_FAILURE;
            }
            std::string id;
            size_t count{};
            while (std::getline(file, id)) {
                if (!id.empty() && id.back() == '\r')
                    id.pop_back();
                if (!id.empty()) {
                    catalogSelection.ids.insert(id);
                    ++count;
                }
            }
            if (!count) {
                std::cerr << "Case ID file is empty\n";
                return EXIT_FAILURE;
            }
        } else if (arg.starts_with("--provenance-line=")) {
            try {
                const auto text = std::string(arg.substr(std::string_view("--provenance-line=").size()));
                size_t consumed{};
                const auto line = std::stoul(text, &consumed);
                if (line > 143 || consumed != text.size())
                    throw std::invalid_argument("invalid display line");
                testProvenanceLine = line;
            } catch (...) {
                std::cerr << "--provenance-line requires a display line from 0 through 143\n";
                return EXIT_FAILURE;
            }
        } else if (arg == "--checkpoint-failures") {
            testCheckpointFailures = true;
        } else if (arg.starts_with("--model=")) {
            catalogSelection.model = ModelFromName(arg.substr(std::string_view("--model=").size()));
            if (!catalogSelection.model || *catalogSelection.model == Model::Auto) {
                std::cerr << "--model requires a catalog hardware revision\n";
                return EXIT_FAILURE;
            }
        } else if (arg.starts_with("--subsystem=")) {
            catalogSelection.subsystem = arg.substr(std::string_view("--subsystem=").size());
            if (catalogSelection.subsystem.empty()) {
                std::cerr << "--subsystem requires a catalog tag\n";
                return EXIT_FAILURE;
            }
        } else if (constexpr std::string_view kBench = "--bench="; arg.rfind(kBench, 0) == 0) {
            benchRom = arg.substr(kBench.size());
        } else if (constexpr std::string_view kBenchModel = "--bench-model="; arg.rfind(kBenchModel, 0) == 0) {
            const auto parsed = ModelFromName(arg.substr(kBenchModel.size()));
            if (!parsed) {
                std::cerr << "Unknown model for --bench-model" << std::endl;
                return EXIT_FAILURE;
            }
            benchModel = *parsed;
        } else if (constexpr std::string_view kPrefix = "--max-threads="; arg.rfind(kPrefix, 0) == 0) {
            try {
                size_t consumed{};
                maxThreads = std::stoul(std::string(arg.substr(kPrefix.size())), &consumed);
                if (consumed != arg.size() - kPrefix.size() || maxThreads == 0 || maxThreads > 256)
                    throw std::invalid_argument("worker limit out of range");
            } catch (...) {
                std::cerr << "Invalid value for --max-threads (expected 1..256)" << std::endl;
                return EXIT_FAILURE;
            }
        } else if (constexpr std::string_view kFrames = "--frames="; arg.rfind(kFrames, 0) == 0) {
            try {
                size_t consumed = 0;
                const auto value = std::stoul(std::string(arg.substr(kFrames.size())), &consumed);
                if (consumed != arg.size() - kFrames.size() || value == 0 || value > 1'000'000) {
                    throw std::invalid_argument("frames out of range");
                }
                romFrames = static_cast<unsigned>(value);
            } catch (...) {
                std::cerr << "Invalid value for --frames (expected 1..1000000)" << std::endl;
                return EXIT_FAILURE;
            }
        } else {
            suiteFilterGiven |=
                    arg.starts_with("--test-suite=") || arg.starts_with("-ts=") || arg.starts_with("--dt-test-suite=") || arg.starts_with("--dt-ts=");
            doctest_args.push_back(argv[i]);
        }
    }

    if (!suites.empty() && suiteFilterGiven) {
        std::cerr << "--roms/--age/--gbmicrotest/--gambatte can't be combined with --test-suite" << std::endl;
        return EXIT_FAILURE;
    }

    if (!benchRom.empty()) {
        return runBenchmark(benchRom, benchModel, romFrames ? romFrames : 3000);
    }

    threadSemaphore = std::make_shared<std::counting_semaphore<>>(maxThreads);
    if (catalogSelection.Active() && doctest_args.size() != 1) {
        std::cerr << "Use catalog selectors without doctest filters\n";
        return EXIT_FAILURE;
    }
    const std::string suiteFilter = "--test-suite=" + suites;
    if (!suites.empty()) {
        doctest_args.push_back(const_cast<char *>(suiteFilter.c_str()));
    }

    doctest::Context ctx;
    ctx.applyCommandLine(static_cast<int>(doctest_args.size()), doctest_args.data());
    struct WorkerScope {
        ~WorkerScope() { testWorkers.reset(); }
    } workerScope;
    try {
        if (!ctx.shouldExit()) {
            if (!catalogSelection.Active()) {
                doctest::Context query;
                auto queryArguments = doctest_args;
                char queryFlag[] = "--list-test-cases";
                queryArguments.push_back(queryFlag);
                query.applyCommandLine(static_cast<int>(queryArguments.size()), queryArguments.data());
                query.setOption("reporters", "diagnostics-selection");
                if (query.run())
                    throw std::runtime_error("Cannot determine selected tests");
            }
            if (reportPath.empty())
                reportPath = diagnostics::DefaultReportPath();
            failureDirectory = reportPath.parent_path() / "failures";
            testExecutableHash = diagnostics::FileHash(argv[0]);
            testReport = std::make_unique<diagnostics::Report>(reportPath, diagnostics::RunIdentity{
                    .executable = argv[0],
                    .executableHash = testExecutableHash,
                    .compiler = __VERSION__,
                    .arguments = std::vector<std::string>(argv + 1, argv + argc)});
            std::cerr << "Diagnostics results: " << reportPath << '\n';
            testWorkers = std::make_unique<TestWorkers>(maxThreads);
        }
        if (catalogSelection.Active()) {
            std::istringstream selectedSuites(suites);
            std::string suite;
            while (std::getline(selectedSuites, suite, ','))
                catalogSelection.suites.insert(suite);
            return runCatalogSelection();
        }
        return ctx.run();
    } catch (const std::exception &error) {
        std::cerr << "Diagnostics infrastructure error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

#endif // STARGBC_TESTROMS_H
