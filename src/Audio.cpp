#include "Audio.h"
#include <cmath>
#include <set>
#include <string>

void Audio::TickFrameSequencer() {
    if (!audioEnabled) return;
    if (skipNextFrameSeqTick) {
        skipNextFrameSeqTick = false;
        return;
    }

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

void Audio::Tick() {
    tickCounter++;
    if (audioEnabled) {
        ch3.alternateRead = false;
        ch1.Tick();
        ch2.Tick();
        ch3.Tick();
        ch4.Tick();
    }
    GenerateSample();
}

void Audio::WriteAudioControl(const uint8_t value, const bool divBit4High) {
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
        skipNextFrameSeqTick = false;
    } else if (!wasEnabled && audioEnabled) {
        // Turning APU on resets frame sequencer
        frameSeqStep = 0;
        // Reset tick counter for phase tracking
        tickCounter = 0;
        // If DIV bit 4 is high when APU is enabled, skip the first DIV-APU event
        skipNextFrameSeqTick = divBit4High;
    }
}

uint8_t Audio::ReadAudioControl() const {
    return (audioEnabled ? 0x80 : 0x00) | (ch4.enabled << 3) | (ch3.enabled << 2) | (ch2.enabled << 1) | (ch1.enabled << 0) | 0x70;
}

uint8_t Audio::ReadByte(const uint16_t address) const {
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

void Audio::WriteByte(const uint16_t address, const uint8_t value, const bool divBit4High) {
    static const std::set<uint16_t> allowedAddresses = {
        0xFF26, 0xFF11, 0xFF16, 0xFF1B, 0xFF20
    };
    if (!audioEnabled && address != 0xFF26 && (!dmg || !allowedAddresses.contains(address))) {
        return;
    }
    switch (address) {
        case 0xFF10 ... 0xFF14: ch1.WriteByte(address, value, audioEnabled, frameSeqStep, tickCounter);
            break;
        case 0xFF15 ... 0xFF19: ch2.WriteByte(address, value, audioEnabled, frameSeqStep, tickCounter);
            break;
        case 0xFF1A ... 0xFF1E: ch3.WriteByte(address, value, frameSeqStep, dmg);
            break;
        case 0xFF1F ... 0xFF23: ch4.WriteByte(address, value, audioEnabled, frameSeqStep);
            break;
        case 0xFF24: nr50 = value;
            break;
        case 0xFF25: nr51 = value;
            break;
        case 0xFF26: WriteAudioControl(value, divBit4High);
            break;
        case 0xFF30 ... 0xFF3F: ch3.WriteWaveRam(address, value, dmg);
            break;
        default: break;
    }
}

uint8_t Audio::ReadPCM12() const {
    return (ch2.GetDigitalOutput() << 4) | (ch1.GetDigitalOutput() & 0x0F);
}

uint8_t Audio::ReadPCM34() const {
    return (ch4.GetDigitalOutput() << 4) | (ch3.GetDigitalOutput() & 0x0F);
}

void Channel1::Trigger(const uint8_t freqStep, const uint32_t tickCounter) {
    if (dacEnabled) enabled = true;
    dacEnabled = (envelope.initialVolume > 0 || envelope.direction);

    if (lengthTimer.lengthTimer == 64) {
        lengthTimer.lengthTimer = 0;
        if (lengthTimer.enabled && (freqStep % 2 != 0)) {
            lengthTimer.lengthTimer++;
        }
    }

    if (freqTimer == 0) {
        dutyStep = (dutyStep + 1) & 7;
    }

    // Samesuite -- Channel 1 align
    const int32_t phaseDelay = (tickCounter & 2) ? 14 : 12;
    freqTimer = (2048 - frequency.Value()) * 4 + phaseDelay;

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

void Channel1::TickLength() {
    if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
        lengthTimer.lengthTimer++;
    }
}

void Channel1::TickSweep() {
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

void Channel1::TickEnvelope() {
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

uint16_t Channel1::CalculateSweep() {
    uint16_t newFreq = sweep.shadowFreq >> sweep.step;
    if (sweep.direction) {
        newFreq = sweep.shadowFreq - newFreq;
        sweep.subtractionCalculationMade = true;
    } else {
        newFreq = sweep.shadowFreq + newFreq;
    }
    return newFreq;
}

void Channel1::Tick() {
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

[[nodiscard]] uint8_t Channel1::ReadByte(const uint16_t address) const {
    switch (address & 0xF) {
        case 0x00: return sweep.Value();
        case 0x01: return lengthTimer.Value();
        case 0x02: return envelope.Value();
        case 0x03: return frequency.ReadLow();
        case 0x04: return frequency.ReadHigh();
        default: throw UnreachableCodeException("Channel1::ReadByte unreachable code at address: " + std::to_string(address));
    }
}

void Channel1::HandleNR14Write(const uint8_t value, const uint8_t freqStep, const uint32_t tickCounter) {
    frequency.WriteHigh(value);
    const bool oldEnabled = lengthTimer.enabled;
    lengthTimer.enabled = value & 0x40;
    if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
        if (lengthTimer.lengthTimer < 64) {
            lengthTimer.lengthTimer++;
        }
        if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
    }
    if (value & 0x80) Trigger(freqStep, tickCounter);
}

void Channel1::WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep, const uint32_t tickCounter) {
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
        case 0x04: HandleNR14Write(value, freqStep, tickCounter);
            break;
        default: throw UnreachableCodeException("Channel1::WriteByte unreachable code at address: " + std::to_string(address));
    }
}

uint8_t Channel1::GetDigitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return DUTY_PATTERNS[lengthTimer.dutyCycle][dutyStep] * envelope.currentVolume;
}

