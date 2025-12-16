#include "DMA.h"

void DMA::Set(const uint8_t value) {
    writtenValue = value;
    const uint16_t newSource = static_cast<uint16_t>(value) << 8;

    if (transferActive && ticks >= STARTUP_CYCLES) {
        // A transfer is already past the setup window --> schedule a restart
        restartPending = true;
        pendingStart = newSource;
        restartCountdown = STARTUP_CYCLES + 1;
    } else {
        // No DMA (or still in 4-cycle setup) --> start immediately.
        transferActive = true;
        startAddress = newSource;
        currentByte = 0;
        ticks = 0;
    }
}
