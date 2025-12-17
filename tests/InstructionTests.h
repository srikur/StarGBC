#ifndef STARGBC_INSTRUCTIONTESTS_H
#define STARGBC_INSTRUCTIONTESTS_H

#include <Gameboy.h>
#include <semaphore>
#include <thread>

#include "doctest.h"
#include "ThreadContext.h"
#include "mocks/MockBus.h"
#include "mocks/MockCPU.h"

class InstructionTests {
protected:
    Registers regs{};
    Interrupts interrupts{};
    MockBus bus{};
    MockCPU<MockBus> cpu{bus};
    Instructions<MockCPU<MockBus>> instructions{regs, interrupts};

    void Reset() {
        regs = Registers{};
        interrupts = Interrupts{};
        bus = MockBus{};
        cpu.Reset();
        instructions.ResetState();
    }
};

TEST_CASE_FIXTURE(InstructionTests, "LD BC, n16 (0x01)") {
    Reset();
    cpu.sp(0xEE);
    cpu.pc(0x100);
    cpu.mCycleCounter(1);
    cpu.prefixed = false;
    cpu.currentInstruction = 0x01;
    bus.WriteByte(0x100, 0x01);
    bus.WriteByte(0x101, 0x12);
    bus.WriteByte(0x102, 0x34);

    bool completed{};
    completed = cpu.ExecuteMicroOp(instructions); // M2
    CHECK_EQ(completed, false);
    completed = cpu.ExecuteMicroOp(instructions); // M3
    CHECK_EQ(completed, false);
    completed = cpu.ExecuteMicroOp(instructions); // M4
    CHECK_EQ(completed, true);

    CHECK_EQ(regs.GetBC(), 0x1201);
    CHECK_EQ(cpu.pc(), 0x103);
}


inline int ExecuteInstructionTests(const int argc, char **argv) {
    std::vector<char *> doctest_args;
    doctest_args.reserve(argc);
    doctest_args.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (constexpr std::string_view kPrefix = "--max-threads="; arg.rfind(kPrefix, 0) == 0) {
            try {
                maxThreads = std::stoul(std::string(arg.substr(kPrefix.size())));
            } catch (...) {
                std::cerr << "Invalid value for --max-threads" << std::endl;
                return EXIT_FAILURE;
            }
        } else {
            doctest_args.push_back(argv[i]);
        }
    }

    if (maxThreads == 0) { maxThreads = 1; }
    threadSemaphore = std::make_shared<std::counting_semaphore<> >(maxThreads);

    doctest::Context ctx;
    ctx.setOption("test-case-exclude", "*blargg*");
    ctx.applyCommandLine(static_cast<int>(doctest_args.size()), doctest_args.data());
    return ctx.run();
}

#endif //STARGBC_INSTRUCTIONTESTS_H
