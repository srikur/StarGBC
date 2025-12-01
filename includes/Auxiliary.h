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
        if ((select_ & 0x10) == 0x00)
            return static_cast<uint8_t>(select_ | (matrix_ & 0x0F));

        if ((select_ & 0x20) == 0x00)
            return static_cast<uint8_t>(select_ | (matrix_ >> 4));

        return select_;
    }

    void SetJoypadState(uint8_t value) {
        select_ = value;
        UpdateKeyFlag();
    }

    void SetMatrix(uint8_t value) {
        matrix_ = value;
        UpdateKeyFlag();
    }

    [[nodiscard]] uint8_t GetMatrix() const { return matrix_; }
    [[nodiscard]] uint8_t GetSelect() const { return select_; }

    void SetSelect(uint8_t value) {
        select_ = value;
        UpdateKeyFlag();
    }

    [[nodiscard]] bool KeyPressed() const { return keyPressed_; }
    void ClearKeyPressed() { keyPressed_ = false; }

    bool SaveState(std::ofstream &f) const {
        if (!f.is_open()) return false;
        f.write(reinterpret_cast<const char *>(&matrix_), sizeof matrix_);
        f.write(reinterpret_cast<const char *>(&select_), sizeof select_);
        f.write(reinterpret_cast<const char *>(&keyPressed_), sizeof keyPressed_);
        return true;
    }

    bool LoadState(std::ifstream &f) {
        if (!f.is_open()) return false;
        f.read(reinterpret_cast<char *>(&matrix_), sizeof matrix_);
        f.read(reinterpret_cast<char *>(&select_), sizeof select_);
        f.read(reinterpret_cast<char *>(&keyPressed_), sizeof keyPressed_);
        return true;
    }

private:
    void UpdateKeyFlag() {
        const bool dirRowSelected = (select_ & 0x10) == 0;
        const bool btnRowSelected = (select_ & 0x20) == 0;

        const bool dirKeyLow = (matrix_ & 0x0F) != 0x0F;
        const bool btnKeyLow = ((matrix_ >> 4) & 0x0F) != 0x0F;

        if ((dirRowSelected && dirKeyLow) || (btnRowSelected && btnKeyLow))
            keyPressed_ = true;
    }

    uint8_t matrix_ = 0xFF;
    uint8_t select_ = 0x00;
    bool keyPressed_ = false;
};

struct Serial {
    bool active_{false};
    uint8_t data_{0}; // SB
    uint8_t control_{0}; // SC
    uint8_t bitsShifted_{0};
    uint32_t ticksUntilShift_{0};
    uint32_t ticksPerBit_{0};

    [[nodiscard]] uint8_t ReadSerial(const uint16_t address) const {
        switch (address) {
            case 0xFF01: return data_;
            case 0xFF02: return control_ | 0x7C;
            default: throw UnreachableCodeException("Improper read from serial");
        }
    }

    void WriteSerial(const uint16_t address, const uint8_t value, const bool doubleSpeed, const bool gbc) {
        switch (address) {
            case 0xFF01:
                data_ = value;
                break;
            case 0xFF02:
                control_ = value | 0x7E;
                if ((control_ & 0x81) == 0x81) {
                    ticksPerBit_ = doubleSpeed ? 256 : 512;
                    if (gbc && control_ & 0x02) ticksPerBit_ /= 32;
                    ticksUntilShift_ = ticksPerBit_ - 47;
                    bitsShifted_ = 0;
                    active_ = true;
                }
                break;
            default:
                throw UnreachableCodeException("Improper write to serial");
        }
    }

    void ShiftOneBit() {
        data_ = static_cast<uint8_t>(data_ << 1 | 0x01);
    }

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(&data_), sizeof(data_));
            stateFile.write(reinterpret_cast<const char *>(&control_), sizeof(control_));
            stateFile.write(reinterpret_cast<const char *>(&active_), sizeof(active_));
            stateFile.write(reinterpret_cast<const char *>(&bitsShifted_), sizeof(bitsShifted_));
            stateFile.write(reinterpret_cast<const char *>(&ticksUntilShift_), sizeof(ticksUntilShift_));
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
            stateFile.read(reinterpret_cast<char *>(&active_), sizeof(active_));
            stateFile.read(reinterpret_cast<char *>(&bitsShifted_), sizeof(bitsShifted_));
            stateFile.read(reinterpret_cast<char *>(&ticksUntilShift_), sizeof(ticksUntilShift_));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading Serial state: " << e.what() << std::endl;
            return false;
        }
    }
};
