#include "RealTimeClock.h"

#include <chrono>
#include <fstream>
#include <stdexcept>

using clk = std::chrono::system_clock;
using secs = std::chrono::seconds;

uint64_t RealTimeClock::NowSeconds() {
    return std::chrono::duration_cast<secs>(clk::now().time_since_epoch()).count();
}

uint64_t RealTimeClock::LoadRTC() const {
    uint64_t saved = NowSeconds();

    if (std::ifstream rtcFile(savepath_, std::ios::binary); rtcFile.good()) {
        rtcFile.read(reinterpret_cast<char *>(&saved), sizeof(saved));
        if (!rtcFile)
            throw std::runtime_error("Failed to read RTC save file");
    }
    return saved;
}

void RealTimeClock::Save() const {
    if (std::ofstream rtcFile(savepath_, std::ios::binary | std::ios::trunc); rtcFile.good())
        rtcFile.write(reinterpret_cast<const char *>(&zeroTime_), sizeof(zeroTime_));
}

void RealTimeClock::Tick() {
    const uint64_t diff = NowSeconds() - zeroTime_;

    seconds_ = static_cast<uint8_t>(diff % 60);
    minutes_ = static_cast<uint8_t>((diff / 60) % 60);
    hours_ = static_cast<uint8_t>((diff / 3600) % 24);

    const auto days = static_cast<uint16_t>(diff / 86400);
    dayLower_ = static_cast<uint8_t>(days & 0xFF);

    dayUpper_ = 0;
    if (days & 0x100) dayUpper_ |= 0x01;
    if (days > 0x1FF) dayUpper_ |= 0x80;
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
        case 0x08: seconds_ = value;
            break;
        case 0x09: minutes_ = value;
            break;
        case 0x0A: hours_ = value;
            break;
        case 0x0B: dayLower_ = value;
            break;
        case 0x0C: dayUpper_ = value;
            break;
        default: throw std::runtime_error("Invalid RTC write");
    }
}
