#include "Gameboy.h"
#include "Instructions.h"

#include <iterator>
#include <map>
#include <thread>
#include <chrono>
#include <bit>

static constexpr uint32_t kFrameCyclesDMG = 70224;
static constexpr uint32_t kFrameCyclesCGB = kFrameCyclesDMG * 2;

bool Gameboy::ShouldRender() const {
    // const bool value = bus->gpu_->vblank;
    // bus->gpu_->vblank = false;
    return true;
}

void Gameboy::Save() const {
    cpu->Save();
}

void Gameboy::KeyUp(const Keys key) const {
    cpu->KeyUp(key);
}

void Gameboy::KeyDown(const Keys key) const {
    cpu->KeyDown(key);
}

uint32_t *Gameboy::GetScreenData() const {
    return cpu->GetScreenData();
}

void Gameboy::ToggleSpeed() {
    speedMultiplier = speedMultiplier == 1 ? 4 : 1;
}

void Gameboy::SetThrottle(const bool throttle) {
    throttleSpeed = throttle;
}

void Gameboy::UpdateEmulator() const {
    if (paused) {
        return;
    }

    using clock = std::chrono::steady_clock;
    static constexpr auto kFramePeriod = std::chrono::microseconds{16'667}; // ≈ 60 FPS (16.667 ms)
    const auto frameStart = clock::now();

    for (uint32_t i = 0; i < kFrameCyclesCGB; i++) {
        cpu->AdvanceFrame();
    }

    const auto elapsed = clock::now() - frameStart;
    if (const auto effectiveFrameTime = kFramePeriod / speedMultiplier; throttleSpeed && elapsed < effectiveFrameTime)
        std::this_thread::sleep_for(effectiveFrameTime - elapsed);
}
