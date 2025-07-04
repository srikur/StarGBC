#include "Bus.h"

Bus::Bus(const std::string &romLocation) {
    cartridge_ = std::make_unique<Cartridge>(romLocation);
}

uint8_t Bus::ReadDMASource(const uint16_t src) const {
    const uint8_t page = src >> 8;
    if (page <= 0x7F) return cartridge_->ReadByte(src);
    if (page <= 0x9F) return gpu_->ReadVRAM(src);
    if (page <= 0xBF) return cartridge_->ReadByte(src);
    if (page <= 0xFF) return memory_.wram_[src & 0x1FFF];
    return 0xFF;
}

uint8_t Bus::ReadByte(const uint16_t address) const {
    if (address >= 0xFE00 && address <= 0xFE9F && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES) return 0xFF;
    switch (address) {
        case 0x0000 ... 0x7FFF: {
            if (bootromRunning) {
                if (gpu_->hardware == GPU::Hardware::CGB) {
                    if ((address < 0x100) || (address > 0x1FF)) {
                        return bootrom[address];
                    }
                }

                if (gpu_->hardware == GPU::Hardware::DMG) {
                    if (address < 0x100) {
                        return bootrom[address];
                    }
                }
            }
            return cartridge_->ReadByte(address);
        }
        case 0x8000 ... 0x9FFF: return gpu_->ReadVRAM(address);
        case 0xA000 ... 0xBFFF: return cartridge_->ReadByte(address);
        case 0xC000 ... 0xCFFF: return memory_.wram_[address - 0xC000];
        case 0xD000 ... 0xDFFF: return memory_.wram_[address - 0xD000 + 0x1000 * memory_.wramBank_];
        case 0xE000 ... 0xEFFF: return memory_.wram_[address - 0xE000];
        case 0xF000 ... 0xFDFF: return memory_.wram_[address - 0xF000 + 0x1000 * memory_.wramBank_];
        case 0xFE00 ... 0xFEFF: return address < 0xFEA0 ? gpu_->oam[address - 0xFE00] : 0x00;
        case 0xFF00: return joypad_.GetJoypadState() | 0xC0;
        case 0xFF01 ... 0xFF02: return serial_.ReadSerial(address, gpu_->hardware == GPU::Hardware::CGB);
        case 0xFF04 ... 0xFF07: return timer_.ReadByte(address);
        case 0xFF0F: return interruptFlag | 0xE0;
        case 0xFF10 ... 0xFF3F: return audio_->ReadByte(address);
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF4D) {
                if (gpu_->hardware == GPU::Hardware::DMG) return 0xFF;
                const uint8_t first = (speed == Speed::Double) ? 0x80 : 0x00;
                const uint8_t second = prepareSpeedShift ? 0x01 : 0x00;
                return first | second | 0x7E;
            }
            if (address == 0xFF46) { return dma_.writtenValue; }
            if (address == 0xFF4C || address == 0xFF4E) { return 0xFF; }
            return gpu_->ReadRegisters(address);
        }
        case 0xFF50 ... 0xFF55: return ReadHDMA(address, gpu_->hardware == GPU::Hardware::CGB);
        case 0xFF68 ... 0xFF6B: return gpu_->ReadRegisters(address);
        case 0xFF70: return gpu_->hardware == GPU::Hardware::CGB ? memory_.wramBank_ : 0xFF;
        case 0xFF80 ... 0xFFFE: return memory_.hram_[address - 0xFF80];
        case 0xFFFF: return interruptEnable;
        default: return 0xFF;
    }
}

void Bus::WriteByte(const uint16_t address, const uint8_t value) {
    if (address >= 0xFE00 && address <= 0xFE9F && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES) return;
    switch (address) {
        case 0x0000 ... 0x7FFF: cartridge_->WriteByte(address, value);
            break;
        case 0x8000 ... 0x9FFF: gpu_->WriteVRAM(address, value);
            break;
        case 0xA000 ... 0xBFFF: cartridge_->WriteByte(address, value);
            break;
        case 0xC000 ... 0xCFFF: memory_.wram_[address - 0xC000] = value;
            break;
        case 0xD000 ... 0xDFFF: memory_.wram_[address - 0xD000 + 0x1000 * memory_.wramBank_] = value;
            break;
        case 0xE000 ... 0xEFFF: memory_.wram_[address - 0xE000] = value;
            break;
        case 0xF000 ... 0xFDFF: memory_.wram_[address - 0xF000 + 0x1000 * memory_.wramBank_] = value;
            break;
        case 0xFE00 ... 0xFE9F: gpu_->oam[address - 0xFE00] = value;
            break;
        case 0xFF00: joypad_.SetJoypadState(value);
            break;
        case 0xFF01 ... 0xFF02: serial_.WriteSerial(address, value, gpu_->hardware == GPU::Hardware::CGB);
            break;
        case 0xFF04 ... 0xFF07: timer_.WriteByte(address, value);
            break;
        case 0xFF0F: interruptFlag = value;
            break;
        case 0xFF10 ... 0xFF3F: audio_->WriteByte(address, value);
            break;
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF46) { dma_.Set(value); } else if (address == 0xFF4D) {
                prepareSpeedShift = (value & 0x01) == 0x01;
            } else { gpu_->WriteRegisters(address, value); }
            break;
        }
        case 0xFF51 ... 0xFF55: WriteHDMA(address, value);
            break;
        case 0xFF68 ... 0xFF6B: gpu_->WriteRegisters(address, value);
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
    SetInterrupt(InterruptType::Joypad);
}

