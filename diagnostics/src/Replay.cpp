#include "GameboyFiles.h"
#include "diagnostics/Replay.h"

#include <fstream>
#include <sstream>
#include "diagnostics/Binary.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"

namespace diagnostics {
    namespace {
        void Validate(const InputLog &log) {
            if (log.schemaVersion != SchemaVersion || log.initial.rom.empty() || log.initial.configuration.empty() || log.events.size() > 1'000'000)
                throw std::invalid_argument("Invalid replay identity or input count");
            if (log.settings && (log.settings->emulation.model == Model::Auto || log.settings->emulation.model > Model::AGBBE ||
                                 log.settings->emulation.real_rtc || log.settings->emulation.dmg_palette_sampling > DmgPaletteSampling::NewValue))
                throw std::invalid_argument("Replay settings require a concrete model and virtual RTC");
            if (log.settings && log.settings->emulation.force_cgb_mode &&
                (!log.settings->emulation.no_bootrom || !log.settings->bios_path.empty() || !IsCgb(log.settings->emulation.model)))
                throw std::invalid_argument("Forced CGB replay startup requires skipped boot on CGB hardware");
            uint64_t tick{};
            for (size_t i = 0; i < log.events.size(); ++i) {
                const auto &event = log.events[i];
                if (event.sequence != i || event.tick < tick)
                    throw std::invalid_argument("Replay inputs must have contiguous sequence and nondecreasing ticks");
                tick = event.tick;
            }
        }

        uint64_t Deadline(const Gameboy &machine, const uint64_t ticks) {
            if (ticks > UINT64_MAX - machine.TimelineTicks())
                throw std::out_of_range("Replay deadline overflows master ticks");
            return machine.TimelineTicks() + ticks;
        }

        std::string Encode(const InputLog &log) {
            Validate(log);
            std::ostringstream output(std::ios::binary);
            output << "SGINPUT1";
            WriteUnsigned(output, log.schemaVersion, 4);
            WriteFingerprints(output, log.initial);
            WriteUnsigned(output, log.settings.has_value(), 1);
            if (log.settings) {
                const auto &settings = *log.settings;
                WriteString(output, settings.rom_path);
                WriteString(output, settings.bios_path);
                WriteUnsigned(output, static_cast<uint8_t>(settings.emulation.model), 1);
                WriteUnsigned(output,
                              settings.emulation.no_bootrom | (settings.emulation.real_rtc << 1) | (settings.emulation.no_audio << 2) |
                                      (settings.emulation.color_correction << 3) |
                                      ((settings.emulation.dmg_palette_sampling == DmgPaletteSampling::NewValue) << 4) |
                                      (settings.emulation.force_cgb_mode << 5),
                              1);
            }
            WriteUnsigned(output, log.initialButtons, 1);
            WriteUnsigned(output, log.events.size());
            for (const auto &event : log.events) {
                WriteUnsigned(output, event.tick);
                WriteUnsigned(output, event.sequence);
                WriteUnsigned(output, event.buttons, 1);
            }
            return output.str();
        }
    } // namespace

    InputRecorder::InputRecorder(Gameboy &machine, InputFingerprint initial, const size_t capacity, std::optional<host::LaunchSettings> settings) :
        machine_(machine), log_{.initial = std::move(initial), .settings = std::move(settings), .initialButtons = machine.PressedButtons()},
        capacity_(capacity) {
        if (machine.TimelineTicks() || machine.UsesRealRTC() || machine.InputRecording() || !capacity || capacity > 1'000'000)
            throw std::invalid_argument("Input recording requires an unrecorded initial machine, virtual RTC and bounded capacity");
        if (log_.settings)
            log_.settings->emulation.model = machine.GetModel();
        Validate(log_);
        log_.events.reserve(capacity);
        sink_ = {.owner = this, .applied = [](void *owner, const uint64_t tick, const uint8_t buttons) noexcept {
                     auto &recorder = *static_cast<InputRecorder *>(owner);
                     if (recorder.log_.events.size() == recorder.capacity_) {
                         recorder.overflow_ = true;
                         return;
                     }
                     recorder.log_.events.push_back({tick, recorder.log_.events.size(), buttons});
                 }};
        machine.RecordInputsWith(&sink_);
    }

    InputRecorder::~InputRecorder() { machine_.RecordInputsWith(nullptr); }

