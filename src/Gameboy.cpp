#include "Gameboy.h"

#include <fstream>
#include <limits>

static constexpr uint32_t kFrameCyclesCGB = Gameboy::FRAME_CYCLES_DMG * 2;

diagnostics::ExecutionPosition Gameboy::ExecutionPosition() const {
    return {.tick = timelineTicks_,
            .frame = gpu_.displayFrames_,
            .dot = static_cast<uint16_t>(gpu_.scanlineCounter),
            .ly = gpu_.currentLine,
            .cpuPhase = static_cast<uint8_t>(cpuTickPhase_ & 3)};
}

void Gameboy::SynchronizeObservationPosition() {
    if (!observations_ && !gpu_.provenance)
        return;
    const auto position = ExecutionPosition();
    if (observations_) {
        observations_->position = position;
        observations_->pc = cpu_.InstructionAddress();
        observations_->pcBank = cpu_.InstructionBank();
    }
    if (gpu_.provenance)
        gpu_.provenance->position = position;
}

void Gameboy::ObserveWith(diagnostics::ObservationContext *context) {
    observations_ = context;
    cpu_.ObserveWith(context);
    bus_.observations = context;
    interrupts_.observations = context;
    timer_.observations = context;
    gpu_.observations = context;
    audio_.ObserveWith(context);
    SynchronizeObservationPosition();
}

void Gameboy::RecordPixelsWith(diagnostics::PixelProvenanceContext *context) {
    gpu_.provenance = context;
    if (context) {
        context->ResetPipeline();
        context->branch = observations_ ? observations_->branch : 0;
    }
    SynchronizeObservationPosition();
}

bool Gameboy::ConsumeFrame() { return std::exchange(gpu_.frameReady, false); }

void Gameboy::Save() const { cartridge_.Save(); }

void Gameboy::ObserveInput(const uint8_t previous) {
    const auto buttons = PressedButtons();
    if (buttons == previous)
        return;
    if (inputSink_ && inputSink_->applied)
        inputSink_->applied(inputSink_->owner, timelineTicks_, buttons);
    if (observations_) {
        SynchronizeObservationPosition();
        observations_->Emit(diagnostics::Component::Input, diagnostics::EventKind::InputApplied, 0xFF00, buttons, previous);
    }
}

void Gameboy::KeyUp(const Keys key) {
    const auto previous = PressedButtons();
    joypad_.KeyUp(key);
    ObserveInput(previous);
}

void Gameboy::KeyDown(const Keys key) {
    const auto previous = PressedButtons();
    joypad_.KeyDown(key);
    ObserveInput(previous);
}

void Gameboy::ApplyButtons(const uint8_t buttons) {
    const auto changed = buttons ^ PressedButtons();
    for (unsigned bit = 1; bit <= 0x80; bit <<= 1) {
        if (!(changed & bit))
            continue;
        if (buttons & bit)
            KeyDown(static_cast<Keys>(bit));
        else
            KeyUp(static_cast<Keys>(bit));
    }
}

const uint32_t *Gameboy::GetScreenData() const { return gpu_.GetScreenData(); }

bool Gameboy::LoadedStateValid() const {
    return gpu_.model != Model::Auto && gpu_.model == audio_.GetModel() && (!bus_.cgbMode || IsCgb(gpu_.model)) &&
           gpu_.dmg_compat == (IsCgb(gpu_.model) && !bus_.cgbMode) && gpu_.doubleSpeed == (bus_.speed == Speed::Double) &&
           gpu_.backgroundQueue.valid() && gpu_.spriteFetchQueue.valid() && gpu_.spriteBuffer.valid() && gpu_.vramBank <= 1 &&
           gpu_.currentLine <= 153 && gpu_.pixelsDrawn <= SCREEN_WIDTH && memory_.wramBank_ <= 7 && cartridge_.BankingStateValid() &&
           (!bus_.bootromRunning || !bus_.bootrom.empty());
}

