#include "Debugger.h"

#include <format>

Debugger::Debugger(EmulationSession &session) : session_(session), machine_(session.Machine()), state_generation_(session.StateGeneration()) {
    if (machine_.Observations())
        throw std::logic_error("The machine already has an observation owner");
    observations_.owner = this;
    observations_.callback = observe;
    machine_.ObserveWith(&observations_);
    pause();
}

Debugger::~Debugger() {
    if (machine_.Observations() == &observations_)
        machine_.ObserveWith(nullptr);
}

uint8_t Debugger::read_memory(const uint16_t address, const bool cpu_visible) const {
    if (cpu_visible || (address > OAM_END && address < HIGH_RAM_BEGIN) || address == INTERRUPT_ENABLE)
        return machine_.InspectCpuRead(address);
    try {
        return machine_.DebugPeek(address);
    } catch (const std::invalid_argument &) {
        return machine_.InspectCpuRead(address);
    }
}

DisassembledInstruction Debugger::instruction(const uint16_t address) const {
    return disassemble(address,
                       {read_memory(address), read_memory(static_cast<uint16_t>(address + 1)), read_memory(static_cast<uint16_t>(address + 2))});
}

void Debugger::write_memory(const uint16_t address, const uint8_t value) {
    if (!paused())
        throw std::logic_error("Pause before editing memory");
    machine_.DebugPoke(address, value);
    status_ = std::format("Wrote ${:02X} to mapped RAM ${:04X}", value, address);
}

void Debugger::pause() {
    session_.SetPaused(true);
    target_.reset();
    status_ = "Paused";
}

void Debugger::skip_current_breakpoint() {
    skip_instruction_ = machine_.InstructionsCompleted();
    skip_address_ = machine_.debug_instruction_address();
    skip_bank_ = machine_.debug_instruction_bank();
    execution_hit_ = false;
}

void Debugger::resume() {
    synchronize();
    if (execution_hit_)
        skip_current_breakpoint();
    else
        skip_instruction_.reset();
    target_.reset();
    session_.SetPaused(false);
    status_ = "Running";
}

void Debugger::synchronize() {
    if (state_generation_ == session_.StateGeneration())
        return;
    state_generation_ = session_.StateGeneration();
    clear_trace();
    skip_instruction_.reset();
    watch_hit_.reset();
    execution_hit_ = false;
    pause();
    status_ = "State loaded; paused";
}

void Debugger::clear_trace() { trace_head_ = trace_size_ = 0; }

const DebugTraceEntry &Debugger::trace_entry(const size_t index) const {
    if (index >= trace_size_)
        throw std::out_of_range("trace index");
    return trace_[(trace_head_ + trace_.size() - trace_size_ + index) % trace_.size()];
}

void Debugger::observe(void *owner, const diagnostics::Event &event) noexcept {
    auto &debugger{*static_cast<Debugger *>(owner)};
    using enum diagnostics::EventKind;
    if (event.kind == InstructionBegin || event.kind == InterruptVector) {
        auto &entry{debugger.trace_[debugger.trace_head_]};
        entry = {.event = event};
        if (event.kind == InstructionBegin) {
            entry.bytes[0] = event.value;
            try {
                entry.bytes[1] = debugger.read_memory(static_cast<uint16_t>(event.pc + 1));
                entry.bytes[2] = debugger.read_memory(static_cast<uint16_t>(event.pc + 2));
            } catch (...) {
            }
        }
        debugger.trace_head_ = (debugger.trace_head_ + 1) % debugger.trace_.size();
        debugger.trace_size_ = std::min(debugger.trace_size_ + 1, debugger.trace_.size());
    }
    if (debugger.watch_hit_ || event.initiator != diagnostics::Initiator::CPU || (event.kind != BusRead && event.kind != BusWrite))
        return;
    for (const auto &point : debugger.breakpoints) {
        if (!point.enabled || point.kind == DebugStopKind::Execute || event.address < point.first || event.address > point.last ||
            (point.bank && *point.bank != event.bank))
            continue;
        if (point.kind == DebugStopKind::ReadWrite || (point.kind == DebugStopKind::Read && event.kind == BusRead) ||
            (point.kind == DebugStopKind::Write && event.kind == BusWrite)) {
            debugger.watch_hit_ = event;
            break;
        }
    }
}

bool Debugger::check_execution_breakpoint() {
    if (!machine_.debug_instruction_boundary())
        return false;
    if (skip_instruction_ == machine_.InstructionsCompleted() && skip_address_ == machine_.debug_instruction_address() &&
        skip_bank_ == machine_.debug_instruction_bank())
        return false;
    skip_instruction_.reset();
    const auto address{machine_.debug_instruction_address()};
    const auto bank{machine_.debug_instruction_bank()};
    if (target_ && target_->address == address && (!target_->bank || *target_->bank == bank) &&
        (!target_->stack || *target_->stack == machine_.InspectCpu().sp)) {
        status_ = std::format("Reached {:04X}:${:04X}", bank, address);
        execution_hit_ = true;
        target_.reset();
        return true;
    }
    for (const auto &point : breakpoints) {
        if (point.enabled && point.kind == DebugStopKind::Execute && address >= point.first && address <= point.last &&
            (!point.bank || *point.bank == bank)) {
            status_ = std::format("Breakpoint before {:04X}:${:04X}", bank, address);
            execution_hit_ = true;
            target_.reset();
            return true;
        }
    }
    return false;
}

