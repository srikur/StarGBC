#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include "diagnostics/Observation.h"

enum class ExecutionPolicy { Optimized, Ordinary };

enum class ExecutionStopReason { TickBudget, InstructionBoundary, Halted, Stopped, Locked, Condition };

struct ExecutionResult {
    ExecutionStopReason reason{};
    uint64_t beginTick{};
    uint64_t stopTick{};
    uint64_t instructions{};
};

enum class ConditionKind { RegisterValue, RegisterChange, PpuMode, Line, Dot };

struct RunCondition {
    ConditionKind kind{};
    uint16_t address{};
    uint32_t value{};
    uint32_t mask{0xFFFFFFFF};
};

struct ConditionalExecutionResult {
    ExecutionResult execution;
    diagnostics::ExecutionPosition stopped;
    std::optional<diagnostics::Event> trigger;
    std::optional<size_t> conditionIndex;
};
