#include "RealTimeClock.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>

using clk = std::chrono::system_clock;
using secs = std::chrono::seconds;

uint64_t RealTimeClock::NowSeconds() {
    return std::chrono::duration_cast<secs>(clk::now().time_since_epoch()).count();
}

uint64_t RealTimeClock::ComposeSeconds() const {
    const auto days =
            static_cast<uint16_t>((dayUpper_ & 0x01) << 8 | dayLower_);
    return seconds_ +
           minutes_ * kSecPerMin +
           hours_ * kSecPerHour +
           days * kSecPerDay;
}

void RealTimeClock::RecalculateZeroTime() {
    zeroTime_ = NowSeconds() - ComposeSeconds();
}

void RealTimeClock::Load(std::ifstream &stateFile) {
    stateFile.read(reinterpret_cast<char *>(&zeroTime_), sizeof(zeroTime_));
    stateFile.read(reinterpret_cast<char *>(&seconds_), sizeof(seconds_));
    stateFile.read(reinterpret_cast<char *>(&minutes_), sizeof(minutes_));
    stateFile.read(reinterpret_cast<char *>(&hours_), sizeof(hours_));
    stateFile.read(reinterpret_cast<char *>(&dayLower_), sizeof(dayLower_));
    stateFile.read(reinterpret_cast<char *>(&dayUpper_), sizeof(dayUpper_));
}

void RealTimeClock::Save(std::ofstream &stateFile) const {
    stateFile.write(reinterpret_cast<const char *>(&zeroTime_), sizeof(zeroTime_));
    stateFile.write(reinterpret_cast<const char *>(&seconds_), sizeof(seconds_));
    stateFile.write(reinterpret_cast<const char *>(&minutes_), sizeof(minutes_));
    stateFile.write(reinterpret_cast<const char *>(&hours_), sizeof(hours_));
    stateFile.write(reinterpret_cast<const char *>(&dayLower_), sizeof(dayLower_));
    stateFile.write(reinterpret_cast<const char *>(&dayUpper_), sizeof(dayUpper_));
}

void RealTimeClock::Tick() {
    if (dayUpper_ & 0x40) // HALT bit – clock is stopped
        return;

    const uint64_t diff = NowSeconds() - zeroTime_;

    seconds_ = static_cast<uint8_t>(diff % 60);
    minutes_ = static_cast<uint8_t>((diff / kSecPerMin) % 60);
    hours_ = static_cast<uint8_t>((diff / kSecPerHour) % 24);

    auto days = static_cast<uint32_t>(diff / kSecPerDay);

    // Preserve HALT while rebuilding flags
    const bool halted = dayUpper_ & 0x40;
    dayUpper_ = halted ? 0x40 : 0x00;

    if (days & 0x100) dayUpper_ |= 0x01; // bit-0 = day bit-8
    if (days > 0x1FF) dayUpper_ |= 0x80; // bit-7 = carry (>511 days)

    dayLower_ = static_cast<uint8_t>(days & 0xFF);
}

uint8_t RealTimeClock::ReadRTC(const uint16_t address) const {
    switch (address) {
        case 0x08: return seconds_;
        case 0x09: return minutes_;
        case 0x0A: return hours_;
        case 0x0B: return dayLower_;
        case 0x0C: return dayUpper_;
        default: throw std::runtime_error("Invalid RTC read");
    }
}

void RealTimeClock::WriteRTC(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0x08: seconds_ = value; break;
        case 0x09: minutes_ = value; break;
        case 0x0A: hours_ = value; break;
        case 0x0B: dayLower_ = value; break;
        case 0x0C: {
            const bool wasHalted = dayUpper_ & 0x40;
            dayUpper_ = value;
            if (const bool nowRunning = !(dayUpper_ & 0x40); wasHalted && nowRunning) RecalculateZeroTime();
            break;
        }
        default: throw std::runtime_error("Invalid RTC write");
    }

    if (!(dayUpper_ & 0x40))
        RecalculateZeroTime();
}
