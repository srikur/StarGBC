#pragma once

#include "Disassembly.h"
#include "stargbc/EmulationSession.h"

enum class DebugStopKind { Execute, Read, Write, ReadWrite };

struct DebugBreakpoint {
    DebugStopKind kind{DebugStopKind::Execute};
    uint16_t first{};
    uint16_t last{};
    std::optional<uint16_t> bank;
    bool enabled{true};
};

struct DebugTraceEntry {
    diagnostics::Event event;
    std::array<uint8_t, 3> bytes{};
};

class Debugger {
public:
    explicit Debugger(EmulationSession &session);
    ~Debugger();
    Debugger(const Debugger &) = delete;
    Debugger &operator=(const Debugger &) = delete;

    void pause();
    void reset();
    void resume();
    void step_instruction();
    void step_over();
    void step_out();
    void step_ticks(uint64_t ticks);
    void step_scanline();
    void step_frame();
    void run_to(uint16_t address, std::optional<uint16_t> bank = {});
    void pump(uint64_t ticks = Gameboy::FRAME_CYCLES_DMG * 2);
    void synchronize();
    void write_memory(uint16_t address, uint8_t value);

    [[nodiscard]] bool paused() const { return session_.Paused(); }
    [[nodiscard]] const std::string &status() const { return status_; }
    [[nodiscard]] DisassembledInstruction instruction(uint16_t address) const;
    [[nodiscard]] uint8_t read_memory(uint16_t address, bool cpu_visible = false) const;
    [[nodiscard]] size_t trace_size() const { return trace_size_; }
    [[nodiscard]] const DebugTraceEntry &trace_entry(size_t index) const;
    void clear_trace();
    std::vector<DebugBreakpoint> breakpoints;

private:
    struct Target {
        uint16_t address{};
        std::optional<uint16_t> bank;
        std::optional<uint16_t> stack;
    };

    static void observe(void *owner, const diagnostics::Event &event) noexcept;
    ExecutionResult execute(uint64_t ticks, bool single_instruction);
    void advance(uint64_t ticks, bool single_instruction, bool manual);
    bool check_execution_breakpoint();
    void skip_current_breakpoint();

    EmulationSession &session_;
    Gameboy &machine_;
    diagnostics::ObservationContext observations_;
    std::array<DebugTraceEntry, 512> trace_{};
    size_t trace_head_{};
    size_t trace_size_{};
    uint64_t state_generation_{};
    std::optional<uint64_t> skip_instruction_;
    uint16_t skip_address_{};
    uint16_t skip_bank_{};
    bool execution_hit_{};
    std::optional<Target> target_;
    std::optional<diagnostics::Event> watch_hit_;
    std::string status_{"Paused"};
};
