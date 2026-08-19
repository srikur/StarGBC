#include "Audio.h"
#include <cmath>
#include <cstring>
#include <set>
#include <string>

void Audio::TickFrameSequencer(const bool divWriteSingleSpeed) {
    if (!audioEnabled) return;
    // Powering the APU on while the DIV-APU bit is high skips the first
    // event; the second event then runs without incrementing the divider
    if (skipState == SkipState::Skip) {
        skipState = SkipState::Skipped;
        return;
    }
    if (skipState == SkipState::Skipped) {
        skipState = SkipState::Inactive;
    } else {
        frameSeqStep++;
    }

    // 64Hz envelope step: the countdown runs freely while the clock is not
    // latched (a latched clock pauses it until the tick consumes it)
    if ((frameSeqStep & 7) == 7) {
        if (!ch1.envelope.clock) ch1.envelope.volumeCountdown = (ch1.envelope.volumeCountdown - 1) & 7;
        if (!ch2.envelope.clock) ch2.envelope.volumeCountdown = (ch2.envelope.volumeCountdown - 1) & 7;
        if (!ch4.envelope.clock) ch4.envelope.volumeCountdown = (ch4.envelope.volumeCountdown - 1) & 7;
    }

    // A clock latched by the previous secondary event ticks the volume on
    // any DIV event, not just the 64Hz one
    if (ch1.envelope.clock) ch1.TickEnvelope();
    if (ch2.envelope.clock) ch2.TickEnvelope();
    if (ch4.envelope.clock) ch4.TickEnvelope();

    if (frameSeqStep & 1) {
        ch1.TickLength();
        ch2.TickLength();
        ch3.TickLength();
        ch4.TickLength();
    }

    if ((frameSeqStep & 3) == 3) {
        ch1.sweep.countdown = (ch1.sweep.countdown + 1) & 7;
        ch1.TickSweep128((tickCounter & 2) ? 0 : 1, divWriteSingleSpeed);
    }
}

void Audio::TickFrameSequencerSecondary() {
    if (!audioEnabled) return;
    const auto latch = [](auto &ch) {
        if (ch.enabled && ch.envelope.volumeCountdown == 0) {
            ch.envelope.volumeCountdown = ch.envelope.sweepPace;
            ch.envelope.SetClock(ch.envelope.volumeCountdown != 0, ch.envelope.direction, ch.envelope.currentVolume);
        }
    };
    latch(ch1);
    latch(ch2);
    latch(ch4);
}

