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
            static_cast<uint16_t>((realClock_.dayUpper_ & 0x01) << 8 | realClock_.dayLower_);
    return realClock_.seconds_ +
           realClock_.minutes_ * kSecPerMin +
           realClock_.hours_ * kSecPerHour +
           days * kSecPerDay;
}

uint64_t RealTimeClock::RecalculateZeroTime() {
    zeroTime_ = NowSeconds() - ComposeSeconds();
    return zeroTime_;
}

void RealTimeClock::Update() {
    // Called every T-cycle, ticks rtc counter and ticks rtc every 1000ms
    if (!halted_) counter_++;
    if (counter_ == RTC_TICKS_PER_SECOND) {
        counter_ = 0;
        Tick();
    }
}

void RealTimeClock::Load(std::ifstream &stateFile) {
    stateFile.read(reinterpret_cast<char *>(&zeroTime_), sizeof(zeroTime_));
    stateFile.read(reinterpret_cast<char *>(&realClock_.seconds_), sizeof(realClock_.seconds_));
    stateFile.read(reinterpret_cast<char *>(&realClock_.minutes_), sizeof(realClock_.minutes_));
    stateFile.read(reinterpret_cast<char *>(&realClock_.hours_), sizeof(realClock_.hours_));
    stateFile.read(reinterpret_cast<char *>(&realClock_.dayLower_), sizeof(realClock_.dayLower_));
    stateFile.read(reinterpret_cast<char *>(&realClock_.dayUpper_), sizeof(realClock_.dayUpper_));
    RecalculateZeroTime();
}

void RealTimeClock::Save(std::ofstream &stateFile) const {
    stateFile.write(reinterpret_cast<const char *>(&zeroTime_), sizeof(zeroTime_));
    stateFile.write(reinterpret_cast<const char *>(&realClock_.seconds_), sizeof(realClock_.seconds_));
    stateFile.write(reinterpret_cast<const char *>(&realClock_.minutes_), sizeof(realClock_.minutes_));
    stateFile.write(reinterpret_cast<const char *>(&realClock_.hours_), sizeof(realClock_.hours_));
    stateFile.write(reinterpret_cast<const char *>(&realClock_.dayLower_), sizeof(realClock_.dayLower_));
    stateFile.write(reinterpret_cast<const char *>(&realClock_.dayUpper_), sizeof(realClock_.dayUpper_));
}

void RealTimeClock::Tick() {
    if (halted_) return;

    const uint32_t elapsedSeconds = [&]() -> uint32_t {
        if (realRTC_) {
            const uint64_t target_total_seconds = NowSeconds() - zeroTime_;
            const uint64_t current_total_seconds = ComposeSeconds();
            return target_total_seconds - current_total_seconds;
        }
        return 1;
    }();

    for (uint32_t i = 0; i < elapsedSeconds; i++) {
        realClock_.seconds_++;
        if (realClock_.seconds_ == 60) realClock_.seconds_ = 0;
        bool recalcDays = false;
        if (realClock_.seconds_ == 0) {
            realClock_.minutes_++;
            if (realClock_.minutes_ == 60) {
                realClock_.minutes_ = 0;
                realClock_.hours_++;
                if (realClock_.hours_ == 24) {
                    realClock_.hours_ = 0;
                    recalcDays = true;
                }
            }
        }

        if (recalcDays) {
            const auto current_days =
                    static_cast<uint16_t>((realClock_.dayUpper_ & 0x01) << 8 | realClock_.dayLower_);
            uint64_t total_days = current_days + 1;

            if (total_days >= kMaxDays) {
                realClock_.dayUpper_ |= 0x80;
                total_days %= kMaxDays;
            }

            realClock_.dayLower_ = static_cast<uint8_t>(total_days & 0xFF);
            realClock_.dayUpper_ = (realClock_.dayUpper_ & 0xFE) | static_cast<uint8_t>((total_days >> 8) & 0x01);
        }
    }
}

uint8_t RealTimeClock::ReadRTC(const uint16_t address) const {
    switch (address) {
        case 0x08: return latchedClock_.seconds_ & 0x3F;
        case 0x09: return latchedClock_.minutes_ & 0x3F;
        case 0x0A: return latchedClock_.hours_ & 0x1F;
        case 0x0B: return latchedClock_.dayLower_;
        case 0x0C: return latchedClock_.dayUpper_ & 0xC1;
        default: return 0xFF;
    }
}

void RealTimeClock::WriteRTC(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0x08: realClock_.seconds_ = value & 0x3F;
            counter_ = 0;
            break;
        case 0x09: realClock_.minutes_ = value & 0x3F;
            break;
        case 0x0A: realClock_.hours_ = value & 0x1F;
            break;
        case 0x0B: realClock_.dayLower_ = value;
            break;
        case 0x0C: {
            const bool wasHalted = realClock_.dayUpper_ & 0x40;
            realClock_.dayUpper_ = value & 0xC1;
            halted_ = (realClock_.dayUpper_ & 0x40);
            if (const bool nowRunning = !(realClock_.dayUpper_ & 0x40); wasHalted && nowRunning) RecalculateZeroTime();
            break;
        }
        default: break;
    }

    RecalculateZeroTime();
}
