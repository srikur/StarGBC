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

    uint64_t zeroTime_{0x00};
    bool halted_{false};

    struct Clock {
        uint8_t seconds_{0x00};
        uint8_t minutes_{0x00};
        uint8_t hours_{0x00};
        uint8_t dayLower_{0x00};
        uint8_t dayUpper_{0x00};
    };

public:
    // t-cycle ticker per second
    static constexpr uint64_t RTC_TICKS_PER_SECOND = 4194304;

    explicit RealTimeClock(const bool realRTC) : realRTC_{realRTC} {
    }

    void Tick();

    [[nodiscard]] uint8_t ReadRTC(uint16_t address) const;

    void WriteRTC(uint16_t address, uint8_t value);

    uint64_t RecalculateZeroTime();

    void Update();

    void Load(std::ifstream &stateFile);

    void Save(std::ofstream &stateFile) const;

    Clock realClock_{};
    Clock latchedClock_{};
    bool realRTC_{false};
    uint64_t counter_{0};
};