void Audio::Tick() {
    tickCounter++;
    if (audioEnabled) {
        ch3.alternateRead = false;
        // Square channels run on the 2MHz APU tick grid
        if ((tickCounter & 1) == 0) {
            ch1.TickSweepUnit((tickCounter & 2) ? 0 : 1);
            ch1.Tick2M();
            ch2.Tick2M();
            ch4.Tick2M(frameSeqStep, dmg);
        }
        ch3.Tick();
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
        skipState = SkipState::Inactive;
    } else if (!wasEnabled && audioEnabled) {
        // Reset tick counter for phase tracking
        tickCounter = 0;
        // If the DIV-APU bit is high at power-on the first event is skipped
        // and the divider starts at 1, so a length-enabling NRx4 write lands
        // on an odd divider right away
        frameSeqStep = divBit4High ? 1 : 0;
        skipState = divBit4High ? SkipState::Skip : SkipState::Inactive;
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
    const uint8_t lfDiv = (tickCounter & 2) ? 0 : 1;
    switch (address) {
        case 0xFF10 ... 0xFF14: ch1.WriteByte(address, value, audioEnabled, frameSeqStep, lfDiv, dmg);
            break;
        case 0xFF15 ... 0xFF19: ch2.WriteByte(address, value, audioEnabled, frameSeqStep, lfDiv, dmg);
            break;
        case 0xFF1A ... 0xFF1E: ch3.WriteByte(address, value, frameSeqStep, dmg);
            break;
        case 0xFF1F ... 0xFF23: ch4.WriteByte(address, value, audioEnabled, frameSeqStep, dmg);
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

void Envelope::SetClock(const bool value, const bool dir, const uint8_t volume) {
    if (clock == value) return;
    if (value) {
        clock = true;
        shouldLock = (volume == 0xF && dir) || (volume == 0x0 && !dir);
    } else {
        clock = false;
        locked |= shouldLock;
    }
}

void Envelope::NRx2GlitchSingle(const uint8_t value, const uint8_t old) {
    // "Zombie mode": writing NRx2 while the channel plays steps or inverts
    // the volume depending on how the pace and direction bits change
    if (clock) {
        volumeCountdown = value & 7;
    }
    bool shouldTick = (value & 7) && !(old & 7) && !locked;
    const bool shouldInvert = (value & 8) ^ (old & 8);

    if ((value & 0xF) == 8 && (old & 0xF) == 8 && !locked) {
        shouldTick = true;
    }

    if (shouldInvert) {
        if (value & 8) {
            if (!(old & 7) && !locked) {
                currentVolume ^= 0xF;
            } else {
                currentVolume = (0xE - currentVolume) & 0xF;
            }
            shouldTick = false;
        } else {
            currentVolume = (0x10 - currentVolume) & 0xF;
        }
    }
    if (shouldTick) {
        currentVolume = (currentVolume + ((value & 8) ? 1 : -1)) & 0xF;
    } else if (!(value & 7) && clock) {
        SetClock(false, false, 0);
    }
}

void Envelope::NRx2Glitch(const uint8_t value, const uint8_t old, const bool dmg) {
    // Pre-CGB-D revisions pass through $FF as an intermediate value
    if (dmg) {
        NRx2GlitchSingle(0xFF, old);
        NRx2GlitchSingle(value, 0xFF);
    } else {
        NRx2GlitchSingle(value, old);
    }
}

uint8_t Audio::ReadPCM12() const {
    return (ch2.GetDigitalOutput() << 4) | (ch1.GetDigitalOutput() & 0x0F);
}

uint8_t Audio::ReadPCM34() const {
    return (ch4.GetDigitalOutput() << 4) | (ch3.GetDigitalOutput() & 0x0F);
}

void Channel1::Trigger(const uint8_t value, const uint16_t oldFreq, const uint8_t freqStep, const uint8_t lfDiv, const bool dmg) {
    const bool wasActive = enabled;
    dacEnabled = (envelope.initialVolume > 0 || envelope.direction);
    didTick = false;

    if (lengthTimer.lengthTimer == 64) {
        lengthTimer.lengthTimer = 0;
        if (lengthTimer.enabled && (freqStep % 2 != 0)) {
            lengthTimer.lengthTimer++;
        }
    }

    // The duty position is never reset by a trigger, only by APU power-off.
    // A fresh start keeps the old PCM value suppressed until the first duty
    // advance, (2048-freq)+2 1MHz ticks later; a restart while active keeps
    // the current sample playing and restarts one tick sooner. On CGB D/E a
    // phantom duty step occurs when bit 10 of both the countdown and the new
    // frequency are clear
    bool forceUnsurpressed = false;
    if (!enabled) {
        if (!dmg && !(value & 4) && !(((sampleCountdown - trigDelay) / 2) & 0x400)) {
            dutyStep = (dutyStep + 1) & 7;
            forceUnsurpressed = true;
        }
        trigDelay = 6 - lfDiv;
    } else {
        uint8_t extraDelay = 0;
        if (!dmg) {
            if (!justReloaded && !(value & 4) && !(((sampleCountdown - 1 - trigDelay) / 2) & 0x400)) {
                dutyStep = (dutyStep + 1) & 7;
                sampleSurpressed = false;
            } else if (frequency.Value() == 0x7FF && oldFreq != 0x7FF && sampleSurpressed) {
                extraDelay += 2;
            }
        }
        trigDelay = 4 - lfDiv + extraDelay;
    }
    sampleCountdown = (frequency.Value() ^ 0x7FF) * 2 + trigDelay;

    envelope.volumeCountdown = envelope.sweepPace;
    envelope.currentVolume = envelope.initialVolume;
    envelope.clock = false;
    envelope.locked = false;
    if (enabled) UpdateOutput();

    if (dacEnabled && !enabled) {
        enabled = true;
        sampleSurpressed = !forceUnsurpressed;
    }

    sweep.instantCalcDone = false;
    sweep.shadowLength = 0;
    sweep.completedAddend = 0;
    if (sweep.step) {
        // APU bug: with a nonzero shift the overflow check also runs on
        // trigger, after a short delay
        sweep.calcCountdown = sweep.step;
        if (dmg) {
            sweep.calcReloadTimer = (lfDiv ^ 1) ? 3 : 2;
        } else {
            sweep.calcReloadTimer = 2;
        }
        if (!wasActive) sweep.calcReloadTimer++;
        sweep.unshifted = false;
        sweep.lengthAddend = frequency.Value() >> sweep.step;
    } else {
        sweep.lengthAddend = 0;
    }
    restartHold = 2 - lfDiv + (dmg ? 0 : 3);
    sweep.countdown = sweep.pace ^ 7;
}

void Channel1::TickLength() {
    if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
        lengthTimer.lengthTimer++;
    }
}

void Channel1::TickSweep128(const uint8_t lfDiv, const bool fromDivWrite) {
    // 128Hz sweep event: apply the previously computed addend to the
    // frequency, then schedule the recalculation and overflow check
    if (sweep.pace && sweep.countdown == 7) {
        if (sweep.step) {
            frequency.Write((sweep.lengthAddend + sweep.shadowLength + (sweep.direction ? 1 : 0)) & 0x7FF);
        }
        if (restartHold == 0) {
            sweep.lengthAddend = frequency.Value() >> sweep.step;
        }
        sweep.calcCountdown = sweep.step;
        sweep.calcReloadTimer = fromDivWrite ? 1 : 1 + lfDiv;
        sweep.unshifted = sweep.step == 0;
        sweep.countdown = sweep.pace ^ 7;
        if (sweep.calcCountdown == 0) {
            sweep.instantCalcDone = true;
        }
    }
}

void Channel1::SweepCalculationDone() {
    // APU bug: the overflow check happens after adding the sweep delta twice
    if (restartHold == 0) {
        sweep.shadowLength = frequency.Value();
    }
    if (sweep.direction) {
        sweep.lengthAddend ^= 0x7FF;
    }
    if (sweep.shadowLength + sweep.lengthAddend > 0x7FF && !sweep.direction) {
        enabled = false;
    }
    sweep.completedAddend = sweep.lengthAddend;
}

void Channel1::TickSweepUnit(const uint8_t lfDiv) {
    // The recalculation countdown runs at 1MHz on a fixed phase
    if (lfDiv == 0) {
        if (sweep.calcReloadTimer > 0) {
            sweep.calcReloadTimer--;
            if (sweep.calcReloadTimer == 0) {
                if (!sweep.calcCountdown && sweep.instantCalcDone) {
                    SweepCalculationDone();
                }
                sweep.instantCalcDone = false;
            }
        } else if (sweep.calcCountdown && (sweep.step != 0 || sweep.unshifted)) {
            if (sweep.calcCountdown > 1) {
                sweep.calcCountdown--;
            } else {
                sweep.calcCountdown = 0;
                SweepCalculationDone();
            }
        }
    }
    if (restartHold > 0) restartHold--;
}

void Channel1::Nr10WriteGlitch(const uint8_t value, const uint8_t lfDiv, const bool dmg) {
    if (dmg) {
        if (sweep.calcReloadTimer <= 1 && sweep.calcCountdown) {
            // Zombie step when the old shift bits are clear
            if (!sweep.step && lfDiv) {
                sweep.calcCountdown--;
                if (sweep.calcCountdown <= 1) {
                    sweep.calcCountdown = 0;
                    SweepCalculationDone();
                }
            }
        }
    } else {
        if (sweep.calcReloadTimer == 2) {
            // Countdown just reloaded, re-reload it
            sweep.calcCountdown = value & 0x7;
            if (!sweep.calcCountdown) {
                sweep.calcReloadTimer = 0;
            }
        }
        if ((value & 7) && !sweep.step && !lfDiv && sweep.calcCountdown > 1) {
            sweep.calcCountdown--;
            if (!sweep.calcCountdown) {
                SweepCalculationDone();
            }
        }
    }
}

void Channel1::TickEnvelope() {
    envelope.SetClock(false, false, 0);
    if (envelope.locked) return;
    if (!envelope.sweepPace) return;
    envelope.currentVolume = (envelope.currentVolume + (envelope.direction ? 1 : -1)) & 0xF;
    if (enabled) UpdateOutput();
}

void Channel1::UpdateOutput() {
    // A freshly triggered channel keeps its previous PCM value until the
    // first duty advance
    if (sampleSurpressed) return;
    sampleOut = DUTY_PATTERNS[lengthTimer.dutyCycle][dutyStep] * envelope.currentVolume;
    currentOutput = static_cast<float>(sampleOut);
}

void Channel1::Tick2M() {
    if (!enabled) return;
    if (trigDelay > 0) trigDelay--;
    if (lengthTimer.enabled && lengthTimer.lengthTimer == 64) {
        enabled = false;
    }
    if (sampleCountdown == 0) {
        sampleCountdown = (frequency.Value() ^ 0x7FF) * 2 + 1;
        dutyStep = (dutyStep + 1) & 7;
        sampleSurpressed = false;
        didTick = true;
        justReloaded = true;
        UpdateOutput();
    } else {
        sampleCountdown--;
        justReloaded = false;
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

void Channel1::HandleNR14Write(const uint8_t value, const uint8_t freqStep, const uint8_t lfDiv, const bool dmg) {
    const uint16_t oldFreq = frequency.Value();
    // Quirk: lowering the frequency high bits from 7 while the countdown was
    // reloaded from the matching length steps the duty position back once
    // (SameBoy's ≥$700 to <$700 hack; on DMG only on an odd countdown phase)
    if (!(value & 0x80) && enabled && (oldFreq >> 8) == 7 && (value & 7) != 7) {
        if (!dmg || (sampleCountdown & 1)) {
            if (didTick && (sampleCountdown >> 1) == (oldFreq ^ 0x7FF)) {
                dutyStep = (dutyStep - 1) & 7;
                sampleSurpressed = false;
            }
        }
    }
    frequency.WriteHigh(value);
    // A write landing on the exact tick the countdown reloaded re-reloads it
    // with the new frequency
    if (justReloaded) {
        sampleCountdown = (frequency.Value() ^ 0x7FF) * 2 + 1;
    }
    const bool oldEnabled = lengthTimer.enabled;
    lengthTimer.enabled = value & 0x40;
    if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
        if (lengthTimer.lengthTimer < 64) {
            lengthTimer.lengthTimer++;
        }
        if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
    }
    if (value & 0x80) Trigger(value, oldFreq, freqStep, lfDiv, dmg);
}

void Channel1::WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep, const uint8_t lfDiv, const bool dmg) {
    switch (address & 0xF) {
        case 0x00: {
            if (sweep.calcCountdown || sweep.calcReloadTimer) {
                Nr10WriteGlitch(value, lfDiv, dmg);
            }
            // Leaving negate mode after a subtraction calculation disables
            // the channel (pre-CGB-D always behaves as if negate was on)
            const bool oldNegate = dmg ? true : sweep.direction;
            sweep.Write(value);
            if (sweep.shadowLength + sweep.completedAddend + (oldNegate ? 1 : 0) > 0x7FF && !(value & 8)) {
                enabled = false;
            }
            TickSweep128(lfDiv, false);
        }
        break;
        case 0x01: lengthTimer.Write(value, audioEnabled);
            break;
        case 0x02:
            if ((value & 0xF8) == 0) {
                dacEnabled = false;
                enabled = false;
            } else {
                if (enabled) {
                    envelope.NRx2Glitch(value, envelope.Value(), dmg);
                    UpdateOutput();
                }
                dacEnabled = true;
            }
            envelope.Write(value);
            break;
        case 0x03: frequency.WriteLow(value);
            if (justReloaded) {
                sampleCountdown = (frequency.Value() ^ 0x7FF) * 2 + 1;
            }
            break;
        case 0x04: HandleNR14Write(value, freqStep, lfDiv, dmg);
            break;
        default: throw UnreachableCodeException("Channel1::WriteByte unreachable code at address: " + std::to_string(address));
    }
}

uint8_t Channel1::GetDigitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return sampleOut;
}

void Channel2::Trigger(const uint8_t value, const uint16_t oldFreq, const uint8_t freqStep, const uint8_t lfDiv, const bool dmg) {
    dacEnabled = (envelope.initialVolume > 0 || envelope.direction);
    didTick = false;

    if (lengthTimer.lengthTimer == 64) {
        lengthTimer.lengthTimer = 0;
        if (lengthTimer.enabled && (freqStep % 2 != 0)) {
            lengthTimer.lengthTimer++;
        }
    }

    bool forceUnsurpressed = false;
    if (!enabled) {
        if (!dmg && !(value & 4) && !(((sampleCountdown - trigDelay) / 2) & 0x400)) {
            dutyStep = (dutyStep + 1) & 7;
            forceUnsurpressed = true;
        }
        trigDelay = 6 - lfDiv;
    } else {
        uint8_t extraDelay = 0;
        if (!dmg) {
            if (!justReloaded && !(value & 4) && !(((sampleCountdown - 1 - trigDelay) / 2) & 0x400)) {
                dutyStep = (dutyStep + 1) & 7;
                sampleSurpressed = false;
            } else if (frequency.Value() == 0x7FF && oldFreq != 0x7FF && sampleSurpressed) {
                extraDelay += 2;
            }
        }
        trigDelay = 4 - lfDiv + extraDelay;
    }
    sampleCountdown = (frequency.Value() ^ 0x7FF) * 2 + trigDelay;

    envelope.volumeCountdown = envelope.sweepPace;
    envelope.currentVolume = envelope.initialVolume;
    envelope.clock = false;
    envelope.locked = false;
    if (enabled) UpdateOutput();

    if (dacEnabled && !enabled) {
        enabled = true;
        sampleSurpressed = !forceUnsurpressed;
    }
}

void Channel2::TickLength() {
    if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
        lengthTimer.lengthTimer++;
    }
}

