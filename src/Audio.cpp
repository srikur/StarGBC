#include "Audio.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr std::array<uint8_t, 4> DUTY_TABLE = {
        0b00000001, // 12.5%
        0b10000001, // 25%
        0b10000111, // 50%
        0b01111110 // 75%
    };

    uint16_t readFreq(const uint8_t lo, const uint8_t hi) {
        return static_cast<uint16_t>((hi & 0x07) << 8 | lo);
    }

    void writeFreq(uint8_t &lo, uint8_t &hi, const uint16_t f) {
        lo = f & 0xFF;
        hi = (hi & ~0x07) | ((f >> 8) & 0x07);
    }
}

void Pulse::writeReg(const uint16_t a, const uint8_t v, const bool ch1) {
    switch (a & 0xF) {
        case 0: nr0 = v & 0xFF;
            break; // NR10 or NR20
        case 1: nr1 = v;
            len.length = 64 - (v & 0x3F);
            break;
        case 2: nr2 = v;
            env.reload(v);
            break;
        case 3: nr3 = v;
            break;
        case 4:
            nr4 = v;
            len.enabled = v & 0x40;
            if (v & 0x80) trigger(ch1);
            break;
        default: break;
    }
}

uint8_t Pulse::readReg(const uint16_t a) const {
    switch (a & 0xF) {
        case 0: return nr0 | 0x80; // sweep bits unused on CH2
        case 1: return nr1;
        case 2: return nr2;
        case 3: return nr3;
        case 4: return nr4 | 0xBF;
        default: return 0xFF;
    }
}

void Pulse::trigger(const bool ch1) {
    enabled = dacEnabled();
cur:
    len.length = len.length ? len.length : 64;
    freqTimer = (2048 - readFreq(nr3, nr4)) * 4;
    dutyStep = 0;
    env.vol = env.initialVol;
    env.timer = 0;

    if (ch1) {
        sweep.reload(nr0, readFreq(nr3, nr4));
        if (sweep.calc(false).second) enabled = false;
    }
}

void Pulse::tickLen() {
    len.tick();
    if (len.isZero()) enabled = false;
}

void Pulse::tickEnv() { env.tick(); }
bool Pulse::tickSweep() { return sweep.tick(*reinterpret_cast<uint16_t *>(&nr3)); }

void Pulse::tickTimer(uint32_t cycles) {
    while (cycles--) {
        if (--freqTimer == 0) {
            freqTimer = (2048 - readFreq(nr3, nr4)) * 4;
            dutyStep = (dutyStep + 1) & 7;
        }
    }
}

uint8_t Pulse::output() const {
    if (!enabled) return 0;
    const uint8_t dutyMask = DUTY_TABLE[(nr1 >> 6) & 3];
    const uint8_t bit = (dutyMask >> dutyStep) & 1;
    return bit ? env.vol : 0;
}

void Wave::writeReg(const uint16_t a, const uint8_t v) {
    switch (a & 0xF) {
        case 0: nr0 = v & 0xDF;
            break;
        case 1: nr1 = v;
            len.length = 256 - v;
            break;
        case 2: nr2 = v & 0x60;
            break;
        case 3: nr3 = v;
            break;
        case 4:
            nr4 = v;
            len.enabled = v & 0x40;
            if (v & 0x80) trigger();
            break;
        default: break;
    }
}

uint8_t Wave::readReg(uint16_t a) const {
    switch (a & 0xF) {
        case 0: return nr0 | 0x7F;
        case 1: return 0xFF;
        case 2: return nr2 | 0x9F;
        case 3: return 0xFF;
        case 4: return nr4 | 0xBF;
        default: return 0xFF;
    }
}

void Wave::writeWaveRAM(const uint16_t a, const uint8_t v) {
    waveRam[(a - 0xFF30) & 0xF] = v;
}

uint8_t Wave::readWaveRAM(const uint16_t a) const {
    return waveRam[(a - 0xFF30) & 0xF];
}

