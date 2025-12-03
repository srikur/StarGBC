#ifndef STARGBC_SERIAL_H
#define STARGBC_SERIAL_H

#include <fstream>
#include "Common.h"

struct Serial {
    [[nodiscard]] uint8_t ReadSerial(uint16_t) const;

    void WriteSerial(uint16_t, uint8_t, bool, bool);

    void ShiftOneBit();

    bool SaveState(std::ofstream &) const;

    bool LoadState(std::ifstream &);

    uint32_t ticksUntilShift_{0};
    uint32_t ticksPerBit_{0};
    uint8_t data_{0}; // SB
    uint8_t control_{0}; // SC
    uint8_t bitsShifted_{0};
    bool active_{false};
};


#endif //STARGBC_SERIAL_H
