#ifndef STARGBC_CPU_H
#define STARGBC_CPU_H

#include "Bus.h"
#include "Inspection.h"
#include "Instructions.h"
#include "Registers.h"

template<BusLike BusT>
class CPU {
public:
    using Self = CPU<BusT>;

    explicit CPU(const Model requestedModel, const std::string &biosPath, const bool noBootrom, BusT &bus, Interrupts &interrupts,
                 Registers &registers) : bus_(bus), interrupts_(interrupts), regs_(registers) {
        const Model hw = ResolveModel(requestedModel, (bus.cartridge_.ReadByte(0x143) & 0x80) != 0);
        bus.memory_.initialize_power_on(hw);
        bus.gpu_.SetModel(hw);
        bus.gpu_.initialize_palette_ram();
        bus.audio_.SetModel(hw);
        const bool sgbFamily = IsSgb(hw);
        // The SGB BIOS only honors ICD2 command packets from SGB-flagged carts
        bus.joypad_.ConfigureSgb(sgbFamily && bus.cartridge_.ReadByte(0x146) == 0x03 && bus.cartridge_.ReadByte(0x14B) == 0x33);
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
            bus.cgbMode = IsCgb(hw) && (bus.cartridge_.ReadByte(0x143) & 0x80) == 0x80;
            bus.gpu_.dmg_compat = IsCgb(hw) && !bus.cgbMode;
            pc_ = 0x100;
            InitializeSystem();
        }
        SetInstructionLocation(pc_);
        currentInstruction = bus.ReadByte(pc_++, ComponentSource::CPU);
    }

    void InitializeBootrom(const std::string &) const;

    void InitializeEmbeddedBootrom(bool cgb) const;

    void InitializeSystem();

    void ExecuteMicroOp(Instructions<Self> &instructions, bool);

    void Lock() { locked_ = true; }

    [[nodiscard]] bool locked() const { return locked_; }

    [[nodiscard]] bool IsHalted() const { return halted_; }

    [[nodiscard]] bool AtInstructionBoundary() const {
        return !instrRunning && !prefixed && (interruptState == InterruptState::M1 || interruptState == InterruptState::M6);
    }

    [[nodiscard]] uint64_t InstructionsCompleted() const { return instructionsCompleted_; }

    void RestoreInstructionCount(const uint64_t count) { instructionsCompleted_ = count; }

    [[nodiscard]] uint16_t InstructionAddress() const { return instructionAddress_; }

    [[nodiscard]] uint16_t InstructionBank() const { return instructionBank_; }

    void ObserveWith(diagnostics::ObservationContext *context) { observations_ = context; }

    [[nodiscard]] CpuInspection Inspect() const {
        return {.pc = pc_,
                .sp = sp_,
                .af = regs_.GetAF(),
                .bc = regs_.GetBC(),
                .de = regs_.GetDE(),
                .hl = regs_.GetHL(),
                .instruction = currentInstruction,
                .nextInstruction = nextInstruction_,
                .instructionCycle = icount_,
                .machineCycle = mCycleCounter_,
                .sampledRunningInterrupts = runningPending_,
                .sampledHaltInterrupts = haltPending_,
                .interruptState = interruptState,
                .interruptMask = interruptMask,
                .instructionRunning = instrRunning,
                .prefixed = prefixed,
                .halted = halted_,
                .stopped = stopped_,
                .locked = locked_,
                .haltBug = haltBug_};
    }

    void SampleHaltInterrupts() {
        // Called before peripherals advance through T2. The DMG wake circuit uses this sample at T4; an edge later in the cycle waits for T2 again
        haltPending_ = interrupts_.interrupt_enable & interrupts_.interrupt_flag & 0x1F;
        if (halted_ && haltPending_ && !IsCgb(bus_.gpu_.model))
            bus_.dma_.halt_step_pending = true;
        if (observations_)
            observations_->Emit(diagnostics::Component::CPU, diagnostics::EventKind::InterruptSample, 0xFF0F, haltPending_, 1);
    }

    void SampleRunningInterrupts() {
        // The DMG instruction boundary samples before this T4's peripheral edges
        runningPending_ = interrupts_.interrupt_enable & interrupts_.interrupt_flag & 0x1F;
        if (observations_)
            observations_->Emit(diagnostics::Component::CPU, diagnostics::EventKind::InterruptSample, 0xFF0F, runningPending_, 3);
    }

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t> pc() { return pc_; }

    void pc(const uint16_t value) { pc_ = value; }

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t> sp() { return sp_; }

    void sp(const uint16_t value) { sp_ = value; }

    void icount(const uint8_t value) { icount_ = value; }

    [[nodiscard]] std::add_lvalue_reference_t<uint8_t> mCycleCounter() { return mCycleCounter_; }

    void mCycleCounter(const uint8_t value) { mCycleCounter_ = value; }

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t> nextInstruction() { return nextInstruction_; }

    void nextInstruction(const uint16_t value) { nextInstruction_ = value; }

    void halted(const bool value, const bool immediate_wake = false) {
        halted_ = value;
        halt_prefetch_retained = value && bus_.gpu_.hdma.block_requested;
        bus_.set_cpu_halted(value, false, immediate_wake);
        if (value)
            bus_.dma_.halt_step_pending = bus_.dma_.transferActive;
        if (!value)
            speedSwitchWakePending_ = false;
    }

    void SpeedSwitchHalt(const bool pending) {
        halted_ = !pending;
        speedSwitchWakePending_ = !pending;
        speed_switch_resume_pending = !pending;
        speed_switch_prefetch_retained = bus_.gpu_.hdma.block_requested;
        halt_prefetch_retained = false;
        bus_.set_cpu_halted(!pending, bus_.gpu_.doubleSpeed);
        shortInterruptEntry_ = pending && bus_.gpu_.model <= Model::CGBC && (interrupts_.interrupt_master_enable || interrupts_.interrupt_delay);
    }

    void haltBug(const bool value) { haltBug_ = value; }

    void stopped(const bool value) { stopped_ = value; }

    std::add_lvalue_reference_t<bool> stopped() { return stopped_; }

    Model model() { return bus_.gpu_.model; }

    BusT &bus_;
    uint16_t currentInstruction{0x0000};
    bool prefixed{false};

private:
    void ExecuteMicroOpValue(Instructions<Self> &instructions, bool hdmaActive);

    void SetInstructionLocation(const uint16_t address) {
        instructionAddress_ = address;
        if constexpr (requires { bus_.DebugBank(address); })
            instructionBank_ = bus_.DebugBank(address);
        if (observations_) {
            observations_->pc = instructionAddress_;
            observations_->pcBank = instructionBank_;
        }
    }

    uint8_t RunInstructionCycle(Instructions<Self> &, uint8_t, bool);

    [[nodiscard]] uint8_t InterruptAddress(uint8_t) const;

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
    bool halt_prefetch_retained{};
    bool speedSwitchWakePending_{false};
    bool speed_switch_resume_pending{};
    bool speed_switch_prefetch_retained{};
    bool shortInterruptEntry_{false};
    bool locked_{false};
    uint8_t haltPending_{0};
    uint8_t runningPending_{0};
    bool haltBug_{false};
    bool stopped_{false};

    InterruptState interruptState{InterruptState::M1};
    uint8_t interruptBit{0x00};
    uint8_t interruptMask{0x00};
    bool instrRunning{false};
    [[=NotStateAware]] uint64_t instructionsCompleted_{};
    [[=NotStateAware]] diagnostics::ObservationContext *observations_{};
    uint16_t instructionAddress_{};
    uint16_t instructionBank_{};
};

#endif // STARGBC_CPU_H