void Channel2::TickEnvelope() {
    envelope.SetClock(false, false, 0);
    if (envelope.locked) return;
    if (!envelope.sweepPace) return;
    envelope.currentVolume = (envelope.currentVolume + (envelope.direction ? 1 : -1)) & 0xF;
    if (enabled) UpdateOutput();
}

void Channel2::UpdateOutput() {
    if (sampleSurpressed) return;
    sampleOut = DUTY_PATTERNS[lengthTimer.dutyCycle][dutyStep] * envelope.currentVolume;
    currentOutput = static_cast<float>(sampleOut);
}

void Channel2::Tick2M() {
    if (!enabled) return;
    if (trigDelay > 0) trigDelay--;
    if (lengthTimer.enabled && lengthTimer.lengthTimer == 64) {
        enabled = false;
    }
    if (sampleCountdown == 0) {
        sampleCountdown = (frequency.Value() ^ 0x7FF) * 2 + 1;
        dutyStep = (dutyStep + 1) & 7;
        sampleSurpressed = false;
        didTick = true;
        justReloaded = true;
        UpdateOutput();
    } else {
        sampleCountdown--;
        justReloaded = false;
    }
}

void Channel2::HandleNR24Write(const uint8_t value, const uint8_t freqStep, const uint8_t lfDiv, const bool dmg) {
    const uint16_t oldFreq = frequency.Value();
    if (!(value & 0x80) && enabled && (oldFreq >> 8) == 7 && (value & 7) != 7) {
        if (!dmg || (sampleCountdown & 1)) {
            if (didTick && (sampleCountdown >> 1) == (oldFreq ^ 0x7FF)) {
                dutyStep = (dutyStep - 1) & 7;
                sampleSurpressed = false;
            }
        }
    }
    frequency.WriteHigh(value);
    if (justReloaded) {
        sampleCountdown = (frequency.Value() ^ 0x7FF) * 2 + 1;
    }
    const bool oldEnabled = lengthTimer.enabled;
    lengthTimer.enabled = value & 0x40;
    if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
        if (lengthTimer.lengthTimer < 64) lengthTimer.lengthTimer++;
        if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
    }
    if (value & 0x80) Trigger(value, oldFreq, freqStep, lfDiv, dmg);
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

