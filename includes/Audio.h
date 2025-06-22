#pragma once

#include <set>

#include "Common.h"

struct Frequency {
    uint16_t value{0}; // Only 11 bits used

    void WriteLow(const uint8_t lower) { value = value & 0x700 | lower; }
    void WriteHigh(const uint8_t higher) { value = value & 0x00FF | higher << 8; }
    [[nodiscard]] uint8_t ReadLow() const { return static_cast<uint8_t>(value & 0xFF) | 0xFF; }
    [[nodiscard]] uint8_t ReadHigh() const { return static_cast<uint8_t>(value >> 8) | 0xBF; }
};

struct Sweep {
    uint8_t pace{0};
    bool direction{false};
    uint8_t step{0};

    void Write(const uint8_t v) {
        pace = v >> 4 & 0x07;
        direction = (v & 0x08) != 0;
        step = v & 0x07;
    }

    [[nodiscard]] uint8_t Value() const {
        return static_cast<uint8_t>(pace << 4 | (direction ? 0x08 : 0x00) | step | 0x80);
    }
};

struct Envelope {
    uint8_t initialVolume{0};
    bool direction{false};
    uint8_t sweepPace{0};

    void Write(const uint8_t value) {
        initialVolume = value >> 4 & 0x0F;
        direction = (value & 0x08) != 0;
        sweepPace = value & 0x07;
    }

    [[nodiscard]] uint8_t Value() const {
        return static_cast<uint8_t>(initialVolume << 4 | (direction ? 0x08 : 0x00) | sweepPace | 0x00);
    }
};

struct Length {
    bool enabled{false};
    uint8_t lengthTimer{0};
    uint8_t dutyCycle{0};

    void Write(const uint8_t value, const bool audioEnabled) {
        if (audioEnabled) dutyCycle = value >> 6 & 0x03;
        lengthTimer = value & 0x3F;
    }

    [[nodiscard]] uint8_t Value() const {
        return static_cast<uint8_t>(dutyCycle << 6 | lengthTimer | 0x3F);
    }
};

struct Noise {
    uint8_t clockShift{0x00};
    bool lfsr{false};
    uint8_t clockDivider{0x00};

    void Write(const uint8_t value) {
        clockShift = value >> 4 & 0x0F;
        lfsr = (value & 0x08) != 0;
        clockDivider = value & 0x07;
    }

    [[nodiscard]] uint8_t Value() const {
        return static_cast<uint8_t>(clockShift << 4 | (lfsr ? 0x08 : 0x00) | clockDivider | 0x00);
    }
};

struct Channel1 {
    bool enabled{false};
    Sweep sweep{};
    Frequency frequency{};
    Length lengthTimer{};
    Envelope envelope{};

    [[nodiscard]] uint8_t ReadByte(const uint16_t address) const {
        switch (address & 0xF) {
            case 0x00: return sweep.Value();
            case 0x01: return lengthTimer.Value();
            case 0x02: return envelope.Value();
            case 0x03: return frequency.ReadLow();
            case 0x04: return frequency.ReadHigh();
            default: throw UnreachableCodeException("Channel1::ReadByte unreachable code at address: " + std::to_string(address));
        }
    }

    void WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled) {
        switch (address & 0xF) {
            case 0x00: sweep.Write(value);
                break;
            case 0x01: lengthTimer.Write(value, audioEnabled);
                break;
            case 0x02: envelope.Write(value);
                break;
            case 0x03: frequency.WriteLow(value);
                break;
            case 0x04: frequency.WriteHigh(value);
                lengthTimer.enabled = value & 0x40;
                break;
            default: throw UnreachableCodeException("Channel1::WriteByte unreachable code at address: " + std::to_string(address));
        }
    }
};

struct Channel2 {
    bool enabled{false};
    Frequency frequency{};
    Length lengthTimer{};
    Envelope envelope{};

    [[nodiscard]] uint8_t ReadByte(const uint16_t address) const {
        switch (address & 0xF) {
            case 0x05: return 0xFF;
            case 0x06: return lengthTimer.Value();
            case 0x07: return envelope.Value();
            case 0x08: return frequency.ReadLow();
            case 0x09: return frequency.ReadHigh();
            default: throw UnreachableCodeException("Channel2::ReadByte unreachable code at address: " + std::to_string(address));
        }
    }

    void WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled) {
        switch (address & 0xF) {
            case 0x05: break;
            case 0x06: lengthTimer.Write(value, audioEnabled);
                break;
            case 0x07: envelope.Write(value);
                break;
            case 0x08: frequency.WriteLow(value);
                break;
            case 0x09: frequency.WriteHigh(value);
                lengthTimer.enabled = value & 0x40;
                break;
            default: throw UnreachableCodeException("Channel2::WriteByte unreachable code at address: " + std::to_string(address));
        }
    }
};

struct Channel3 {
    bool enabled{false};
    bool dac{false};
    uint8_t lengthTimer{0};
    uint8_t outputLevel{0};
    Frequency frequency{};

