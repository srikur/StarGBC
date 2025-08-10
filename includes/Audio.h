#pragma once

#include <array>
#include <cstring>
#include <set>
#include <string>

#include "Common.h"

class Audio;

struct Frequency {
    uint16_t value{0}; // Only 11 bits used

    void Write(const uint16_t v) { value = v & 0x7FF; }
    void WriteLow(const uint8_t lower) { value = value & 0x700 | lower; }
    void WriteHigh(const uint8_t higher) { value = value & 0x00FF | higher << 8; }
    [[nodiscard]] uint16_t Value() const { return value & 0x7FF; }
    [[nodiscard]] uint8_t ReadLow() const { return static_cast<uint8_t>(value & 0xFF) | 0xFF; }
    [[nodiscard]] uint8_t ReadHigh() const { return static_cast<uint8_t>(value >> 8) | 0xBF; }
};

struct Sweep {
    uint8_t pace{0};
    bool direction{false};
    uint8_t step{0};

    uint8_t timer{0};
    bool enabled{false};
    uint16_t shadowFreq{0};
    bool subtractionCalculationMade{false};

    void Write(const uint8_t v) {
        pace = v >> 4 & 0x07;
        direction = v & 0x08;
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
    uint8_t periodTimer{0};
    uint8_t currentVolume{0};

    void Write(const uint8_t value) {
        initialVolume = value >> 4 & 0x0F;
        direction = (value & 0x08) != 0;
        sweepPace = value & 0x07;
    }

    [[nodiscard]] uint8_t Value() const {
        return static_cast<uint8_t>(initialVolume << 4 | (direction ? 0x08 : 0x00) | sweepPace);
    }
};

struct Length {
    bool enabled{false};
    uint16_t lengthTimer{0};
    uint8_t dutyCycle{0};

    void Write(const uint8_t value, const bool audioEnabled) {
        if (audioEnabled) dutyCycle = value >> 6 & 0x03;
        lengthTimer = value & 0x3F;
    }

    [[nodiscard]] uint8_t Value() const {
        return static_cast<uint8_t>(dutyCycle << 6 | 0x3F);
    }
};

struct Noise {
    uint8_t clockShift{0x00};
    bool lfsrWidth{false};
    uint8_t clockDivider{0x00};

    void Write(const uint8_t value) {
        clockShift = value >> 4 & 0x0F;
        lfsrWidth = (value & 0x08) != 0;
        clockDivider = value & 0x07;
    }

    [[nodiscard]] uint8_t Value() const {
        return static_cast<uint8_t>(clockShift << 4 | (lfsrWidth ? 0x08 : 0x00) | clockDivider);
    }
};

struct Channel {
    bool enabled{false};
    bool dacEnabled{false};

