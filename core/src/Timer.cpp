#include "Timer.h"

#include "stargbc/Common.h"

void Timer::RecomputeTickCache(const Speed speed) {
    cachedTac_ = tac;
    cachedSpeed_ = speed;
    timerFallMask_ = tac & 0x04 ? static_cast<uint16_t>((1u << (TimerBit(tac) + 1)) - 1) : 0;
    const int frameSeqBit = audio_.IsDMG() || speed == Speed::Regular ? 12 : 13;
    frameSeqBitMask_ = static_cast<uint16_t>(1u << frameSeqBit);
    frameSeqToggleMask_ = static_cast<uint16_t>((1u << frameSeqBit) - 1);
}

void Timer::WriteByte(const uint16_t address, const uint8_t value, const Speed speed) {
    if (address == 0xFF04)
        WriteDIV(speed == Speed::Double);
    else if (address == 0xFF05)
        WriteTIMA(value);
    else if (address == 0xFF06)
        WriteTMA(value);
    else if (address == 0xFF07)
        WriteTAC(value);
    if (observations)
        observations->Emit(diagnostics::Component::Timer, diagnostics::EventKind::RegisterApply, address, ReadByte(address), reloadActive);
}

[[nodiscard]] uint8_t Timer::ReadByte(const uint16_t address) const {
    if (address == 0xFF04)
        return divCounter >> 8;
    if (address == 0xFF05)
        return tima;
    if (address == 0xFF06)
        return tma;
    if (address == 0xFF07)
        return tac | 0xF8;
    return 0xFF;
}

void Timer::WriteDIV(const bool doubleSpeed, const bool duringStop) {
    const bool enabled = tac & 0x04;
    const int bit = TimerBit(tac);
    const bool oldSignal = enabled && (divCounter & (1u << bit));

    // Resetting DIV while the DIV-APU bit is high is a falling edge and
    // fires the frame sequencer
    const int frameSeqBit = audio_.IsDMG() || !doubleSpeed ? 12 : 13;
    if ((divCounter & (1u << frameSeqBit)) && !(duringStop && (divCounter & ((1u << (frameSeqBit + 1)) - 1)) == (1u << frameSeqBit))) {
        audio_.TickFrameSequencer(!doubleSpeed);
    }

    const auto oldCounter = divCounter;
    divCounter = 0;
    if (serial) {
        serial->divider_reset(oldCounter);
    }

    const bool delayedStopEdge = duringStop && ((tac & 3) == 0 || (audio_.GetModel() == Model::CGBE && (tac & 3) != 1)) &&
                                 (oldCounter & ((1u << (bit + 1)) - 1)) == (1u << bit);
    if (oldSignal && !delayedStopEdge)
        IncrementTIMA();
}

void Timer::WriteTAC(uint8_t value) {
    value &= 0x07;
    const bool oldEnabled = tac & 0x04;
    const int oldBit = TimerBit(tac);
    const bool oldSignal = oldEnabled && (divCounter & (1u << oldBit));

    tac = value;

    const bool newEnabled = tac & 0x04;
    const int newBit = TimerBit(tac);
    const bool newSignal = newEnabled && (divCounter & (1u << newBit));

    if (oldSignal && !newSignal)
        IncrementTIMA();
}

void Timer::WriteTIMA(const uint8_t value) {
    if (reloadActive)
        return;
    if (overflowPending) {
        overflowPending = false;
        overflowDelay = 0;
    }
    tima = value;
}

void Timer::WriteTMA(const uint8_t value) {
    tma = value;
    if (reloadActive)
        tima = value;
}

int Timer::TimerBit(const uint8_t tacMode) const {
    switch (tacMode & 0x03) {
        case 0x00:
            return 9; // 4096 Hz
        case 0x01:
            return 3; // 262144 Hz
        case 0x02:
            return 5; // 65536 Hz
        case 0x03:
            return 7; // 16384 Hz
        default:;
    }
    return 9;
}

void Timer::IncrementTIMA() {
    if (observations)
        observations->Emit(diagnostics::Component::Timer, diagnostics::EventKind::TimerEdge, 0xFF05, static_cast<uint8_t>(tima + 1), divCounter);
    if (++tima == 0) {
        if (observations)
            observations->Emit(diagnostics::Component::Timer, diagnostics::EventKind::TimerOverflow, 0xFF05, 0, 4);
        overflowPending = true;
        overflowDelay = 4;
    }
}
