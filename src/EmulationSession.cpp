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
    const uint8_t changed = desired ^ gameboy_.PressedButtons();
    for (unsigned bit = 1; bit <= 0x80; bit <<= 1) {
        if (!(changed & bit))
            continue;
        if (desired & bit)
            gameboy_.KeyDown(static_cast<Keys>(bit));
        else
            gameboy_.KeyUp(static_cast<Keys>(bit));
    }
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
    ClearOverrides();
    ++stateGeneration_;
    Emit(SessionEvent::StateLoaded);
    return true;
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
    if (running_)
        throw std::logic_error("recursive frame execution is not allowed");
    if (paused_)
        return false;
    FlagGuard guard(running_);
    overrideMask_ = overridePressed_ = 0;
    Emit(SessionEvent::BeforeFrame);
    if (ApplyQueuedState())
        return false;
    if (paused_) {
        ClearOverrides();
        return false;
    }
    ApplyButtons();
    gameboy_.RunFrame();
    ++frameCount_;
    Emit(SessionEvent::AfterFrame);
    ApplyQueuedState();
    return true;
}