void Channel2::Trigger(const uint8_t freqStep, const uint32_t tickCounter) {
    if (dacEnabled) enabled = true;
    dacEnabled = (envelope.initialVolume > 0 || envelope.direction);

    if (lengthTimer.lengthTimer == 64) {
        lengthTimer.lengthTimer = 0;
        if (lengthTimer.enabled && (freqStep % 2 != 0)) {
            lengthTimer.lengthTimer++;
        }
    }

    if (freqTimer == 0) {
        dutyStep = (dutyStep + 1) & 7;
    }

    // Samesuite -- Channel 3 align
    const int32_t phaseDelay = (tickCounter & 2) ? 14 : 12;
    freqTimer = (2048 - frequency.Value()) * 4 + phaseDelay;

    envelope.periodTimer = envelope.sweepPace ? envelope.sweepPace : 8;
    envelope.currentVolume = envelope.initialVolume;
}

void Channel2::TickLength() {
    if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
        lengthTimer.lengthTimer++;
    }
}

void Channel2::TickEnvelope() {
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

void Channel2::Tick() {
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

void Channel2::HandleNR24Write(const uint8_t value, const uint8_t freqStep, const uint32_t tickCounter) {
    frequency.WriteHigh(value);
    const bool oldEnabled = lengthTimer.enabled;
    lengthTimer.enabled = value & 0x40;
    if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
        if (lengthTimer.lengthTimer < 64) lengthTimer.lengthTimer++;
        if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
    }
    if (value & 0x80) Trigger(freqStep, tickCounter);
}

[[nodiscard]] uint8_t Channel2::ReadByte(const uint16_t address) const {
    switch (address & 0xF) {
        case 0x05: return 0xFF;
        case 0x06: return lengthTimer.Value();
        case 0x07: return envelope.Value();
        case 0x08: return frequency.ReadLow();
        case 0x09: return frequency.ReadHigh();
        default: throw UnreachableCodeException("Channel2::ReadByte unreachable code at address: " + std::to_string(address));
    }
}

void Channel2::WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep, const uint32_t tickCounter) {
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
        case 0x09: HandleNR24Write(value, freqStep, tickCounter);
            break;
        default: throw UnreachableCodeException("Channel2::WriteByte unreachable code at address: " + std::to_string(address));
    }
}

