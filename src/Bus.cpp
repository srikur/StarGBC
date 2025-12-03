#include "Bus.h"

#include <algorithm>

uint8_t Bus::ReadDMASource(const uint16_t src) const {
    const uint8_t page = src >> 8;
    if (page <= 0x7F) return cartridge_.ReadByte(src);
    if (page <= 0x9F) return gpu_.ReadVRAM(src);
    if (page <= 0xBF) return cartridge_.ReadByte(src);
    if (page <= 0xFF) return memory_.wram_[src & 0x1FFF];
    return 0xFF;
}

uint8_t Bus::ReadOAM(const uint16_t address) const {
    return gpu_.stat.mode == GPU::Mode::MODE_3 ? 0xFF : gpu_.oam[address - 0xFE00];
}

void Bus::WriteOAM(const uint16_t address, const uint8_t value) const {
    if (gpu_.stat.mode != GPU::Mode::MODE_3) gpu_.oam[address - 0xFE00] = value;
}

uint8_t Bus::ReadByte(const uint16_t address) const {
    if (address >= 0xFE00 && address <= 0xFE9F && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES) return 0xFF;
    switch (address) {
        case 0x0000 ... 0x7FFF: {
            if (bootromRunning) {
                if (gpu_.hardware == GPU::Hardware::CGB && (address < 0x100 || address > 0x1FF)) {
                    return bootrom[address];
                }

                if (gpu_.hardware == GPU::Hardware::DMG && address < 0x100) {
                    return bootrom[address];
                }
            }
            return cartridge_.ReadByte(address);
        }
        case 0x8000 ... 0x9FFF: return gpu_.ReadVRAM(address);
        case 0xA000 ... 0xBFFF: return cartridge_.ReadByte(address);
        case 0xC000 ... 0xCFFF: return memory_.wram_[address - 0xC000];
        case 0xD000 ... 0xDFFF: return memory_.wram_[address - 0xD000 + 0x1000 * memory_.wramBank_];
        case 0xE000 ... 0xEFFF: return memory_.wram_[address - 0xE000];
        case 0xF000 ... 0xFDFF: return memory_.wram_[address - 0xF000 + 0x1000 * memory_.wramBank_];
        case 0xFE00 ... 0xFEFF: return address < 0xFEA0 ? ReadOAM(address) : 0xFF;
        case 0xFF00: return joypad_.GetJoypadState() | 0xC0;
        case 0xFF01 ... 0xFF02: return serial_.ReadSerial(address);
        case 0xFF04 ... 0xFF07: return timer_.ReadByte(address);
        case 0xFF0F: return interruptFlag | 0xE0;
        case 0xFF10 ... 0xFF3F: return audio_.ReadByte(address);
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF4D) {
                if (gpu_.hardware == GPU::Hardware::DMG) return 0xFF;
                const uint8_t first = speed == Speed::Double ? 0x80 : 0x00;
                const uint8_t second = prepareSpeedShift ? 0x01 : 0x00;
                return first | second | 0x7E;
            }
            if (address == 0xFF46) { return dma_.writtenValue; }
            if (address == 0xFF4C || address == 0xFF4E) { return 0xFF; }
            return gpu_.ReadRegisters(address);
        }
        case 0xFF50 ... 0xFF55: return gpu_.hdma.ReadHDMA(address, gpu_.hardware == GPU::Hardware::CGB);
        case 0xFF68 ... 0xFF6C: return gpu_.ReadRegisters(address);
        case 0xFF70: return gpu_.hardware == GPU::Hardware::CGB ? memory_.wramBank_ : 0xFF;
        case 0xFF80 ... 0xFFFE: return memory_.hram_[address - 0xFF80];
        case 0xFFFF: return interruptEnable;
        default: return 0xFF;
    }
}

