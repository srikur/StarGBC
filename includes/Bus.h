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

    [[nodiscard]] uint8_t ReadDMASource(uint16_t);

    [[nodiscard]] uint8_t ReadOAM(uint16_t) const;

    void WriteOAM(uint16_t, uint8_t) const;

    void WriteByte(uint16_t, uint8_t, ComponentSource);

    void UpdateTimers() const;

    void UpdateDMA();

    uint32_t RunHDMA() const;

    void ChangeSpeed();

    void HandleOAMCorruption(uint16_t, CorruptionType) const;

    bool SaveState(std::ofstream &) const;

    void LoadState(std::ifstream &);

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
    Speed speed{Speed::Regular};
    uint8_t dmaReadByte{};
    std::vector<uint8_t> bootrom;
};
