#pragma once
#include "Common.h"

class RealTimeClock {
    static constexpr uint64_t kSecPerMin = 60;
    static constexpr uint64_t kSecPerHour = 60 * kSecPerMin;
    static constexpr uint64_t kSecPerDay = 24 * kSecPerHour;
    static constexpr uint64_t kMinPerHour = 60;
    static constexpr uint64_t kHourPerDay = 24;
    static constexpr uint64_t kMaxDays = 511;
    static constexpr uint64_t kHoursPerDay = 24;

    [[nodiscard]] static uint64_t NowSeconds();

    [[nodiscard]] uint64_t ComposeSeconds() const;

    uint64_t zeroTime_ = 0x00;

    struct Clock {
        uint8_t seconds_ = 0x00;
        uint8_t minutes_ = 0x00;
        uint8_t hours_ = 0x00;
        uint8_t dayLower_ = 0x00;
        uint8_t dayUpper_ = 0x00;
    };

public:
    explicit RealTimeClock() = default;

    void Tick();

    [[nodiscard]] uint8_t ReadRTC(uint16_t address) const;

    void WriteRTC(uint16_t address, uint8_t value);

    uint64_t RecalculateZeroTime();

    Clock realClock{};
    Clock latchedClock{};
    bool halted{false};

    void Load(std::ifstream &stateFile) {
        stateFile.read(reinterpret_cast<char *>(&zeroTime_), sizeof(zeroTime_));
        stateFile.read(reinterpret_cast<char *>(&realClock.seconds_), sizeof(realClock.seconds_));
        stateFile.read(reinterpret_cast<char *>(&realClock.minutes_), sizeof(realClock.minutes_));
        stateFile.read(reinterpret_cast<char *>(&realClock.hours_), sizeof(realClock.hours_));
        stateFile.read(reinterpret_cast<char *>(&realClock.dayLower_), sizeof(realClock.dayLower_));
        stateFile.read(reinterpret_cast<char *>(&realClock.dayUpper_), sizeof(realClock.dayUpper_));
        RecalculateZeroTime();
    }

    void Save(std::ofstream &stateFile) const {
        stateFile.write(reinterpret_cast<const char *>(&zeroTime_), sizeof(zeroTime_));
        stateFile.write(reinterpret_cast<const char *>(&realClock.seconds_), sizeof(realClock.seconds_));
        stateFile.write(reinterpret_cast<const char *>(&realClock.minutes_), sizeof(realClock.minutes_));
        stateFile.write(reinterpret_cast<const char *>(&realClock.hours_), sizeof(realClock.hours_));
        stateFile.write(reinterpret_cast<const char *>(&realClock.dayLower_), sizeof(realClock.dayLower_));
        stateFile.write(reinterpret_cast<const char *>(&realClock.dayUpper_), sizeof(realClock.dayUpper_));
    }
};