    static constexpr uint8_t DUTY_PATTERNS[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
        {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
        {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
        {0, 1, 1, 1, 1, 1, 1, 0} // 75%
    };

    virtual ~Channel() = default;

    virtual void TickLength() = 0;

    virtual void TickEnvelope() {
    }
};

struct Channel1 final : Channel {
    Sweep sweep{};
    Length lengthTimer{};
    Envelope envelope{};
    Frequency frequency{};

    int32_t freqTimer{0};
    uint8_t dutyStep{0};
    float currentOutput{0.0f};

    void Trigger(const uint8_t freqStep) {
        if (dacEnabled) enabled = true;
        dacEnabled = (envelope.initialVolume > 0 || envelope.direction);

        if (lengthTimer.lengthTimer == 64) {
            lengthTimer.lengthTimer = 0;
            if (lengthTimer.enabled && (freqStep % 2 != 0)) {
                lengthTimer.lengthTimer++;
            }
        }

        freqTimer = (2048 - frequency.Value()) * 4;

        envelope.periodTimer = envelope.sweepPace ? envelope.sweepPace : 8;
        envelope.currentVolume = envelope.initialVolume;

        sweep.enabled = sweep.pace > 0 || sweep.step > 0;
        sweep.shadowFreq = frequency.Value();
        sweep.timer = sweep.pace ? sweep.pace : 8;
        sweep.subtractionCalculationMade = false;
        if (sweep.step > 0 && CalculateSweep() > 2047) {
            enabled = false;
        }
    }

    void TickLength() override {
        if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
            lengthTimer.lengthTimer++;
        }
    }

    void TickSweep() {
        if (!sweep.enabled || !enabled) return;
        if (--sweep.timer > 0) {
            return;
        }

        if (sweep.pace != 0) {
            sweep.timer = sweep.pace;
            if (const uint16_t newFreq = CalculateSweep(); newFreq > 2047) {
                enabled = false;
            } else {
                if (sweep.step > 0) {
                    frequency.Write(newFreq);
                    sweep.shadowFreq = newFreq;
                    freqTimer = (2048 - frequency.Value()) * 4;
                }
                if (CalculateSweep() > 2047) {
                    enabled = false;
                }
            }
        } else {
            sweep.timer = 8;
        }
    }

    void TickEnvelope() override {
        if (envelope.sweepPace > 0) {
            envelope.periodTimer--;
            if (envelope.periodTimer == 0) {
                envelope.periodTimer = envelope.sweepPace;
                if (envelope.direction && envelope.currentVolume < 15) {
                    envelope.currentVolume++;
                } else if (!envelope.direction && envelope.currentVolume > 0) {
                    envelope.currentVolume--;
                }
            }
        }
    }

    uint16_t CalculateSweep() {
        uint16_t newFreq = sweep.shadowFreq >> sweep.step;
        if (sweep.direction) {
            newFreq = sweep.shadowFreq - newFreq;
            sweep.subtractionCalculationMade = true;
        } else {
            newFreq = sweep.shadowFreq + newFreq;
        }
        return newFreq;
    }

    void Tick() {
        if (!enabled) return;
        freqTimer--;
        if (lengthTimer.enabled && lengthTimer.lengthTimer == 64) {
            enabled = false;
        }
        if (freqTimer <= 0) {
            freqTimer = (2048 - frequency.Value()) * 4;
            dutyStep = (dutyStep + 1) % 8;
            if (dacEnabled) {
                currentOutput = static_cast<float>(DUTY_PATTERNS[lengthTimer.dutyCycle][dutyStep]) * static_cast<float>(envelope.currentVolume);
            }
        }
    }

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

    void HandleNR14Write(const uint8_t value, const uint8_t freqStep) {
        frequency.WriteHigh(value);
        const bool oldEnabled = lengthTimer.enabled;
        lengthTimer.enabled = value & 0x40;
        if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
            if (lengthTimer.lengthTimer < 64) {
                lengthTimer.lengthTimer++;
            }
            if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
        }
        if (value & 0x80) Trigger(freqStep);
    }

    void WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep) {
        switch (address & 0xF) {
            case 0x00: {
                const bool oldDirection = sweep.direction;
                sweep.Write(value);
                if (oldDirection && !sweep.direction && sweep.subtractionCalculationMade) enabled = false;
            }
            break;
            case 0x01: lengthTimer.Write(value, audioEnabled);
                break;
            case 0x02: envelope.Write(value);
                dacEnabled = (value & 0xF8) != 0;
                if (!dacEnabled) enabled = false;
                break;
            case 0x03: frequency.WriteLow(value);
                break;
            case 0x04: HandleNR14Write(value, freqStep);
                break;
            default: throw UnreachableCodeException("Channel1::WriteByte unreachable code at address: " + std::to_string(address));
        }
    }
};

struct Channel2 final : Channel {
    Length lengthTimer{};
    Envelope envelope{};
    Frequency frequency{};
    int32_t freqTimer{0};
    uint8_t dutyStep{0};
    float currentOutput{0.0f};

    void Trigger(const uint8_t freqStep) {
        if (dacEnabled) enabled = true;
        dacEnabled = (envelope.initialVolume > 0 || envelope.direction);

        if (lengthTimer.lengthTimer == 64) {
            lengthTimer.lengthTimer = 0;
            if (lengthTimer.enabled && (freqStep % 2 != 0)) {
                lengthTimer.lengthTimer++;
            }
        }
        freqTimer = (2048 - frequency.Value()) * 4;

        envelope.periodTimer = envelope.sweepPace ? envelope.sweepPace : 8;
        envelope.currentVolume = envelope.initialVolume;
    }

