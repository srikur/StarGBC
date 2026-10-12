#pragma once

#include <cstdint>
#include <optional>

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
    enum class FetchStage : uint8_t { Map, Attributes, BackgroundLow, BackgroundHigh, SpriteOam, SpriteLow, SpriteHigh };
    enum class StallReason : uint8_t { FifoStarvation, SpriteFetch, SpriteAlignment };
    enum class DiscardReason : uint8_t { FineScroll, WindowClipping, WindowRestart };
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
        void *conditionOwner{};
        bool (*condition)(void *, const Event &) noexcept{};
        std::optional<Event> stopEvent;

        void Dispatch(Event event) noexcept {
            event.sequence = sequence++;
            if (callback)
                callback(owner, event);
            if (!stopEvent && condition && condition(conditionOwner, event))
                stopEvent = event;
        }

        void Emit(const Component component, const EventKind kind, const uint16_t address = 0, const uint32_t value = 0,
                  const uint32_t detail = 0, const Initiator initiator = Initiator::Peripheral, const bool blocked = false,
                  const uint16_t bank = 0) noexcept {
            if (callback || condition)
                Dispatch({.position = position, .branch = branch, .pc = pc, .pcBank = pcBank,
                          .address = address, .bank = bank, .component = component, .kind = kind, .initiator = initiator,
                          .blocked = blocked, .value = value, .detail = detail});
        }

        void EmitDeferred(const uint64_t tick, const Component component, const EventKind kind, const uint32_t value,
                          const uint32_t detail = 0) noexcept {
            if (callback || condition)
                Dispatch({.position = {.tick = tick, .frame = UINT64_MAX, .dot = UINT16_MAX, .ly = UINT8_MAX, .cpuPhase = UINT8_MAX},
                          .branch = branch, .pc = UINT16_MAX, .pcBank = UINT16_MAX,
                          .component = component, .kind = kind, .initiator = Initiator::Peripheral, .value = value, .detail = detail});
        }

        void Restore(const ExecutionPosition restored) noexcept {
            stopEvent.reset();
            const auto parent = branch++;
            const auto previousTick = position.tick;
            position = restored;
            if (callback || condition)
                Dispatch({.position = position, .branch = branch, .pc = pc, .pcBank = pcBank,
                          .component = Component::Control, .kind = EventKind::Branch,
                          .relatedTick = previousTick, .parentBranch = parent});
        }
    };
}
