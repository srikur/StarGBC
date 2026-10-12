#include "Gameboy.h"

ConditionalExecutionResult Gameboy::RunUntil(const std::span<const RunCondition> conditions, const uint64_t maxTicks) {
    if (conditions.size() > 16)
        throw std::invalid_argument("At most sixteen execution conditions are supported");
    if (maxTicks > std::numeric_limits<uint64_t>::max() - timelineTicks_)
        throw std::out_of_range("Conditional execution deadline overflows master ticks");
    const auto read = [this](const RunCondition &condition) -> uint32_t {
        switch (condition.kind) {
            case ConditionKind::RegisterValue:
            case ConditionKind::RegisterChange:
                return InspectRegister(condition.address).cpuVisible;
            case ConditionKind::PpuMode:
                return static_cast<uint32_t>(gpu_.stat.mode);
            case ConditionKind::Line:
                return gpu_.currentLine;
            case ConditionKind::Dot:
                return gpu_.scanlineCounter;
        }
        throw std::invalid_argument("Unknown execution condition");
    };
    std::array<uint32_t, 16> initial{};
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (!conditions[i].mask)
            throw std::invalid_argument("Execution condition masks must be nonzero");
        initial[i] = read(conditions[i]) & conditions[i].mask;
    }
    const auto begin = timelineTicks_;
    const auto instructions = cpu_.InstructionsCompleted();
    if (observations_)
        observations_->stopEvent.reset();
    while (true) {
        for (size_t i = 0; i < conditions.size(); ++i) {
            const auto &condition = conditions[i];
            const auto value = read(condition) & condition.mask;
            const bool matches = condition.kind == ConditionKind::RegisterChange ? value != initial[i] : value == (condition.value & condition.mask);
            if (matches)
                return {{ExecutionStopReason::Condition, begin, timelineTicks_, cpu_.InstructionsCompleted() - instructions},
                        ExecutionPosition(), observations_ ? observations_->stopEvent : std::nullopt, i};
        }
        if (observations_ && observations_->stopEvent)
            return {{ExecutionStopReason::Condition, begin, timelineTicks_, cpu_.InstructionsCompleted() - instructions},
                    ExecutionPosition(), observations_->stopEvent, {}};
        if (timelineTicks_ - begin == maxTicks)
            return {{ExecutionStopReason::TickBudget, begin, timelineTicks_, cpu_.InstructionsCompleted() - instructions}, ExecutionPosition(), {}, {}};
        static_cast<void>(AdvanceTicks(1));
    }
}
