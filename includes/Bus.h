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

class Bus {
public:
    explicit Bus(Joypad &joypad, Memory &memory,
                 Timer &timer, Cartridge &cartridge,
                 Serial &serial, DMA &dma,
                 Audio &audio, Interrupts &interrupts, GPU &gpu) : joypad_(joypad),
                                                                   memory_(memory),
                                                                   timer_(timer),
                                                                   cartridge_(cartridge),
                                                                   serial_(serial),
                                                                   dma_(dma),
                                                                   audio_(audio),
                                                                   interrupts_(interrupts),
                                                                   gpu_(gpu) {
    }

    [[nodiscard]] uint8_t ReadByte(uint16_t, ComponentSource) const;

    [[nodiscard]] int DmaBusFor(uint16_t) const;

    [[nodiscard]] uint8_t ReadDMASource(uint16_t);

    [[nodiscard]] uint8_t ReadOAM(uint16_t) const;

    [[nodiscard]] uint8_t ReadHDMASource(uint16_t) const;

    void WriteOAM(uint16_t, uint8_t) const;

    void WriteByte(uint16_t, uint8_t, ComponentSource);

    void UpdateDMA();

    void RunHDMA() const;

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
};