void Channel2::WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep, const uint8_t lfDiv, const bool dmg) {
    switch (address & 0xF) {
        case 0x05: break;
        case 0x06: lengthTimer.Write(value, audioEnabled);
            break;
        case 0x07:
            if ((value & 0xF8) == 0) {
                dacEnabled = false;
                enabled = false;
            } else {
                if (enabled) {
                    envelope.NRx2Glitch(value, envelope.Value(), dmg);
                    UpdateOutput();
                }
                dacEnabled = true;
            }
            envelope.Write(value);
            break;
        case 0x08: frequency.WriteLow(value);
            if (justReloaded) {
                sampleCountdown = (frequency.Value() ^ 0x7FF) * 2 + 1;
            }
            break;
        case 0x09: HandleNR24Write(value, freqStep, lfDiv, dmg);
            break;
        default: throw UnreachableCodeException("Channel2::WriteByte unreachable code at address: " + std::to_string(address));
    }
}

uint8_t Channel2::GetDigitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return sampleOut;
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

void Channel4::TickLength() {
    if (lengthTimer.enabled && lengthTimer.lengthTimer < 64) {
        lengthTimer.lengthTimer++;
    }
}

void Channel4::TickEnvelope() {
    envelope.SetClock(false, false, 0);
    if (envelope.locked) return;
    if (!envelope.sweepPace) return;
    envelope.currentVolume = (envelope.currentVolume + (envelope.direction ? 1 : -1)) & 0xF;
}