    void TickLength() override {
        if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
            lengthTimer.lengthTimer++;
        }
    }

    void TickEnvelope() override {
        if (envelope.sweepPace > 0) {
            envelope.periodTimer--;
            if (envelope.periodTimer == 0) {
                envelope.periodTimer = envelope.sweepPace;
                if (envelope.direction && envelope.currentVolume < 15) {
                    envelope.currentVolume++;
                } else if (!envelope.direction && envelope.currentVolume > 0) {
                    envelope.currentVolume--;
                }
            }
        }
    }

    void Tick() {
        if (!enabled) return;
        freqTimer--;
        if (lengthTimer.enabled && lengthTimer.lengthTimer == 64) {
            enabled = false;
        }
        if (freqTimer <= 0) {
            freqTimer = (2048 - frequency.Value()) * 4;
            dutyStep = (dutyStep + 1) % 8;
            if (dacEnabled) {
                currentOutput = static_cast<float>(DUTY_PATTERNS[lengthTimer.dutyCycle][dutyStep]) * static_cast<float>(envelope.currentVolume);
            }
        }
    }

    void HandleNR24Write(const uint8_t value, const uint8_t freqStep) {
        frequency.WriteHigh(value);
        const bool oldEnabled = lengthTimer.enabled;
        lengthTimer.enabled = value & 0x40;
        if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
            if (lengthTimer.lengthTimer < 64) lengthTimer.lengthTimer++;
            if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
        }
        if (value & 0x80) Trigger(freqStep);
    }

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

    void WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep) {
        switch (address & 0xF) {
            case 0x05: break;
            case 0x06: lengthTimer.Write(value, audioEnabled);
                break;
            case 0x07: envelope.Write(value);
                dacEnabled = (value & 0xF8) != 0;
                if (!dacEnabled) enabled = false;
                break;
            case 0x08: frequency.WriteLow(value);
                break;
            case 0x09: HandleNR24Write(value, freqStep);
                break;
            default: throw UnreachableCodeException("Channel2::WriteByte unreachable code at address: " + std::to_string(address));
        }
    }
};

struct Channel3 final : Channel {
    static constexpr uint8_t volumeShifts[4] = {4, 0, 1, 2};

    bool lengthEnabled{false};
    bool playing{false};
    bool alternateRead{false};
    uint16_t lengthTimer{0};
    uint8_t outputLevel{0};
    uint8_t volumeShift{0};
    Frequency frequency{};
    uint8_t sampleByte{0};
    int32_t period{0};
    uint8_t waveStep{0};
    uint8_t ticks{0};
    float currentOutput{0.0f};

    std::array<uint8_t, 0x10> waveRam{};

    [[nodiscard]] uint8_t ReadWaveRam(const uint16_t address, const bool dmg) const {
        if (!enabled) return waveRam[address - 0xFF30];
        return (!dmg || alternateRead) ? waveRam[waveStep >> 1] : 0xFF;
    }

    void WriteWaveRam(const uint16_t address, const uint8_t value, const bool dmg) {
        if (!enabled) {
            waveRam[address - 0xFF30] = value;
        } else if (!dmg || alternateRead) {
            waveRam[waveStep >> 1] = value;
        }
    }

    void Reset() {
        lengthTimer = outputLevel = frequency.value = waveStep = period = ticks = sampleByte = 0;
        currentOutput = 0.0f;
        lengthEnabled = enabled = dacEnabled = playing = alternateRead = false;
    }

    void Trigger(const uint8_t freqStep, const bool dmg) {
        if (dacEnabled) enabled = true;
        dacEnabled = true;

        if (lengthTimer == 256) {
            lengthTimer = 0;
            if (lengthEnabled && (freqStep % 2 != 0)) {
                lengthTimer++;
            }
        }
        period = (2048 - frequency.Value()) * 2;
        if (dmg && playing && (ticks == 2)) {
            const uint8_t position = (waveStep + 1) & 31;
            const uint8_t sampleByte = waveRam[position >> 1];
            if ((position >> 3) == 0) {
                waveRam[0] = sampleByte;
            } else if ((position >> 3) <= 3) {
                std::memcpy(&waveRam[0x00], &waveRam[(position >> 1) & 12], 4);
            }
        }
        waveStep = 0;
        ticks = period + 6;
        playing = true;
    }

    void TickLength() override {
        if (lengthEnabled && lengthTimer < 256) {
            lengthTimer++;
        }
    }

    void Tick() {
        if (!enabled) return;
        if (lengthEnabled && lengthTimer == 256) {
            enabled = false;
        }

        ticks--;
        if (ticks <= 0) {
            waveStep = (waveStep + 1) % 32;
            const uint8_t byte = waveRam[waveStep >> 1];
            if ((waveStep & 1) == 0) {
                sampleByte = byte >> 4;
            } else {
                sampleByte = byte & 0x0F;
            }
            alternateRead = true;
            ticks = period;
            if (dacEnabled) {
                currentOutput = static_cast<float>(sampleByte >> volumeShift);
            } else {
                currentOutput = 0.0f;
            }
        }
    }