void Bus::WriteByte(const uint16_t address, const uint8_t value) {
    if (address >= 0xFE00 && address <= 0xFE9F && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES) return;
    switch (address) {
        case 0x0000 ... 0x7FFF: cartridge_.WriteByte(address, value);
            break;
        case 0x8000 ... 0x9FFF: gpu_.WriteVRAM(address, value);
            break;
        case 0xA000 ... 0xBFFF: cartridge_.WriteByte(address, value);
            break;
        case 0xC000 ... 0xCFFF: memory_.wram_[address - 0xC000] = value;
            break;
        case 0xD000 ... 0xDFFF: memory_.wram_[address - 0xD000 + 0x1000 * memory_.wramBank_] = value;
            break;
        case 0xE000 ... 0xEFFF: memory_.wram_[address - 0xE000] = value;
            break;
        case 0xF000 ... 0xFDFF: memory_.wram_[address - 0xF000 + 0x1000 * memory_.wramBank_] = value;
            break;
        case 0xFE00 ... 0xFE9F: WriteOAM(address, value);
            break;
        case 0xFF00: joypad_.SetJoypadState(value);
            break;
        case 0xFF01 ... 0xFF02: serial_.WriteSerial(address, value, speed == Speed::Double, gpu_.hardware == GPU::Hardware::CGB);
            break;
        case 0xFF04 ... 0xFF07: timer_.WriteByte(address, value);
            break;
        case 0xFF0F: interruptFlag = value;
            break;
        case 0xFF10 ... 0xFF3F: audio_.WriteByte(address, value);
            break;
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF46) { dma_.Set(value); } else if (address == 0xFF4D) {
                prepareSpeedShift = (value & 0x01) == 0x01;
            } else { gpu_.WriteRegisters(address, value); }
            break;
        }
        case 0xFF51 ... 0xFF55: gpu_.hdma.WriteHDMA(address, value);
            break;
        case 0xFF68 ... 0xFF6C: gpu_.WriteRegisters(address, value);
            break;
        case 0xFF70: memory_.wramBank_ = (value & 0x07) ? value : 1;
            break;
        case 0xFF80 ... 0xFFFE: memory_.hram_[address - 0xFF80] = value;
            break;
        case 0xFFFF: interruptEnable = value;
            break;
        default: break;
    }
}

void Bus::KeyDown(Keys key) {
    joypad_.SetMatrix(joypad_.GetMatrix() & ~static_cast<uint8_t>(key));
    SetInterrupt(InterruptType::Joypad, false);
}

void Bus::KeyUp(Keys key) const {
    joypad_.SetMatrix(joypad_.GetMatrix() | static_cast<uint8_t>(key));
}

void Bus::UpdateGraphics() {
    if (interruptSetDelay > 0) {
        interruptSetDelay--;
        if (interruptSetDelay == 0) {
            interruptFlag = interruptFlagDelayed;
            interruptFlagDelayed = 0;
        }
    }

    if (gpu_.LCDDisabled()) {
        return;
    }

    if (gpu_.currentLine == gpu_.lyc) {
        gpu_.stat.coincidenceFlag = true;
        if (gpu_.stat.enableLYInterrupt && !gpu_.statTriggered) {
            SetInterrupt(InterruptType::LCDStat, true);
            gpu_.statTriggered = true;
        }
    } else {
        gpu_.stat.coincidenceFlag = false;
    }

    switch (gpu_.stat.mode) {
        case GPU::Mode::MODE_0:
            if (gpu_.stat.enableM0Interrupt && !gpu_.statTriggered) {
                SetInterrupt(InterruptType::LCDStat, true);
                gpu_.statTriggered = true;
            }
            break;
        case GPU::Mode::MODE_1:
            if (gpu_.stat.enableM1Interrupt && !gpu_.statTriggered) {
                SetInterrupt(InterruptType::LCDStat, true);
                gpu_.statTriggered = true;
            }
            break;

        case GPU::Mode::MODE_2:
            gpu_.hblank = false;
            gpu_.vblank = false;
            if (gpu_.stat.enableM2Interrupt && !gpu_.statTriggered) {
                SetInterrupt(InterruptType::LCDStat, true);
                gpu_.statTriggered = true;
            }
            gpu_.TickOAMScan();
            if (gpu_.scanlineCounter == 79) {
                gpu_.stat.mode = GPU::Mode::MODE_3;
                gpu_.pixelsDrawn = 0;
                if (gpu_.hardware != GPU::Hardware::CGB || gpu_.objectPriority) {
                    std::ranges::sort(gpu_.spriteBuffer, [](const GPU::Sprite &a, const GPU::Sprite &b) {
                        return a < b;
                    });
                } else if (gpu_.hardware == GPU::Hardware::CGB && !gpu_.objectPriority) {
                    std::ranges::sort(gpu_.spriteBuffer, [](const GPU::Sprite &a, const GPU::Sprite &b) {
                        return a.spriteNum < b.spriteNum;
                    });
                }
                gpu_.ResetScanlineState(false);
            }
            break;

        case GPU::Mode::MODE_3: {
            gpu_.TickMode3();
            if (gpu_.pixelsDrawn == GPU::SCREEN_WIDTH) {
                gpu_.stat.mode = GPU::Mode::MODE_0;
                gpu_.hblank = true;
                if (gpu_.stat.enableM0Interrupt && !gpu_.statTriggered) {
                    SetInterrupt(InterruptType::LCDStat, true);
                    gpu_.statTriggered = true;
                }
                break;
            }
        }
        break;
        default: break;
    }
    gpu_.scanlineCounter++;
    // std::fprintf(stderr, "currentLine: %d, scanlineCounter: %d, mode: %d\n", gpu_.currentLine, gpu_.scanlineCounter, gpu_.stat.mode);
    if (gpu_.scanlineCounter == 456) {
        gpu_.scanlineCounter = 0;
        gpu_.currentLine++;
        gpu_.statTriggered = false;

        if (gpu_.isFetchingWindow_) {
            gpu_.windowLineCounter_++;
        }

        if (gpu_.currentLine >= 154) {
            gpu_.currentLine = 0;
            gpu_.stat.mode = GPU::Mode::MODE_2;
            gpu_.windowLineCounter_ = 0;
            gpu_.ResetScanlineState(true);
            gpu_.windowTriggeredThisFrame = false;
            if (gpu_.currentLine >= gpu_.windowY) {
                gpu_.windowTriggeredThisFrame = true;
            }
            gpu_.initialSCXSet = false;
            // std::fprintf(stderr, "Scroll X Reset 1, line %d, scanline counter: %d\n", gpu_.currentLine, gpu_.scanlineCounter);
        } else if (gpu_.currentLine == 144) {
            gpu_.stat.mode = GPU::Mode::MODE_1;
            gpu_.vblank = true;
            SetInterrupt(InterruptType::VBlank, true);
        } else if (gpu_.currentLine < 144) {
            gpu_.stat.mode = GPU::Mode::MODE_2;
            if (gpu_.currentLine >= gpu_.windowY) {
                gpu_.windowTriggeredThisFrame = true;
            }
            gpu_.ResetScanlineState(true);
            gpu_.initialSCXSet = false;
            // std::fprintf(stderr, "Scroll X Reset 2, line %d, scanline counter %d\n", gpu_.currentLine, gpu_.scanlineCounter);
        }
    }
}

