#include <Gameboy.h>
#include "doctest.h"
#include "mocks/MockBus.h"

int ExecuteInstructionTests() {
    Registers regs{};
    Interrupts interrupts{};
    MockBus bus{};
    Instructions instructions{regs, bus, interrupts};
    return 0;
}
