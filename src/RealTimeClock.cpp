#include "RealTimeClock.h"

uint64_t RealTimeClock::LoadRTC() const {
    uint64_t time = std::chrono::system_clock::now().time_since_epoch().count() / 1000;
    std::ifstream rtcFile(savepath_, std::ios::binary | std::ios::ate);
    if (!rtcFile.is_open()) {
        return time;
    }
    const std::streamsize length = rtcFile.tellg();
    rtcFile.seekg(0, std::ios::beg);
    rtcFile.read(reinterpret_cast<char *>(&time), length);
    if (!rtcFile) {
        throw std::runtime_error("Failed to read from RTC file");
    }
    return time;
}

void RealTimeClock::Tick() {
    uint64_t diff = std::chrono::system_clock::now().time_since_epoch().count() / 1000;
    diff -= zeroTime_;
    seconds_ = (diff % 60);
    minutes_ = (diff / 60) % 60;
    hours_ = (diff / 3600) % 24;
    const auto days = static_cast<uint16_t>(diff / 3600 / 24);
    dayLower_ = days % 256;

    if (days < 0xFF) {
    } else if (days < 0x1FF) {
        dayUpper_ |= 0x01;
    } else {
        dayUpper_ |= 0x01;
        dayUpper_ |= 0x80;
    }
}

void RealTimeClock::Save() const {
    std::fstream rtcSave(savepath_);
    if (rtcSave.is_open()) {
        rtcSave << zeroTime_;
    }
    rtcSave.close();
}

uint8_t RealTimeClock::ReadRTC(const uint16_t address) const {
    switch (address) {
        case 0x08: return seconds_;
        case 0x09: return minutes_;
        case 0xA: return hours_;
        case 0xB: return dayLower_;
        case 0xC: return dayUpper_;
        default: throw UnreachableCodeException("Improper read from RTC");
    }
}

void RealTimeClock::WriteRTC(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0x08: seconds_ = value;
            break;
        case 0x09: minutes_ = value;
            break;
        case 0xA: hours_ = value;
            break;
        case 0xB: dayLower_ = value;
            break;
        case 0xC: dayUpper_ = value;
            break;
        default: throw UnreachableCodeException("Improper write to RTC");
    }
}
