#ifndef STARGBC_CPU_H
#define STARGBC_CPU_H

#include <cstdint>
#include <memory>

#include "Bus.h"
#include "Instructions.h"
#include "Registers.h"

class Instructions;

class CPU {
public:
    void Save() const;

    void KeyDown(Keys) const;

    void KeyUp(Keys) const;

    uint32_t *GetScreenData() const;

    void InitializeBootrom(const std::string &) const;

    bool IsDMG() const;

private:
    friend class Instructions;

    uint8_t DecodeInstruction(uint8_t, bool);

    std::unique_ptr<Bus> bus{nullptr};
    std::unique_ptr<Registers> regs{std::make_unique<Registers>()};
    std::unique_ptr<Instructions> instructions{std::make_unique<Instructions>()};

    uint16_t pc{0x0000};
    uint16_t sp{0x0000};
    uint8_t icount{0};
    uint8_t mCycleCounter{0x01};
    uint16_t nextInstruction{0x0000};
    bool halted{false};
    bool haltBug{false};
    bool stopped{false};
    uint16_t currentInstruction{0x0000};
    bool prefixed{false};

};

#endif //STARGBC_CPU_H
