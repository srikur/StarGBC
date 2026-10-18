#pragma once
#include "stargbc/Common.h"
#include <functional>
#include <span>

class RealTimeClock {
    static constexpr u64 kSecPerMin = 60;
    static constexpr u64 kSecPerHour = 60 * kSecPerMin;
    static constexpr u64 kSecPerDay = 24 * kSecPerHour;
    static constexpr u64 kMinPerHour = 60;
    static constexpr u64 kHourPerDay = 24;
    static constexpr u64 kMaxDays = 511;
    static constexpr u64 kHoursPerDay = 24;

    [[nodiscard]] u64 now_seconds() const;

    [[nodiscard]] u64 compose_seconds() const;

    // Wall-clock anchor derived from the host time; recomputed on load
    [[= NotStateAware]] u64 zero_time{0x00};
    bool halted{false};

    struct Clock {
        u8 seconds{0x00};
        u8 minutes{0x00};
        u8 hours{0x00};
        u8 day_lower{0x00};
        u8 day_upper{0x00};
    };

public:
    static constexpr size_t kSaveFooterSize = 48;
    static constexpr size_t kSaveFooterMinSize = 44;
    static constexpr u64 RTC_TICKS_PER_SECOND = 4194304;

    using TimeSource = std::function<u64()>;

    explicit RealTimeClock(const bool real, TimeSource time_source = {}) : real_rtc(real), time_source_(std::move(time_source)) {
        if (real && !time_source_)
            throw std::invalid_argument("Real RTC requires a host clock");
    }

    void tick();

    [[nodiscard]] u8 read_rtc(u16 address) const;

    void write_rtc(u16 address, u8 value);

    u64 recalculate_zero_time();

    // Called every T-cycle; inline so the idle path is a counter bump.
    // Rolls the rtc over every emulated second
    void Update() {
        if (!halted)
            counter++;
        if (counter == RTC_TICKS_PER_SECOND) {
            counter = 0;
            tick();
        }
    }

    void LoadSaveFooter(std::span<const u8> bytes);

    [[nodiscard]] std::array<u8, kSaveFooterSize> SaveFooter() const;

    Clock realClock{};
    Clock latchedClock{};
    [[= NotStateAware]] bool real_rtc{false};
    u64 counter{0};

private:
    [[= NotStateAware]] TimeSource time_source_;
};