ExecutionResult Debugger::execute(const uint64_t ticks, const bool single_instruction) {
    const auto begin{machine_.TimelineTicks()};
    const auto instructions{machine_.InstructionsCompleted()};
    auto reason{ExecutionStopReason::TickBudget};
    watch_hit_.reset();
    for (auto tick{0ULL}; tick < ticks; ++tick) {
        if (check_execution_breakpoint()) {
            reason = ExecutionStopReason::Condition;
            break;
        }
        const auto cpu{machine_.InspectCpu()};
        if (single_instruction && (cpu.halted || cpu.stopped || cpu.locked)) {
            reason = cpu.locked ? ExecutionStopReason::Locked : cpu.stopped ? ExecutionStopReason::Stopped : ExecutionStopReason::Halted;
            break;
        }
        static_cast<void>(machine_.AdvanceTicks(1));
        if (watch_hit_) {
            const auto &hit{*watch_hit_};
            status_ = std::format("CPU {} ${:04X} = ${:02X} at tick {}{}", hit.kind == diagnostics::EventKind::BusRead ? "read" : "write",
                                  hit.address, hit.value, hit.position.tick, hit.blocked ? " (bus blocked)" : "");
            target_.reset();
            reason = ExecutionStopReason::Condition;
            break;
        }
        if (single_instruction && machine_.InstructionsCompleted() != instructions) {
            reason = ExecutionStopReason::InstructionBoundary;
            break;
        }
    }
    return {reason, begin, machine_.TimelineTicks(), machine_.InstructionsCompleted() - instructions};
}

void Debugger::advance(uint64_t ticks, const bool single_instruction, const bool manual) {
    synchronize();
    if (manual) {
        skip_current_breakpoint();
        target_.reset();
        session_.SetPaused(false);
        status_ = "Stepped";
    }
    try {
        while (ticks && !session_.Paused()) {
            const auto result{session_.run_slice(ticks, [&](const uint64_t budget) { return execute(budget, single_instruction); })};
            const auto elapsed{result.stopTick - result.beginTick};
            ticks -= elapsed;
            if (state_generation_ != session_.StateGeneration()) {
                synchronize();
                break;
            }
            if (result.reason != ExecutionStopReason::TickBudget || !elapsed) {
                session_.SetPaused(true);
                if (result.reason == ExecutionStopReason::Halted)
                    status_ = "CPU halted; use cycle, scanline, frame or Run to wait for an interrupt";
                else if (result.reason == ExecutionStopReason::Stopped)
                    status_ = "CPU stopped; advance clocks or provide joypad input to continue";
                else if (result.reason == ExecutionStopReason::Locked)
                    status_ = "CPU locked on an illegal opcode; load a state or restart the ROM";
                break;
            }
        }
    } catch (...) {
        session_.SetPaused(true);
        throw;
    }
    if (manual)
        session_.SetPaused(true);
}

void Debugger::pump(const uint64_t ticks) { advance(ticks, false, false); }

void Debugger::step_instruction() { advance(Gameboy::FRAME_CYCLES_DMG * 2, true, true); }

void Debugger::step_ticks(const uint64_t ticks) { advance(ticks, false, true); }

void Debugger::step_frame() { step_ticks(session_.frame_ticks_remaining()); }

void Debugger::step_scanline() {
    synchronize();
    skip_current_breakpoint();
    target_.reset();
    session_.SetPaused(false);
    status_ = "Stepped";
    const auto line{machine_.ExecutionPosition().ly};
    const auto start{machine_.TimelineTicks()};
    do {
        pump(1);
        if (session_.Paused() || machine_.TimelineTicks() == start)
            break;
    } while (machine_.ExecutionPosition().ly == line && machine_.TimelineTicks() - start < SCANLINE_CYCLES * 2);
    session_.SetPaused(true);
    if (machine_.ExecutionPosition().ly == line && status_ == "Stepped")
        status_ = "Advanced one scanline duration; LY did not change (LCD off or stopped)";
}

void Debugger::run_to(const uint16_t address, const std::optional<uint16_t> bank) {
    resume();
    target_ = Target{address, bank, {}};
    status_ = std::format("Running to ${:04X}", address);
}

void Debugger::step_over() {
    if (!machine_.debug_instruction_boundary() || machine_.InspectCpu().interruptState == InterruptState::M6) {
        step_instruction();
        return;
    }
    auto next{instruction(machine_.debug_instruction_address())};
    next.bytes[0] = machine_.InspectCpu().instruction;
    next = disassemble(next.address, next.bytes);
    if (!next.call) {
        step_instruction();
        return;
    }
    const auto address{static_cast<uint16_t>(next.address + next.size)};
    const auto stack{machine_.InspectCpu().sp};
    run_to(address, machine_.debug_bank(address));
    skip_current_breakpoint();
    target_->stack = stack;
}

void Debugger::step_out() {
    if (!machine_.debug_instruction_boundary()) {
        status_ = "Finish the current instruction before stepping out";
        return;
    }
    const auto stack{machine_.InspectCpu().sp};
    const auto address{static_cast<uint16_t>(read_memory(stack) | read_memory(static_cast<uint16_t>(stack + 1)) << 8)};
    run_to(address);
    skip_current_breakpoint();
    target_->stack = static_cast<uint16_t>(stack + 2);
    status_ = std::format("Waiting for stack return to ${:04X}; pause if SP does not point to a return address", address);
}
