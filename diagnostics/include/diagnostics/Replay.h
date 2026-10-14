#pragma once

#include <filesystem>
#include "Checkpoint.h"
#include "Gameboy.h"
#include "ReplayInput.h"

namespace diagnostics {
    struct InputLog {
        uint32_t schemaVersion{SchemaVersion};
        InputFingerprint initial;
        std::optional<GameboySettings> settings;
        uint8_t initialButtons{};
        std::vector<AppliedInput> events;
    };

    class InputRecorder {
        Gameboy &machine_;
        InputLog log_;
        InputSink sink_;
        size_t capacity_{};
        bool overflow_{};

    public:
        InputRecorder(Gameboy &machine, InputFingerprint initial, size_t capacity = 65536, std::optional<GameboySettings> settings = {});

        ~InputRecorder();

        InputRecorder(const InputRecorder &) = delete;

        InputRecorder &operator=(const InputRecorder &) = delete;

        [[nodiscard]] const InputLog &Log() const;
    };

    [[nodiscard]] InputFingerprint IdentifyInitialInputs(const Gameboy &machine, const GameboySettings &settings);

    void WriteInputLog(const std::filesystem::path &path, const InputLog &log);

    [[nodiscard]] InputLog ReadInputLog(const std::filesystem::path &path);

    [[nodiscard]] std::string InputLogHash(const InputLog &log);

    class ReplayPlayer {
        Gameboy &machine_;
        InputLog log_;
        size_t cursor_{};

        void ApplyDueInputs(bool stopOnEvent = false);

        [[nodiscard]] uint64_t NextTick(uint64_t deadline) const;

    public:
        ReplayPlayer(Gameboy &machine, InputLog log, const InputFingerprint &initial);

        [[nodiscard]] size_t Cursor() const { return cursor_; }

        [[nodiscard]] ExecutionResult AdvanceTicks(uint64_t ticks, ExecutionPolicy policy = ExecutionPolicy::Ordinary);

        [[nodiscard]] ExecutionResult StepInstruction(uint64_t maxTicks);

        [[nodiscard]] ConditionalExecutionResult RunUntil(std::span<const RunCondition> conditions, uint64_t maxTicks);

        void SaveCheckpoint(const std::filesystem::path &path, CheckpointIdentity identity) const;

        void RestoreCheckpoint(const std::filesystem::path &path, CheckpointIdentity compatible);
    };
} // namespace diagnostics
