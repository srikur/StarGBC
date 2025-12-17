#ifndef STARGBC_INSTRUCTIONTESTS_H
#define STARGBC_INSTRUCTIONTESTS_H

#include <Gameboy.h>
#include "doctest.h"
#include "mocks/MockBus.h"
#include "mocks/MockCPU.h"

class InstructionTests {
public:

    /**
     * Functions:
     * - LD rr, nn: Load 16-bit immediate into register pair
     *      1. 0x01 - LD BC, n16
     *
     * - LN (nn), SP: Load SP into given address
     * - LD, SP, HL: Load SP from HL
     * - PUSH rr: Push register pair onto stack
     * - POP rr: Pop register pair from stack
     * - LD HL, SP+e: Load SP plus signed immediate into HL
     */
    void Load16Bit() {
        // instructions
    }

    static void ExecuteAll() {
        InstructionTests tests{};
        tests.Load16Bit();
    }
private:
    Registers regs{};
    Interrupts interrupts{};
    MockBus bus{};
    MockCPU<MockBus> cpu{bus};
    Instructions<MockCPU<MockBus>> instructions{regs, interrupts};
};

#endif //STARGBC_INSTRUCTIONTESTS_H