#ifndef STARGBC_SERIAL_H
#define STARGBC_SERIAL_H

#include <fstream>
#include "Common.h"
#include "Interrupts.h"

struct Serial {
    explicit Serial(Interrupts &interrupts) : interrupts_(interrupts) {
    }

    [[nodiscard]] uint8_t ReadSerial(uint16_t) const;

    void WriteSerial(uint16_t, uint8_t, bool, bool);

    void ShiftOneBit();

    void Update();

    bool SaveState(std::ofstream &) const;

    bool LoadState(std::ifstream &);

    uint16_t ticksUntilShift_{0};
    uint16_t ticksPerBit_{0};
    uint8_t data_{0}; // SB
    uint8_t control_{0}; // SC
    uint8_t bitsShifted_{0};
    bool active_{false};
    Interrupts &interrupts_;
};


#endif //STARGBC_SERIAL_H
