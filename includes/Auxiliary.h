#pragma once
#include "Common.h"

struct Audio {
    static constexpr uint16_t SOUND_BEGIN = 0xFF10;
    static constexpr uint16_t SOUND_END = 0xFF3F;

    [[nodiscard]] uint8_t ReadByte(const uint16_t address) const {
        return soundData[address - SOUND_BEGIN];
    }

    void WriteByte(const uint16_t address, const uint8_t value) {
        soundData[address - SOUND_BEGIN] = value;
    }

    uint8_t soundData[0x30];
};

enum class Keys {
    Right = 0x01,
    Left = 0x02,
    Up = 0x04,
    Down = 0x08,
    A = 0x10,
    B = 0x20,
    Select = 0x40,
    Start = 0x80,
};

struct Joypad {
    [[nodiscard]] uint8_t GetJoypadState() const {
        if ((select_ & 0x10) == 0x00) {
            return select_ | matrix_ & 0x0F;
        }
        if ((select_ & 0x20) == 0x00) {
            return select_ | matrix_ >> 4;
        }
        return select_;
    }

    void SetJoypadState(const uint8_t value) {
        select_ = value;
    }

    void SetMatrix(const uint8_t value) {
        matrix_ = value;
    }

    [[nodiscard]] uint8_t GetMatrix() const {
        return matrix_;
    }

    [[nodiscard]] uint8_t GetSelect() const {
        return select_;
    }

    void SetSelect(const uint8_t value) {
        select_ = value;
    }

private:
    uint8_t matrix_ = 0xFF;
    uint8_t select_ = 0x00;
};

struct Timer {
    static constexpr uint16_t DIVIDER_REGISTER = 0xFF04;
    static constexpr uint16_t TIMA = 0xFF05;
    static constexpr uint16_t TMA = 0xFF06;
    static constexpr uint16_t TAC = 0xFF07;
    static constexpr uint32_t MAX_CYCLES = 70224;

    uint32_t clockCounter = 0x400;
    uint32_t dividerCounter = 0x00;
    uint32_t clockSpeed = 0x400; // tac & 0x03
    uint8_t timerModulo = 0x00; // tma
    uint8_t timerCounter = 0x00; // tima
    uint8_t dividerRegister = 0x00;
    bool clockEnabled = false; // tac & 0x04
};

struct Serial {
    [[nodiscard]] uint8_t ReadSerial(const uint16_t address) const {
        switch (address) {
            case 0xFF01:
                return data_;
            case 0xFF02:
                return control_;
            default:
                throw UnreachableCodeException("Improper read from serial");
        }
    }

    void WriteSerial(const uint16_t address, const uint8_t value) {
        switch (address) {
            case 0xFF01:
                data_ = value;
                break;
            case 0xFF02:
                control_ = value;
                break;
            default:
                throw UnreachableCodeException("Improper write to serial");
        }
    }

private:
    uint8_t data_ = 0;
    uint8_t control_ = 0;
};