uint8_t Channel2::GetDigitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return DUTY_PATTERNS[lengthTimer.dutyCycle][dutyStep] * envelope.currentVolume;
}


uint8_t Channel3::ReadWaveRam(const uint16_t address, const bool dmg) const {
    return !enabled ? waveRam[address - 0xFF30] : !dmg || alternateRead ? waveRam[waveStep >> 1] : 0xFF;
}

void Channel3::WriteWaveRam(const uint16_t address, const uint8_t value, const bool dmg) {
    if (!enabled) {
        waveRam[address - 0xFF30] = value;
    } else if (!dmg || alternateRead) {
        waveRam[waveStep >> 1] = value;
    }
}

void Channel3::Reset() {
    lengthTimer = outputLevel = frequency.value = waveStep = period = sampleByte = 0;
    currentOutput = 0.0f;
    lengthEnabled = enabled = dacEnabled = playing = alternateRead = false;
}

void Channel3::Trigger(const uint8_t freqStep, const bool dmg) {
    if (dacEnabled) enabled = true;
    dacEnabled = true;

    if (lengthTimer == 256) {
        lengthTimer = 0;
        if (lengthEnabled && (freqStep % 2 != 0)) {
            lengthTimer++;
        }
    }
    if (dmg && playing && (period == 2)) {
        const uint8_t position = (waveStep + 1) & 31;
        const uint8_t byte = waveRam[position >> 1];
        if ((position >> 3) == 0) {
            waveRam[0] = byte;
        } else if ((position >> 3) <= 3) {
            std::memcpy(&waveRam[0x00], &waveRam[(position >> 1) & 12], 4);
        }
    }
    waveStep = 0;
    period = (2048 - frequency.Value()) * 2 + 6;
    playing = true;
}

void Channel3::TickLength() {
    if (lengthEnabled && lengthTimer < 256) {
        lengthTimer++;
    }
}

void Channel3::Tick() {
    if (!enabled) return;
    if (lengthEnabled && lengthTimer == 256) {
        enabled = false;
    }

    period--;
    if (period <= 0) {
        waveStep = (waveStep + 1) % 32;
        const uint8_t byte = waveRam[waveStep >> 1];
        if ((waveStep & 1) == 0) {
            sampleByte = byte >> 4;
        } else {
            sampleByte = byte & 0x0F;
        }
        alternateRead = true;
        period = (2048 - frequency.Value()) * 2;
        if (dacEnabled) {
            currentOutput = static_cast<float>(sampleByte >> volumeShift);
        } else {
            currentOutput = 0.0f;
        }
    }
}

void Channel3::HandleNR34Write(const uint8_t value, const uint8_t freqStep, const bool dmg) {
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

[[nodiscard]] uint8_t Channel3::ReadByte(const uint16_t address) const {
    switch (address & 0xF) {
        case 0x0A: return (dacEnabled ? 0x80 : 0x00) | 0x7F;
        case 0x0B: return 0xFF;
        case 0x0C: return outputLevel << 5 | 0x9F;
        case 0x0D: return frequency.ReadLow();
        case 0x0E: return frequency.ReadHigh();
        default: throw UnreachableCodeException("Channel3::ReadByte unreachable code at address: " + std::to_string(address));
    }
}

void Channel3::WriteByte(const uint16_t address, const uint8_t value, const uint8_t freqStep, const bool dmg) {
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
            break;
        case 0x0E: HandleNR34Write(value, freqStep, dmg);
            break;
        default: throw UnreachableCodeException("Channel3::WriteByte unreachable code at address: " + std::to_string(address));
    }
}

uint8_t Channel3::GetDigitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return sampleByte >> volumeShift;
}