// One whole M-cycle (4 dots, up to 8 master cycles) at regular speed with the
// CPU phase aligned: the per-dot phase tests constant-fold, the speed/stop
// checks hoist to the caller, and quiet GPU dots skip the Update call
// entirely. Mirrors the regular-speed path of AdvanceCycles dot for dot.
uint32_t Gameboy::AdvanceMCycle() {
    const auto dot = [&](const unsigned phase) {
        if (phase == 3)
            cpu_.sample_running_inputs();
        if (phase == 1)
            cpu_.SampleHaltInterrupts();
        timer_.Tick(Speed::Regular);
        rtc_.Update();
        audio_.Tick();
        serial_.update(timer_.divCounter);
        interrupts_.tick_cpu();
        if (dma_.transferActive) {
            gpu_.oam_dma_active = dma_.ticks > DMA::STARTUP_CYCLES;
            gpu_.oamDmaDest_ = dma_.currentByte;
        } else if (gpu_.oam_dma_active) {
            gpu_.oam_dma_active = false;
        }
        bus_.UpdateDMA();
        if (gpu_.idleDots_ > 0) {
            interrupts_.tick();
            --gpu_.idleDots_;
            ++gpu_.scanlineCounter;
        } else if (gpu_.lcdOffIdle_) {
            interrupts_.tick();
        } else {
            gpu_.Update();
        }
        bus_.RunHDMA(phase == 0 && cpu_.AtInstructionBoundary());
        if (bus_.speedSwitchHalt > 0) {
            --bus_.speedSwitchHalt;
            if (interrupts_.interrupt_enable & interrupts_.interrupt_flag & 0x1F)
                bus_.speedSwitchHalt = 0;
            else if (bus_.speedSwitchHalt == 0) {
                const bool wasHalted = cpu_.IsHalted();
                cpu_.halted(false, true);
                if (observations_ && wasHalted)
                    observations_->Emit(diagnostics::Component::CPU, diagnostics::EventKind::Halt, 0, false);
            }
        }
    };
    dot(0);
    dot(1);
    dot(2);
    dot(3);
    cpuTickPhase_ += 4;
    cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU() || bus_.speedSwitchHalt > 0);
    // A STOP/speed switch executed this M-cycle consumes only 1 master cycle
    // on its final dot (matching AdvanceCycles), leaving masterCycles odd so
    // the slow path takes over
    const uint32_t consumed = bus_.speed == Speed::Regular && !cpu_.stopped() ? 8 : 7;
    masterCycles += consumed;
    return consumed;
}

uint32_t Gameboy::AdvanceMCycleDouble() {
    const auto cycle = [&](const unsigned phase) {
        if (phase == 3)
            cpu_.sample_running_inputs();
        if (phase == 1)
            cpu_.SampleHaltInterrupts();
        timer_.Tick(Speed::Double);
        const bool evenCycle = (phase & 1) == 0;
        if (evenCycle) {
            rtc_.Update();
            audio_.Tick();
        }
        serial_.update(timer_.divCounter);
        interrupts_.tick_cpu();
        if (evenCycle) {
            if (dma_.transferActive) {
                gpu_.oam_dma_active = dma_.ticks > DMA::STARTUP_CYCLES;
                gpu_.oamDmaDest_ = dma_.currentByte;
            } else if (gpu_.oam_dma_active) {
                gpu_.oam_dma_active = false;
            }
        }
        bus_.UpdateDMA();
        if (evenCycle) {
            if (gpu_.idleDots_ > 0) {
                interrupts_.tick();
                --gpu_.idleDots_;
                ++gpu_.scanlineCounter;
            } else if (gpu_.lcdOffIdle_) {
                interrupts_.tick();
            } else {
                gpu_.Update();
            }
            bus_.RunHDMA(phase == 0 && cpu_.AtInstructionBoundary());
        }
        if (bus_.speedSwitchHalt > 0) {
            --bus_.speedSwitchHalt;
            if (interrupts_.interrupt_enable & interrupts_.interrupt_flag & 0x1F)
                bus_.speedSwitchHalt = 0;
            else if (bus_.speedSwitchHalt == 0) {
                const bool wasHalted = cpu_.IsHalted();
                cpu_.halted(false, true);
                if (observations_ && wasHalted)
                    observations_->Emit(diagnostics::Component::CPU, diagnostics::EventKind::Halt, 0, false);
            }
        }
    };
    cycle(0);
    cycle(1);
    cycle(2);
    cycle(3);
    cpuTickPhase_ += 4;
    cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU() || bus_.speedSwitchHalt > 0);
    masterCycles += 4;
    return 4;
}

