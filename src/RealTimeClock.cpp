#include "RealTimeClock.h"

#include <chrono>
#include <cstdint>
#include <fstream>

using clk = std::chrono::system_clock;
using secs = std::chrono::seconds;

uint64_t RealTimeClock::NowSeconds() {
    return std::chrono::duration_cast<secs>(clk::now().time_since_epoch()).count();
}

uint64_t RealTimeClock::ComposeSeconds() const {
    const auto days =
            static_cast<uint16_t>((realClock.dayUpper_ & 0x01) << 8 | realClock.dayLower_);
    return realClock.seconds_ +
           realClock.minutes_ * kSecPerMin +
           realClock.hours_ * kSecPerHour +
           days * kSecPerDay;
}

uint64_t RealTimeClock::RecalculateZeroTime() {
    zeroTime_ = NowSeconds() - ComposeSeconds();
    return zeroTime_;
}

void RealTimeClock::Tick() {
    if (halted) return;

    const uint32_t elapsedSeconds = [&]() -> uint32_t {
        if (realRTC) {
            const uint64_t target_total_seconds = NowSeconds() - zeroTime_;
            const uint64_t current_total_seconds = ComposeSeconds();
            return target_total_seconds - current_total_seconds;
        }
        return 1;
    }();

    for (uint32_t i = 0; i < elapsedSeconds; i++) {
        realClock.seconds_++;
        if (realClock.seconds_ == 60) realClock.seconds_ = 0;
        bool recalcDays = false;
        if (realClock.seconds_ == 0) {
            realClock.minutes_++;
            if (realClock.minutes_ == 60) {
                realClock.minutes_ = 0;
                realClock.hours_++;
                if (realClock.hours_ == 24) {
                    realClock.hours_ = 0;
                    recalcDays = true;
                }
            }
        }

        if (recalcDays) {
            const auto current_days =
                    static_cast<uint16_t>((realClock.dayUpper_ & 0x01) << 8 | realClock.dayLower_);
            uint64_t total_days = current_days + 1;

            if (total_days >= kMaxDays) {
                realClock.dayUpper_ |= 0x80;
                total_days %= kMaxDays;
            }

            realClock.dayLower_ = static_cast<uint8_t>(total_days & 0xFF);
            realClock.dayUpper_ = (realClock.dayUpper_ & 0xFE) | static_cast<uint8_t>((total_days >> 8) & 0x01);
        }
    }
}

uint8_t RealTimeClock::ReadRTC(const uint16_t address) const {
    switch (address) {
        case 0x08: return latchedClock.seconds_ & 0x3F;
        case 0x09: return latchedClock.minutes_ & 0x3F;
        case 0x0A: return latchedClock.hours_ & 0x1F;
        case 0x0B: return latchedClock.dayLower_;
        case 0x0C: return latchedClock.dayUpper_ & 0xC1;
        default: return 0xFF;
    }
}

void RealTimeClock::WriteRTC(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0x08: realClock.seconds_ = value & 0x3F;
            counter = 0;
            break;
        case 0x09: realClock.minutes_ = value & 0x3F;
            break;
        case 0x0A: realClock.hours_ = value & 0x1F;
            break;
        case 0x0B: realClock.dayLower_ = value;
            break;
        case 0x0C: {
            const bool wasHalted = realClock.dayUpper_ & 0x40;
            realClock.dayUpper_ = value & 0xC1;
            halted = (realClock.dayUpper_ & 0x40);
            if (const bool nowRunning = !(realClock.dayUpper_ & 0x40); wasHalted && nowRunning) RecalculateZeroTime();
            break;
        }
        default: break;
    }

    RecalculateZeroTime();
}
