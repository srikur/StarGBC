#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>
#include "diagnostics/Observation.h"

namespace diagnostics {
    struct ReferenceCapabilities {
        uint32_t schemaVersion{1};
        std::string name;
        std::string repository;
        std::string revision;
        std::vector<std::string> models;
        std::vector<std::string> startup;
        uint64_t resolutionTicks{};
        std::string executionGranularity;
        std::optional<uint64_t> maximumOvershootTicks;
        std::vector<EventKind> events;
        bool busValues{};
        bool instructionValues{};
        bool banks{};
        std::string instructionPhase;
        std::string audio;
        std::vector<std::string> limitations;
    };

    struct ReferenceSettings {
        std::string rom;
        std::string bios;
        std::string model;
        bool noBootrom{};
    };

    struct ReferenceRegisters {
        uint16_t pc{};
        uint16_t sp{};
        uint16_t af{};
        uint16_t bc{};
        uint16_t de{};
        uint16_t hl{};
    };

    class ReferenceAdapter {
    public:
        virtual ~ReferenceAdapter() = default;
        virtual void Load(const ReferenceSettings &settings) = 0;
        virtual void ObserveWith(ObservationContext *context) = 0;
        [[nodiscard]] virtual uint64_t AdvanceTo(uint64_t deadline) = 0;
        [[nodiscard]] virtual ReferenceRegisters Registers() = 0;
        [[nodiscard]] virtual std::span<const uint32_t> Pixels() const = 0;
        [[nodiscard]] virtual uint64_t Frames() const = 0;
    };

    [[nodiscard]] ReferenceCapabilities Capabilities();
    [[nodiscard]] std::unique_ptr<ReferenceAdapter> MakeReferenceAdapter();
}
