#pragma once

#include <cstdint>

namespace diagnostics {
    struct ExecutionPosition {
        uint64_t tick{};
        uint64_t frame{};
        uint16_t dot{};
        uint8_t ly{};
        uint8_t cpuPhase{};
    };

    enum class Component : uint8_t { CPU, Bus, Interrupt, PPU, Timer, DMA, APU, Input, Control };
    enum class EventKind : uint8_t {
        InstructionBegin, InstructionEnd, BusRead, BusWrite, Halt, Stop, InterruptSample,
        InterruptRequest, InterruptVisible, InterruptAccept, InterruptVector,
        RegisterWrite, RegisterApply, PpuMode, WindowActivate, TileFetch, FifoPush, FifoPop,
        PixelDiscard, PixelStall, PixelOutput, PixelRecolor, DmaOwnership, DmaTransfer,
        TimerEdge, TimerOverflow, TimerReload, AudioRegister, AudioTrigger, AudioSequencer,
        AudioMix, InputApplied, Branch, Overflow
    };
    enum class Initiator : uint8_t { CPU, DMA, PPU, HDMA, Peripheral, Debugger, Replay };

    struct Event {
        ExecutionPosition position;
        uint64_t sequence{};
        uint64_t branch{};
        uint16_t pc{};
        uint16_t pcBank{};
        uint16_t address{};
        uint16_t bank{};
        Component component{};
        EventKind kind{};
        Initiator initiator{};
        bool blocked{};
        uint32_t value{};
        uint32_t detail{};
        uint64_t relatedTick{};
        uint64_t parentBranch{};
    };

    struct ObservationContext {
        using Callback = void (*)(void *, const Event &) noexcept;
        void *owner{};
        Callback callback{};
        ExecutionPosition position;
        uint16_t pc{};
        uint16_t pcBank{};
        uint64_t sequence{};
        uint64_t branch{};

        void Emit(const Component component, const EventKind kind, const uint16_t address = 0, const uint32_t value = 0,
                  const uint32_t detail = 0, const Initiator initiator = Initiator::Peripheral, const bool blocked = false,
                  const uint16_t bank = 0) noexcept {
            if (callback)
                callback(owner, {.position = position, .sequence = sequence++, .branch = branch, .pc = pc, .pcBank = pcBank,
                                 .address = address, .bank = bank, .component = component, .kind = kind, .initiator = initiator,
                                 .blocked = blocked, .value = value, .detail = detail});
        }

        void Restore(const ExecutionPosition restored) noexcept {
            const auto parent = branch++;
            const auto previousTick = position.tick;
            position = restored;
            if (callback)
                callback(owner, {.position = position, .sequence = sequence++, .branch = branch, .pc = pc, .pcBank = pcBank,
                                 .component = Component::Control, .kind = EventKind::Branch,
                                 .relatedTick = previousTick, .parentBranch = parent});
        }
    };
}
