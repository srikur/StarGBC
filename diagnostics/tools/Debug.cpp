#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <sstream>
#include <starparse/starparse.hpp>
#include "Gameboy.h"
#include "diagnostics/Artifacts.h"
#include "diagnostics/Comparison.h"
#include "diagnostics/Contracts.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"
#include "diagnostics/Provenance.h"
#include "diagnostics/Replay.h"
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
        [[=Opt{"DMG LCD sampling during palette writes"}]] DmgPaletteSampling dmg_palette_sampling{DmgPaletteSampling::BitwiseOr};
        [[=Opt{"Force native CGB mode when skipping the boot ROM"}]] bool force_cgb_mode{};
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
        [[=Opt{"Ordered canonical observations for differential comparison"}]] std::string comparison_output;
        [[=Opt{"First differential observation tick"}]] uint64_t comparison_begin{};
        [[=Opt{"Final differential observation tick"}]] uint64_t comparison_end{};
        [[=Opt{"Master ticks between differential observations"}]] uint64_t sample_ticks{912};
        [[=Opt{"Compare event histories using per-tick scheduling"}]] bool comparison_events{};
        [[=Opt{"Include individual events in differential samples"}]] bool comparison_event_details{};
        [[=Opt{"Restore a compatible debug checkpoint before execution"}]] std::string checkpoint_load;
        [[=Opt{"Save a debug checkpoint after execution"}]] std::string checkpoint_save;
        [[=Opt{"Replay a verified timestamped input log"}]] std::string replay_inputs;
        [[=Opt{"Record the buttons actually applied to the machine"}]] std::string record_inputs;
        [[=Opt{"Schedule buttons as decimal tick:mask; repeat for more inputs"}]] std::vector<std::string> input_at;
        [[=Opt{"Pixel provenance JSON output file"}]] std::string provenance;
        [[=Opt{"Capture all display lines instead of one selected line"}]] bool provenance_whole_frame{};
        [[=Opt{"Maximum retained pixel pipeline and recolor events"}]] std::optional<unsigned> pixel_event_capacity;
        [[=Opt{"Horizontal coordinate to explain"}]] std::optional<uint16_t> pixel_x;
        [[=Opt{"Vertical coordinate to explain and line to capture"}]] std::optional<uint16_t> pixel_y;
        [[=Opt{"Capture and query only this displayed frame counter"}]] std::optional<uint64_t> pixel_frame;
        [[=Opt{"First provenance capture tick"}]] uint64_t pixel_begin{};
        [[=Opt{"Last provenance capture tick"}]] uint64_t pixel_end{UINT64_MAX};
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
        diagnostics::InputFingerprint initialInputs;
        uint64_t replayCursor{};
        std::optional<diagnostics::PixelExplanation> selectedPixel;
        std::optional<diagnostics::ProvenanceMetadata> provenance;
    };
} // namespace

