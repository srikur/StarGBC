#pragma once

#include "Audio.h"
#include "Cartridge.h"
#include "DMA.h"
#include "GPU.h"
#include "Interrupts.h"
#include "Joypad.h"
#include "Memory.h"
#include "Serial.h"
#include "Timer.h"
#include "diagnostics/Observation.h"

class Bus {
public:
    explicit Bus(Joypad &joypad, Memory &memory, Timer &timer, Cartridge &cartridge, Serial &serial, DMA &dma, Audio &audio, Interrupts &interrupts,
                 GPU &gpu) :
        joypad_(joypad), memory_(memory), timer_(timer), cartridge_(cartridge), serial_(serial), dma_(dma), audio_(audio), interrupts_(interrupts),
        gpu_(gpu) {}

    [[nodiscard]] uint8_t ReadByte(uint16_t, ComponentSource) const;

    [[nodiscard]] uint8_t InspectRead(uint16_t address, ComponentSource source) const;

    [[nodiscard]] uint16_t DebugBank(uint16_t address) const;

    [[nodiscard]] bool AccessBlocked(uint16_t address, ComponentSource source, bool write) const;

    [[nodiscard]] uint8_t DebugPeek(uint16_t address) const;

    void DebugPoke(uint16_t address, uint8_t value) const;

    [[nodiscard]] int DmaBusFor(uint16_t) const;

    [[nodiscard]] uint8_t ReadDMASource(uint16_t);

    [[nodiscard]] uint8_t ReadOAM(uint16_t, ComponentSource) const;

    [[nodiscard]] uint8_t ReadHDMASource(uint16_t) const;

    void WriteOAM(uint16_t, uint8_t, ComponentSource) const;

    void WriteByte(uint16_t, uint8_t, ComponentSource);

    // Inline fast path: 3 of 4 calls only advance the phase counter, and the
    // 4th is idle unless a transfer is active or just finished
    void UpdateDMA() {
        gpu_.oam_dma_write_offset = 0xFF;
        if (++dma_.dmaTickCounter % 4 != 0)
            return;
        dma_.dmaTickCounter = 0;
        if (!dma_.transferActive && !dma_.transferComplete)
            return;
        UpdateDMAWork();
    }

    void UpdateDMAWork();

    // Inline idle fast path; HDMA is inactive almost always
    void RunHDMA(const bool cpuAtInstructionBoundary) const {
        if ((!gpu_.hdma.hdmaActive && !gpu_.hdma.block_requested && !gpu_.hdma.transferringBlock) || !gpu_.is_cgb)
            return;
        if (cpuAtInstructionBoundary && gpu_.cpu_halted)
            gpu_.hdma.wake_hblank_sample = gpu_.hdma.hblank_sample;
        gpu_.hdma.hblank_sample = hdma_hblank_active();
        const bool wasOwned = gpu_.hdma.ShouldHaltCPU();
        RunHDMAWork(cpuAtInstructionBoundary);
        if (observations && wasOwned != gpu_.hdma.ShouldHaltCPU())
            observations->Emit(diagnostics::Component::DMA, diagnostics::EventKind::DmaOwnership, 0xFF55, gpu_.hdma.ShouldHaltCPU(), 0,
                               diagnostics::Initiator::HDMA);
    }

    void RunHDMAWork(bool cpuAtInstructionBoundary) const;

    void set_cpu_halted(bool value, bool preserve_dma_request = false, bool immediate_wake = false) const;

    [[nodiscard]] bool hdma_hblank_active() const;


    void ChangeSpeed();

    void HandleOAMCorruption(uint16_t, CorruptionType) const;

    Joypad &joypad_;
    Memory &memory_;
    Timer &timer_;
    Cartridge &cartridge_;
    Serial &serial_;
    DMA &dma_;
    Audio &audio_;
    Interrupts &interrupts_;
    GPU &gpu_;

    bool bootromRunning{false};
    bool prepareSpeedShift{false};
    bool speedShiftActive{false};
    // A STOP speed switch halts the CPU while the rest of the machine keeps
    // running; counted in speed-switched T-cycles (SameBoy: 0x20008)
    int32_t speedSwitchHalt{0};
    Speed speed{Speed::Regular};
    uint8_t dmaReadByte{};
    // CGB mode vs DMG-compat mode on CGB hardware. True while the CGB bootrom
    // runs; the bootrom writing KEY0 ($FF4C) with bit 2/3 set locks the
    // machine into DMG-compat mode, which unmaps most CGB-only registers
    bool cgbMode{false};
    bool key0Written{false};
    uint8_t svbkReg{0xF8}; // SVBK ($FF70) readback latch
    uint8_t rp{0x00}; // RP ($FF56) infrared port latch
    uint8_t psw72{0x00}; // undocumented $FF72
    uint8_t psw73{0x00}; // undocumented $FF73
    uint8_t psw74{0x00}; // undocumented $FF74, CGB mode only
    uint8_t pgb75{0x00}; // undocumented $FF75, bits 4-6 writable
    [[=NotStateAware]] std::vector<uint8_t> bootrom;
    [[=NotStateAware]] diagnostics::ObservationContext *observations{};

private:
    [[nodiscard]] bool cpu_dma_conflict(u16 address) const;

    u32 dma_wram_offset(u16 address) const;

    template<bool Inspect>
    [[nodiscard]] uint8_t ReadByteValue(uint16_t address, ComponentSource source) const;

    void WriteByteValue(uint16_t address, uint8_t value, ComponentSource source);
};
