#pragma once
#include "Common.h"

class RealTimeClock {
    static constexpr uint64_t kSecPerMin = 60;
    static constexpr uint64_t kSecPerHour = 60 * kSecPerMin;
    static constexpr uint64_t kSecPerDay = 24 * kSecPerHour;

    [[nodiscard]] static uint64_t NowSeconds();

    [[nodiscard]] uint64_t ComposeSeconds() const;

    void RecalculateZeroTime();

    uint64_t zeroTime_ = 0x00;
    uint8_t seconds_ = 0x00;
    uint8_t minutes_ = 0x00;
    uint8_t hours_ = 0x00;
    uint8_t dayLower_ = 0x00;
    uint8_t dayUpper_ = 0x00;

public:
    explicit RealTimeClock() = default;

    void Tick();

    void Load(std::ifstream &stateFile);

    void Save(std::ofstream &stateFile) const;

    [[nodiscard]] uint8_t ReadRTC(uint16_t address) const;

    void WriteRTC(uint16_t address, uint8_t value);
};