int main(const int argc, char **argv) {
    constexpr Settings parse_settings{.allow_case_insensitivity = true};
    const auto arguments = StarParse::parse_or_exit<Arguments, parse_settings>(argc, argv);
    if (arguments.help_requested()) {
        std::print("{}", arguments.help());
        return 0;
    }
    if (arguments.version_requested()) {
        std::print("{}", arguments.version());
        return 0;
    }
    try {
        GameboySettings settings{.romName = arguments->rom,
                                 .biosPath = arguments->bios,
                                 .model = arguments->model,
                                 .noBootrom = arguments->no_bootrom,
                                 .colorCorrection = !arguments->raw_color,
                                 .executionPolicy = arguments->policy,
                                 .dmg_palette_sampling = arguments->dmg_palette_sampling,
                                 .force_cgb_mode = arguments->force_cgb_mode};
        std::optional<diagnostics::InputLog> inputLog;
        if (!arguments->replay_inputs.empty()) {
            if (!arguments->input_at.empty())
                throw std::invalid_argument("Choose a recorded input log or scheduled inputs");
            inputLog = diagnostics::ReadInputLog(arguments->replay_inputs);
            if (!inputLog->settings)
                throw std::invalid_argument("Command-line replay requires recorded initial settings");
            settings = *inputLog->settings;
            settings.romName = arguments->rom;
            if (!arguments->bios.empty())
                settings.biosPath = arguments->bios;
            if (arguments->model != Model::Auto && arguments->model != settings.model)
                throw std::invalid_argument("Requested model differs from the recorded model");
            settings.executionPolicy = arguments->policy;
        }
        Gameboy machine(settings);
        settings.model = machine.GetModel();
        const auto initialInputs = diagnostics::IdentifyInitialInputs(machine, settings);
        const diagnostics::CheckpointIdentity checkpointIdentity{.build = std::string(Gameboy::CoreBuildIdentity()), .inputs = initialInputs};
        std::unique_ptr<diagnostics::InputRecorder> inputRecorder;
        if (!arguments->record_inputs.empty()) {
            if (!arguments->checkpoint_load.empty())
                throw std::invalid_argument("Record inputs from initial conditions rather than a checkpoint");
            inputRecorder = std::make_unique<diagnostics::InputRecorder>(machine, initialInputs, 65536, settings);
        }
        if (!arguments->input_at.empty()) {
            inputLog = diagnostics::InputLog{.initial = initialInputs, .settings = settings};
            const auto number = [](const std::string &text) {
                size_t consumed{};
                const auto value = std::stoull(text, &consumed, 10);
                if (text.empty() || text.front() == '-' || consumed != text.size())
                    throw std::invalid_argument("Scheduled inputs require decimal tick:mask");
                return value;
            };
            for (const auto &input : arguments->input_at) {
                const auto colon = input.find(':');
                if (colon == std::string::npos)
                    throw std::invalid_argument("Scheduled inputs require decimal tick:mask");
                const auto tick = number(input.substr(0, colon));
                const auto buttons = number(input.substr(colon + 1));
                if (buttons > 255)
                    throw std::invalid_argument("Button mask exceeds eight bits");
                inputLog->events.push_back({tick, inputLog->events.size(), static_cast<uint8_t>(buttons)});
            }
        }
        std::unique_ptr<diagnostics::ReplayPlayer> replay;
        const auto replayHash = inputLog ? diagnostics::InputLogHash(*inputLog) : std::string{};
        if (inputLog)
            replay = std::make_unique<diagnostics::ReplayPlayer>(machine, std::move(*inputLog), initialInputs);
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
            conditions.push_back({arguments->until_changed ? ConditionKind::RegisterChange : ConditionKind::RegisterValue, *arguments->until_register,
                                  arguments->until_value.value_or(0), arguments->until_mask});
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
                    watchpoints->Add(
                            {.events = {.kinds = uint64_t{1} << static_cast<unsigned>(*arguments->watch_event)}, .value = arguments->watch_value});
            }
            machine.ObserveWith(&recorder->Context());
        }
        std::unique_ptr<diagnostics::ProvenanceRecorder> pixels;
        if (arguments->pixel_x.has_value() != arguments->pixel_y.has_value())
            throw std::invalid_argument("A pixel query requires both coordinates");
        if (arguments->pixel_x.value_or(0) > 159 || arguments->pixel_y.value_or(0) > 143)
            throw std::invalid_argument("Pixel query is outside the display");
        if (!arguments->provenance.empty() || arguments->pixel_x) {
            diagnostics::PixelRegion region{.y = {arguments->pixel_y.value_or(0), arguments->pixel_y.value_or(0)},
                                            .ticks = {arguments->pixel_begin, arguments->pixel_end}};
            if (arguments->provenance_whole_frame)
                region.y = {0, 143};
            if (arguments->pixel_frame)
                region.frames = {*arguments->pixel_frame, *arguments->pixel_frame};
            pixels = std::make_unique<diagnostics::ProvenanceRecorder>(
                    region, 160 * (region.y.last - region.y.first + 1),
                    arguments->pixel_event_capacity.value_or(arguments->provenance_whole_frame ? 65536 : 4096));
            pixels->Identify(std::string(Gameboy::CoreBuildIdentity()), initialInputs.rom);
            machine.RecordPixelsWith(&pixels->Context());
        }
        if (!arguments->checkpoint_load.empty()) {
            if (replay)
                replay->RestoreCheckpoint(arguments->checkpoint_load, checkpointIdentity);
            else
                static_cast<void>(diagnostics::RestoreCheckpoint(arguments->checkpoint_load, machine, checkpointIdentity));
        }
        if (!arguments->comparison_output.empty()) {
            if (arguments->intervals || arguments->ticks || arguments->step_instruction || hasWatchpoint || !conditions.empty() ||
                !arguments->checkpoint_load.empty() || recorder || pixels)
                throw std::invalid_argument("Differential capture requires its own initial execution and capture plan");
            diagnostics::WriteObservations(arguments->comparison_output, machine, replay.get(), initialInputs, replayHash,
                                           {.beginTick = arguments->comparison_begin,
                                            .endTick = arguments->comparison_end,
                                            .sampleTicks = arguments->sample_ticks,
                                            .policy = arguments->policy,
                                            .events = arguments->comparison_events,
                                            .eventDetails = arguments->comparison_event_details,
                                            .eventCapacity = arguments->trace_capacity});
        }
        for (unsigned interval = 0; interval < arguments->intervals; ++interval) {
            if (replay)
                static_cast<void>(replay->AdvanceTicks(diagnostics::RunFrameTicks, arguments->policy));
            else
                machine.RunFrame();
        }
        std::optional<ExecutionResult> execution;
        if (arguments->ticks)
            execution =
                    replay ? replay->AdvanceTicks(arguments->ticks, arguments->policy) : machine.AdvanceTicks(arguments->ticks, arguments->policy);
        if (arguments->step_instruction)
            execution = replay ? replay->StepInstruction(arguments->deadline_ticks) : machine.StepInstruction(arguments->deadline_ticks);
        std::optional<ConditionalExecutionResult> conditional;
        if (hasWatchpoint || !conditions.empty())
            conditional = replay ? replay->RunUntil(conditions, arguments->deadline_ticks) : machine.RunUntil(conditions, arguments->deadline_ticks);
        InspectionReport report{.romHash = diagnostics::FileHash(arguments->rom),
                                .machine = machine.Inspect(),
                                .execution = execution,
                                .conditional = conditional,
                                .initialInputs = initialInputs,
                                .replayCursor = replay ? replay->Cursor() : 0};
        if (!arguments->checkpoint_save.empty()) {
            if (inputRecorder) {
                auto recordedIdentity = checkpointIdentity;
                recordedIdentity.replayCursor = inputRecorder->Log().events.size();
                recordedIdentity.replayHash = diagnostics::InputLogHash(inputRecorder->Log());
                diagnostics::WriteCheckpoint(arguments->checkpoint_save, machine, std::move(recordedIdentity));
            } else if (replay)
                replay->SaveCheckpoint(arguments->checkpoint_save, checkpointIdentity);
            else
                diagnostics::WriteCheckpoint(arguments->checkpoint_save, machine, checkpointIdentity);
        }
        if (inputRecorder)
            diagnostics::WriteInputLog(arguments->record_inputs, inputRecorder->Log());
        if (pixels) {
            report.provenance = pixels->Metadata();
            if (arguments->pixel_x)
                report.selectedPixel = pixels->Query(*arguments->pixel_x, *arguments->pixel_y, arguments->pixel_frame);
            if (!arguments->provenance.empty()) {
                pixels->WriteJson(arguments->provenance);
                diagnostics::WriteBitmap(arguments->provenance + ".bmp", std::span(machine.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT),
                                         SCREEN_WIDTH, SCREEN_HEIGHT);
            }
            machine.RecordPixelsWith(nullptr);
        }
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
