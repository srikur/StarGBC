#include "Gameboy.h"

#include <map>
#include <thread>
#include <chrono>

static constexpr uint32_t kFrameCyclesDMG = 70224;
static constexpr uint32_t kFrameCyclesCGB = kFrameCyclesDMG * 2;

bool Gameboy::ConsumeFrame() {
    return std::exchange(gpu_.frameReady, false);
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

uint32_t Gameboy::AdvanceCycles(const uint32_t maxCycles) {
    if (masterCycles == CGB_CYCLES_PER_SECOND) masterCycles = 0;
    if (cpu_.stopped()) {
        if (bus_.joypad_.KeyPressed()) {
            cpu_.stopped() = false;
        } else {
            masterCycles++;
            return 1;
        }
    }
    if (bus_.speed == Speed::Regular) {
        if (masterCycles % 2 != 0) {
            masterCycles++;
            return 1;
        }
        timer_.Tick(bus_.speed);
        rtc_.Update();
        audio_.Tick();
        serial_.Update();
        bus_.UpdateDMA();
        gpu_.Update();
        bus_.RunHDMA();
        if ((++cpuTickPhase_ & 3) == 0) {
            cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU());
        }
        const uint32_t consumed = bus_.speed == Speed::Regular && !cpu_.stopped() && maxCycles >= 2 ? 2 : 1;
        masterCycles += consumed;
        return consumed;
    }
    const bool evenCycle = masterCycles % 2 == 0;
    timer_.Tick(bus_.speed);
    if (evenCycle) {
        rtc_.Update();
        audio_.Tick();
    }
    serial_.Update();
    bus_.UpdateDMA();
    if (evenCycle) {
        gpu_.Update();
        bus_.RunHDMA();
    }
    if ((++cpuTickPhase_ & 3) == 0) {
        cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU());
    }
    masterCycles++;
    return 1;
}

void Gameboy::UpdateEmulator() {
    if (paused_) {
        return;
    }

    using clock = std::chrono::steady_clock;
    static constexpr auto kFramePeriod = std::chrono::nanoseconds{
        kFrameCyclesDMG * 1'000'000'000LL / DMG_CYCLES_PER_SECOND
    };

    uint32_t remaining = kFrameCyclesCGB;
    while (remaining > 0) {
        remaining -= AdvanceCycles(remaining);
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
