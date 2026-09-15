#ifndef STARGBC_CPU_H
#define STARGBC_CPU_H

#include "Bus.h"
#include "Instructions.h"
#include "Registers.h"

template<BusLike BusT>
class CPU {
public:
    using Self = CPU<BusT>;

    explicit CPU(const Model requestedModel,
                 const std::string &biosPath,
                 const bool noBootrom,
                 BusT &bus,
                 Interrupts &interrupts,
                 Registers &registers) : bus_(bus),
                                         interrupts_(interrupts),
                                         regs_(registers) {
        const Model hw = ResolveModel(requestedModel, (bus.cartridge_.ReadByte(0x143) & 0x80) != 0);
        bus.gpu_.model = hw;
        bus.audio_.SetModel(hw);
        const bool sgbFamily = IsSgb(hw);
        // The SGB BIOS only honors ICD2 command packets from SGB-flagged carts
        bus.joypad_.ConfigureSgb(sgbFamily &&
                                 bus.cartridge_.ReadByte(0x146) == 0x03 &&
                                 bus.cartridge_.ReadByte(0x14B) == 0x33);
        if (!biosPath.empty()) {
            // CGB bootroms run in CGB mode; KEY0 writes can drop to DMG-compat
            bus.cgbMode = IsCgb(hw);
            bus.bootromRunning = true;
            // Power-on DIV phase, calibrated so the official bootroms hand off
            // with the DIV value and phase mooneye's boot_div tests verify
            bus.timer_.divCounter = 0x0008;
            InitializeBootrom(biosPath);
            pc_ = 0x0000;
        } else if (!noBootrom && !sgbFamily) {
            bus.cgbMode = IsCgb(hw);
            bus.bootromRunning = true;
            bus.timer_.divCounter = 0x0008;
            InitializeEmbeddedBootrom(IsCgb(hw));
            embeddedBootrom_ = true;
            pc_ = 0x0000;
        } else {
            bus.cgbMode = IsCgb(hw) &&
                          (bus.cartridge_.ReadByte(0x143) & 0x80) == 0x80;
            bus.gpu_.dmgCompat = IsCgb(hw) && !bus.cgbMode;
            pc_ = 0x100;
            InitializeSystem();
        }
        currentInstruction = bus.ReadByte(pc_++, ComponentSource::CPU);
    }

    void InitializeBootrom(const std::string &) const;

    void InitializeEmbeddedBootrom(bool cgb) const;

    void InitializeSystem();

    void ExecuteMicroOp(Instructions<Self> &instructions, bool);

    void Lock() { locked_ = true; }

    [[nodiscard]] bool locked() const { return locked_; }

    void SampleHaltInterrupts() {
        // Called before peripherals advance through T2. The DMG wake circuit uses this sample at T4; an edge later in the cycle waits for T2 again
        haltPending_ = interrupts_.interruptEnable & interrupts_.interruptFlag & 0x1F;
    }

    void SampleRunningInterrupts() {
        // The DMG instruction boundary samples before this T4's peripheral edges
        runningPending_ = interrupts_.interruptEnable & interrupts_.interruptFlag & 0x1F;
    }

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

    Model model() {
        return bus_.gpu_.model;
    }

    BusT &bus_;
    uint16_t currentInstruction{0x0000};
    bool prefixed{false};

private:
    uint8_t RunInstructionCycle(Instructions<Self> &, uint8_t, bool);

    uint8_t InterruptAddress(uint8_t) const;

    bool ProcessInterrupts();

    void BeginMCycle();

    void RunPostCompletion(Instructions<Self> &);

    Interrupts &interrupts_;
    Registers &regs_;

    [[=NotStateAware]] bool embeddedBootrom_{false};

    uint16_t pc_{0x0000};
    uint16_t sp_{0x0000};
    uint8_t icount_{0};
    uint8_t mCycleCounter_{0x01};
    uint16_t nextInstruction_{0x0000};
    bool halted_{false};
    bool locked_{false};
    uint8_t haltPending_{0};
    uint8_t runningPending_{0};
    bool haltBug_{false};
    bool stopped_{false};

    InterruptState interruptState{InterruptState::M1};
    uint8_t interruptBit{0x00};
    uint8_t interruptMask{0x00};
    bool instrRunning{false};
};

#endif //STARGBC_CPU_H
