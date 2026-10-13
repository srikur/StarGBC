#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <starparse/starparse.hpp>
#include "Reference.h"
#include "diagnostics/Artifacts.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"
#include "diagnostics/Trace.h"

namespace {
    using namespace StarParse;

    struct[[=Program{"StarGBC_Reference", "Optional reference-core observations", "1"}]] Arguments {
        [[=Positional{0}]] std::string rom;
        [[=Opt{"Print the pinned adapter capabilities"}]] bool capabilities{};
        [[=Opt{"Hardware model supported by this adapter"}]] std::string model{"CGBC"};
        [[=Opt{"Explicit shared boot ROM"}]] std::string bios;
        [[=Opt{"Use this core's documented skip-boot startup"}]] bool no_bootrom{};
        [[=Opt{"Requested master-tick deadline"}]] uint64_t ticks{140448};
        [[=Opt{"Requested interval between observations"}, =Range{1, 140448}]] unsigned sample_ticks{912};
        [[=Opt{"First captured event tick"}]] uint64_t trace_begin{};
        [[=Opt{"Run without callbacks to validate observational equivalence"}]] bool no_events{};
        [[=Opt{"Maximum retained events"}, =Range{1, 1'000'000}]] unsigned trace_capacity{1000000};
        [[=Opt{"Output basename for observations, traces and framebuffer"}]] std::string output;
    };

    struct Header {
        uint32_t schemaVersion{1};
        std::string record{"reference"};
        diagnostics::ReferenceCapabilities capabilities;
        diagnostics::ReferenceSettings settings;
        std::string executableHash;
        std::string romHash;
        std::string bootromHash;
        uint64_t masterTicksPerSecond{8388608};
        uint64_t requestedTicks{};
        uint64_t sampleTicks{};
        std::string initialCartridgeRam{"ZeroFilled"};
        std::string rtc{"VirtualZero"};
    };

    struct Sample {
        std::string record{"sample"};
        uint64_t requestedTick{};
        uint64_t actualTick{};
        uint64_t overshootTicks{};
        uint64_t frames{};
        diagnostics::ReferenceRegisters registers;
        std::string framebufferHash;
    };

    struct Complete {
        std::string record{"complete"};
        uint64_t requestedTick{};
        uint64_t actualTick{};
        uint64_t samples{};
        uint64_t overwrittenEvents{};
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
        const auto capabilities = diagnostics::Capabilities();
        if (arguments->capabilities) {
            diagnostics::WriteJson(std::cout, capabilities);
            std::cout << '\n';
            return 0;
        }
        if (arguments->rom.empty() || arguments->output.empty() || arguments->ticks > 8388608ULL * 3600 ||
            (!arguments->no_bootrom && arguments->bios.empty()) || (arguments->no_bootrom && !arguments->bios.empty()))
            throw std::invalid_argument("Specify ROM, output, bounded ticks and exactly one explicit boot or skip-boot option");
        if (!std::ranges::contains(capabilities.models, arguments->model) ||
            !std::ranges::contains(capabilities.startup, arguments->no_bootrom ? "SkipBootrom" : "ExplicitBootrom"))
            throw std::invalid_argument("Unsupported model or startup; inspect --capabilities");
        const diagnostics::ReferenceSettings settings{arguments->rom, arguments->bios, arguments->model, arguments->no_bootrom};
        auto adapter = diagnostics::MakeReferenceAdapter();
        adapter->Load(settings);
        const auto romHash = diagnostics::FileHash(arguments->rom);
        const auto bootHash = arguments->no_bootrom ? "skip" : diagnostics::FileHash(arguments->bios);
        std::ofstream output(arguments->output + ".jsonl");
        output.exceptions(std::ios::failbit | std::ios::badbit);
        const auto write = [&](const auto &record) {
            diagnostics::WriteJson(output, record);
            output << '\n';
        };
        write(Header{.capabilities = capabilities,
                     .settings = settings,
                     .executableHash = diagnostics::FileHash(argv[0]),
                     .romHash = romHash,
                     .bootromHash = bootHash,
                     .requestedTicks = arguments->ticks,
                     .sampleTicks = arguments->sample_ticks});
        uint64_t kinds{};
        for (const auto kind : capabilities.events)
            kinds |= uint64_t{1} << static_cast<unsigned>(kind);
        diagnostics::TraceRecorder trace(arguments->trace_capacity, {.kinds = kinds, .ticks = {arguments->trace_begin, arguments->ticks}});
        std::ostringstream configuration;
        diagnostics::WriteJson(configuration, capabilities);
        trace.Identify(diagnostics::FileHash(argv[0]), romHash, configuration.str());
        adapter->ObserveWith(arguments->no_events ? nullptr : &trace.Context());
        uint64_t actual{}, count{};
        const auto sample = [&](const uint64_t requested) {
            const auto pixels = adapter->Pixels();
            write(Sample{.requestedTick = requested,
                         .actualTick = actual,
                         .overshootTicks = actual - requested,
                         .frames = adapter->Frames(),
                         .registers = adapter->Registers(),
                         .framebufferHash = diagnostics::Sha256(std::as_bytes(pixels))});
            ++count;
        };
        sample(0);
        uint64_t requested{};
        while (requested < arguments->ticks) {
            requested = std::min<uint64_t>(arguments->ticks, requested + arguments->sample_ticks);
            if (actual < requested) {
                const auto previous = actual;
                actual = adapter->AdvanceTo(requested);
                if (actual < requested || actual <= previous)
                    throw std::runtime_error("Reference core did not reach the requested boundary");
            }
            sample(requested);
        }
        adapter->ObserveWith(nullptr);
        write(Complete{.requestedTick = arguments->ticks, .actualTick = actual, .samples = count, .overwrittenEvents = trace.Metadata().overwritten});
        trace.WriteJsonLines(arguments->output + ".events.jsonl");
        trace.WriteBinary(arguments->output + ".events.trace");
        diagnostics::WriteBitmap(arguments->output + ".bmp", adapter->Pixels(), 160, 144);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Reference adapter error: " << error.what() << '\n';
        return 2;
    }
}