void Channel4::StepLfsr() {
    lfsrBit7BeforeStep = lfsr & 0x80;
    const uint16_t highBitMask = narrow ? 0x4040 : 0x4000;
    const bool newHighBit = (lfsr ^ (lfsr >> 1) ^ 1) & 1;
    lfsr >>= 1;
    if (newHighBit) {
        lfsr |= highBitMask;
    } else {
        // Not redundant: this matters when switching LFSR widths
        lfsr &= ~highBitMask;
    }
    currentLfsrSample = lfsr & 1;
    if (dacEnabled) {
        currentOutput = currentLfsrSample ? static_cast<float>(envelope.currentVolume) : 0.0f;
    }
    lfsrSteppedInNarrow = narrow;
}

void Channel4::Tick2M(const uint8_t freqStep, const bool dmg) {
    if (enabled && lengthTimer.enabled && lengthTimer.lengthTimer == 64) {
        enabled = false;
    }
    if (dmgDelayedStart > 0 && --dmgDelayedStart == 0) {
        // The deferred start always fires; do not re-defer
        DoTrigger(freqStep, false);
    }
    alignment++;
    if (counterActive || backgroundCounterActive) {
        unsigned divisor = noise.clockDivider << 2;
        if (!divisor) divisor = 2;
        if (counterCountdown == 0) counterCountdown = divisor;
        if (counterCountdown == 1) {
            counterCountdown = divisor;
            const uint16_t mask = 1 << noise.clockShift;
            const bool oldBit = counter & mask;
            counter = (counter + 1) & 0x3FFF;
            didStepCounter = true;
            const bool newBit = counter & mask;
            // The LFSR steps on rising edges of the selected counter bit
            if (newBit && !oldBit && enabled) {
                StepLfsr();
            }
            countdownReloaded = true;
        } else {
            counterCountdown--;
            countdownReloaded = false;
        }
    }
}

