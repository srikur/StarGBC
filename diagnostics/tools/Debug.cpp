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
#include "diagnostics/Watchpoints.h"

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
        [[=Opt{"Break on an instruction address at its first machine cycle"}]] std::optional<uint16_t> break_pc;
        [[=Opt{"Instruction bank for the PC breakpoint"}]] std::optional<uint16_t> break_bank;
        [[=Opt{"Stop after a read of this address"}]] std::optional<uint16_t> watch_read;
        [[=Opt{"Stop after a write to this address"}]] std::optional<uint16_t> watch_write;
        [[=Opt{"Stop after this event kind"}]] std::optional<diagnostics::EventKind> watch_event;
        [[=Opt{"Required event value"}]] std::optional<uint32_t> watch_value;
        [[=Opt{"Register used by a value or change condition"}]] std::optional<uint16_t> until_register;
        [[=Opt{"Required CPU-visible register value"}]] std::optional<uint8_t> until_value;
        [[=Opt{"Stop when the selected register changes"}]] bool until_changed{};
        [[=Opt{"Register comparison mask"}]] uint8_t until_mask{0xFF};
        [[=Opt{"Stop at this PPU mode"}]] std::optional<unsigned> until_mode;
        [[=Opt{"Stop at this display scanline"}]] std::optional<unsigned> until_ly;
        [[=Opt{"Stop at this display dot"}]] std::optional<unsigned> until_dot;
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
        [[=Opt{"Trigger on this event kind"}]] std::optional<diagnostics::EventKind> trigger_kind;
        [[=Opt{"Trigger on this address"}]] std::optional<uint16_t> trigger_address;
        [[=Opt{"Earliest trigger tick"}]] uint64_t trigger_begin{};
        [[=Opt{"Events retained before the trigger"}]] unsigned trigger_history{256};
        [[=Opt{"Events retained after the trigger"}]] unsigned trigger_following{256};
    };

    struct InspectionReport {
        uint32_t schemaVersion{diagnostics::SchemaVersion};
        std::string romHash;
        MachineInspection machine;
        std::optional<RegisterInspection> selectedRegister;
        std::optional<ExecutionResult> execution;
        std::optional<ConditionalExecutionResult> conditional;
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
        const bool hasWatchpoint = arguments->break_pc || arguments->watch_read || arguments->watch_write || arguments->watch_event;
        if ((arguments->break_bank && !arguments->break_pc) || (arguments->watch_value && !arguments->watch_event))
            throw std::invalid_argument("Bank and event values require their breakpoint or event condition");
        if (arguments->until_register.has_value() != (arguments->until_value.has_value() || arguments->until_changed) ||
            (arguments->until_value && arguments->until_changed))
            throw std::invalid_argument("A register condition requires its address and either a value or change detection");
        if (arguments->until_mode.value_or(0) > 3 || arguments->until_ly.value_or(0) > 153 || arguments->until_dot.value_or(0) > 455)
            throw std::invalid_argument("PPU condition is outside its hardware range");
        std::vector<RunCondition> conditions;
        if (arguments->until_register)
            conditions.push_back({arguments->until_changed ? ConditionKind::RegisterChange : ConditionKind::RegisterValue,
                                  *arguments->until_register, arguments->until_value.value_or(0), arguments->until_mask});
        if (arguments->until_mode)
            conditions.push_back({ConditionKind::PpuMode, 0, *arguments->until_mode});
        if (arguments->until_ly)
            conditions.push_back({ConditionKind::Line, 0, *arguments->until_ly});
        if (arguments->until_dot)
            conditions.push_back({ConditionKind::Dot, 0, *arguments->until_dot});
        std::unique_ptr<diagnostics::TraceRecorder> recorder;
        std::unique_ptr<diagnostics::Watchpoints> watchpoints;
        if (!arguments->trace.empty() || hasWatchpoint) {
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
            if (arguments->trigger_kind || arguments->trigger_address) {
                diagnostics::EventFilter match{.ticks = {arguments->trigger_begin, UINT64_MAX}};
                if (arguments->trigger_kind)
                    match.kinds = uint64_t{1} << static_cast<unsigned>(*arguments->trigger_kind);
                if (arguments->trigger_address)
                    match.addresses = {*arguments->trigger_address, *arguments->trigger_address};
                recorder->Arm({match, arguments->trigger_history, arguments->trigger_following});
            }
            std::ostringstream configuration;
            diagnostics::WriteJson(configuration, *arguments);
            recorder->Identify(diagnostics::FileHash(argv[0]), diagnostics::FileHash(arguments->rom), configuration.str());
            recorder->SetCaptureEnabled(!arguments->trace.empty());
            if (hasWatchpoint) {
                watchpoints = std::make_unique<diagnostics::Watchpoints>(recorder->Context());
                if (arguments->break_pc)
                    watchpoints->BreakAt(*arguments->break_pc, arguments->break_bank);
                if (arguments->watch_read)
                    watchpoints->ReadAt({*arguments->watch_read, *arguments->watch_read});
                if (arguments->watch_write)
                    watchpoints->WriteAt({*arguments->watch_write, *arguments->watch_write});
                if (arguments->watch_event)
                    watchpoints->Add({.events = {.kinds = uint64_t{1} << static_cast<unsigned>(*arguments->watch_event)},
                                      .value = arguments->watch_value});
            }
            machine.ObserveWith(&recorder->Context());
        }
        for (unsigned interval = 0; interval < arguments->intervals; ++interval)
            machine.RunFrame();
        std::optional<ExecutionResult> execution;
        if (arguments->ticks)
            execution = machine.AdvanceTicks(arguments->ticks, arguments->policy);
        if (arguments->step_instruction)
            execution = machine.StepInstruction(arguments->deadline_ticks);
        std::optional<ConditionalExecutionResult> conditional;
        if (hasWatchpoint || !conditions.empty())
            conditional = machine.RunUntil(conditions, arguments->deadline_ticks);
        InspectionReport report{.romHash = diagnostics::FileHash(arguments->rom), .machine = machine.Inspect(),
                                .execution = execution, .conditional = conditional};
        if (arguments->register_address)
            report.selectedRegister = machine.InspectRegister(*arguments->register_address);
        if (recorder && !arguments->trace.empty()) {
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
