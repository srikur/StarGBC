#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>

constexpr double GB_CPU_FREQ = 4'194'304.0;
constexpr uint32_t FRAME_SEQ_STEP = 8'192;
constexpr uint32_t ENV_FREQ = 64;
constexpr uint32_t LEN_FREQ = 256;
constexpr uint32_t SWEEP_FREQ = 128;

struct StereoSample {
    int16_t l{}, r{};
};

struct Envelope {
    uint8_t initialVol = 0; // bits 7-4 of NRx2
    bool increase = false; // bit 3  of NRx2
    uint8_t period = 0; // bits 2-0 of NRx2 (0→8)
    uint8_t vol = 0; // current output volume (0-15)

    uint32_t timer = 0;

    void reload(uint8_t n) {
        initialVol = n >> 4;
        increase = n & 0x08;
        period = (n & 0x07) ? (n & 0x07) : 8;
        vol = initialVol;
        timer = 0;
    }

    void tick() {
        if (!period) return;
        if (++timer >= period) {
            timer = 0;
            if (increase && vol < 15) ++vol;
            else if (!increase && vol > 0) --vol;
        }
    }
};

constexpr size_t SAMPLE_FIFO_CAP = 4096;

class SampleFifo {
public:
    bool push(const StereoSample &s) {
        const size_t w = (write + 1) % SAMPLE_FIFO_CAP;
        if (w == read.load(std::memory_order_acquire))
            return false;
        data[write] = s;
        write = w;
        return true;
    }

    bool pop(StereoSample &s) {
        const size_t r = read.load(std::memory_order_acquire);
        if (r == write) return false;
        s = data[r];
        read.store((r + 1) % SAMPLE_FIFO_CAP,
                   std::memory_order_release);
        return true;
    }

    size_t size() const {
        const size_t r = read.load(std::memory_order_acquire);
        return (write + SAMPLE_FIFO_CAP - r) % SAMPLE_FIFO_CAP;
    }

private:
    std::array<StereoSample, SAMPLE_FIFO_CAP> data{};
    std::atomic_size_t read{0};
    size_t write{0};
};


struct LengthCounter {
    uint16_t length = 0;
    bool enabled = false;

    void tick() {
        if (enabled && length) {
            --length;
        }
    }

    [[nodiscard]] bool isZero() const { return length == 0 && enabled; }
};

struct Sweep {
    uint8_t period = 0; // bits 6-4 of NR10
    bool negate = false; // bit 3
    uint8_t shift = 0; // bits 2-0
    uint32_t timer = 0;
    bool enabled = false;
    uint16_t shadow = 0;

    void reload(const uint8_t n, const uint16_t freq) {
        period = (n >> 4) & 0x07;
        negate = n & 0x08;
        shift = n & 0x07;
        timer = period ? period : 8;
        enabled = (period || shift);
        shadow = freq;
    }

    std::pair<uint16_t, bool> calc(bool writeBack) {
        if (!shift) return {shadow, false};
        const uint16_t delta = shadow >> shift;
        uint16_t nf = negate ? shadow - delta : shadow + delta;
        if (nf > 2047) {
            enabled = false;
            return {nf, true};
        }
        if (writeBack) shadow = nf;
        return {nf, false};
    }

    bool tick(uint16_t &freqHiLo) {
        if (!enabled) return false;
        if (--timer == 0) {
            timer = period ? period : 8;
            auto [nf,ov] = calc(true);
            if (ov) return true;
            freqHiLo = nf;
            calc(false);
        }
        return false;
    }
};

class Pulse {
public:
    void writeReg(uint16_t a, uint8_t v, bool ch1 = false);

    [[nodiscard]] uint8_t readReg(uint16_t a) const;

    void trigger(bool ch1 = false);

    void tickLen(); // 256 Hz
    void tickEnv(); // 64 Hz
    bool tickSweep(); // 128 Hz (CH1 only)
    void tickTimer(uint32_t cycles);

    [[nodiscard]] uint8_t output() const;

    [[nodiscard]] bool dacEnabled() const {
        return (nr2 & 0xF8) != 0;
    }

    [[nodiscard]] bool active() const { return enabled; }

private:
    uint8_t nr0{}, nr1{}, nr2{}, nr3{}, nr4{};

    Envelope env{};
    LengthCounter len{};
    Sweep sweep{};