    void HandleNR34Write(const uint8_t value, const uint8_t freqStep, const bool dmg) {
        frequency.WriteHigh(value);
        const bool oldEnabled = lengthEnabled;
        lengthEnabled = value & 0x40;
        if (!oldEnabled && lengthEnabled && (freqStep % 2 != 0)) {
            if (lengthTimer < 256) lengthTimer++;
            if (lengthTimer == 256 && !(value & 0x80)) enabled = false;
        }
        if (value & 0x80) {
            Trigger(freqStep, dmg);
        }
    }

    [[nodiscard]] uint8_t ReadByte(const uint16_t address) const {
        switch (address & 0xF) {
            case 0x0A: return (dacEnabled ? 0x80 : 0x00) | 0x7F;
            case 0x0B: return 0xFF;
            case 0x0C: return outputLevel << 5 | 0x9F;
            case 0x0D: return frequency.ReadLow();
            case 0x0E: return frequency.ReadHigh();
            default: throw UnreachableCodeException("Channel3::ReadByte unreachable code at address: " + std::to_string(address));
        }
    }

    void WriteByte(const uint16_t address, const uint8_t value, const uint8_t freqStep, const bool dmg) {
        switch (address & 0xF) {
            case 0x0A: dacEnabled = (value & 0x80) != 0;
                if (!dacEnabled) {
                    enabled = false;
                    playing = false;
                }
                break;
            case 0x0B: lengthTimer = value;
                break;
            case 0x0C: outputLevel = value >> 5 & 0x03;
                volumeShift = volumeShifts[outputLevel];
                break;
            case 0x0D: frequency.WriteLow(value);
                period = (2048 - frequency.Value()) * 2;
                break;
            case 0x0E: HandleNR34Write(value, freqStep, dmg);
                break;
            default: throw UnreachableCodeException("Channel3::WriteByte unreachable code at address: " + std::to_string(address));
        }
    }
};

struct Channel4 final : Channel {
    Length lengthTimer{};
    Envelope envelope{};
    Noise noise{};

    int32_t freqTimer{0};
    uint16_t lfsr{0xFFFF};
    float currentOutput{0.0f};
    uint8_t trigger{0};

    void Trigger(const uint8_t freqStep) {
        if (dacEnabled) enabled = true;
        dacEnabled = (envelope.initialVolume > 0 || envelope.direction);

        if (lengthTimer.lengthTimer == 64) {
            lengthTimer.lengthTimer = 0;
            if (lengthTimer.enabled && (freqStep % 2 != 0)) {
                lengthTimer.lengthTimer++;
            }
        }

        const int divisor = noise.clockDivider == 0 ? 8 : noise.clockDivider * 16;
        freqTimer = divisor << noise.clockShift;

        envelope.periodTimer = envelope.sweepPace ? envelope.sweepPace : 8;
        envelope.currentVolume = envelope.initialVolume;

        lfsr = 0xFFFF;
    }

    void TickLength() override {
        if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
            lengthTimer.lengthTimer++;
        }
    }

    void TickEnvelope() override {
        if (envelope.sweepPace > 0) {
            envelope.periodTimer--;
            if (envelope.periodTimer == 0) {
                envelope.periodTimer = envelope.sweepPace;
                if (envelope.direction && envelope.currentVolume < 15) {
                    envelope.currentVolume++;
                } else if (!envelope.direction && envelope.currentVolume > 0) {
                    envelope.currentVolume--;
                }
            }
        }
    }

    void TickLfsr() {
        const uint8_t xor_result = (lfsr & 1) ^ ((lfsr >> 1) & 1);
        lfsr >>= 1;
        lfsr |= (xor_result << 14);
        if (noise.lfsrWidth) {
            lfsr &= ~(1 << 6);
            lfsr |= (xor_result << 6);
        }
    }

    void Tick() {
        if (!enabled) return;
        freqTimer--;
        if (lengthTimer.enabled && lengthTimer.lengthTimer == 64) {
            enabled = false;
        }
        if (freqTimer <= 0) {
            const int divisor = (noise.clockDivider == 0) ? 8 : (noise.clockDivider * 16);
            freqTimer = divisor << noise.clockShift;
            TickLfsr();
            if (dacEnabled && (~lfsr & 1)) {
                currentOutput = envelope.currentVolume;
            } else {
                currentOutput = 0;
            }
        }
    }

    void HandleNR44Write(const uint8_t value, const uint8_t freqStep) {
        trigger = value >> 7 & 0x01;
        const bool oldEnabled = lengthTimer.enabled;
        lengthTimer.enabled = value & 0x40;
        if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
            if (lengthTimer.lengthTimer != 64) lengthTimer.lengthTimer++;
            if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
        }
        if (value & 0x80) Trigger(freqStep);
    }

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

    void WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep) {
        switch (address & 0xF) {
            case 0x0F: break;
            case 0x00: lengthTimer.Write(value, audioEnabled);
                break;
            case 0x01: envelope.Write(value);
                dacEnabled = (value & 0xF8) != 0;
                if (!dacEnabled) enabled = false;
                break;
            case 0x02: noise.Write(value);
                break;
            case 0x03: HandleNR44Write(value, freqStep);
                break;
            default: throw UnreachableCodeException("Channel4::WriteByte unreachable code at address: " + std::to_string(address));
        }
    }
};

