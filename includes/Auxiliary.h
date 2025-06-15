#pragma once
#include <iostream>

#include "Audio.h"
#include "Common.h"

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

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(&matrix_), sizeof(matrix_));
            stateFile.write(reinterpret_cast<const char *>(&select_), sizeof(select_));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error saving Joypad state: " << e.what() << std::endl;
            return false;
        }
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.read(reinterpret_cast<char *>(&matrix_), sizeof(matrix_));
            stateFile.read(reinterpret_cast<char *>(&select_), sizeof(select_));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading Joypad state: " << e.what() << std::endl;
            return false;
        }
    }

private:
    uint8_t matrix_ = 0xFF;
    uint8_t select_ = 0x00;
};

struct Timer {
    uint16_t divCounter = 0;
    uint8_t tma = 0x00;
    uint8_t tima = 0x00;
    uint8_t tac = 0x00;

    bool overflowPending = false;
    uint8_t overflowDelay = 0;

    void WriteByte(const uint16_t address, const uint8_t value) {
        if (address == 0xFF04) WriteDIV();
        else if (address == 0xFF05) WriteTIMA(value);
        else if (address == 0xFF06) tma = value;
        else if (address == 0xFF07) WriteTAC(value);
    }

    [[nodiscard]] uint8_t ReadByte(const uint16_t address) const {
        if (address == 0xFF04) return divCounter >> 8;
        if (address == 0xFF05) return tima;
        if (address == 0xFF06) return tma;
        if (address == 0xFF07) return tac | 0xF8;
        return 0xFF;
    }

    void WriteDIV() {
        const bool enabled = tac & 0x04;
        const int bit = TimerBit(tac);
        const bool oldSignal = enabled && (divCounter & (1u << bit));

        divCounter = 0;

        if (oldSignal) IncrementTIMA();
    }

    void WriteTAC(uint8_t value) {
        value &= 0x07;
        const bool oldEnabled = tac & 0x04;
        const int oldBit = TimerBit(tac);
        const bool oldSignal = oldEnabled && (divCounter & (1u << oldBit));

        tac = value;

        const bool newEnabled = tac & 0x04;
        const int newBit = TimerBit(tac);
        const bool newSignal = newEnabled && (divCounter & (1u << newBit));

        if (oldSignal && !newSignal) IncrementTIMA();
    }

    void WriteTIMA(const uint8_t value) {
        if (overflowPending) {
            overflowPending = false;
            overflowDelay = 0;
        }
        tima = value;
    }

    static constexpr int TimerBit(const uint8_t tacMode) {
        switch (tacMode & 0x03) {
            case 0x00: return 9; // 4096 Hz
            case 0x01: return 3; // 262144 Hz
            case 0x02: return 5; // 65536 Hz
            case 0x03: return 7; // 16384 Hz
            default: ;
        }
        return 9;
    }

    void IncrementTIMA() {
        if (++tima == 0) {
            overflowPending = true;
            overflowDelay = 4;
        }
    }

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(&divCounter), sizeof(divCounter));
            stateFile.write(reinterpret_cast<const char *>(&tima), sizeof(tima));
            stateFile.write(reinterpret_cast<const char *>(&tma), sizeof(tma));
            stateFile.write(reinterpret_cast<const char *>(&tac), sizeof(tac));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error saving Timer state: " << e.what() << std::endl;
            return false;
        }
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.read(reinterpret_cast<char *>(&divCounter), sizeof(divCounter));
            stateFile.read(reinterpret_cast<char *>(&tima), sizeof(tima));
            stateFile.read(reinterpret_cast<char *>(&tma), sizeof(tma));
            stateFile.read(reinterpret_cast<char *>(&tac), sizeof(tac));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading Timer state: " << e.what() << std::endl;
            return false;
        }
    }
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

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(&data_), sizeof(data_));
            stateFile.write(reinterpret_cast<const char *>(&control_), sizeof(control_));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error saving Serial state: " << e.what() << std::endl;
            return false;
        }
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.read(reinterpret_cast<char *>(&data_), sizeof(data_));
            stateFile.read(reinterpret_cast<char *>(&control_), sizeof(control_));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading Serial state: " << e.what() << std::endl;
            return false;
        }
    }

private:
    uint8_t data_ = 0;
    uint8_t control_ = 0;
};