void Bus::UpdateTimers() {
    const int frameSeqBit = audio_.IsDMG() || speed == Speed::Regular ? 12 : 13;

    timer_.reloadActive = false;
    if (timer_.overflowPending && --timer_.overflowDelay == 0) {
        timer_.tima = timer_.tma;
        SetInterrupt(InterruptType::Timer, false);
        timer_.overflowPending = false;
        timer_.reloadActive = true;
    }

    const bool timerEnabled = timer_.tac & 0x04;
    const int timerBit = timer_.TimerBit(timer_.tac);
    const bool oldSignal = timerEnabled && (timer_.divCounter & (1u << timerBit));
    const bool oldFrameSeqSignal = timer_.divCounter & 1u << frameSeqBit;

    ++timer_.divCounter;

    const bool newSignal = timerEnabled && (timer_.divCounter & (1u << timerBit));
    if (oldSignal && !newSignal) {
        timer_.IncrementTIMA();
    }

    // Check for a falling edge for the APU Frame Sequencer
    const bool newFrameSeqSignal = (timer_.divCounter & (1u << frameSeqBit));
    if (oldFrameSeqSignal && !newFrameSeqSignal) {
        audio_.TickFrameSequencer();
    }
}

void Bus::UpdateDMA() const {
    if (++dma_.dmaTickCounter % 4 == 0) {
        dma_.dmaTickCounter = 0;
        if (dma_.transferComplete) {
            dma_.transferActive = false;
            dma_.transferComplete = false;
            dma_.ticks = 0;
            dma_.currentByte = 0;
        }
        if (!dma_.transferActive) { return; }
        if (dma_.restartPending && --dma_.restartCountdown == 0) {
            dma_.restartPending = false;
            dma_.startAddress = dma_.pendingStart;
            dma_.currentByte = 0;
            dma_.ticks = 1;
        }

        ++dma_.ticks;
        if (dma_.ticks <= DMA::STARTUP_CYCLES) return; // OAM still accessible here

        gpu_.oam[dma_.currentByte++] = ReadDMASource(dma_.startAddress + dma_.currentByte);
        if (dma_.currentByte == DMA::TOTAL_BYTES) {
            dma_.transferComplete = true;
        }
    }
}

void Bus::UpdateSerial() {
    if (!serial_.active_) return;

    if (--serial_.ticksUntilShift_ == 0) {
        serial_.ShiftOneBit();
        if (++serial_.bitsShifted_ == 8) {
            serial_.active_ = false;
            serial_.control_ &= 0x7F;
            SetInterrupt(InterruptType::Serial, false);
            return;
        }
        serial_.ticksUntilShift_ = serial_.ticksPerBit_;
    }
}

