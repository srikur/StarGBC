#ifndef STARGBC_CPU_H
#define STARGBC_CPU_H

#include "Bus.h"
#include "Instructions.h"
#include "Registers.h"

template<BusLike BusT>
class CPU {
public:
    using Self = CPU<BusT>;

    explicit CPU(const Mode mode,
                 const std::string &biosPath,
                 BusT &bus,
                 Interrupts &interrupts,
                 Registers &registers) : bus_(bus),
                                         interrupts_(interrupts),
                                         regs_(registers),
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
            pc_ = 0x0000;
        } else {
            pc_ = 0x100;
            InitializeSystem(mode);
        }
        currentInstruction = bus.ReadByte(pc_++, ComponentSource::CPU);
    }

    void InitializeBootrom(const std::string &) const;

    void InitializeSystem(Mode);

    void ExecuteMicroOp(Instructions<Self> &instructions);

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t> pc() {
        return pc_;
    }

    void pc(const uint16_t value) {
        pc_ = value;
    }

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t>  sp() {
        return sp_;
    }

    void sp(const uint16_t value) {
        sp_ = value;
    }

    void icount(const uint8_t value) {
        icount_ = value;
    }

    [[nodiscard]] std::add_lvalue_reference_t<uint8_t> mCycleCounter() {
        return mCycleCounter_;
    }

    void mCycleCounter(const uint8_t value) {
        mCycleCounter_ = value;
    }

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t> nextInstruction() {
        return nextInstruction_;
    }

    void nextInstruction(const uint16_t value) {
        nextInstruction_ = value;
    }

    void halted(const bool value) {
        halted_ = value;
    }

    void haltBug(const bool value) {
        haltBug_ = value;
    }

    void stopped(const bool value) {
        stopped_ = value;
    }

    std::add_lvalue_reference_t<bool> stopped() {
        return stopped_;
    }

    BusT &bus_;
    uint16_t currentInstruction{0x0000};
    bool prefixed{false};

private:
    uint8_t RunInstructionCycle(Instructions<Self> &, uint8_t, bool);

    uint8_t InterruptAddress(uint8_t) const;

    bool ProcessInterrupts();

    void BeginMCycle();

    bool AdvanceTCycle();

    void RunPostCompletion(Instructions<Self> &);

    Interrupts &interrupts_;
    Registers &regs_;

    Mode mode_{Mode::DMG};

    uint16_t pc_{0x0000};
    uint16_t sp_{0x0000};
    uint8_t icount_{0};
    uint8_t mCycleCounter_{0x01};
    uint16_t nextInstruction_{0x0000};
    bool halted_{false};
    bool haltBug_{false};
    bool stopped_{false};

    InterruptState interruptState{InterruptState::M1};
    uint8_t tCycleCounter{0x00};
    uint8_t interruptBit{0x00};
    uint8_t interruptMask{0x00};
    bool instrRunning{false};
};

#endif //STARGBC_CPU_H
