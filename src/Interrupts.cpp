#include "Interrupts.h"

void Interrupts::Set(const InterruptType interrupt, const bool delayed) {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    if (!delayed) {
        interruptFlag |= mask;
        // Keep an in-flight delayed snapshot coherent so its commit doesn't erase this bit
        if (interruptSetDelay > 0) interruptFlagDelayed |= mask;
    } else if (interruptSetDelay > 0) {
        // A delayed set is already pending (e.g., VBlank + the OAM STAT quirk at line 144 in the same dot)
        interruptFlagDelayed |= mask;
    } else {
        interruptSetDelay = 4;
        interruptFlagDelayed = interruptFlag | mask;
    }
}

void Interrupts::SetAfter(const InterruptType interrupt, const uint8_t dots) {
    if (dots == 0) {
        Set(interrupt, false);
        return;
    }
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    if (interruptSetDelay > 0) {
        interruptFlagDelayed |= mask;
    } else {
        interruptSetDelay = dots;
        interruptFlagDelayed = interruptFlag | mask;
    }
}

bool Interrupts::IsSet(InterruptType interrupt) const {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    return (interruptFlag & mask) != 0;
}
