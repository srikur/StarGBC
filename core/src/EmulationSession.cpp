#include "EmulationSession.h"

namespace {
    struct FlagGuard {
        bool &flag;
        explicit FlagGuard(bool &value) : flag(value) { flag = true; }
        ~FlagGuard() { flag = false; }
    };
} // namespace

void EmulationSession::ApplyButtons() const {
    const uint8_t desired = (physical_ & ~overrideMask_) | (overridePressed_ & overrideMask_);
    gameboy_.ApplyButtons(desired);
}

void EmulationSession::SetPhysicalButton(const Keys key, const bool pressed) {
    if (pressed)
        physical_ |= static_cast<uint8_t>(key);
    else
        physical_ &= ~static_cast<uint8_t>(key);
    ApplyButtons();
}

void EmulationSession::SetOverrides(const uint8_t mask, const uint8_t pressed) {
    overrideMask_ = mask;
    overridePressed_ = pressed;
}

void EmulationSession::ClearOverrides() {
    overrideMask_ = overridePressed_ = 0;
    ApplyButtons();
}

void EmulationSession::FocusLost() {
    physical_ = 0;
    ClearOverrides();
}

void EmulationSession::Emit(const SessionEvent event) {
    if (!handler_)
        return;
    // Copy before invocation so a handler can detach itself without destroying its callable.
    const auto handler = handler_;
    FlagGuard guard(dispatching_);
    handler(event, frameCount_ + (event == SessionEvent::BeforeFrame ? 1 : 0));
}

bool EmulationSession::LoadState(const std::span<const std::byte> state) {
    if (dispatching_)
        throw std::logic_error("queue state restoration from callbacks");
    if (!gameboy_.LoadState(state))
        return false;
    frame_active_ = false;
    frame_ticks_remaining_ = Gameboy::FRAME_CYCLES_DMG * 2;
    ClearOverrides();
    ++stateGeneration_;
    Emit(SessionEvent::StateLoaded);
    return true;
}

void EmulationSession::reset() {
    if (running_ || dispatching_)
        throw std::logic_error("Reset the ROM outside session callbacks");
    gameboy_.reset_to_startup(startup_state_);
    pendingState_.clear();
    frame_active_ = false;
    frame_ticks_remaining_ = Gameboy::FRAME_CYCLES_DMG * 2;
    frameCount_ = 0;
    FocusLost();
    ++stateGeneration_;
    Emit(SessionEvent::StateLoaded);
}

void EmulationSession::QueueState(const std::span<const std::byte> state) {
    if (state.size() != kGameboyStateSize)
        throw std::invalid_argument("invalid state size");
    pendingState_.assign(state.begin(), state.end());
}

bool EmulationSession::ApplyQueuedState() {
    if (pendingState_.empty())
        return false;
    const auto state = std::move(pendingState_);
    pendingState_.clear();
    if (!LoadState(state))
        throw std::runtime_error("queued state failed validation");
    return true;
}

bool EmulationSession::RunFrame() {
    const auto previous_frame{frameCount_};
    run_slice(frame_ticks_remaining_,
              [&](const uint64_t budget) { return gameboy_.AdvanceTicks(budget, gameboy_.InitialSettings().execution_policy); });
    return frameCount_ != previous_frame;
}

ExecutionResult EmulationSession::run_slice(const uint64_t max_ticks, const std::function<ExecutionResult(uint64_t)> &execute) {
    if (running_)
        throw std::logic_error("recursive frame execution is not allowed");
    const auto tick{gameboy_.TimelineTicks()};
    const ExecutionResult idle{ExecutionStopReason::TickBudget, tick, tick, 0};
    if (paused_ || !max_ticks)
        return idle;
    FlagGuard guard(running_);
    if (!frame_active_) {
        overrideMask_ = overridePressed_ = 0;
        Emit(SessionEvent::BeforeFrame);
        if (ApplyQueuedState())
            return idle;
        if (paused_) {
            ClearOverrides();
            return idle;
        }
        ApplyButtons();
        frame_active_ = true;
    }
    const auto budget{std::min(max_ticks, frame_ticks_remaining_)};
    const auto result{execute(budget)};
    const auto elapsed{result.stopTick - result.beginTick};
    if (elapsed > budget)
        throw std::logic_error("session executor exceeded its tick budget");
    frame_ticks_remaining_ -= elapsed;
    if (!frame_ticks_remaining_) {
        frame_active_ = false;
        frame_ticks_remaining_ = Gameboy::FRAME_CYCLES_DMG * 2;
        ++frameCount_;
        Emit(SessionEvent::AfterFrame);
        ApplyQueuedState();
    }
    return result;
}
