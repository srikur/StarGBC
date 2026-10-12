#include "Interrupts.h"

void Interrupts::Set(const InterruptType interrupt, const bool delayed) {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    if (!delayed) {
        if (observations)
            observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptRequest, 0xFF0F, mask);
        interruptFlag |= mask;
        if (observations)
            observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptVisible,
                               0xFF0F, interruptFlag | interruptVisiblePending | 0xE0);
    } else {
        SetAfter(interrupt, 4);
    }
}

void Interrupts::SetAfter(const InterruptType interrupt, const uint8_t dots, const bool visibleEarly, const uint8_t visibleDelay) {
    if (dots == 0) {
        Set(interrupt, false);
        return;
    }
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    const auto index = static_cast<uint8_t>(interrupt);
    if (observations)
        observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptRequest,
                           0xFF0F, mask, dots | (visibleEarly << 8) | (visibleDelay << 16));
    if (visibleEarly) {
        if (visibleDelay) {
            interruptVisibleDelays[index] = visibleDelay;
        } else {
            interruptVisiblePending |= mask;
            if (observations)
                observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptVisible,
                                   0xFF0F, interruptFlag | interruptVisiblePending | 0xE0);
        }
    }
    if (interruptSetDelays[index] == 0)
        interruptSetDelays[index] = dots;
    interruptFlagDelayed |= mask;
    delaysPending_ = true;
}

void Interrupts::TickPending() {
    uint8_t remaining = 0;
    for (unsigned i = 0; i < interruptSetDelays.size(); ++i) {
        const auto previousFlags = interruptFlag | interruptVisiblePending;
        const uint8_t mask = 1u << i;
        if (interruptVisibleDelays[i] && --interruptVisibleDelays[i] == 0)
            interruptVisiblePending |= mask;
        if (interruptSetDelays[i] && --interruptSetDelays[i] == 0) {
            interruptFlag |= interruptFlagDelayed & mask;
            interruptFlagDelayed &= ~mask;
            interruptVisiblePending &= ~mask;
        }
        remaining |= interruptSetDelays[i] | interruptVisibleDelays[i];
        if (observations && previousFlags != (interruptFlag | interruptVisiblePending))
            observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptVisible,
                               0xFF0F, interruptFlag | interruptVisiblePending | 0xE0);
    }
    delaysPending_ = remaining != 0;
}

bool Interrupts::IsSet(InterruptType interrupt) const {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    return (interruptFlag & mask) != 0;
}
