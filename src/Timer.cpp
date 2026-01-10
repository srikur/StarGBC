#include "Timer.h"

#include "Common.h"

void Timer::Tick(const Speed speed) {
    const int frameSeqBit = audio_.IsDMG() || speed == Speed::Regular ? 12 : 13;

    reloadActive = false;
    if (overflowPending && --overflowDelay == 0) {
        tima = tma;
        interrupts_.Set(InterruptType::Timer, false);
        overflowPending = false;
        reloadActive = true;
    }

    const bool timerEnabled = tac & 0x04;
    const int timerBit = TimerBit(tac);
    const bool oldSignal = timerEnabled && (divCounter & (1u << timerBit));
    const bool oldFrameSeqSignal = divCounter & 1u << frameSeqBit;

    ++divCounter;

    const bool newSignal = timerEnabled && (divCounter & (1u << timerBit));
    if (oldSignal && !newSignal) {
        IncrementTIMA();
    }

    // Check for a falling edge for the APU Frame Sequencer
    const bool newFrameSeqSignal = (divCounter & (1u << frameSeqBit));
    if (oldFrameSeqSignal && !newFrameSeqSignal) {
        audio_.TickFrameSequencer();
    }
}

void Timer::WriteByte(const uint16_t address, const uint8_t value, const Speed speed) {
    if (address == 0xFF04) WriteDIV(speed == Speed::Double);
    else if (address == 0xFF05) WriteTIMA(value);
    else if (address == 0xFF06) WriteTMA(value);
    else if (address == 0xFF07) WriteTAC(value);
}

[[nodiscard]] uint8_t Timer::ReadByte(const uint16_t address) const {
    if (address == 0xFF04) return divCounter >> 8;
    if (address == 0xFF05) return tima;
    if (address == 0xFF06) return tma;
    if (address == 0xFF07) return tac | 0xF8;
    return 0xFF;
}

void Timer::WriteDIV(const bool doubleSpeed) {
    const bool enabled = tac & 0x04;
    const int bit = TimerBit(tac);
    const bool oldSignal = enabled && (divCounter & (1u << bit));

    const uint8_t frameSeqBit = audio_.IsDMG() || !doubleSpeed ? 4 : 5;
    if (divCounter & (1u << frameSeqBit)) {
        audio_.TickFrameSequencer();
    }

    divCounter = 0;

    if (oldSignal) IncrementTIMA();
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

    if (oldSignal && !newSignal) IncrementTIMA();
}

void Timer::WriteTIMA(const uint8_t value) {
    if (reloadActive) return;
    if (overflowPending) {
        overflowPending = false;
        overflowDelay = 0;
    }
    tima = value;
}

void Timer::WriteTMA(const uint8_t value) {
    tma = value;
    if (reloadActive) tima = value;
}

int Timer::TimerBit(const uint8_t tacMode) const {
    switch (tacMode & 0x03) {
        case 0x00: return 9; // 4096 Hz
        case 0x01: return 3; // 262144 Hz
        case 0x02: return 5; // 65536 Hz
        case 0x03: return 7; // 16384 Hz
        default: ;
    }
    return 9;
}

void Timer::IncrementTIMA() {
    if (++tima == 0) {
        overflowPending = true;
        overflowDelay = 4;
    }
}

bool Timer::SaveState(std::ofstream &stateFile) const {
    try {
        if (!stateFile.is_open()) return false;
        stateFile.write(reinterpret_cast<const char *>(&divCounter), sizeof(divCounter));
        stateFile.write(reinterpret_cast<const char *>(&tima), sizeof(tima));
        stateFile.write(reinterpret_cast<const char *>(&tma), sizeof(tma));
        stateFile.write(reinterpret_cast<const char *>(&tac), sizeof(tac));
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}

bool Timer::LoadState(std::ifstream &stateFile) {
    try {
        if (!stateFile.is_open()) return false;
        stateFile.read(reinterpret_cast<char *>(&divCounter), sizeof(divCounter));
        stateFile.read(reinterpret_cast<char *>(&tima), sizeof(tima));
        stateFile.read(reinterpret_cast<char *>(&tma), sizeof(tma));
        stateFile.read(reinterpret_cast<char *>(&tac), sizeof(tac));
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}
