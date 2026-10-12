#pragma once

#include <filesystem>
#include "Replay.h"

namespace diagnostics {
    struct ObservationPlan {
        uint64_t beginTick{};
        uint64_t endTick{};
        uint64_t sampleTicks{912};
        ExecutionPolicy policy{ExecutionPolicy::Ordinary};
        bool events{};
        bool eventDetails{};
        size_t eventCapacity{16384};
    };

    struct CanonicalObservation {
        ExecutionPosition position;
        MachineInspection machine;
        std::string hardwareHash;
        std::string memoryHash;
        std::string framebufferHash;
        std::string audioHash;
        uint64_t audioSamples{};
    };

    [[nodiscard]] CanonicalObservation CaptureObservation(Gameboy &machine);

    void WriteObservations(const std::filesystem::path &path, Gameboy &machine, ReplayPlayer *replay,
                           const InputFingerprint &initial, std::string replayHash, const ObservationPlan &plan);
}