class Audio {
    bool audioEnabled{false};
    bool dmg{false};
    int32_t cycleCounter{0};
    uint8_t frameSeqStep{0};

public:
    Channel1 ch1{};
    Channel2 ch2{};
    Channel3 ch3{};
    Channel4 ch4{};

    uint8_t nr50{};
    uint8_t nr51{};

    void SetDMG(const bool value) { dmg = value; }
    [[nodiscard]] bool IsDMG() const { return dmg; }

    // DIV-APU
    void TickFrameSequencer() {
        if (!audioEnabled) return;

        switch (frameSeqStep) {
            case 0: ch1.TickLength();
                ch2.TickLength();
                ch3.TickLength();
                ch4.TickLength();
                break;
            case 1: break;
            case 2: ch1.TickLength();
                ch1.TickSweep();
                ch2.TickLength();
                ch3.TickLength();
                ch4.TickLength();
                break;
            case 3: break;
            case 4: ch1.TickLength();
                ch2.TickLength();
                ch3.TickLength();
                ch4.TickLength();
                break;
            case 5: break;
            case 6: ch1.TickLength();
                ch1.TickSweep();
                ch2.TickLength();
                ch3.TickLength();
                ch4.TickLength();
                break;
            case 7: ch1.TickEnvelope();
                ch2.TickEnvelope();
                ch4.TickEnvelope();
                break;
            default: throw UnreachableCodeException("Audio::TickFrameSequencer unreachable code at step: " + std::to_string(frameSeqStep));
        }

        frameSeqStep = (frameSeqStep + 1) % 8;
    }

    void Tick() {
        if (!audioEnabled) return;
        ch3.alternateRead = false;
        ch1.Tick();
        ch2.Tick();
        ch3.Tick();
        ch4.Tick();
    }

    void WriteAudioControl(const uint8_t value) {
        const bool wasEnabled = audioEnabled;
        audioEnabled = (value & 0x80) != 0;

        if (wasEnabled && !audioEnabled) {
            // Turning APU off clears all registers except for Wave RAM on DMG
            audioEnabled = false;
            ch1 = {};
            ch2 = {};
            ch3.Reset();
            ch4 = {};
            nr50 = 0;
            nr51 = 0;
        } else if (!wasEnabled && audioEnabled) {
            // Turning APU on resets frame sequencer
            frameSeqStep = 0;
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
            case 0xFF30 ... 0xFF3F: return ch3.ReadWaveRam(address, dmg);
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
            case 0xFF10 ... 0xFF14: ch1.WriteByte(address, value, audioEnabled, frameSeqStep);
                break;
            case 0xFF15 ... 0xFF19: ch2.WriteByte(address, value, audioEnabled, frameSeqStep);
                break;
            case 0xFF1A ... 0xFF1E: ch3.WriteByte(address, value, frameSeqStep, dmg);
                break;
            case 0xFF1F ... 0xFF23: ch4.WriteByte(address, value, audioEnabled, frameSeqStep);
                break;
            case 0xFF24: nr50 = value;
                break;
            case 0xFF25: nr51 = value;
                break;
            case 0xFF26: WriteAudioControl(value);
                break;
            case 0xFF30 ... 0xFF3F: ch3.WriteWaveRam(address, value, dmg);
                break;
            default: break;
        }
    }
};
