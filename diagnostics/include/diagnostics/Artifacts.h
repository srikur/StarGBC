#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include "Contracts.h"

namespace diagnostics {
    struct PixelCoordinate {
        uint32_t x{};
        uint32_t y{};
    };

    struct PixelDifference {
        uint32_t width{};
        uint32_t height{};
        uint32_t mask{};
        uint64_t mismatches{};
        std::optional<PixelCoordinate> first;
        std::optional<PixelCoordinate> minimum;
        std::optional<PixelCoordinate> maximum;
    };

    struct Reproduction {
        uint32_t schemaVersion{SchemaVersion};
        CaseIdentity test;
        InputFingerprint inputs;
        std::string rom;
        std::string bootrom;
        std::string reference;
        std::string cartridgeSave;
        std::string expectation;
        uint64_t intervals{};
        bool noBootrom{};
        bool realRTC{};
        bool noAudio{};
        bool colorCorrection{true};
        std::string normalization;
        std::string completion;
        std::string executableHash;
        bool fixedIntervals{};
    };

    struct TerminalState {
        uint16_t pc{};
        uint16_t sp{};
        uint16_t af{};
        uint16_t bc{};
        uint16_t de{};
        uint16_t hl{};
        uint8_t interruptEnable{};
        uint8_t interruptFlag{};
        uint8_t lcdc{};
        uint8_t ly{};
        uint16_t dot{};
        bool halted{};
        bool stopped{};
        bool locked{};
        std::string decodedDigits;
        uint64_t audioBeginTick{};
        uint64_t audioEndTick{};
        uint64_t mixerLevelChanges{};
    };

    [[nodiscard]] PixelDifference ComparePixels(std::span<const uint32_t> actual, std::span<const uint32_t> expected,
                                                uint32_t width, uint32_t height, uint32_t mask = 0xFFFFFFFF);

    void WriteBitmap(const std::filesystem::path &path, std::span<const uint32_t> pixels, uint32_t width, uint32_t height);

    void WriteFailureBundle(const std::filesystem::path &directory, Result &result, Reproduction reproduction,
                            const TerminalState &state, std::span<const uint32_t> actual, std::span<const uint32_t> expected = {},
                            uint32_t mask = 0xFFFFFFFF);
}