    const InputLog &InputRecorder::Log() const {
        if (overflow_)
            throw std::runtime_error("Replay input recording overflowed; incomplete logs cannot be replayed");
        return log_;
    }

    void WriteInputLog(const std::filesystem::path &path, const InputLog &log) {
        const auto bytes = Encode(log);
        std::ofstream output(path, std::ios::binary);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output << bytes << TextHash(bytes);
        std::ofstream metadata(path.string() + ".json");
        metadata.exceptions(std::ios::badbit | std::ios::failbit);
        WriteJson(metadata, log);
        metadata << '\n';
    }

    InputLog ReadInputLog(const std::filesystem::path &path) {
        const auto size = std::filesystem::file_size(path);
        if (size < 76 || size > 32 * 1024 * 1024)
            throw std::invalid_argument("Invalid replay file size");
        std::ifstream file(path, std::ios::binary);
        std::string bytes(size, '\0');
        if (!file.read(bytes.data(), static_cast<std::streamsize>(size)))
            throw std::runtime_error("Cannot read replay input log");
        const auto body = std::string_view(bytes).substr(0, bytes.size() - 64);
        if (!body.starts_with("SGINPUT1") || TextHash(body) != std::string_view(bytes).substr(bytes.size() - 64))
            throw std::invalid_argument("Replay format or integrity check failed");
        std::istringstream input(std::string(body.substr(8)), std::ios::binary);
        InputLog log;
        log.schemaVersion = ReadUnsigned(input, 4);
        log.initial = ReadFingerprints(input);
        const auto hasSettings = ReadUnsigned(input, 1);
        if (hasSettings > 1)
            throw std::invalid_argument("Invalid replay settings marker");
        if (hasSettings) {
            host::LaunchSettings settings;
            settings.rom_path = ReadString(input);
            settings.bios_path = ReadString(input);
            settings.emulation.model = static_cast<Model>(ReadUnsigned(input, 1));
            const auto flags = ReadUnsigned(input, 1);
            if (flags > 63)
                throw std::invalid_argument("Invalid replay settings flags");
            settings.emulation.no_bootrom = flags & 1;
            settings.emulation.real_rtc = flags & 2;
            settings.emulation.no_audio = flags & 4;
            settings.emulation.color_correction = flags & 8;
            settings.emulation.dmg_palette_sampling = flags & 16 ? DmgPaletteSampling::NewValue : DmgPaletteSampling::BitwiseOr;
            settings.emulation.force_cgb_mode = flags & 32;
            log.settings = std::move(settings);
        }
        log.initialButtons = ReadUnsigned(input, 1);
        const auto count = ReadUnsigned(input);
        if (count > 1'000'000)
            throw std::invalid_argument("Replay input count exceeds its limit");
        log.events.reserve(count);
        for (size_t i = 0; i < count; ++i)
            log.events.push_back({ReadUnsigned(input), ReadUnsigned(input), static_cast<uint8_t>(ReadUnsigned(input, 1))});
        if (input.peek() != std::char_traits<char>::eof())
            throw std::invalid_argument("Unexpected trailing replay data");
        Validate(log);
        return log;
    }

    InputFingerprint IdentifyInitialInputs(const Gameboy &machine, const host::LaunchSettings &settings) {
        if (machine.TimelineTicks() || machine.UsesRealRTC())
            throw std::invalid_argument("Initial replay identity requires tick zero and virtual RTC");
        struct Configuration {
            Model model;
            bool noBootrom;
            bool realRTC;
            bool noAudio;
            bool colorCorrection;
            DmgPaletteSampling dmg_palette_sampling;
            bool force_cgb_mode;
        };
        std::ostringstream configuration;
        WriteJson(configuration,
                  Configuration{machine.GetModel(), settings.emulation.no_bootrom, settings.emulation.real_rtc, settings.emulation.no_audio,
                                settings.emulation.color_correction, settings.emulation.dmg_palette_sampling, settings.emulation.force_cgb_mode});
        auto save = std::filesystem::path(settings.rom_path);
        save.replace_extension(".sav");
        return {.rom = FileHash(settings.rom_path),
                .bootrom = machine.BootromBytes().empty() ? "skipped" : Sha256(std::as_bytes(machine.BootromBytes())),
                .configuration = TextHash(configuration.str()),
                .cartridgeSave = std::filesystem::exists(save) ? FileHash(save) : "absent"};
    }