uint32_t Gameboy::AdvanceCycles(const uint32_t maxCycles) {
    if (masterCycles >= CGB_CYCLES_PER_SECOND)
        masterCycles -= CGB_CYCLES_PER_SECOND;
    if (cpu_.stopped()) {
        if (bus_.joypad_.KeyPressed()) {
            cpu_.stopped() = false;
            if (observations_)
                observations_->Emit(diagnostics::Component::CPU, diagnostics::EventKind::Stop, 0, false);
        } else {
            masterCycles++;
            return 1;
        }
    }
    if (bus_.speed == Speed::Regular) {
        if (masterCycles % 2 != 0) {
            masterCycles++;
            return 1;
        }
        if ((cpuTickPhase_ & 3) == 3)
            cpu_.sample_running_inputs();
        if ((cpuTickPhase_ & 3) == 1)
            cpu_.SampleHaltInterrupts();
        timer_.Tick(bus_.speed);
        rtc_.Update();
        audio_.Tick();
        serial_.update(timer_.divCounter);
        interrupts_.tick_cpu();
        if (dma_.transferActive) {
            gpu_.oam_dma_active = dma_.ticks > DMA::STARTUP_CYCLES;
            gpu_.oamDmaDest_ = dma_.currentByte;
        } else if (gpu_.oam_dma_active) {
            gpu_.oam_dma_active = false;
        }
        bus_.UpdateDMA();
        gpu_.Update();
        bus_.RunHDMA((cpuTickPhase_ & 3) == 0 && cpu_.AtInstructionBoundary());
        if (bus_.speedSwitchHalt > 0) {
            --bus_.speedSwitchHalt;
            if (interrupts_.interrupt_enable & interrupts_.interrupt_flag & 0x1F)
                bus_.speedSwitchHalt = 0;
            else if (bus_.speedSwitchHalt == 0) {
                const bool wasHalted = cpu_.IsHalted();
                cpu_.halted(false, true);
                if (observations_ && wasHalted)
                    observations_->Emit(diagnostics::Component::CPU, diagnostics::EventKind::Halt, 0, false);
            }
        }
        if ((++cpuTickPhase_ & 3) == 0) {
            cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU() || bus_.speedSwitchHalt > 0);
        }
        const uint32_t consumed = bus_.speed == Speed::Regular && !cpu_.stopped() && maxCycles >= 2 ? 2 : 1;
        masterCycles += consumed;
        return consumed;
    }
    const bool evenCycle = masterCycles % 2 == 0;
    if ((cpuTickPhase_ & 3) == 3)
        cpu_.sample_running_inputs();
    if ((cpuTickPhase_ & 3) == 1)
        cpu_.SampleHaltInterrupts();
    timer_.Tick(bus_.speed);
    if (evenCycle) {
        rtc_.Update();
        audio_.Tick();
    }
    serial_.update(timer_.divCounter);
    interrupts_.tick_cpu();
    if (evenCycle) {
        if (dma_.transferActive) {
            gpu_.oam_dma_active = dma_.ticks > DMA::STARTUP_CYCLES;
            gpu_.oamDmaDest_ = dma_.currentByte;
        } else if (gpu_.oam_dma_active) {
            gpu_.oam_dma_active = false;
        }
    }
    bus_.UpdateDMA();
    if (evenCycle) {
        gpu_.Update();
        // After a speed switch, T1 can fall on an odd master tick. The first
        // PPU dot of that machine cycle is then T2 rather than T1.
        bus_.RunHDMA((cpuTickPhase_ & 3) < 2 && cpu_.AtInstructionBoundary());
    }
    if (bus_.speedSwitchHalt > 0) {
        --bus_.speedSwitchHalt;
        if (interrupts_.interrupt_enable & interrupts_.interrupt_flag & 0x1F)
            bus_.speedSwitchHalt = 0;
        else if (bus_.speedSwitchHalt == 0) {
            const bool wasHalted = cpu_.IsHalted();
            cpu_.halted(false, true);
            if (observations_ && wasHalted)
                observations_->Emit(diagnostics::Component::CPU, diagnostics::EventKind::Halt, 0, false);
        }
    }
    if ((++cpuTickPhase_ & 3) == 0) {
        cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU() || bus_.speedSwitchHalt > 0);
    }
    masterCycles++;
    return 1;
}