void Wave::trigger() {
    enabled = dacEnabled();
    len.length = len.length ? len.length : 256;
    freqTimer = (2048 - readFreq(nr3, nr4)) * 2;
    sampleIdx = 0;
    latch = 0;
}

void Wave::tickLen() {
    len.tick();
    if (len.isZero()) enabled = false;
}

void Wave::tickTimer(uint32_t cycles) {
    while (cycles--) {
        if (--freqTimer == 0) {
            freqTimer = (2048 - readFreq(nr3, nr4)) * 2;
            sampleIdx = (sampleIdx + 1) & 31;
            uint8_t byte = waveRam[sampleIdx >> 1];
            latch = (sampleIdx & 1) ? (byte & 0x0F) : (byte >> 4);
        }
    }
}

uint8_t Wave::output() const {
    if (!enabled) return 0;
    const uint8_t shift = (nr2 >> 5) & 0x03; // 00:mute, 01:100%, 10:50%, 11:25%
    if (shift == 0) return 0;
    return latch >> (shift - 1);
}

void Noise::writeReg(uint16_t a, uint8_t v) {
    switch (a & 0xF) {
        case 1: nr1 = v;
            len.length = 64 - (v & 0x3F);
            break;
        case 2: nr2 = v;
            env.reload(v);
            break;
        case 3: nr3 = v;
            break;
        case 4:
            nr4 = v;
            len.enabled = v & 0x40;
            if (v & 0x80) trigger();
            break;
        default: break;
    }
}

uint8_t Noise::readReg(uint16_t a) const {
    switch (a & 0xF) {
        case 1: return nr1;
        case 2: return nr2;
        case 3: return 0xFF;
        case 4: return nr4 | 0xBF;
        default: return 0xFF;
    }
}