void Channel4::Trigger(const uint8_t freqStep) {
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

void Channel4::TickLength() {
    if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
        lengthTimer.lengthTimer++;
    }
}

void Channel4::TickEnvelope() {
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

void Channel4::TickLfsr() {
    const uint8_t xor_result = (lfsr & 1) ^ ((lfsr >> 1) & 1);
    lfsr >>= 1;
    lfsr |= (xor_result << 14);
    if (noise.lfsrWidth) {
        lfsr &= ~(1 << 6);
        lfsr |= (xor_result << 6);
    }
}

void Channel4::Tick() {
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

void Channel4::HandleNR44Write(const uint8_t value, const uint8_t freqStep) {
    trigger = value >> 7 & 0x01;
    const bool oldEnabled = lengthTimer.enabled;
    lengthTimer.enabled = value & 0x40;
    if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
        if (lengthTimer.lengthTimer != 64) lengthTimer.lengthTimer++;
        if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
    }
    if (value & 0x80) Trigger(freqStep);
}

[[nodiscard]] uint8_t Channel4::ReadByte(const uint16_t address) const {
    switch (address & 0xF) {
        case 0x0F:
        case 0x00: return 0xFF;
        case 0x01: return envelope.Value();
        case 0x02: return noise.Value();
        case 0x03: return trigger << 7 | (lengthTimer.enabled ? 0x40 : 0x00) | 0xBF;
        default: throw UnreachableCodeException("Channel4::ReadByte unreachable code at address: " + std::to_string(address));
    }
}

void Channel4::WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep) {
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

uint8_t Channel4::GetDigitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return (~lfsr & 1) ? envelope.currentVolume : 0;
}

void Audio::InitBandLimitedTable() {
    constexpr double a0 = 7938.0 / 18608.0;
    constexpr double a1 = 9240.0 / 18608.0;
    constexpr double a2 = 1430.0 / 18608.0;

    for (int phase = 0; phase < BL_PHASES; phase++) {
        double sum = 0.0;
        for (int i = 0; i < BL_WIDTH; i++) {
            constexpr double lowpass = 0.9375;
            const double x = static_cast<double>(i - BL_WIDTH / 2) +
                             static_cast<double>(phase) / BL_PHASES;
            const double angle = x * M_PI * lowpass;

            const double sinc = (std::abs(angle) < 1e-10) ? 1.0 : std::sin(angle) / angle;

            const double windowAngle = M_PI * (static_cast<double>(i) + 0.5) / BL_WIDTH;
            const double window = a0 - a1 * std::cos(windowAngle) + a2 * std::cos(2.0 * windowAngle);

            blSteps[phase][i] = sinc * window;
            sum += blSteps[phase][i];
        }

        if (sum > 0.0) {
            for (int i = 0; i < BL_WIDTH; i++) {
                blSteps[phase][i] /= sum;
            }
        }
    }
}

void Audio::BandLimitedUpdate(const int channel, const double left, const double right, const int phase) {
    BandLimited &bl = bandLimited[channel];

    const double deltaLeft = left - bl.lastLeft;
    const double deltaRight = right - bl.lastRight;

    if (std::abs(deltaLeft) < 1e-10 && std::abs(deltaRight) < 1e-10) {
        return;
    }

    bl.lastLeft = left;
    bl.lastRight = right;

    const auto &kernel = blSteps[phase];
    for (int i = 0; i < BL_WIDTH; i++) {
        const int idx = (bl.pos + i) & (BL_BUFFER_SIZE - 1);
        bl.bufferLeft[idx] += deltaLeft * kernel[i];
        bl.bufferRight[idx] += deltaRight * kernel[i];
    }
}

void Audio::BandLimitedRead(const int channel, double &outLeft, double &outRight) {
    BandLimited &bl = bandLimited[channel];

    bl.outputLeft += bl.bufferLeft[bl.pos];
    bl.outputRight += bl.bufferRight[bl.pos];
    bl.bufferLeft[bl.pos] = 0.0;
    bl.bufferRight[bl.pos] = 0.0;
    bl.pos = (bl.pos + 1) & (BL_BUFFER_SIZE - 1);
    outLeft = bl.outputLeft;
    outRight = bl.outputRight;
}

