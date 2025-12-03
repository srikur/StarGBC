#pragma once

#include "Audio.h"
#include "Cartridge.h"
#include "DMA.h"
#include "GPU.h"
#include "Joypad.h"
#include "Memory.h"
#include "Serial.h"
#include "Timer.h"

class Bus {
public:
    explicit Bus(Joypad &joypad, Memory &memory,
                 Timer &timer, Cartridge &cartridge,
                 Serial &serial, DMA &dma,
                 Audio &audio, GPU &gpu) : joypad_(joypad),
                                           memory_(memory),
                                           timer_(timer),
                                           cartridge_(cartridge),
                                           serial_(serial),
                                           dma_(dma),
                                           audio_(audio),
                                           gpu_(gpu) {
    }

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    [[nodiscard]] uint8_t ReadDMASource(uint16_t src) const;

    [[nodiscard]] uint8_t ReadOAM(uint16_t address) const;

    void WriteOAM(uint16_t address, uint8_t value) const;

    void WriteByte(uint16_t address, uint8_t value);

    void KeyDown(Keys key);

    void KeyUp(Keys key) const;

    void UpdateGraphics();

    void UpdateTimers();

    void UpdateDMA() const;

    void UpdateSerial();

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
    GPU &gpu_;

    uint8_t interruptEnable{0x00};
    uint8_t interruptFlag{0xE1};
    uint8_t interruptFlagDelayed{0x00};
    uint8_t interruptSetDelay{0x00};
    bool interruptMasterEnable{false};
    bool interruptDelay{false};
    bool bootromRunning{false};
    bool prepareSpeedShift{false};
    Speed speed{Speed::Regular};
    std::vector<uint8_t> bootrom;

    void SetInterrupt(InterruptType interrupt, bool delayed);
};
