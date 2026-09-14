#ifndef STARGBC_INTERRUPTS_H
#define STARGBC_INTERRUPTS_H

#include "Common.h"

struct Interrupts {
    uint8_t interruptEnable{0x00};
    uint8_t interruptFlag{0xE1};
    uint8_t interruptFlagDelayed{0x00};
    uint8_t interruptVisiblePending{0x00};
    uint8_t interruptSetDelay{0x00};
    bool interruptMasterEnable{false};
    bool interruptDelay{false};

    void Set(InterruptType, bool);

    void SetAfter(InterruptType, uint8_t dots, bool visibleEarly = false);

    [[nodiscard]] bool IsSet(InterruptType) const;
};

#endif //STARGBC_INTERRUPTS_H