void Audio::GenerateSample() {
    auto dac = [](const double digital, const bool dacOn) -> double {
        if (!dacOn) return 0.0;
        return (15.0 - digital * 2.0) / 15.0;
    };

    const int phase = static_cast<int>((sampleCounter / CYCLES_PER_SAMPLE) * BL_PHASES) & (BL_PHASES - 1);
    auto getChannelOutput = [&](const int ch, const double output, const bool enabled, const bool dacEnabled,
                                const uint8_t leftMask, const uint8_t rightMask) {
        const double val = dac(output, enabled && dacEnabled);
        double left = (nr51 & leftMask) ? val : 0.0;
        double right = (nr51 & rightMask) ? val : 0.0;

        const double leftVol = (nr50 >> 4 & 0x07) + 1;
        const double rightVol = (nr50 & 0x07) + 1;
        left *= leftVol;
        right *= rightVol;

        BandLimitedUpdate(ch, left, right, phase);
    };

    getChannelOutput(0, ch1.currentOutput, ch1.enabled, ch1.dacEnabled, 0x10, 0x01);
    getChannelOutput(1, ch2.currentOutput, ch2.enabled, ch2.dacEnabled, 0x20, 0x02);
    getChannelOutput(2, ch3.currentOutput, ch3.enabled, ch3.dacEnabled, 0x40, 0x04);
    getChannelOutput(3, ch4.currentOutput, ch4.enabled, ch4.dacEnabled, 0x80, 0x08);

    sampleCounter += 1.0;
    if (sampleCounter < CYCLES_PER_SAMPLE) {
        return;
    }
    sampleCounter -= CYCLES_PER_SAMPLE;

    if (samplesAvailable >= AUDIO_BUFFER_SIZE) {
        for (int i = 0; i < 4; i++) {
            double dummy1, dummy2;
            BandLimitedRead(i, dummy1, dummy2);
        }
        return;
    }

    double outLeft = 0.0;
    double outRight = 0.0;
    for (int i = 0; i < 4; i++) {
        double chLeft, chRight;
        BandLimitedRead(i, chLeft, chRight);
        outLeft += chLeft;
        outRight += chRight;
    }

    outLeft /= 32.0;
    outRight /= 32.0;

    highpassLeft = outLeft - (outLeft - highpassLeft) * highpassRate;
    highpassRight = outRight - (outRight - highpassRight) * highpassRate;
    outLeft -= highpassLeft;
    outRight -= highpassRight;

    sampleBuffer[bufferWritePos * 2] = static_cast<float>(outLeft);
    sampleBuffer[bufferWritePos * 2 + 1] = static_cast<float>(outRight);
    bufferWritePos = (bufferWritePos + 1) % AUDIO_BUFFER_SIZE;
    samplesAvailable++;
}

size_t Audio::ReadSamples(float *output, const size_t numSamples) {
    const size_t samplesToRead = std::min(numSamples, samplesAvailable);

    for (size_t i = 0; i < samplesToRead; i++) {
        output[i * 2] = sampleBuffer[bufferReadPos * 2];
        output[i * 2 + 1] = sampleBuffer[bufferReadPos * 2 + 1];
        bufferReadPos = (bufferReadPos + 1) % AUDIO_BUFFER_SIZE;
    }

    samplesAvailable -= samplesToRead;
    return samplesToRead;
}

void Audio::ClearBuffer() {
    bufferWritePos = 0;
    bufferReadPos = 0;
    samplesAvailable = 0;
    sampleCounter = 0.0;
    highpassLeft = 0.0;
    highpassRight = 0.0;
    for (auto &bl: bandLimited) {
        bl.Reset();
    }
}