/* Lots of pointer indirection going on here :( */
uint32_t Bus::RunHDMA() const {
    if (!gpu_.hdma.hdmaActive || gpu_.hardware == GPU::Hardware::DMG) {
        return 0;
    }

    const uint32_t cyclesPerBlock = speed == Speed::Double ? 16 : 8;
    switch (gpu_.hdma.hdmaMode) {
        case HDMAMode::GDMA: {
            const uint32_t blocks = static_cast<uint32_t>(gpu_.hdma.hdmaRemain) + 1;
            for (uint32_t unused = 0; unused < blocks; ++unused) {
                const uint16_t memSource = gpu_.hdma.hdmaSource;
                for (uint16_t i = 0; i < 0x10; ++i) {
                    const uint8_t byte = ReadByte(memSource + i);
                    gpu_.WriteVRAM(gpu_.hdma.hdmaDestination + i, byte);
                }
                gpu_.hdma.hdmaSource += 0x10;
                gpu_.hdma.hdmaDestination += 0x10;
                gpu_.hdma.hdmaRemain = (gpu_.hdma.hdmaRemain == 0) ? 0x7F : static_cast<uint8_t>(gpu_.hdma.hdmaRemain - 1);
            }
            gpu_.hdma.hdmaActive = false;
            return blocks * cyclesPerBlock;
        }
        case HDMAMode::HDMA: {
            if (!gpu_.hblank) return 0;

            const uint16_t memSource = gpu_.hdma.hdmaSource;
            for (uint16_t i = 0; i < 0x10; ++i) {
                const uint8_t byte = ReadByte(memSource + i);
                gpu_.WriteVRAM(gpu_.hdma.hdmaDestination + i, byte);
            }
            gpu_.hdma.hdmaSource += 0x10;
            gpu_.hdma.hdmaDestination += 0x10;
            gpu_.hdma.hdmaRemain = (gpu_.hdma.hdmaRemain == 0) ? 0x7F : static_cast<uint8_t>(gpu_.hdma.hdmaRemain - 1);
            if (gpu_.hdma.hdmaRemain == 0x7F) gpu_.hdma.hdmaActive = false;
            return cyclesPerBlock;
        }
        default: return 0;
    }
}


void Bus::SetInterrupt(const InterruptType interrupt, const bool delayed) {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    if (!delayed) interruptFlag |= mask;
    else {
        interruptSetDelay = 4;
        interruptFlagDelayed = interruptFlag | mask;
    }
}

void Bus::ChangeSpeed() {
    if (prepareSpeedShift) {
        speed = speed == Speed::Regular ? Speed::Double : Speed::Regular;
        prepareSpeedShift = false;
    }
}

bool Bus::SaveState(std::ofstream &stateFile) const {
    try {
        stateFile.write(reinterpret_cast<const char *>(&speed), sizeof(speed));
        stateFile.write(reinterpret_cast<const char *>(&prepareSpeedShift), sizeof(prepareSpeedShift));
        stateFile.write(reinterpret_cast<const char *>(&bootromRunning), sizeof(bootromRunning));
        stateFile.write(reinterpret_cast<const char *>(&interruptFlag), sizeof(interruptFlag));
        stateFile.write(reinterpret_cast<const char *>(&interruptEnable), sizeof(interruptEnable));
        stateFile.write(reinterpret_cast<const char *>(&interruptMasterEnable), sizeof(interruptMasterEnable));
        stateFile.write(reinterpret_cast<const char *>(&interruptDelay), sizeof(interruptDelay));
        return cartridge_.SaveState(stateFile) && gpu_.SaveState(stateFile) &&
               joypad_.SaveState(stateFile) && memory_.SaveState(stateFile) &&
               timer_.SaveState(stateFile) && serial_.SaveState(stateFile);
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}

void Bus::LoadState(std::ifstream &stateFile) {
    try {
        stateFile.read(reinterpret_cast<char *>(&speed), sizeof(speed));
        stateFile.read(reinterpret_cast<char *>(&prepareSpeedShift), sizeof(prepareSpeedShift));
        stateFile.read(reinterpret_cast<char *>(&bootromRunning), sizeof(bootromRunning));
        stateFile.read(reinterpret_cast<char *>(&interruptFlag), sizeof(interruptFlag));
        stateFile.read(reinterpret_cast<char *>(&interruptEnable), sizeof(interruptEnable));
        stateFile.read(reinterpret_cast<char *>(&interruptMasterEnable), sizeof(interruptMasterEnable));
        stateFile.read(reinterpret_cast<char *>(&interruptDelay), sizeof(interruptDelay));

        cartridge_.LoadState(stateFile);
        gpu_.LoadState(stateFile);
        joypad_.LoadState(stateFile);
        memory_.LoadState(stateFile);
        timer_.LoadState(stateFile);
        serial_.LoadState(stateFile);
    } catch ([[maybe_unused]] const std::exception &e) {
    }
}