    uint16_t freqTimer = 0;
    uint8_t dutyStep = 0;
    bool enabled = false;
};

class Wave {
public:
    void writeReg(uint16_t a, uint8_t v);

    [[nodiscard]] uint8_t readReg(uint16_t a) const;

    void writeWaveRAM(uint16_t a, uint8_t v);

    [[nodiscard]] uint8_t readWaveRAM(uint16_t a) const;

    void trigger();

    void tickLen();
    void tickTimer(uint32_t cycles);

    [[nodiscard]] uint8_t output() const;

    [[nodiscard]] bool dacEnabled() const { return nr0 & 0x80; }
    [[nodiscard]] bool active() const { return enabled; }

private:
    uint8_t nr0{}, nr1{}, nr2{}, nr3{}, nr4{};
    std::array<uint8_t, 16> waveRam{}; // 32 samples (4-bit nibbles)

    LengthCounter len{};
    uint16_t freqTimer = 0;
    uint8_t sampleIdx = 0; // 0-31
    uint8_t latch = 0; // sample buffer
    bool enabled = false;
};

class Noise {
public:
    void writeReg(uint16_t a, uint8_t v);

    [[nodiscard]] uint8_t readReg(uint16_t a) const;

    void trigger();

    void tickLen(); // 256 Hz
    void tickEnv(); // 64 Hz
    void tickTimer(uint32_t cycles);

    [[nodiscard]] uint8_t output() const;

    [[nodiscard]] bool dacEnabled() const {
        return (nr2 & 0xF8) != 0;
    }

    [[nodiscard]] bool active() const { return enabled; }

private:
    uint8_t nr1{}, nr2{}, nr3{}, nr4{};
    Envelope env{};
    LengthCounter len{};

    uint16_t lfsr = 0x7FFF;
    uint16_t freqTimer = 0;
    bool enabled = false;
};

class Audio {
public:
    explicit Audio(uint32_t sampleRate = 48'000);

    [[nodiscard]] std::optional<StereoSample> Tick(uint32_t cycles);

    [[nodiscard]] uint8_t ReadByte(uint16_t addr) const;

    void WriteByte(uint16_t addr, uint8_t value);

    SampleFifo gSampleFifo;

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(&nr50), sizeof(nr50));
            stateFile.write(reinterpret_cast<const char *>(&nr51), sizeof(nr51));
            stateFile.write(reinterpret_cast<const char *>(&nr52), sizeof(nr52));
            stateFile.write(reinterpret_cast<const char *>(&frameCounter), sizeof(frameCounter));
            stateFile.write(reinterpret_cast<const char *>(&samplePeriod), sizeof(samplePeriod));
            stateFile.write(reinterpret_cast<const char *>(&sampleCounter), sizeof(sampleCounter));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error saving Audio state: " << e.what() << std::endl;
            return false;
        }
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.read(reinterpret_cast<char *>(&nr50), sizeof(nr50));
            stateFile.read(reinterpret_cast<char *>(&nr51), sizeof(nr51));
            stateFile.read(reinterpret_cast<char *>(&nr52), sizeof(nr52));
            stateFile.read(reinterpret_cast<char *>(&frameCounter), sizeof(frameCounter));
            stateFile.read(reinterpret_cast<char *>(&samplePeriod), sizeof(samplePeriod));
            stateFile.read(reinterpret_cast<char *>(&sampleCounter), sizeof(sampleCounter));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading Audio state: " << e.what() << std::endl;
            return false;
        }
    }

private:
    Pulse ch1, ch2;
    Wave ch3;
    Noise ch4;

    uint8_t nr50 = 0; // SO2/1 output level / Vin routing
    uint8_t nr51 = 0; // channel → output routing
    uint8_t nr52 = 0xF0; // master on/off + channel act flags

    uint32_t frameCounter = 0;
    uint32_t samplePeriod = 0; // CPU cycles per audio sample
    uint32_t sampleCounter = 0;

    double capL = 0.0, capR = 0.0;
    double capCharge = 0.0;

    [[nodiscard]] uint8_t mixChannelOutput(uint8_t chanMask) const;

    [[nodiscard]] StereoSample mix() const;

    [[nodiscard]] int16_t highPass(double &cap, double in) const;

    void updateNR52();
};