    [[nodiscard]] uint8_t ReadByte(const uint16_t address) const {
        switch (address & 0xF) {
            case 0x0A: return dac << 7 | 0x7F;
            case 0x0B: return 0xFF;
            case 0x0C: return outputLevel << 5 | 0x9F;
            case 0x0D: return frequency.ReadLow();
            case 0x0E: return frequency.ReadHigh();
            default: throw UnreachableCodeException("Channel3::ReadByte unreachable code at address: " + std::to_string(address));
        }
    }

    void WriteByte(const uint16_t address, const uint8_t value) {
        switch (address & 0xF) {
            case 0x0A: dac = value >> 7 & 0x01;
                break;
            case 0x0B: lengthTimer = value;
                break;
            case 0x0C: outputLevel = value >> 5 & 0x03;
                break;
            case 0x0D: frequency.WriteLow(value);
                break;
            case 0x0E: frequency.WriteHigh(value);
                break;
            default: throw UnreachableCodeException("Channel3::WriteByte unreachable code at address: " + std::to_string(address));
        }
    }
};

struct Channel4 {
    bool enabled{false};
    Length lengthTimer{};
    Envelope envelope{};
    Noise noise{};
    uint8_t trigger{0};

    [[nodiscard]] uint8_t ReadByte(const uint16_t address) const {
        switch (address & 0xF) {
            case 0x0F:
            case 0x00: return 0xFF;
            case 0x01: return envelope.Value();
            case 0x02: return noise.Value();
            case 0x03: return trigger << 7 | (lengthTimer.enabled ? 0x40 : 0x00) | 0xBF;
            default: throw UnreachableCodeException("Channel4::ReadByte unreachable code at address: " + std::to_string(address));
        }
    }

    void WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled) {
        switch (address & 0xF) {
            case 0x0F: break;
            case 0x00: lengthTimer.Write(value, audioEnabled);
                break;
            case 0x01: envelope.Write(value);
                break;
            case 0x02: noise.Write(value);
                break;
            case 0x03: trigger = value >> 7 & 0x01;
                lengthTimer.enabled = (value & 0x40) != 0;
                break;
            default: throw UnreachableCodeException("Channel4::WriteByte unreachable code at address: " + std::to_string(address));
        }
    }
};

class Audio {
    bool audioEnabled{false};
    bool dmg{false};

    Channel1 ch1{};
    Channel2 ch2{};
    Channel3 ch3{};
    Channel4 ch4{};

    uint8_t nr50{};
    uint8_t nr51{};

    std::array<uint8_t, 0xF> waveRam{};

public:
    void SetDMG(const bool value) { dmg = value; }

    void WriteAudioControl(const uint8_t value) {
        if ((value & 0x80) == 0x80) {
            audioEnabled = true;
        } else {
            audioEnabled = false;
            ch1 = {};
            ch2 = {};
            ch3 = {};
            ch4 = {};
            nr50 = 0;
            nr51 = 0;
        }
    }

    [[nodiscard]] uint8_t ReadAudioControl() const {
        return (audioEnabled ? 0x80 : 0x00) | (ch4.enabled << 3) | (ch3.enabled << 2) | (ch2.enabled << 1) | (ch1.enabled << 0) | 0x70;
    }

    [[nodiscard]] uint8_t ReadByte(const uint16_t address) const {
        switch (address) {
            case 0xFF10 ... 0xFF14: return ch1.ReadByte(address);
            case 0xFF15 ... 0xFF19: return ch2.ReadByte(address);
            case 0xFF1A ... 0xFF1E: return ch3.ReadByte(address);
            case 0xFF1F ... 0xFF23: return ch4.ReadByte(address);
            case 0xFF30 ... 0xFF3F: return waveRam[address - 0xFF30];
            case 0xFF24: return nr50 | 0x00;
            case 0xFF25: return nr51 | 0x00;
            case 0xFF26: return ReadAudioControl();
            default: return 0xFF;
        }
    }

    void WriteByte(const uint16_t address, const uint8_t value) {
        static const std::set<uint16_t> allowedAddresses = {
            0xFF26, 0xFF11, 0xFF16, 0xFF1B, 0xFF20
        };
        if (!audioEnabled && address != 0xFF26 && (!dmg || !allowedAddresses.contains(address))) {
            return;
        }
        switch (address) {
            case 0xFF10 ... 0xFF14: ch1.WriteByte(address, value, audioEnabled);
                break;
            case 0xFF15 ... 0xFF19: ch2.WriteByte(address, value, audioEnabled);
                break;
            case 0xFF1A ... 0xFF1E: ch3.WriteByte(address, value);
                break;
            case 0xFF1F ... 0xFF23: ch4.WriteByte(address, value, audioEnabled);
                break;
            case 0xFF24: nr50 = value;
                break;
            case 0xFF25: nr51 = value;
                break;
            case 0xFF26: WriteAudioControl(value);
                break;
            case 0xFF30 ... 0xFF3F: waveRam[address - 0xFF30] = value;
                break;
            default: break;
        }
    }
};
