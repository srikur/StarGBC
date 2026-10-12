#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "Model.h"
#include "Contracts.h"

namespace diagnostics {
    enum class ExperimentMode { Component, WholeMachine };

    enum class ExperimentFamily { Window, Sprites, Stat, TimerReload, Dma };

    enum class ExperimentBasis { InternalInvariant, HardwareMeasurement, ReferenceComparison, Hypothesis };

    struct Scenario {
        uint32_t schemaVersion{1};
        ExperimentMode mode{ExperimentMode::Component};
        ExperimentFamily family{ExperimentFamily::TimerReload};
        Model model{Model::DMGB};
        bool doubleSpeed{};
        uint64_t seed{1};
        int phase{};
        uint8_t scx{3};
        uint8_t wx{5};
        uint8_t spriteX{16};
        int spriteOffset{};
        bool coordinatePriority{};
        std::vector<uint8_t> noise;
        uint64_t budgetTicks{};
        std::optional<uint8_t> expectedValue;
        ExperimentBasis basis{ExperimentBasis::InternalInvariant};
        std::string basisEvidence{"Platform invariants checked against captured hardware events"};
    };

    struct ExperimentAction {
        std::string operation;
        uint64_t offsetTicks{};
        uint16_t address{};
        uint8_t value{};
        uint16_t instructionAddress{};
    };

    struct ExperimentCheck {
        std::string invariant;
        bool passed{};
        std::string evidence;
    };

    struct ExperimentResult {
        uint32_t schemaVersion{1};
        std::string record{"experiment"};
        Scenario scenario;
        std::string build;
        std::string romHash;
        std::string completionContract;
        std::string phaseUnit;
        Outcome outcome{Outcome::InfrastructureError};
        std::string failureSignature;
        std::string details;
        uint64_t completedTick{};
        uint64_t anchorTick{};
        std::vector<ExperimentAction> actions;
        std::vector<ExperimentCheck> checks;
        uint32_t probeValue{};
        std::string framebufferHash;
        std::string eventHash;
        uint64_t events{};
    };

    [[nodiscard]] ExperimentResult RunExperiment(Scenario scenario, const std::filesystem::path &directory);
}