void Channel4::PrepareNoiseStart(const bool dmg) {
    counterActive = dacEnabled;
    const bool wasStartedWithDacDisabled = startedWithDacDisabled;
    startedWithDacDisabled = !counterActive;
    unsigned divisor = noise.clockDivider;
    const bool wasBackgroundCounting = backgroundCounterActive;
    backgroundCounterActive = true;
    bool instantStep = false;
    bool div1Glitch = false;

    if (divisor > 1 && counterCountdown == 1) {
        counter = (counter + 1) & 0x3FFF;
    } else if (counterCountdown == 2 && (alignment & 3) == 0 && enabled) {
        if (divisor == 0) {
            divisor = 8;
        } else if (divisor == 1) {
            if (!didStepCounter) {
                div1Glitch = true;
            }
            const uint16_t mask = 1 << noise.clockShift;
            const bool oldBit = counter & mask;
            counter = (counter + 1) & 0x3FFF;
            const bool newBit = counter & mask;
            if (newBit && !oldBit) {
                instantStep = true;
            }
        }
    }
    counterCountdown = divisor == 0 ? 6 : divisor * 4 + 6;
    if (alignment & 1) {
        if (!divisor) {
            if (dmg) {
                counterCountdown++;
            } else if (wasBackgroundCounting) {
                counterCountdown--;
            } else {
                counterCountdown++;
            }
        } else {
            if (alignment & 2) {
                if (divisor == 1 && !enabled) {
                    counterCountdown++;
                } else {
                    counterCountdown -= 3;
                }
            } else {
                counterCountdown--;
                if (divisor == 1 && enabled) {
                    counterCountdown -= 4;
                }
            }
        }
    } else {
        if (divisor) {
            if (alignment & 2) {
                counterCountdown -= 2;
            } else if (divisor > 1) {
                counterCountdown -= 4;
            } else if (divisor == 1 && enabled && !(noise.Value() & 0xF0)) {
                counterCountdown -= 4;
            }
        }
    }

    // Background counting glitches
    if (divisor > 1) {
        if (!counterActive && !(alignment & 3)) {
            counterCountdown += 4;
        }
    } else {
        if (wasBackgroundCounting && !enabled && !(alignment & 3)) {
            if (divisor == 0) {
                if (wasStartedWithDacDisabled) {
                    counterCountdown += 28;
                }
            } else {
                counterCountdown -= 4;
            }
        }
    }
    if (div1Glitch) {
        counterCountdown -= 4;
    }

    if (!divisor && enabled && (alignment & 3) == 3) {
        lfsr = 0x0055;
    } else {
        lfsr = 0;
    }
    if (instantStep) {
        StepLfsr();
    }
}

