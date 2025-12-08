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

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    [[nodiscard]] uint8_t ReadDMASource(uint16_t src) const;

    [[nodiscard]] uint8_t ReadOAM(uint16_t address) const;

    void WriteOAM(uint16_t address, uint8_t value) const;

    void WriteByte(uint16_t address, uint8_t value);

    void UpdateTimers() const;

    void UpdateDMA() const;

    uint32_t RunHDMA() const;

    void ChangeSpeed();

    bool SaveState(std::ofstream &stateFile) const;

    void LoadState(std::ifstream &stateFile);

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
    std::vector<uint8_t> bootrom;
};
