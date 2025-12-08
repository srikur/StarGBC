#ifndef STARGBC_CPU_H
#define STARGBC_CPU_H

#include "Bus.h"
#include "Instructions.h"
#include "Registers.h"

class Instructions;

class CPU {
public:
    explicit CPU(const Mode mode, const std::string &biosPath, Bus &bus, Interrupts &interrupts) : bus_(bus),
                                                                                                   interrupts_(interrupts),
                                                                                                   instructions_(std::make_unique<Instructions>(regs_, bus_, interrupts_)),
                                                                                                   mode_(mode) {
        if (mode_ != Mode::None) {
            bus.gpu_.hardware = mode == Mode::DMG ? Hardware::DMG : Hardware::CGB;
            bus.audio_.SetDMG(bus.gpu_.hardware == Hardware::DMG);
        } else if ((bus.cartridge_.ReadByte(0x143) & 0x80) == 0x80) {
            mode_ = Mode::CGB_GBC;
            bus.gpu_.hardware = Hardware::CGB;
            bus.audio_.SetDMG(false);
        }
        if (!biosPath.empty()) {
            bus.bootromRunning = true;
            InitializeBootrom(biosPath);
        } else {
            pc = 0x100;
            InitializeSystem(mode);
            currentInstruction = bus.ReadByte(pc++);
        }
    }

    void InitializeBootrom(const std::string &) const;

    bool IsDMG() const;

    void InitializeSystem(Mode);

    void ExecuteMicroOp();

private:
    friend class Instructions;

    uint8_t DecodeInstruction(uint8_t, bool);

    uint8_t InterruptAddress(uint8_t) const;

    bool ProcessInterrupts();

    Bus &bus_;
    Interrupts &interrupts_;
    Registers regs_{};
    std::unique_ptr<Instructions> instructions_{nullptr};

    Mode mode_{Mode::DMG};

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

    InterruptState interruptState{InterruptState::M1};
    uint8_t tCycleCounter{0x00};
    uint8_t interruptBit{0x00};
    uint8_t interruptMask{0x00};
    bool instrRunning{false};
};

#endif //STARGBC_CPU_H
