#pragma once

struct Registers {
    [[nodiscard]] uint16_t GetAF() const {
        return static_cast<uint16_t>(a) << 8 | f;
    }

    [[nodiscard]] uint16_t GetBC() const {
        return static_cast<uint16_t>(b) << 8 | c;
    }

    [[nodiscard]] uint16_t GetDE() const {
        return static_cast<uint16_t>(d) << 8 | e;
    }

    [[nodiscard]] uint16_t GetHL() const {
        return static_cast<uint16_t>(h) << 8 | l;
    }

    [[nodiscard]] bool FlagZero() const {
        return !!(f & 0x80);
    }

    [[nodiscard]] bool FlagSubtract() const {
        return !!(f & 0x40);
    }

    [[nodiscard]] bool FlagHalf() const {
        return !!(f & 0x20);
    }

    [[nodiscard]] bool FlagCarry() const {
        return !!(f & 0x10);
    }

    void SetZero(const bool x) {
        f = (f & 0x7F) | (x << 7);
    }

    void SetSubtract(const bool x) {
        f = (f & 0xBF) | (x << 6);
    }

    void SetHalf(const bool x) {
        f = (f & 0xDF) | (x << 5);
    }

    void SetCarry(const bool x) {
        f = (f & 0xEF) | (x << 4);
    }

    void SetAF(const uint16_t value) {
        a = value >> 8;
        f = value & 0xFF;
    }

    void SetBC(const uint16_t value) {
        b = value >> 8;
        c = value & 0xFF;
    }

    void SetDE(const uint16_t value) {
        d = value >> 8;
        e = value & 0xFF;
    }

    void SetHL(const uint16_t value) {
        h = value >> 8;
        l = value & 0xFF;
    }

    uint8_t a = 0x00;
    uint8_t b = 0x00;
    uint8_t c = 0x00;
    uint8_t d = 0x00;
    uint8_t e = 0x00;
    uint8_t f = 0x00;
    uint8_t h = 0x00;
    uint8_t l = 0x00;
};
