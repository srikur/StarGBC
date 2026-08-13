#include "Gameboy.h"

#include <map>
#include <thread>
#include <chrono>

static constexpr uint32_t kFrameCyclesDMG = 70224;
static constexpr uint32_t kFrameCyclesCGB = kFrameCyclesDMG * 2;

bool Gameboy::ShouldRender() const {
    // const bool value = bus->gpu_->vblank;
    // bus->gpu_->vblank = false;
    return true;
}

void Gameboy::Save() const {
    cartridge_.Save();
}

void Gameboy::KeyUp(const Keys key) {
    joypad_.KeyUp(key);
}

void Gameboy::KeyDown(const Keys key) {
    joypad_.KeyDown(key);
}

const uint32_t *Gameboy::GetScreenData() const {
    return gpu_.GetScreenData();
}

void Gameboy::ToggleSpeed() {
    speedMultiplier_ = speedMultiplier_ == 1 ? 4 : 1;
}

void Gameboy::SetThrottle(const bool throttle) {
    throttleSpeed_ = throttle;
}

void Gameboy::SaveScreen() const {
    try {
        std::ofstream file(romPath_ + ".screen", std::ios::binary | std::ios::trunc);
        if (!file.is_open()) throw std::runtime_error("Could not open " + romPath_ + ".screen");
        file.write(reinterpret_cast<const char *>(GetScreenData()), 160 * 144 * 4);
        std::fprintf(stderr, "Saved screen to %s.screen\n", romPath_.c_str());
    } catch (const std::exception &e) {
        std::fprintf(stderr, "Failed to save screen: %s\n", e.what());
    }
}

void Gameboy::AdvanceFrame() {
    const uint32_t speedDivider = bus_.speed == Speed::Regular ? 2 : 1;
    if (masterCycles == CGB_CYCLES_PER_SECOND) masterCycles = 0;
    if (cpu_.stopped()) {
        if (bus_.joypad_.KeyPressed()) {
            cpu_.stopped() = false;
        } else {
            masterCycles++;
            return;
        }
    }
    if (masterCycles % speedDivider == 0) timer_.Tick(bus_.speed);
    if (masterCycles % RTC_CLOCK_DIVIDER == 0) rtc_.Update();
    if (masterCycles % AUDIO_CLOCK_DIVIDER == 0) audio_.Tick();
    if (masterCycles % speedDivider == 0) serial_.Update();
    if (masterCycles % speedDivider == 0) bus_.UpdateDMA();
    if (masterCycles % GRAPHICS_CLOCK_DIVIDER == 0) gpu_.Update();
    if (masterCycles % 2 == 0) bus_.RunHDMA();
    if (masterCycles % speedDivider == 0) cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU());
    masterCycles++;
}

void Gameboy::UpdateEmulator() {
    if (paused_) {
        return;
    }

    using clock = std::chrono::steady_clock;
    static constexpr auto kFramePeriod = std::chrono::nanoseconds{
        kFrameCyclesDMG * 1'000'000'000LL / DMG_CYCLES_PER_SECOND
    };

    for (uint32_t i = 0; i < kFrameCyclesCGB; i++) {
        AdvanceFrame();
    }

    if (throttleSpeed_) {
        nextFrameTime_ += kFramePeriod / speedMultiplier_;
        if (const auto now = clock::now(); nextFrameTime_ > now) {
            std::this_thread::sleep_until(nextFrameTime_);
        } else {
            nextFrameTime_ = now; // fell behind; don't try to catch up in a burst
        }
    } else {
        nextFrameTime_ = clock::now();
    }
}
