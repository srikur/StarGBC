#include "Bus.h"

Bus::Bus(const std::string &romLocation) {
    cartridge_ = std::make_unique<Cartridge>(romLocation);
    speedShift = false;
    runBootrom = false;
    speed = cartridge_->ReadByte(0x0143) & 0x80 ? Speed::Double : Speed::Regular;
    gpu_->hdmaMode = GPU::HDMAMode::GDMA;
    hdmaSource = 0x0000;
    hdmaDestination = 0x8000;
    hdmaActive = false;
    hdmaRemain = 0x00;
}

uint8_t Bus::ReadByte(uint16_t address) const {
    switch (address & 0xF000) {
        case 0x0000:
        case 0x1000:
        case 0x2000:
        case 0x3000:
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:
            // Rom banks
            if (runBootrom) {
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
        case 0x8000:
        case 0x9000:
            return gpu_->ReadVRAM(address);
        case 0xA000:
        case 0xB000:
            // External RAM
            return cartridge_->ReadByte(address);
        case 0xC000:
            return memory_.wram_[address - 0xC000];
        case 0xD000:
            return memory_.wram_[address - 0xD000 + 0x1000 * memory_.wramBank_];
        case 0xE000:
            return memory_.wram_[address - 0xE000];
        case 0xF000:
            switch (address & 0xF00) {
                case 0x000:
                case 0x100:
                case 0x200:
                case 0x300:
                case 0x400:
                case 0x500:
                case 0x600:
                case 0x700:
                case 0x800:
                case 0x900:
                case 0xA00:
                case 0xB00:
                case 0xC00:
                case 0xD00:
                    return memory_.wram_[address - 0xF000 + 0x1000 * memory_.wramBank_];
                case 0xE00:
                    return address < 0xFEA0 ? gpu_->oam[address - 0xFE00] : 0x00;
                case 0xF00:
                    switch (address & 0xF0) {
                        case 0x0:
                            switch (address & 0xFF) {
                                case 0x00:
                                    return joypad_.GetJoypadState();
                                case 0x01:
                                case 0x02:
                                    return serial_.ReadSerial(address);
                                case 0x04:
                                    return timer_.dividerRegister;
                                case 0x05:
                                    return timer_.timerCounter;
                                case 0x06:
                                    return timer_.timerModulo;
                                case 0x07: {
                                    const uint8_t clock = timer_.clockEnabled ? 1 : 0;
                                    const uint8_t speed = [&] {
                                        switch (timer_.clockSpeed) {
                                            case 1024: return 0;
                                            case 16: return 1;
                                            case 64: return 2;
                                            case 256: return 3;
                                            default: return 0;
                                        }
                                    }();
                                    return (clock << 2) | speed;
                                }
                                case 0x0F:
                                    return interruptFlag;
                                default: throw UnreachableCodeException("Bus::ReadByte case 0xF00");
                            }
                        case 0x10:
                        case 0x20:
                        case 0x30:
                            return audio_.ReadByte(address);
                        case 0x40: {
                            if (address == 0xFF4D) {
                                const uint8_t first = (speed == Speed::Double) ? 0x80 : 0x00;
                                const uint8_t second = speedShift ? 0x01 : 0x00;
                                return first | second;
                            }
                            if (address == 0xFF4C) { return 0x00; }
                            return gpu_->ReadRegisters(address);
                        }
                        case 0x50: {
                            if ((address > 0xFF50) && (address < 0xFF56)) {
                                return ReadHDMA(address);
                            }
                            return 0x00;
                        }
                        case 0x60: {
                            if ((address > 0xFF67) && (address < 0xFF6C)) {
                                return gpu_->ReadRegisters(address);
                            }
                            return 0xFF;
                        }
                        case 0x70:
                            if (address == 0xFF70) return memory_.wramBank_;
                            break;
                        case 0x80:
                        case 0x90:
                        case 0xA0:
                        case 0xB0:
                        case 0xC0:
                        case 0xD0:
                        case 0xE0:
                        case 0xF0:
                            return address == 0xFFFF ? interruptEnable : memory_.hram_[address - 0xFF80];
                        default: throw UnreachableCodeException("Bus::ReadByte Unreachable Code");
                    }
                    break;
                default: throw UnreachableCodeException("Bus::ReadByte Unreachable Code");
            }
            break;
        default:
            return 0xFF;
    }
    return 0xFF;
}

void Bus::WriteByte(uint16_t address, uint8_t value) {
    switch (address & 0xF000) {
        case 0x0000:
        case 0x1000:
        case 0x2000:
        case 0x3000:
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:
            cartridge_->writeByte(address, value);
            break;
        case 0x8000:
        case 0x9000:
            gpu_->WriteVRAM(address, value);
            break;
        case 0xA000:
        case 0xB000:
            cartridge_->writeByte(address, value);
            break;
        case 0xC000:
            memory_.wram_[address - 0xC000] = value;
            break;
        case 0xD000:
            memory_.wram_[address - 0xD000 + 0x1000 * memory_.wramBank_] = value;
            break;
        case 0xE000:
            memory_.wram_[address - 0xE000] = value;
            break;
        case 0xF000:
            switch (address & 0xF00) {
                case 0x000:
                case 0x100:
                case 0x200:
                case 0x300:
                case 0x400:
                case 0x500:
                case 0x600:
                case 0x700:
                case 0x800:
                case 0x900:
                case 0xA00:
                case 0xB00:
                case 0xC00:
                case 0xD00:
                    memory_.wram_[address - 0xF000 + 0x1000 * memory_.wramBank_] = value;
                    break;
                case 0xE00:
                    if (address < 0xFEA0) { gpu_->oam[address - 0xFE00] = value; }
                    break;
                case 0xF00:
                    switch (address & 0xF0) {
                        case 0x0:
                            switch (address & 0xFF) {
                                case 0x00:
                                    joypad_.SetJoypadState(value);
                                    break;
                                case 0x01:
                                case 0x02:
                                    serial_.WriteSerial(address, value);
                                    break;
                                case 0x04:
                                    timer_.dividerRegister = 0;
                                    break;
                                case 0x05:
                                    timer_.timerCounter = value;
                                    break;
                                case 0x06:
                                    timer_.timerModulo = value;
                                    break;
                                case 0x07:
                                    timer_.clockEnabled = (value & 0x04) != 0;
                                    int newSpeed;
                                    switch (value & 0x03) {
                                        case 0: newSpeed = 1024;
                                            break;
                                        case 1: newSpeed = 16;
                                            break;
                                        case 2: newSpeed = 64;
                                            break;
                                        case 3: newSpeed = 256;
                                            break;
                                        default: newSpeed = 1024;
                                            break;
                                    }
                                    if (newSpeed != timer_.clockSpeed)
                                        timer_.clockSpeed = static_cast<uint32_t>(newSpeed);
                                    break;
                                case 0x0F:
                                    interruptFlag = value;
                                    break;
                                default: throw UnreachableCodeException("Bus::WriteByte Unreachable Code");
                            }
                            break;
                        case 0x10:
                        case 0x20:
                        case 0x30:
                            audio_.WriteByte(address, value);
                            break;
                        case 0x40:
                            if (address == 0xFF46) {
                                /* DMA Transfer */
                                const uint16_t newVal = static_cast<uint16_t>(value) << 8;
                                for (uint16_t i = 0; i < 0xA0; i++) {
                                    gpu_->oam[i] = ReadByte(newVal + i);
                                }
                                return;
                            }
                            if (((address >= 0xFF40) && (address <= 0xFF4B)) || (address == 0xFF4F)) {
                                gpu_->WriteRegisters(address, value);
                            }
                            if (address == 0xFF4D) { speedShift = (value & 0x01) == 0x01; }
                            break;
                        case 0x50:
                            if ((address > 0xFF50) && (address < 0xFF56)) { WriteHDMA(address, value); }
                            break;
                        case 0x60:
                            if ((address > 0xFF67) && (address < 0xFF6C)) { gpu_->WriteRegisters(address, value); }
                            break;
                        case 0x70:
                            if (address == 0xFF70) {
                                if (!(value & 0x07)) {
                                    memory_.wramBank_ = 1;
                                } else {
                                    memory_.wramBank_ = value;
                                }
                            }
                            break;
                        case 0x80:
                        case 0x90:
                        case 0xA0:
                        case 0xB0:
                        case 0xC0:
                        case 0xD0:
                        case 0xE0:
                        case 0xF0:
                            if (address == 0xFFFF) {
                                interruptEnable = value;
                            } else {
                                memory_.hram_[address - 0xFF80] = value;
                            }
                            break;
                        default: throw UnreachableCodeException("Bus::WriteByte 0xFF0");
                    }
                    break;
                default: throw UnreachableCodeException("Bus::WriteByte 0xF00");
            }
            break;
        default:
            throw UnreachableCodeException("Bus::WriteByte unknown write byte called!");
    }
}

void Bus::KeyDown(Keys key) {
    joypad_.SetMatrix(joypad_.GetMatrix() & ~static_cast<uint8_t>(key));
    SetInterrupt(InterruptType::Joypad);
}

void Bus::KeyUp(Keys key) {
    joypad_.SetMatrix(joypad_.GetMatrix() | static_cast<uint8_t>(key));
}

void Bus::UpdateGraphics(const uint32_t cycles) {
    gpu_->hblank = false;
    if (!(gpu_->lcdc & 0x80)) {
        return;
    }
    if (cycles == 0) {
        return;
    }

    uint32_t c = (cycles - 1) / 80 + 1;
    for (uint8_t i = 0; i < c; i++) {
        if (i == (c - 1)) {
            gpu_->scanlineCounter += cycles % 80;
        } else {
            gpu_->scanlineCounter += 80;
        }
        uint32_t d = gpu_->scanlineCounter;
        gpu_->scanlineCounter %= 456;
        if (d != gpu_->scanlineCounter) {
            gpu_->currentLine = (gpu_->currentLine + 1) % 154;
            if (gpu_->stat.enableLYInterrupt && (gpu_->currentLine == gpu_->lyc)) {
                SetInterrupt(InterruptType::LCDStat);
            }
        }
        if (gpu_->currentLine >= 144) {
            if (gpu_->stat.mode == 1) {
                continue;
            }
            gpu_->stat.mode = 1;
            gpu_->vblank = true;
            SetInterrupt(InterruptType::VBlank);
            if (gpu_->stat.enableM1Interrupt) {
                SetInterrupt(InterruptType::LCDStat);
            }
        } else if (gpu_->scanlineCounter <= 80) {
            if (gpu_->stat.mode == 2) {
                continue;
            }
            gpu_->stat.mode = 2;
            if (gpu_->stat.enableM2Interrupt) {
                SetInterrupt(InterruptType::LCDStat);
            }
        } else if (gpu_->scanlineCounter <= (80 + 172)) {
            gpu_->stat.mode = 3;
        } else {
            if (gpu_->stat.mode == 0) {
                continue;
            }
            gpu_->stat.mode = 0;
            gpu_->hblank = true;
            if (gpu_->stat.enableM0Interrupt) {
                SetInterrupt(InterruptType::LCDStat);
            }
            gpu_->DrawScanline();
        }
    }
}

void Bus::UpdateTimers(const uint32_t cycles) {
    timer_.dividerCounter += cycles;
    const uint32_t divCycles = AdjustedCycles(cycles);
    while (timer_.dividerCounter >= divCycles) {
        timer_.dividerCounter -= divCycles;
        timer_.dividerRegister += 1;
    }

    if (timer_.clockEnabled) {
        timer_.clockCounter += cycles;
        const uint32_t rs = timer_.clockCounter / timer_.clockSpeed;
        timer_.clockCounter %= timer_.clockSpeed;
        for (uint8_t i = 0; i < rs; i++) {
            timer_.timerCounter += 1;
            if (timer_.timerCounter == 0x00) {
                timer_.timerCounter = timer_.timerModulo;
                SetInterrupt(InterruptType::Timer);
            }
        }
    }
}

void Bus::SetInterrupt(const InterruptType interrupt) {
    const uint8_t mask = [&]() -> uint8_t {
        switch (interrupt) {
            using enum InterruptType;
            case VBlank: return 0x01;
            case LCDStat: return 0x02;
            case Timer: return 0x04;
            case Joypad: return 0x10;
            default: throw UnreachableCodeException("Bus::SetInterrupt -- unknown interrupt type");
        }
    }();

    interruptFlag |= mask;
}

void Bus::WriteWord(const uint16_t address, const uint16_t value) {
    const uint8_t lower = value >> 8;
    const uint8_t higher = value & 0xFF;
    WriteByte(address, higher);
    WriteByte(address + 1, lower);
}

uint8_t Bus::ReadHDMA(const uint16_t address) const {
    switch (address) {
        case 0xFF51: return hdmaSource >> 8;
        case 0xFF52: return static_cast<uint8_t>(hdmaSource);
        case 0xFF53: return hdmaDestination >> 8;
        case 0xFF54: return static_cast<uint8_t>(hdmaDestination);
        case 0xFF55: return hdmaRemain | (hdmaActive ? 0x00 : 0x80);
        default:
            throw UnreachableCodeException("Bus::ReadHDMA unreachable code");
    }
}

void Bus::WriteHDMA(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0xFF51:
            hdmaSource = (static_cast<uint16_t>(value) << 8) | (hdmaSource & 0xFF);
            break;
        case 0xFF52:
            hdmaSource = (hdmaSource & 0xFF00) | static_cast<uint16_t>(value & 0xF0);
            break;
        case 0xFF53:
            hdmaDestination = 0x8000 | (static_cast<uint16_t>(value & 0x1F) << 8) | (hdmaDestination & 0xFF);
            break;
        case 0xFF54:
            hdmaDestination = (hdmaDestination & 0xFF00) | static_cast<uint16_t>(value & 0xF0);
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
        default:
            throw UnreachableCodeException("Bus::WriteHDMA unreachable code");
    }
}

void Bus::ChangeSpeed() {
    if (speedShift) {
        if (speed == Speed::Double) {
            speed = Speed::Regular;
        } else {
            speed = Speed::Double;
        }
    }
    speedShift = false;
}

inline uint32_t Bus::AdjustedCycles(const uint32_t cycles) const {
    return cycles << (speed == Speed::Double ? 1 : 0);
}
