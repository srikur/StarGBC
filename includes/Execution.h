#pragma once

#include <cstdint>

enum class ExecutionPolicy { Optimized, Ordinary };
enum class ExecutionStopReason { TickBudget, InstructionBoundary, Halted, Stopped, Locked, Condition };

struct ExecutionResult {
    ExecutionStopReason reason{};
    uint64_t beginTick{};
    uint64_t stopTick{};
    uint64_t instructions{};
};
