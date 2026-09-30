#include "Interrupts.h"

void Interrupts::Set(const InterruptType interrupt, const bool delayed) {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    if (!delayed) {
        interruptFlag |= mask;
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
    if (visibleEarly) {
        if (visibleDelay) {
            interruptVisibleDelays[index] = visibleDelay;
        } else interruptVisiblePending |= mask;
    }
    if (interruptSetDelays[index] == 0) interruptSetDelays[index] = dots;
    interruptFlagDelayed |= mask;
}

void Interrupts::Tick() {
    for (unsigned i = 0; i < interruptSetDelays.size(); ++i) {
        const uint8_t mask = 1u << i;
        if (interruptVisibleDelays[i] && --interruptVisibleDelays[i] == 0) interruptVisiblePending |= mask;
        if (interruptSetDelays[i] && --interruptSetDelays[i] == 0) {
            interruptFlag |= interruptFlagDelayed & mask;
            interruptFlagDelayed &= ~mask;
            interruptVisiblePending &= ~mask;
        }
    }
}

bool Interrupts::IsSet(InterruptType interrupt) const {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    return (interruptFlag & mask) != 0;
}
