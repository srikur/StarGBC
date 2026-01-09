#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Common.h"

class Audio;

static constexpr int AUDIO_SAMPLE_RATE = 44100;
static constexpr int AUDIO_BUFFER_SIZE = 2048;
static constexpr float APU_CLOCK_RATE = 4213440.0f;
static constexpr float CYCLES_PER_SAMPLE = APU_CLOCK_RATE / AUDIO_SAMPLE_RATE;

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

    void Trigger(uint8_t freqStep);

    void TickLength() override;

    void TickSweep();

    void TickEnvelope() override;

    uint16_t CalculateSweep();

    void Tick();

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    void HandleNR14Write(uint8_t value, uint8_t freqStep);

    void WriteByte(uint16_t address, uint8_t value, bool audioEnabled, uint8_t freqStep);

    [[nodiscard]] uint8_t GetDigitalOutput() const;
};

struct Channel2 final : Channel {
    Length lengthTimer{};
    Envelope envelope{};
    Frequency frequency{};
    int32_t freqTimer{0};
    uint8_t dutyStep{0};
    float currentOutput{0.0f};

    void Trigger(uint8_t freqStep);

    void TickLength() override;

    void TickEnvelope() override;

    void Tick();

    void HandleNR24Write(uint8_t value, uint8_t freqStep);

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    void WriteByte(uint16_t address, uint8_t value, bool audioEnabled, uint8_t freqStep);

    [[nodiscard]] uint8_t GetDigitalOutput() const;
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
    uint32_t period{0};
    uint8_t waveStep{0};
    float currentOutput{0.0f};

    std::array<uint8_t, 0x10> waveRam{};

    [[nodiscard]] uint8_t ReadWaveRam(uint16_t address, bool dmg) const;

    void WriteWaveRam(uint16_t address, uint8_t value, bool dmg);

    void Reset();

    void Trigger(uint8_t freqStep, bool dmg);

    void TickLength() override;

    void Tick();

    void HandleNR34Write(uint8_t value, uint8_t freqStep, bool dmg);

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    void WriteByte(uint16_t address, uint8_t value, uint8_t freqStep, bool dmg);

    [[nodiscard]] uint8_t GetDigitalOutput() const;
};

struct Channel4 final : Channel {
    Length lengthTimer{};
    Envelope envelope{};
    Noise noise{};

    int32_t freqTimer{0};
    uint16_t lfsr{0xFFFF};
    float currentOutput{0.0f};
    uint8_t trigger{0};

    void Trigger(uint8_t freqStep);

    void TickLength() override;

    void TickEnvelope() override;

    void TickLfsr();

    void Tick();

    void HandleNR44Write(uint8_t value, uint8_t freqStep);

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    void WriteByte(uint16_t address, uint8_t value, bool audioEnabled, uint8_t freqStep);

    [[nodiscard]] uint8_t GetDigitalOutput() const;
};

class Audio {
    bool audioEnabled{false};
    bool dmg{false};
    int32_t cycleCounter{0};
    uint8_t frameSeqStep{0};

    std::vector<float> sampleBuffer{};
    size_t bufferWritePos{0};
    size_t bufferReadPos{0};
    size_t samplesAvailable{0};
    float sampleCounter{0.0f};

    float highPassLeft{0.0f};
    float highPassRight{0.0f};
    static constexpr float HIGH_PASS_FACTOR = 0.999f;

public:
    Audio() { sampleBuffer.resize(AUDIO_BUFFER_SIZE * 2); } // *2 for stereo

    Channel1 ch1{};
    Channel2 ch2{};
    Channel3 ch3{};
    Channel4 ch4{};

    uint8_t nr50{};
    uint8_t nr51{};

    void SetDMG(const bool value) { dmg = value; }
    [[nodiscard]] bool IsDMG() const { return dmg; }

    // DIV-APU
    void TickFrameSequencer();

    void Tick();

    void WriteAudioControl(uint8_t value);

    [[nodiscard]] uint8_t ReadAudioControl() const;

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    void WriteByte(uint16_t address, uint8_t value);

    [[nodiscard]] uint8_t ReadPCM12() const;

    [[nodiscard]] uint8_t ReadPCM34() const;

    void GenerateSample();

    [[nodiscard]] size_t GetSamplesAvailable() const { return samplesAvailable; }

    size_t ReadSamples(float *output, size_t numSamples);

    void ClearBuffer();
};