void Bus::KeyUp(Keys key) {
    joypad_.SetMatrix(joypad_.GetMatrix() | static_cast<uint8_t>(key));
}

void Bus::UpdateGraphics(const uint32_t tCycles) {
    constexpr uint32_t LINE_CYCLES = 456;
    constexpr uint32_t MODE3_CYCLES = 172;
    constexpr uint32_t MODE2_CYCLES = 80;
    constexpr uint32_t MODE0_CYCLES = LINE_CYCLES - MODE2_CYCLES - MODE3_CYCLES; // 204 – HBlank

    if (!gpu_->LCDEnabled()) {
        gpu_->scanlineCounter = 0;
        gpu_->currentLine = 0;
        gpu_->stat.mode = 0;
        gpu_->hblank = false;
        return;
    }
    gpu_->hblank = false;

    for (uint32_t i = 0; i < tCycles; i++) {
        gpu_->scanlineCounter++;
        switch (gpu_->stat.mode) {
            case 0: {
                if (gpu_->scanlineCounter < MODE0_CYCLES) break;
                gpu_->scanlineCounter -= MODE0_CYCLES;
                ++gpu_->currentLine;

                if (gpu_->stat.enableLYInterrupt && gpu_->currentLine == gpu_->lyc) SetInterrupt(InterruptType::LCDStat);

                if (gpu_->currentLine == 144) {
                    gpu_->stat.mode = 1;
                    gpu_->vblank = true;
                    SetInterrupt(InterruptType::VBlank);
                    if (gpu_->stat.enableM1Interrupt) SetInterrupt(InterruptType::LCDStat);
                } else {
                    gpu_->stat.mode = 2;
                    if (gpu_->stat.enableM2Interrupt) SetInterrupt(InterruptType::LCDStat);
                }
            }
            break;

            case 1: {
                if (gpu_->scanlineCounter < LINE_CYCLES) break;
                gpu_->scanlineCounter -= LINE_CYCLES;
                ++gpu_->currentLine;

                // Ten lines in VBlank: LY 144-153
                if (gpu_->currentLine > 153) {
                    gpu_->currentLine = 0;
                    gpu_->stat.mode = 2;
                    gpu_->scanlineCounter = 0;
                    if (gpu_->stat.enableM2Interrupt) SetInterrupt(InterruptType::LCDStat);
                }

                if (gpu_->stat.enableLYInterrupt && gpu_->currentLine == gpu_->lyc) SetInterrupt(InterruptType::LCDStat);
            }
            break;

            case 2: {
                if (gpu_->scanlineCounter < MODE2_CYCLES) break;
                gpu_->scanlineCounter -= MODE2_CYCLES;
                gpu_->stat.mode = 3;
            }
            break;

            case 3: {
                if (gpu_->scanlineCounter < MODE3_CYCLES) break;
                gpu_->scanlineCounter -= MODE3_CYCLES;

                gpu_->stat.mode = 0;
                gpu_->hblank = true;
                gpu_->DrawScanline();

                if (gpu_->stat.enableM0Interrupt) SetInterrupt(InterruptType::LCDStat);
                break;
            }
            default: break;
        }
    }
}

void Bus::UpdateTimers(const uint32_t cycles) {
    const int frameSeqBit = audio_->IsDMG() || speed == Speed::Regular ? 12 : 13;

    for (uint32_t i = 0; i < cycles; ++i) {
        timer_.reloadActive = false;

        if (timer_.overflowPending && --timer_.overflowDelay == 0) {
            timer_.tima = timer_.tma;
            SetInterrupt(InterruptType::Timer);
            timer_.overflowPending = false;
            timer_.reloadActive = true;
        }

        const bool timerEnabled = timer_.tac & 0x04;
        const int timerBit = Timer::TimerBit(timer_.tac);
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
            audio_->TickFrameSequencer();
        }
    }
}

void Bus::UpdateDMA(const uint32_t cycles) {
    for (uint32_t i = 0; i < cycles; ++i) {
        if (dma_.transferComplete) {
            dma_.transferActive = false;
            dma_.transferComplete = false;
            dma_.ticks = 0;
            dma_.currentByte = 0;
        }
        if (!dma_.transferActive) { continue; }
        if (dma_.restartPending && --dma_.restartCountdown == 0) {
            dma_.restartPending = false;
            dma_.startAddress = dma_.pendingStart;
            dma_.currentByte = 0;
            dma_.ticks = 1;
        }

        ++dma_.ticks;

        if (dma_.ticks <= DMA::STARTUP_CYCLES) continue; // OAM still accessible here

        gpu_->oam[dma_.currentByte++] =
                ReadDMASource(dma_.startAddress + dma_.currentByte);
        if (dma_.currentByte == DMA::TOTAL_BYTES) {
            dma_.transferComplete = true;
        }
    }
}

