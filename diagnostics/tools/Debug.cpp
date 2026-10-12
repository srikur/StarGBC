#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <sstream>
#include <starparse/starparse.hpp>
#include "Gameboy.h"
#include "diagnostics/Contracts.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"
#include "diagnostics/Trace.h"

namespace {
    using namespace StarParse;

    struct[[=Program{"StarGBC_Debug", "Headless accuracy diagnostics", "1"}]] Arguments {
        [[=Positional{0}, =Required]] std::string rom;
        [[=Opt{"Hardware revision"}]] Model model{Model::Auto};
        [[=Opt{"Boot ROM path"}]] std::string bios;
        [[=Opt{"Skip boot ROM execution"}]] bool no_bootrom{};
        [[=Opt{"Disable color correction"}]] bool raw_color{};
        [[=Opt{"Fixed RunFrame intervals before inspection"}, =Range{0, 1'000'000}]] unsigned intervals{};
        [[=Opt{"Master ticks to advance after the intervals"}]] uint64_t ticks{};
        [[=Opt{"Scheduler execution policy"}]] ExecutionPolicy policy{ExecutionPolicy::Ordinary};
        [[=Opt{"Step to the next completed instruction"}]] bool step_instruction{};
        [[=Opt{"Maximum master ticks for instruction stepping"}]] uint64_t deadline_ticks{140'448};
        [[=Opt{"Register address to inspect"}]] std::optional<uint16_t> register_address;
        [[=Opt{"JSON output file; stdout when omitted"}]] std::string output;
        [[=Opt{"Trace output basename; writes JSONL and binary"}]] std::string trace;
        [[=Opt{"Maximum retained events"}, =Range{1, 1'000'000}]] unsigned trace_capacity{65536};
        [[=Opt{"First captured master tick"}]] uint64_t trace_begin{};
        [[=Opt{"Last captured master tick"}]] uint64_t trace_end{std::numeric_limits<uint64_t>::max()};
        [[=Opt{"Capture only this component"}]] std::optional<diagnostics::Component> trace_component;
        [[=Opt{"Capture only this event kind"}]] std::optional<diagnostics::EventKind> trace_kind;
        [[=Opt{"Capture only this bus or register address"}]] std::optional<uint16_t> trace_address;
        [[=Opt{"Capture only this instruction address"}]] std::optional<uint16_t> trace_pc;
        [[=Opt{"Capture only this instruction bank"}]] std::optional<uint16_t> trace_bank;
        [[=Opt{"Capture only this scanline"}]] std::optional<uint8_t> trace_ly;
        [[=Opt{"Capture only this scanline dot"}]] std::optional<uint16_t> trace_dot;
    };

    struct InspectionReport {
        uint32_t schemaVersion{diagnostics::SchemaVersion};
        std::string romHash;
        MachineInspection machine;
        std::optional<RegisterInspection> selectedRegister;
        std::optional<ExecutionResult> execution;
    };
}

int main(const int argc, char **argv) {
    const auto arguments = StarParse::parse_or_exit<Arguments>(argc, argv, {.allow_case_insensitivity = true});
    if (arguments.help_requested()) {
        std::print("{}", arguments.help());
        return 0;
    }
    if (arguments.version_requested()) {
        std::print("{}", arguments.version());
        return 0;
    }
    try {
        Gameboy machine({.romName = arguments->rom, .biosPath = arguments->bios, .model = arguments->model,
                         .noBootrom = arguments->no_bootrom, .colorCorrection = !arguments->raw_color, .executionPolicy = arguments->policy});
        std::unique_ptr<diagnostics::TraceRecorder> recorder;
        if (!arguments->trace.empty()) {
            if (arguments->trace_begin > arguments->trace_end)
                throw std::invalid_argument("Trace interval is reversed");
            diagnostics::EventFilter filter{.ticks = {arguments->trace_begin, arguments->trace_end}};
            if (arguments->trace_component)
                filter.components = uint64_t{1} << static_cast<unsigned>(*arguments->trace_component);
            if (arguments->trace_kind)
                filter.kinds = uint64_t{1} << static_cast<unsigned>(*arguments->trace_kind);
            if (arguments->trace_address)
                filter.addresses = {*arguments->trace_address, *arguments->trace_address};
            if (arguments->trace_pc)
                filter.pcs = {*arguments->trace_pc, *arguments->trace_pc};
            filter.pcBank = arguments->trace_bank;
            if (arguments->trace_ly)
                filter.lines = {*arguments->trace_ly, *arguments->trace_ly};
            if (arguments->trace_dot)
                filter.dots = {*arguments->trace_dot, *arguments->trace_dot};
            recorder = std::make_unique<diagnostics::TraceRecorder>(arguments->trace_capacity, filter);
            std::ostringstream configuration;
            diagnostics::WriteJson(configuration, *arguments);
            recorder->Identify(diagnostics::FileHash(argv[0]), diagnostics::FileHash(arguments->rom), configuration.str());
            machine.ObserveWith(&recorder->Context());
        }
        for (unsigned interval = 0; interval < arguments->intervals; ++interval)
            machine.RunFrame();
        std::optional<ExecutionResult> execution;
        if (arguments->ticks)
            execution = machine.AdvanceTicks(arguments->ticks, arguments->policy);
        if (arguments->step_instruction)
            execution = machine.StepInstruction(arguments->deadline_ticks);
        InspectionReport report{.romHash = diagnostics::FileHash(arguments->rom), .machine = machine.Inspect(), .execution = execution};
        if (arguments->register_address)
            report.selectedRegister = machine.InspectRegister(*arguments->register_address);
        if (recorder) {
            recorder->WriteJsonLines(arguments->trace + ".jsonl");
            recorder->WriteBinary(arguments->trace + ".trace");
            machine.ObserveWith(nullptr);
        }
        if (arguments->output.empty()) {
            diagnostics::WriteJson(std::cout, report);
            std::cout << '\n';
        } else {
            std::ofstream output;
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output.open(arguments->output);
            diagnostics::WriteJson(output, report);
            output << '\n';
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Diagnostics error: " << error.what() << '\n';
        return 2;
    }
}