ExecutionResult Gameboy::AdvanceTicks(const uint64_t ticks, const ExecutionPolicy policy) {
    if (ticks > std::numeric_limits<uint64_t>::max() - timelineTicks_)
        throw std::out_of_range("Timeline deadline overflows master ticks");
    const auto begin = timelineTicks_;
    const auto instructions = cpu_.InstructionsCompleted();
    const bool optimized = policy == ExecutionPolicy::Optimized;
    if (gpu_.optimized_execution != optimized) {
        gpu_.InvalidateIdle();
        gpu_.optimized_execution = optimized;
    }
    audio_.SetOptimizedExecution(optimized);
    uint64_t remaining = ticks;
    while (remaining > 0) {
        if (!observations_ && !gpu_.provenance && policy == ExecutionPolicy::Optimized && !cpu_.stopped() && (cpuTickPhase_ & 3) == 0 &&
            (masterCycles & 1) == 0) [[likely]] {
            if (bus_.speed == Speed::Regular && remaining >= 8) {
                if (masterCycles >= CGB_CYCLES_PER_SECOND)
                    masterCycles -= CGB_CYCLES_PER_SECOND;
                const auto consumed = AdvanceMCycle();
                remaining -= consumed;
                timelineTicks_ += consumed;
                continue;
            }
            if (bus_.speed == Speed::Double && remaining >= 4) {
                if (masterCycles >= CGB_CYCLES_PER_SECOND)
                    masterCycles -= CGB_CYCLES_PER_SECOND;
                const auto consumed = AdvanceMCycleDouble();
                remaining -= consumed;
                timelineTicks_ += consumed;
                continue;
            }
        }
        SynchronizeObservationPosition();
        const auto consumed = AdvanceCycles(static_cast<uint32_t>(std::min<uint64_t>(remaining, 2)));
        remaining -= consumed;
        timelineTicks_ += consumed;
    }
    if (ticks)
        audio_.CatchUp();
    SynchronizeObservationPosition();
    return {ExecutionStopReason::TickBudget, begin, timelineTicks_, cpu_.InstructionsCompleted() - instructions};
}

ExecutionResult Gameboy::StepInstruction(const uint64_t maxTicks) {
    if (maxTicks > std::numeric_limits<uint64_t>::max() - timelineTicks_)
        throw std::out_of_range("Instruction deadline overflows master ticks");
    const auto begin = timelineTicks_;
    const auto instructions = cpu_.InstructionsCompleted();
    const auto stoppedReason = [&]() -> std::optional<ExecutionStopReason> {
        const auto cpu = cpu_.Inspect();
        if (cpu.locked)
            return ExecutionStopReason::Locked;
        if (cpu.stopped)
            return ExecutionStopReason::Stopped;
        if (cpu.halted)
            return ExecutionStopReason::Halted;
        return std::nullopt;
    };
    if (const auto reason = stoppedReason())
        return {*reason, begin, begin, 0};
    while (timelineTicks_ - begin < maxTicks) {
        static_cast<void>(AdvanceTicks(1));
        if (cpu_.InstructionsCompleted() != instructions)
            return {ExecutionStopReason::InstructionBoundary, begin, timelineTicks_, cpu_.InstructionsCompleted() - instructions};
        if (const auto reason = stoppedReason())
            return {*reason, begin, timelineTicks_, 0};
    }
    return {ExecutionStopReason::TickBudget, begin, timelineTicks_, 0};
}

void Gameboy::RunFrame() { static_cast<void>(AdvanceTicks(kFrameCyclesCGB, executionPolicy_)); }