void Bus::UpdateSerial(uint32_t tCycles) {
    if (!serial_.active_) return;
    while (tCycles) {
        const uint32_t step = (tCycles < serial_.ticksUntilShift_) ? tCycles : serial_.ticksUntilShift_;
        tCycles -= step;
        serial_.ticksUntilShift_ -= step;

        if (serial_.ticksUntilShift_ == 0) {
            serial_.ShiftOneBit();
            if (++serial_.bitsShifted_ == 8) {
                serial_.active_ = false;
                serial_.control_ &= 0x7F;
                SetInterrupt(InterruptType::Serial);
                break;
            }
            serial_.ticksUntilShift_ = serial_.ticksPerBit_;
        }
    }
}

void Bus::SetInterrupt(const InterruptType interrupt) {
    using enum InterruptType;
    const uint8_t mask = [&]() -> uint8_t {
        switch (interrupt) {
            case VBlank: return 0x01;
            case LCDStat: return 0x02;
            case Timer: return 0x04;
            case Serial: return 0x08;
            case Joypad: return 0x10;
            default: throw UnreachableCodeException("Bus::SetInterrupt -- unknown interrupt type");
        }
    }();

    interruptFlag |= mask;
}

uint8_t Bus::ReadHDMA(const uint16_t address, const bool gbc) const {
    switch (address) {
        case 0xFF50 ... 0xFF54: return 0xFF;
        case 0xFF55: return gbc ? (hdmaRemain | (hdmaActive ? 0x00 : 0x80)) : 0xFF;
        default: throw UnreachableCodeException("Bus::ReadHDMA unreachable code at address: " + std::to_string(address));
    }
}

void Bus::WriteHDMA(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0xFF51: hdmaSource = (static_cast<uint16_t>(value) << 8) | (hdmaSource & 0xFF);
            break;
        case 0xFF52: hdmaSource = (hdmaSource & 0xFF00) | static_cast<uint16_t>(value & 0xF0);
            break;
        case 0xFF53: hdmaDestination = 0x8000 | (static_cast<uint16_t>(value & 0x1F) << 8) | (hdmaDestination & 0xFF);
            break;
        case 0xFF54: hdmaDestination = (hdmaDestination & 0xFF00) | static_cast<uint16_t>(value & 0xF0);
            break;
        case 0xFF55: {
            if (hdmaActive && gpu_->hdmaMode == GPU::HDMAMode::HDMA) {
                if ((value & 0x80) == 0x00) {
                    hdmaActive = false;
                }
                return;
            }
            hdmaActive = true;
            hdmaRemain = value & 0x7F;
            if ((value & 0x80) != 0x00) {
                gpu_->hdmaMode = GPU::HDMAMode::HDMA;
            } else {
                gpu_->hdmaMode = GPU::HDMAMode::GDMA;
            }
            break;
        }
        default: throw UnreachableCodeException("Bus::WriteHDMA unreachable code");
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
        stateFile.write(reinterpret_cast<const char *>(&hdmaSource), sizeof(hdmaSource));
        stateFile.write(reinterpret_cast<const char *>(&hdmaDestination), sizeof(hdmaDestination));
        stateFile.write(reinterpret_cast<const char *>(&hdmaActive), sizeof(hdmaActive));
        stateFile.write(reinterpret_cast<const char *>(&hdmaRemain), sizeof(hdmaRemain));
        return cartridge_->SaveState(stateFile) && gpu_->SaveState(stateFile) &&
               joypad_.SaveState(stateFile) && memory_.SaveState(stateFile) &&
               timer_.SaveState(stateFile) && serial_.SaveState(stateFile);
    } catch (const std::exception &e) {
        std::cerr << "Error saving state: " << e.what() << std::endl;
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
        stateFile.read(reinterpret_cast<char *>(&hdmaSource), sizeof(hdmaSource));
        stateFile.read(reinterpret_cast<char *>(&hdmaDestination), sizeof(hdmaDestination));
        stateFile.read(reinterpret_cast<char *>(&hdmaActive), sizeof(hdmaActive));
        stateFile.read(reinterpret_cast<char *>(&hdmaRemain), sizeof(hdmaRemain));

        cartridge_->LoadState(stateFile);
        gpu_->LoadState(stateFile);
        joypad_.LoadState(stateFile);
        memory_.LoadState(stateFile);
        timer_.LoadState(stateFile);
        serial_.LoadState(stateFile);
    } catch (const std::exception &e) {
        std::cerr << "Error loading state: " << e.what() << std::endl;
    }
}