void Channel4::DoTrigger(const uint8_t freqStep, const bool dmg) {
    envelope.locked = false;
    envelope.clock = false;
    if (dmg && (alignment & 3) != 0 && dmgDelayedStart == 0) {
        // DMG quirk: misaligned noise starts are deferred a few ticks
        dmgDelayedStart = 6;
        return;
    }
    lfsr = 0;
    PrepareNoiseStart(dmg);

    envelope.currentVolume = envelope.initialVolume;
    currentLfsrSample = false;
    envelope.volumeCountdown = envelope.sweepPace;
    didStepCounter = (alignment & 3) == 2;
    currentOutput = 0.0f;

    if (dacEnabled) enabled = true;

    if (lengthTimer.lengthTimer == 64) {
        lengthTimer.lengthTimer = 0;
        if (lengthTimer.enabled && (freqStep % 2 != 0)) {
            lengthTimer.lengthTimer++;
        }
    }
}

void Channel4::HandleNR43Write(const uint8_t value) {
    const uint8_t old = noise.Value();
    const bool oldNarrow = narrow;
    narrow = value & 8;
    noise.Write(value);

    if ((old & 0xF0) == (value & 0xF0)) return;

    // NR43 writes glitch the LFSR through an intermediate register value
    // (CGB-E deterministic variant)
    const uint16_t effectiveCounter = counter;
    const bool oldBit = (effectiveCounter >> (old >> 4)) & 1;
    const uint8_t glitchValue = (old & 0x7F) | (value & 0x80);
    const bool glitchBit = (effectiveCounter >> (glitchValue >> 4)) & 1;
    const bool newBit = (effectiveCounter >> (value >> 4)) & 1;

    if (oldBit == newBit && newBit != glitchBit) {
        if (newBit) {
            // Category 1
            if (!(value & 0x80)) {
                StepLfsr();
            } else {
                const uint8_t t1 = (old >> 4) & 7;
                const uint8_t t2 = (value >> 4) & 7;
                if ((t1 ^ 7) + t2 > 7 || ((t1 ^ 7) & t2)) {
                    // Copy bit 8 to bit 7
                    lfsr &= ~0x80;
                    lfsr |= (lfsr >> 1) & 0x80;
                    if ((t1 == 0 || t1 == 4) && t2 == 3) {
                        lfsr &= (lfsr >> 1) | 0x545;
                        currentLfsrSample = lfsr & 1;
                    } else if (t1 == 2 && t2 == 3) {
                        uint16_t mask = 0x555;
                        if ((lfsr & 0xC) == 0xC) mask |= 8;
                        if ((lfsr & 0xC00) == 0xC00) mask |= 0x800;
                        lfsr &= (lfsr >> 1) | mask;
                        currentLfsrSample = lfsr & 1;
                    }
                    if (!narrow && oldNarrow && lfsrSteppedInNarrow) {
                        if (lfsrBit7BeforeStep) {
                            lfsr |= 0x40;
                        } else {
                            lfsr &= ~0x40;
                        }
                    }
                    lfsr |= narrow ? 0x4040 : 0x4000;
                    lfsrSteppedInNarrow = narrow;
                }
            }
        } else {
            // Category 2
            static constexpr uint8_t glitchMap[8 * 8] = {
                0, 0, 4, 2, 2, 2, 0, 0,
                0, 0, 2, 4, 2, 2, 0, 0,
                1, 2, 0, 1, 5, 3, 0, 0,
                0, 0, 0, 0, 2, 2, 0, 0,
                0, 2, 2, 2, 0, 0, 0, 0,
                6, 0, 2, 2, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,
            };
            const unsigned glitch = (value & 0x80) ? glitchMap[((old & 0x70) >> 1) | ((value & 0x70) >> 4)] : 0;
            switch (glitch) {
                case 1:
                case 6:
                    StepLfsr();
                    if (glitch == 6) {
                        if ((narrow && (lfsr & 0x71) == 0x20) || (lfsr & 0x71) == 0x61) {
                            lfsr &= ~0x20;
                        }
                        if ((lfsr & 0x7001) == 0x2000 || (lfsr & 0x7001) == 0x6001) {
                            lfsr &= ~0x2000;
                        }
                    }
                    if ((lfsr & 0x3) == 2) {
                        lfsr &= ~2;
                    }
                    break;
                case 2: {
                    const uint16_t prev = lfsr;
                    StepLfsr();
                    lfsr &= prev | 1;
                    break;
                }
                case 5:
                    if ((lfsr & 0x3) == 2) {
                        lfsr &= narrow ? ~0x4040 : ~0x4000;
                    }
                    if ((lfsr & 0x19) == 8) {
                        lfsr &= ~8;
                    }
                    [[fallthrough]];
                case 3:
                    lfsr &= ~1;
                    lfsr |= (lfsr >> 1) & 1;
                    currentLfsrSample = lfsr & 1;
                    lfsrSteppedInNarrow = narrow;
                    break;
                case 4: {
                    const uint16_t prev = lfsr;
                    StepLfsr();
                    lfsr &= prev | (narrow ? ~0x2022 : ~0x2002);
                    break;
                }
                default:
                    StepLfsr();
                    break;
            }
        }
    } else if (!oldBit && newBit) {
        StepLfsr();
    }
    currentLfsrSample = lfsr & 1;
    if (dacEnabled && enabled) {
        currentOutput = currentLfsrSample ? static_cast<float>(envelope.currentVolume) : 0.0f;
    }
}

