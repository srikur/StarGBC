#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "Common.h"
#include "HDMA.h"

struct CpuInspection {
    uint16_t pc{};
    uint16_t sp{};
    uint16_t af{};
    uint16_t bc{};
    uint16_t de{};
    uint16_t hl{};
    uint16_t instruction{};
    uint16_t nextInstruction{};
    uint8_t instructionCycle{};
    uint8_t machineCycle{};
    uint8_t sampledRunningInterrupts{};
    uint8_t sampledHaltInterrupts{};
    InterruptState interruptState{};
    uint8_t interruptMask{};
    bool instructionRunning{};
    bool prefixed{};
    bool halted{};
    bool stopped{};
    bool locked{};
    bool haltBug{};
};

struct InterruptInspection {
    uint8_t enable{};
    uint8_t storedFlags{};
    uint8_t cpuVisibleFlags{};
    uint8_t delayedRequests{};
    uint8_t visiblePending{};
    std::array<uint8_t, 5> requestDelays{};
    std::array<uint8_t, 5> visibilityDelays{};
    bool masterEnable{};
    bool enablePending{};
};

struct TimerInspection {
    uint16_t divider{};
    uint8_t tima{};
    uint8_t tma{};
    uint8_t tac{};
    uint8_t reloadDelay{};
    uint8_t apuEventDelay{};
    bool overflowPending{};
    bool reloadActive{};
    bool apuEventSecondary{};
};

struct DmaInspection {
    uint16_t source{};
    uint8_t destinationOffset{};
    uint8_t busLatch{};
    uint8_t cpuPhase{};
    uint16_t cycles{};
    uint16_t pendingSource{};
    uint16_t restartCountdown{};
    bool active{};
    bool complete{};
    bool restartPending{};
    HDMA hdma;
};

struct RegisterInspection {
    uint16_t address{};
    uint8_t stored{};
    uint8_t cpuVisible{};
    std::optional<uint8_t> pending;
    uint8_t pendingStage{};
    std::optional<uint8_t> peripheralValue;
};

enum class GPUMode;
enum class FetcherState;

struct PpuInspection {
    GPUMode mode{};
    uint8_t ly{};
    uint32_t dot{};
    uint8_t pixelsDrawn{};
    uint8_t backgroundQueueSize{};
    uint8_t spriteQueueSize{};
    FetcherState fetcherState{};
    uint16_t fetchAddress{};
    uint8_t tile{};
    uint8_t tileRow{};
    uint8_t lowPlane{};
    uint8_t highPlane{};
    uint8_t scrollDiscard{};
    uint8_t windowLine{};
    bool fetchingWindow{};
    bool fetchingSprite{};
    bool windowTriggered{};
    bool statRequestLine{};
    std::array<RegisterInspection, 9> registers;
};

struct AudioChannelInspection {
    bool enabled{};
    bool dacEnabled{};
    uint8_t digitalOutput{};
    float analogOutput{};
};

struct AudioInspection {
    uint32_t processedTicks{};
    uint32_t deferredTicks{};
    uint32_t effectiveTicks{};
    uint8_t frameSequencerStep{};
    uint8_t storedControl{};
    uint8_t cpuVisibleControl{};
    std::array<AudioChannelInspection, 4> storedChannels;
    std::array<AudioChannelInspection, 4> effectiveChannels;
    std::array<uint8_t, 0x30> cpuVisibleRegisters;
    uint8_t pcm12{};
    uint8_t pcm34{};
    uint64_t mixerLevelChanges{};
};

struct MachineInspection {
    uint64_t tick{};
    Model model{};
    Speed speed{};
    bool cgbMode{};
    bool bootromRunning{};
    uint8_t cpuPhase{};
    CpuInspection cpu;
    InterruptInspection interrupts;
    TimerInspection timer;
    DmaInspection dma;
    PpuInspection ppu;
    AudioInspection audio;
};

struct GraphicsInspection {
    std::array<uint8_t, VRAM_SIZE> vram;
    std::array<uint8_t, 160> oam;
    std::array<uint32_t, 32> background_colors;
    std::array<uint32_t, 32> object_colors;
    std::array<uint16_t, 32> background_rgb15;
    std::array<uint16_t, 32> object_rgb15;
    bool cgb_mode{};
    uint8_t lcd_control{};
    uint8_t scroll_x{};
    uint8_t scroll_y{};
    uint8_t window_x{};
    uint8_t window_y{};
};
