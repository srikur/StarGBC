#pragma once


#include "Audio.h"
#include "Interrupts.h"
#include "Serial.h"

class Timer {
public:
    uint8_t tma{0x00};
    uint8_t tima{0x00};
    uint8_t tac{0x00};
    uint8_t overflowDelay{0x00};
    uint8_t apuEventDelay{0};
    bool apuEventSecondary{false};
    uint16_t divCounter{0x0000};
    bool overflowPending{false};
    bool reloadActive{false};
    // Per-tick edge masks derived from (tac, speed); refreshed lazily when the
    // key changes, which also covers LoadState restoring a different tac.
    // 0xFF is unreachable for tac (WriteTAC masks to 0x07), so it forces the
    // first recompute.
    [[=NotStateAware]] uint8_t cachedTac_{0xFF};
    [[=NotStateAware]] Speed cachedSpeed_{Speed::Regular};
    // A falling edge of bit b happens exactly when the incremented counter's
    // low b+1 bits are zero; a toggle when its low b bits are zero
    [[=NotStateAware]] uint16_t timerFallMask_{0};
    [[=NotStateAware]] uint16_t frameSeqBitMask_{0};
    [[=NotStateAware]] uint16_t frameSeqToggleMask_{0};
    [[=NotStateAware]] diagnostics::ObservationContext *observations{};
    [[=NotStateAware]] Serial *serial{};
    Audio &audio_;
    Interrupts &interrupts_;

    explicit Timer(Audio &audio, Interrupts &interrupts) : audio_(audio), interrupts_(interrupts) {}

    // Inline: runs every T-cycle (2x per dot at double speed); the masks make
    // the common case two edge tests on register-resident values
    void Tick(const Speed speed) {
        if (tac != cachedTac_ || speed != cachedSpeed_) [[unlikely]]
            RecomputeTickCache(speed);

        if (apuEventDelay && --apuEventDelay == 0) {
            if (apuEventSecondary)
                audio_.TickFrameSequencerSecondary();
            else
                audio_.TickFrameSequencer();
        }
        reloadActive = false;
        if (overflowPending && --overflowDelay == 0) {
            tima = tma;
            if (observations)
                observations->Emit(diagnostics::Component::Timer, diagnostics::EventKind::TimerReload, 0xFF05, tima, divCounter);
            // DMG exposes IF before the request reaches the CPU wake/dispatch path
            interrupts_.set_after(InterruptType::Timer, audio_.IsDMG() ? 4 : 0, audio_.IsDMG());
            overflowPending = false;
            reloadActive = true;
        }

        ++divCounter;

        if (timerFallMask_ && (divCounter & timerFallMask_) == 0) {
            IncrementTIMA();
        }

        // Falling edge of the DIV-APU bit fires the frame sequencer; the rising
        // edge fires the secondary event that latches envelope clocks
        if ((divCounter & frameSeqToggleMask_) == 0) {
            const bool newFrameSeqSignal = divCounter & frameSeqBitMask_;
            // Evaluated per edge (512Hz), not per tick: speedSwitchFrameSeqDelay
            // can be restored by LoadState without a (tac, speed) key change
            const unsigned apuDelay = speed == Speed::Double && audio_.HasSpeedSwitchFrameSeqDelay() ? 4 : 0;
            if (apuDelay) {
                apuEventDelay = apuDelay;
                apuEventSecondary = newFrameSeqSignal;
            } else if (newFrameSeqSignal)
                audio_.TickFrameSequencerSecondary();
            else
                audio_.TickFrameSequencer();
        }
    }

    void RecomputeTickCache(Speed);

    void WriteByte(uint16_t, uint8_t, Speed);

    [[nodiscard]] uint8_t ReadByte(uint16_t) const;

    void WriteDIV(bool, bool duringStop = false);

    void WriteTAC(uint8_t);

    void WriteTIMA(uint8_t);

    void WriteTMA(uint8_t);

    [[nodiscard]] int TimerBit(uint8_t) const;

    void IncrementTIMA();
};