void Channel4::HandleNR44Write(const uint8_t value, const uint8_t freqStep, const bool dmg) {
    trigger = value >> 7 & 0x01;
    const bool oldEnabled = lengthTimer.enabled;
    lengthTimer.enabled = value & 0x40;
    if (!oldEnabled && lengthTimer.enabled && (freqStep % 2 != 0)) {
        if (lengthTimer.lengthTimer != 64) lengthTimer.lengthTimer++;
        if (lengthTimer.lengthTimer == 64 && !(value & 0x80)) enabled = false;
    }
    if (value & 0x80) DoTrigger(freqStep, dmg);
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

void Channel4::WriteByte(const uint16_t address, const uint8_t value, const bool audioEnabled, const uint8_t freqStep, const bool dmg) {
    switch (address & 0xF) {
        case 0x0F: break;
        case 0x00: lengthTimer.Write(value, audioEnabled);
            break;
        case 0x01:
            if ((value & 0xF8) == 0) {
                // Disabling the DAC also stops the background counter
                if (enabled && noise.clockDivider) {
                    if (counterCountdown <= 2) {
                        counter = (counter + 1) & 0x3FFF;
                    }
                    backgroundCounterActive = false;
                }
                dacEnabled = false;
                enabled = false;
                counterActive = false;
            } else {
                if (enabled) {
                    envelope.NRx2Glitch(value, envelope.Value(), dmg);
                }
                dacEnabled = true;
            }
            envelope.Write(value);
            break;
        case 0x02:
            // A write landing on the exact tick the countdown reloaded
            // re-reloads it with the new divisor, offset by the alignment
            if (countdownReloaded) {
                unsigned divisor = (value & 0x07) << 2;
                if (!divisor) divisor = 2;
                static constexpr uint8_t offsetsCgb[4] = {2, 1, 0, 3};
                static constexpr uint8_t offsetsDmg[4] = {2, 1, 4, 3};
                counterCountdown = divisor + (divisor == 2 ? 0 : (dmg ? offsetsDmg : offsetsCgb)[alignment & 3]);
            }
            if (dmg) {
                HandleNR43Write(0xFF);
            }
            HandleNR43Write(value);
            break;
        case 0x03: HandleNR44Write(value, freqStep, dmg);
            break;
        default: throw UnreachableCodeException("Channel4::WriteByte unreachable code at address: " + std::to_string(address));
    }
}

uint8_t Channel4::GetDigitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return (lfsr & 1) ? envelope.currentVolume : 0;
}

void Audio::InitBandLimitedTable() {
    constexpr double a0 = 7938.0 / 18608.0;
    constexpr double a1 = 9240.0 / 18608.0;
    constexpr double a2 = 1430.0 / 18608.0;

    for (int phase = 0; phase < BL_PHASES; phase++) {
        double sum = 0.0;
        for (int i = 0; i < BL_WIDTH; i++) {
            constexpr double lowpass = 0.9375;
            const double x = static_cast<double>(i - BL_WIDTH / 2) -
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
