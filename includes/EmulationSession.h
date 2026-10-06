#pragma once

#include <functional>
#include <vector>

#include "Gameboy.h"

enum class SessionEvent { BeforeFrame, AfterFrame, StateLoaded };

class EmulationSession {
public:
    explicit EmulationSession(Gameboy &gameboy) : gameboy_(gameboy) {}

    [[nodiscard]] Gameboy &Machine() const { return gameboy_; }

    bool RunFrame();

    [[nodiscard]] uint64_t FrameCount() const { return frameCount_; }

    [[nodiscard]] uint64_t StateGeneration() const { return stateGeneration_; }

    [[nodiscard]] bool Paused() const { return paused_; }

    void SetPaused(const bool paused) { paused_ = paused; }

    void SetEventHandler(std::function<void(SessionEvent, uint64_t)> handler) { handler_ = std::move(handler); }

    void SetPhysicalButton(Keys key, bool pressed);

    [[nodiscard]] uint8_t PhysicalButtons() const { return physical_; }

    void SetOverrides(uint8_t mask, uint8_t pressed);

    void ClearOverrides();

    void FocusLost();

    bool LoadState(std::span<const std::byte> state);

    void QueueState(std::span<const std::byte> state);

    void CancelQueuedState() { pendingState_.clear(); }

private:
    void ApplyButtons() const;

    void Emit(SessionEvent event);

    bool ApplyQueuedState();

    Gameboy &gameboy_;
    std::function<void(SessionEvent, uint64_t)> handler_;
    std::vector<std::byte> pendingState_;
    uint64_t frameCount_{0};
    uint64_t stateGeneration_{0};
    uint8_t physical_{0};
    uint8_t overrideMask_{0};
    uint8_t overridePressed_{0};
    bool paused_{false};
    bool running_{false};
    bool dispatching_{false};
};