    std::string InputLogHash(const InputLog &log) { return TextHash(Encode(log)); }

    ReplayPlayer::ReplayPlayer(Gameboy &machine, InputLog log, const InputFingerprint &initial) : machine_(machine), log_(std::move(log)) {
        Validate(log_);
        if (machine.TimelineTicks() || machine.UsesRealRTC() || !CompatibleInputs(log_.initial, initial))
            throw std::invalid_argument("Replay requires matching initial inputs and virtual RTC at tick zero");
        machine_.ApplyButtons(log_.initialButtons);
    }

    void ReplayPlayer::ApplyDueInputs(const bool stopOnEvent) {
        while (cursor_ < log_.events.size() && log_.events[cursor_].tick == machine_.TimelineTicks()) {
            machine_.ApplyButtons(log_.events[cursor_++].buttons);
            if (stopOnEvent && machine_.Observations() && machine_.Observations()->stopEvent)
                return;
        }
        if (cursor_ < log_.events.size() && log_.events[cursor_].tick < machine_.TimelineTicks())
            throw std::logic_error("Machine execution skipped a replay input");
    }

    uint64_t ReplayPlayer::NextTick(const uint64_t deadline) const {
        return cursor_ < log_.events.size() ? std::min(deadline, log_.events[cursor_].tick) : deadline;
    }

    ExecutionResult ReplayPlayer::AdvanceTicks(const uint64_t ticks, const ExecutionPolicy policy) {
        const auto begin = machine_.TimelineTicks();
        const auto deadline = Deadline(machine_, ticks);
        const auto instructions = machine_.InstructionsCompleted();
        while (true) {
            ApplyDueInputs();
            if (machine_.TimelineTicks() == deadline)
                return {ExecutionStopReason::TickBudget, begin, deadline, machine_.InstructionsCompleted() - instructions};
            static_cast<void>(machine_.AdvanceTicks(NextTick(deadline) - machine_.TimelineTicks(), policy));
        }
    }

    ExecutionResult ReplayPlayer::StepInstruction(const uint64_t maxTicks) {
        const auto begin = machine_.TimelineTicks();
        const auto deadline = Deadline(machine_, maxTicks);
        const auto instructions = machine_.InstructionsCompleted();
        while (true) {
            ApplyDueInputs();
            auto result = machine_.StepInstruction(NextTick(deadline) - machine_.TimelineTicks());
            result.beginTick = begin;
            result.instructions = machine_.InstructionsCompleted() - instructions;
            if (result.reason != ExecutionStopReason::TickBudget || machine_.TimelineTicks() == deadline)
                return result;
        }
    }

    ConditionalExecutionResult ReplayPlayer::RunUntil(const std::span<const RunCondition> conditions, const uint64_t maxTicks) {
        const auto begin = machine_.TimelineTicks();
        const auto deadline = Deadline(machine_, maxTicks);
        const auto instructions = machine_.InstructionsCompleted();
        if (machine_.Observations())
            machine_.Observations()->stopEvent.reset();
        while (true) {
            ApplyDueInputs(true);
            if (machine_.Observations() && machine_.Observations()->stopEvent)
                return {{ExecutionStopReason::Condition, begin, machine_.TimelineTicks(), machine_.InstructionsCompleted() - instructions},
                        machine_.ExecutionPosition(),
                        machine_.Observations()->stopEvent,
                        {}};
            const bool atDeadline = machine_.TimelineTicks() == deadline;
            auto result = machine_.RunUntil(conditions, NextTick(deadline) - machine_.TimelineTicks());
            result.execution.beginTick = begin;
            result.execution.instructions = machine_.InstructionsCompleted() - instructions;
            if (result.execution.reason != ExecutionStopReason::TickBudget || atDeadline)
                return result;
        }
    }

    void ReplayPlayer::SaveCheckpoint(const std::filesystem::path &path, CheckpointIdentity identity) const {
        identity.replayCursor = cursor_;
        identity.replayHash = InputLogHash(log_);
        WriteCheckpoint(path, machine_, std::move(identity));
    }

    void ReplayPlayer::RestoreCheckpoint(const std::filesystem::path &path, CheckpointIdentity compatible) {
        compatible.replayHash = InputLogHash(log_);
        const auto identity = diagnostics::RestoreCheckpoint(path, machine_, compatible, log_.events.size());
        cursor_ = identity.replayCursor;
    }
} // namespace diagnostics
