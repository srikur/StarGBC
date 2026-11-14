#include "RealTimeClock.h"

u64 RealTimeClock::now_seconds() const {
    return time_source_ ? time_source_() : 0;
}

u64 RealTimeClock::compose_seconds() const {
    const auto days = static_cast<uint16_t>((realClock.day_upper & 0x01) << 8 | realClock.day_lower);
    return realClock.seconds + realClock.minutes * kSecPerMin + realClock.hours * kSecPerHour + days * kSecPerDay;
}

uint64_t RealTimeClock::recalculate_zero_time() {
    zero_time = now_seconds() - compose_seconds();
    return zero_time;
}

void RealTimeClock::LoadSaveFooter(const std::span<const u8> bytes) {
    if (bytes.size() < kSaveFooterMinSize || bytes.size() > kSaveFooterSize)
        throw std::invalid_argument("Invalid RTC footer size");
    size_t offset = 0;
    auto readRegister = [&] {
        const auto value = bytes[offset];
        offset += 4;
        return value;
    };
    for (Clock* clock : {&realClock, &latchedClock}) {
        clock->seconds = readRegister();
        clock->minutes = readRegister();
        clock->hours = readRegister();
        clock->day_lower = readRegister();
        clock->day_upper = readRegister();
    }
    halted = realClock.day_upper & 0x40;
    counter = 0;
    const u64 save_time = readRegister() << 32 | readRegister();
    const u64 current_time = now_seconds();
    if (save_time <= current_time) {
        advance(save_time - current_time);
        zero_time = current_time;
    }
}

std::array<u8, RealTimeClock::kSaveFooterSize> RealTimeClock::SaveFooter() const {
    std::array<u8, kSaveFooterSize> bytes{};
    size_t offset = 0;
    auto writeLE = [&](u64 value, const size_t size) {
        for (size_t i = 0; i < size; ++i, value >>= 8)
            bytes[offset++] = static_cast<u8>(value);
    };
    for (const Clock& clock : {realClock, latchedClock}) {
        writeLE(clock.seconds, 4);
        writeLE(clock.minutes, 4);
        writeLE(clock.hours, 4);
        writeLE(clock.day_lower, 4);
        writeLE(clock.day_upper, 4);
    }
    writeLE(now_seconds(), 8);
    return bytes;
}

void RealTimeClock::advance(const u64 ticks) {
    for (auto i{0_u64}; i < ticks; i++) {
        realClock.seconds++;
        if (realClock.seconds == 60)
            realClock.seconds = 0;
        bool recalcDays = false;
        if (realClock.seconds == 0) {
            realClock.minutes++;
            if (realClock.minutes == 60) {
                realClock.minutes = 0;
                realClock.hours++;
                if (realClock.hours == 24) {
                    realClock.hours = 0;
                    recalcDays = true;
                }
            }
        }

        if (recalcDays) {
            const auto current_days = static_cast<uint16_t>((realClock.day_upper & 0x01) << 8 | realClock.day_lower);
            uint64_t total_days = current_days + 1;

            if (total_days >= kMaxDays) {
                realClock.day_upper |= 0x80;
                total_days %= kMaxDays;
            }

            realClock.day_lower = static_cast<uint8_t>(total_days & 0xFF);
            realClock.day_upper = (realClock.day_upper & 0xFE) | static_cast<uint8_t>((total_days >> 8) & 0x01);
        }
    }
}

void RealTimeClock::tick() {
    if (halted) {
        return;
    }

    const uint32_t elapsed_seconds = [&]() -> uint32_t {
        if (real_rtc) {
            const uint64_t target_total_seconds = now_seconds() - zero_time;
            const uint64_t current_total_seconds = compose_seconds();
            return target_total_seconds - current_total_seconds;
        }
        return 1;
    }();
    advance(elapsed_seconds);
}

u8 RealTimeClock::read_rtc(const u16 address) const {
    switch (address) {
        case RTC_REGISTER_SECONDS:
            // ranges from $00 to $3B; bottom 6 bits
            return latchedClock.seconds & 0b111111;
        case RTC_REGISTER_MINUTES:
            // ranges from $00 to $3B; bottom 6 bits
            return latchedClock.minutes & 0b111111;
        case RTC_REGISTER_HOURS:
            // ranges from $00 to $17; bottom 5 bits
            return latchedClock.hours & 0b11111;
        case RTC_REGISTER_DAY_LOWER:
            return latchedClock.day_lower;
        case RTC_REGISTER_DAY_UPPER:
            // only bits 0, 6, 7 used
            // Bit 0: MSB of day counter
            // Bit 6: Halt Flag
            // Bit 7: Day counter overflow
            return latchedClock.day_upper & 0b11000001;
        default:
            return 0xFF;
    }
}

void RealTimeClock::write_rtc(const uint16_t address, const uint8_t value) {
    switch (address) {
        case RTC_REGISTER_SECONDS:
            realClock.seconds = value & 0b111111;
            counter = 0;
            break;
        case RTC_REGISTER_MINUTES:
            realClock.minutes = value & 0b111111;
            break;
        case RTC_REGISTER_HOURS:
            realClock.hours = value & 0b11111;
            break;
        case RTC_REGISTER_DAY_LOWER:
            realClock.day_lower = value;
            break;
        case RTC_REGISTER_DAY_UPPER: {
            const bool wasHalted = Bit<6>(realClock.day_upper);
            realClock.day_upper = value & 0b11000001;
            halted = Bit<6>(realClock.day_upper);
            if (const bool nowRunning = !Bit<6>(realClock.day_upper); wasHalted && nowRunning)
                recalculate_zero_time();
            break;
        }
        default:
            break;
    }

    recalculate_zero_time();
}
