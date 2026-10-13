#pragma once

#include <filesystem>
#include "Contracts.h"

class Gameboy;

namespace diagnostics {
    void WriteCheckpoint(const std::filesystem::path &path, const Gameboy &machine, CheckpointIdentity identity);
    [[nodiscard]] CheckpointIdentity RestoreCheckpoint(const std::filesystem::path &path, Gameboy &machine, const CheckpointIdentity &compatible,
                                                       uint64_t maximumReplayCursor = UINT64_MAX);
    void WriteFingerprints(std::ostream &output, const InputFingerprint &inputs);
    [[nodiscard]] InputFingerprint ReadFingerprints(std::istream &input);
    [[nodiscard]] bool CompatibleInputs(const InputFingerprint &left, const InputFingerprint &right);
} // namespace diagnostics
