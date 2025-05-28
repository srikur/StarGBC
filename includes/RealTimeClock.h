#pragma once
#include "Common.h"

class RealTimeClock {
    [[nodiscard]] static uint64_t NowSeconds();

    [[nodiscard]] uint64_t LoadRTC() const;

    std::string savepath_;
    uint64_t zeroTime_ = 0x00;
    uint8_t seconds_ = 0x00;
    uint8_t minutes_ = 0x00;
    uint8_t hours_ = 0x00;
    uint8_t dayLower_ = 0x00;
    uint8_t dayUpper_ = 0x00;

public:
    explicit RealTimeClock(std::string filepath) : savepath_(std::move(filepath)) {
        zeroTime_ = LoadRTC();
    }

    void Tick();

    void Save() const;

    [[nodiscard]] uint8_t ReadRTC(uint16_t address) const;

    void WriteRTC(uint16_t address, uint8_t value);
};
