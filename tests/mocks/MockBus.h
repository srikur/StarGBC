#ifndef STARGBC_MOCKBUS_H
#define STARGBC_MOCKBUS_H

#include <cstdint>
#include <unordered_map>
#include "Instructions.h"

struct MockBus {
    uint8_t ReadByte(const uint16_t address) {
        return memory_.contains(address) ? memory_[address] : 0x00;
    }

    void WriteByte(const uint16_t address, const uint8_t value) {
        memory_[address] = value;
    }

    void ChangeSpeed() {
        prepareSpeedShift = false;
    }

    void HandleOAMCorruption(const uint16_t location, const CorruptionType type) const {
        return;
    }

    bool prepareSpeedShift{false};

private:
    std::unordered_map<uint16_t, uint8_t> memory_{};
};

#endif //STARGBC_MOCKBUS_H