void Noise::trigger() {
    enabled = dacEnabled();
    len.length = len.length ? len.length : 64;
    env.vol = env.initialVol;
    env.timer = 0;
    lfsr = 0x7FFF;
    const uint8_t r = nr3 & 0x07;
    const uint8_t s = (nr3 >> 4) & 0x0F;
    static constexpr uint8_t DIV_TAB[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    freqTimer = (DIV_TAB[r] << s);
}

void Noise::tickLen() {
    len.tick();
    if (len.isZero()) enabled = false;
}

void Noise::tickEnv() { env.tick(); }

void Noise::tickTimer(uint32_t cycles) {
    while (cycles--) {
        if (--freqTimer == 0) {
            const uint8_t r = nr3 & 0x07;
            const uint8_t s = (nr3 >> 4) & 0x0F;
            static constexpr uint8_t DIV_TAB[8] = {8, 16, 32, 48, 64, 80, 96, 112};
            freqTimer = (DIV_TAB[r] << s);

            const uint8_t xorBit = ((lfsr ^ (lfsr >> 1)) & 1);
            lfsr = (lfsr >> 1) | (xorBit << 14);
            if (nr3 & 0x08) lfsr = (lfsr & ~(1 << 6)) | (xorBit << 6);
        }
    }
}

uint8_t Noise::output() const {
    if (!enabled) return 0;
    return (~lfsr & 1) ? env.vol : 0;
}

Audio::Audio(const uint32_t sampleRate) {
    samplePeriod = static_cast<uint32_t>(GB_CPU_FREQ / sampleRate);
    sampleCounter = samplePeriod;
    capCharge = std::pow(0.999958, GB_CPU_FREQ / sampleRate);
}

uint8_t Audio::ReadByte(const uint16_t addr) const {
    if (0xFF10 <= addr && addr <= 0xFF14) return ch1.readReg(addr);
    if (0xFF16 <= addr && addr <= 0xFF19) return ch2.readReg(addr);
    if (0xFF1A <= addr && addr <= 0xFF1E) return ch3.readReg(addr);
    if (0xFF20 <= addr && addr <= 0xFF23) return ch4.readReg(addr);

    switch (addr) {
        case 0xFF24: return nr50;
        case 0xFF25: return nr51;
        case 0xFF26: return nr52 | (ch1.active() << 0) | (ch2.active() << 1)
                            | (ch3.active() << 2) | (ch4.active() << 3);
        case 0xFF30 ... 0xFF3F: return ch3.readWaveRAM(addr);
        default: return 0xFF;
    }
}

void Audio::WriteByte(const uint16_t addr, const uint8_t value) {
    if (0xFF10 <= addr && addr <= 0xFF14) ch1.writeReg(addr, value, true);
    else if (0xFF16 <= addr && addr <= 0xFF19) ch2.writeReg(addr, value, false);
    else if (0xFF1A <= addr && addr <= 0xFF1E) ch3.writeReg(addr, value);
    else if (0xFF20 <= addr && addr <= 0xFF23) ch4.writeReg(addr, value);
    else if (0xFF30 <= addr && addr <= 0xFF3F) ch3.writeWaveRAM(addr, value);
    else {
        switch (addr) {
            case 0xFF24: nr50 = value;
                break;
            case 0xFF25: nr51 = value;
                break;
            case 0xFF26:
                if (!(value & 0x80)) {
                    nr52 = 0;
                    ch1 = Pulse{};
                    ch2 = Pulse{};
                    ch3 = Wave{};
                    ch4 = Noise{};
                } else {
                    nr52 |= 0x80;
                }
                break;
            default: break;
        }
    }
}

std::optional<StereoSample> Audio::Tick(const uint32_t cycles) {
    ch1.tickTimer(cycles);
    ch2.tickTimer(cycles);
    ch3.tickTimer(cycles);
    ch4.tickTimer(cycles);

    frameCounter += cycles;
    while (frameCounter >= FRAME_SEQ_STEP) {
        frameCounter -= FRAME_SEQ_STEP;
        static uint8_t step = 0;
        switch (step) {
            case 0:
            case 4:
                ch1.tickLen();
                ch2.tickLen();
                ch3.tickLen();
                ch4.tickLen();
                break;
            case 2:
            case 6:
                ch1.tickLen();
                ch2.tickLen();
                ch3.tickLen();
                ch4.tickLen();
                ch1.tickSweep(); // CH1 only
                break;
            case 7:
                ch1.tickEnv();
                ch2.tickEnv();
                ch4.tickEnv();
                break;
            default: break;
        }
        step = (step + 1) & 7;
        updateNR52();
    }

    if ((sampleCounter -= cycles) > 0) return std::nullopt;
    sampleCounter += samplePeriod;

    StereoSample s = mix();

    s.l = highPass(capL, s.l);
    s.r = highPass(capR, s.r);

    return s;
}

void Audio::updateNR52() {
    nr52 = 0x80 | (ch1.active() << 0) | (ch2.active() << 1) | (ch3.active() << 2) | (ch4.active() << 3);
}

uint8_t Audio::mixChannelOutput(const uint8_t mask) const {
    uint8_t out = 0;
    if (mask & 0x01) out += ch1.output();
    if (mask & 0x02) out += ch2.output();
    if (mask & 0x04) out += ch3.output();
    if (mask & 0x08) out += ch4.output();
    return out;
}

StereoSample Audio::mix() const {
    const uint8_t left = mixChannelOutput(nr51 & 0x0F);
    const uint8_t right = mixChannelOutput(nr51 >> 4);

    const uint8_t volL = (nr50 >> 4) & 0x07;
    const uint8_t volR = (nr50) & 0x07;

    constexpr double SCALE = 32767.0 / (4.0 * 15.0);
    const auto l = static_cast<int16_t>((left * volL) * SCALE);
    const auto r = static_cast<int16_t>((right * volR) * SCALE);

    return {l, r};
}

int16_t Audio::highPass(double &cap, const double in) const {
    const double out = in - cap;
    cap = in - out * capCharge;
    return static_cast<int16_t>(std::clamp(out, -32768.0, 32767.0));
}
