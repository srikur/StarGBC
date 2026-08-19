#include "Bus.h"

#include <algorithm>
#include <cstring>
#include <meta>
#include <ranges>
#include <vector>

int Bus::DmaBusFor(const uint16_t address) const {
    if (address >= 0x8000 && address <= 0x9FFF) return 1; // VRAM bus
    if (address >= 0xC000 && address <= 0xFDFF) {
        // CGB WRAM sits on its own bus; DMG WRAM shares the external bus
        return gpu_.hardware == Hardware::CGB ? 2 : 0;
    }
    return 0; // ROM / cart RAM / everything else on the external bus
}

uint8_t Bus::ReadDMASource(const uint16_t src) {
    const uint8_t page = src >> 8;
    uint8_t returnValue;
    if (page <= 0x7F) {
        returnValue = cartridge_.ReadByte(src);
    } else if (page <= 0x9F) {
        // The DMA unit reads VRAM on its own bus, regardless of PPU mode
        returnValue = gpu_.vram[gpu_.vramBank * 0x2000 + (src - 0x8000)];
    } else if (page <= 0xBF) {
        returnValue = cartridge_.ReadByte(src);
    } else {
        const uint16_t offset = src & 0x1FFF; // C000-DFFF and echo
        returnValue = offset < 0x1000
                          ? memory_.wram_[offset]
                          : memory_.wram_[(offset & 0x0FFF) + 0x1000 * memory_.wramBank_];
    }
    dmaReadByte = returnValue;
    return returnValue;
}

uint8_t Bus::ReadHDMASource(uint16_t address) const {
    if (address >= 0xE000) address -= 0x4000;
    // Need to research and implement corruption patterns
    if (address >= VRAM_BEGIN && address <= VRAM_END) return 0xFF;
    return ReadByte(address, ComponentSource::HDMA);
}

uint8_t Bus::ReadOAM(const uint16_t address) const {
    return gpu_.stat.mode == GPUMode::MODE_3 ? 0xFF : gpu_.oam[address - 0xFE00];
}

void Bus::WriteOAM(const uint16_t address, const uint8_t value) const {
    if (gpu_.stat.mode != GPUMode::MODE_3) gpu_.oam[address - 0xFE00] = value;
}

uint8_t Bus::ReadByte(const uint16_t address, const ComponentSource source) const {
    if (address >= 0xFE00 && address <= 0xFE9F && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES) return 0xFF;
    // A CPU access on the bus the DMA unit is reading sees the DMA's current
    // byte — only while bytes are actually moving, not during the startup
    // delay or after the last byte (hacktix bully)
    if (source == ComponentSource::CPU && dma_.transferActive &&
        dma_.ticks > DMA::STARTUP_CYCLES && !dma_.transferComplete &&
        address <= 0xFDFF && DmaBusFor(address) == DmaBusFor(dma_.startAddress)) {
        return dmaReadByte;
    }
    switch (address) {
        case 0x0000 ... 0x7FFF: {
            if (bootromRunning) {
                if (gpu_.hardware == Hardware::CGB && (address < 0x100 || address > 0x1FF)) {
                    return bootrom[address];
                }

                if (gpu_.hardware == Hardware::DMG && address < 0x100) {
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
        case 0xFF01 ... 0xFF02: {
            uint8_t value = serial_.ReadSerial(address);
            // The clock-speed bit only exists in CGB mode; it reads 1 otherwise
            if (address == 0xFF02 && !cgbMode) value |= 0x02;
            return value;
        }
        case 0xFF04 ... 0xFF07: return timer_.ReadByte(address);
        case 0xFF0F: return interrupts_.interruptFlag | 0xE0;
        case 0xFF10 ... 0xFF3F: return audio_.ReadByte(address);
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF4D) {
                if (!cgbMode) return 0xFF;
                const uint8_t first = speed == Speed::Double ? 0x80 : 0x00;
                const uint8_t second = prepareSpeedShift ? 0x01 : 0x00;
                return first | second | 0x7E;
            }
            if (address == 0xFF46) { return dma_.writtenValue; }
            if (address == 0xFF4C || address == 0xFF4E) { return 0xFF; }
            return gpu_.ReadRegisters(address);
        }
        case 0xFF50 ... 0xFF55: return gpu_.hdma.ReadHDMA(address, cgbMode);
        case 0xFF56: return cgbMode ? ((rp & 0xC1) | 0x2E) : 0xFF;
        case 0xFF69:
        case 0xFF6B:
            // Palette data is unmapped in DMG-compat mode once the bootrom hands off
            if (!cgbMode && !bootromRunning) return 0xFF;
            return gpu_.ReadRegisters(address);
        case 0xFF68:
        case 0xFF6A:
        case 0xFF6C: return gpu_.ReadRegisters(address);
        case 0xFF70: return cgbMode ? svbkReg : 0xFF;
        case 0xFF72: return gpu_.hardware == Hardware::CGB ? psw72 : 0xFF;
        case 0xFF73: return gpu_.hardware == Hardware::CGB ? psw73 : 0xFF;
        case 0xFF74: return cgbMode ? psw74 : 0xFF;
        case 0xFF75: return gpu_.hardware == Hardware::CGB ? (pgb75 | 0x8F) : 0xFF;
        case 0xFF76: return gpu_.hardware == Hardware::CGB ? audio_.ReadPCM12() : 0xFF;
        case 0xFF77: return gpu_.hardware == Hardware::CGB ? audio_.ReadPCM34() : 0xFF;
        case 0xFF80 ... 0xFFFE: return memory_.hram_[address - 0xFF80];
        case 0xFFFF: return interrupts_.interruptEnable;
        default: return 0xFF;
    }
}

void Bus::WriteByte(const uint16_t address, const uint8_t value, const ComponentSource source) {
    if (address >= 0xFE00 && address <= 0xFE9F && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES) return;
    if (source == ComponentSource::CPU && dma_.transferActive &&
        dma_.ticks > DMA::STARTUP_CYCLES && !dma_.transferComplete &&
        address <= 0xFDFF && DmaBusFor(address) == DmaBusFor(dma_.startAddress))
        return;
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
        case 0xFF01 ... 0xFF02: serial_.WriteSerial(address, value, speed == Speed::Double, cgbMode);
            break;
        case 0xFF04 ... 0xFF07: timer_.WriteByte(address, value, speed);
            break;
        case 0xFF0F: interrupts_.interruptFlag = value;
            break;
        case 0xFF10 ... 0xFF3F: audio_.WriteByte(address, value,
                                                 timer_.divCounter & (speed == Speed::Double ? 0x2000 : 0x1000));
            break;
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF46) {
                dma_.Set(value);
            } else if (address == 0xFF4C) {
                // KEY0: the CGB bootrom writes $04 here for DMG-compat carts;
                // locked once the bootrom hands off
                if (gpu_.hardware == Hardware::CGB && bootromRunning) {
                    key0Written = true;
                    cgbMode = (value & 0x0C) == 0;
                    gpu_.dmgCompat = !cgbMode;
                }
            } else if (address == 0xFF4D) {
                if (cgbMode) prepareSpeedShift = (value & 0x01) == 0x01;
            } else if (address == 0xFF4F) {
                if (cgbMode) gpu_.WriteRegisters(address, value);
            } else { gpu_.WriteRegisters(address, value); }
            break;
        }
        case 0xFF51 ... 0xFF55:
            if (cgbMode) {
                gpu_.hdma.WriteHDMA(address, value, gpu_.LCDDisabled(),
                                    gpu_.stat.mode == GPUMode::MODE_0);
            }
            break;
        case 0xFF56: if (gpu_.hardware == Hardware::CGB) rp = value;
            break;
        case 0xFF68:
        case 0xFF6A:
            if (gpu_.hardware == Hardware::CGB) gpu_.WriteRegisters(address, value);
            break;
        case 0xFF69:
        case 0xFF6B:
            // Palette data writes are ignored in DMG-compat mode after boot
            if (cgbMode || bootromRunning) gpu_.WriteRegisters(address, value);
            break;
        case 0xFF6C:
            // OPRI is only writable while the bootrom runs
            if (gpu_.hardware == Hardware::CGB && bootromRunning) gpu_.WriteRegisters(address, value);
            break;
        case 0xFF70:
            if (cgbMode || (gpu_.hardware == Hardware::CGB && bootromRunning)) {
                memory_.wramBank_ = (value & 0x07) ? (value & 0x07) : 1;
                svbkReg = value | 0xF8;
            }
            break;
        case 0xFF72: if (gpu_.hardware == Hardware::CGB) psw72 = value;
            break;
        case 0xFF73: if (gpu_.hardware == Hardware::CGB) psw73 = value;
            break;
        case 0xFF74: if (gpu_.hardware == Hardware::CGB) psw74 = value;
            break;
        case 0xFF75: if (gpu_.hardware == Hardware::CGB) pgb75 = value & 0x70;
            break;
        case 0xFF80 ... 0xFFFE: memory_.hram_[address - 0xFF80] = value;
            break;
        case 0xFFFF: interrupts_.interruptEnable = value;
            break;
        default: break;
    }
}

void Bus::UpdateDMA() {
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

        gpu_.oam[dma_.currentByte] = ReadDMASource(dma_.startAddress + dma_.currentByte);
        ++dma_.currentByte;
        if (dma_.currentByte == DMA::TOTAL_BYTES) {
            dma_.transferComplete = true;
        }
    }
}

void Bus::RunHDMA() const {
    if (!gpu_.hdma.hdmaActive || gpu_.hardware == Hardware::DMG) {
        return;
    }

    switch (gpu_.hdma.hdmaMode) {
        case HDMAMode::GDMA: {
            if (gpu_.hdma.step == HDMAStep::Read) {
                gpu_.hdma.byte = ReadHDMASource(gpu_.hdma.hdmaSource);
                gpu_.hdma.step = HDMAStep::Write;
            } else {
                gpu_.WriteVRAM(gpu_.hdma.hdmaDestination, gpu_.hdma.byte);
                gpu_.hdma.bytesThisBlock++;
                gpu_.hdma.step = HDMAStep::Read;
                gpu_.hdma.hdmaSource++;
                gpu_.hdma.hdmaDestination++;
                if (gpu_.hdma.bytesThisBlock == 0x10) {
                    gpu_.hdma.bytesThisBlock = 0;
                    gpu_.hdma.hdmaRemain -= 1;
                    gpu_.hdma.hdma5 = gpu_.hdma.hdmaRemain > 0 ? (gpu_.hdma.hdmaRemain - 1) : 0xFF;
                }
            }
            if (gpu_.hdma.hdmaRemain == 0) gpu_.hdma.hdmaActive = false;
            return;
        }
        case HDMAMode::HDMA: {
            // Delay is always 4 cycles whether in single or double speed mode
            if (gpu_.hdma.hdmaStartDelay > 0) {
                gpu_.hdma.hdmaStartDelay--;
                gpu_.hdma.transferringBlock = false;
                return;
            }
            // HDMA copy won't happen if the CPU is in HALT or STOP mode, or during a speed shift
            if (!gpu_.hblank || gpu_.vblank || speedShiftActive) {
                gpu_.hdma.transferringBlock = false;
                return;
            }
            // When LCD is on: one block per H-blank period
            if (!gpu_.LCDDisabled() && gpu_.hdma.hblankBlockFinished) {
                gpu_.hdma.transferringBlock = false;
                return;
            }
            // When LCD is off: only one block transfers until LCD turns back on
            if (gpu_.LCDDisabled() && !gpu_.hdma.singleBlockTransfer) {
                gpu_.hdma.transferringBlock = false;
                return;
            }

            // We're actively transferring
            gpu_.hdma.transferringBlock = true;

            if (gpu_.hdma.step == HDMAStep::Read) {
                gpu_.hdma.byte = ReadHDMASource(gpu_.hdma.hdmaSource);
                gpu_.hdma.step = HDMAStep::Write;
            } else {
                gpu_.WriteVRAM(gpu_.hdma.hdmaDestination, gpu_.hdma.byte);
                gpu_.hdma.bytesThisBlock++;
                gpu_.hdma.step = HDMAStep::Read;
                gpu_.hdma.hdmaSource++;
                gpu_.hdma.hdmaDestination++;
                if (gpu_.hdma.bytesThisBlock == 0x10) {
                    gpu_.hdma.singleBlockTransfer = false;
                    gpu_.hdma.hblankBlockFinished = true;
                    gpu_.hdma.transferringBlock = false; // Block complete, CPU can resume
                    gpu_.hdma.bytesThisBlock = 0;
                    gpu_.hdma.hdmaRemain -= 1;
                    gpu_.hdma.hdma5 = gpu_.hdma.hdmaRemain > 0 ? (gpu_.hdma.hdmaRemain - 1) : 0xFF;
                }
            }
            if (gpu_.hdma.hdmaRemain == 0x00) gpu_.hdma.hdmaActive = false;
            return;
        }
        default: return;
    }
}

void Bus::ChangeSpeed() {
    if (prepareSpeedShift) {
        speed = speed == Speed::Regular ? Speed::Double : Speed::Regular;
        prepareSpeedShift = false;
    }
}

void Bus::HandleOAMCorruption(const uint16_t location, const CorruptionType type) const {
    if ((gpu_.hardware != Hardware::DMG) || (location < 0xFE00 || location > 0xFEFF) || gpu_.stat.mode !=
        GPUMode::MODE_2)
        return;
    if (gpu_.scanlineCounter >= 81) return;
    const int currentRowIndex = gpu_.GetOAMScanRow();

    auto ReadWord = [&](const int index) -> uint16_t {
        return static_cast<uint16_t>(gpu_.oam[index]) << 8 | gpu_.oam[index + 1];
    };
    auto WriteWord = [&](const int index, const uint16_t value) {
        gpu_.oam[index] = static_cast<uint8_t>(value >> 8);
        gpu_.oam[index + 1] = static_cast<uint8_t>(value & 0xFF);
    };

    if (type == CorruptionType::ReadWrite) {
        if (currentRowIndex >= 4 && currentRowIndex < 19) {
            const int row_n_addr = currentRowIndex * 8;
            const int row_n_minus_1_addr = (currentRowIndex - 1) * 8;
            const int row_n_minus_2_addr = (currentRowIndex - 2) * 8;

            const uint16_t a_rw = ReadWord(row_n_minus_2_addr);
            const uint16_t b_rw = ReadWord(row_n_minus_1_addr);
            const uint16_t c_rw = ReadWord(row_n_addr);
            const uint16_t d_rw = ReadWord(row_n_minus_1_addr + 4);

            const uint16_t corrupted_b = (b_rw & (a_rw | c_rw | d_rw)) | (a_rw & c_rw & d_rw);
            WriteWord(row_n_minus_1_addr, corrupted_b);

            uint8_t temp_row[8];
            std::memcpy(temp_row, &gpu_.oam[row_n_minus_1_addr], 8);
            std::memcpy(&gpu_.oam[row_n_addr], temp_row, 8);
            std::memcpy(&gpu_.oam[row_n_minus_2_addr], temp_row, 8);
        }

        if (currentRowIndex > 0) {
            const int currentRowAddr = currentRowIndex * 8;
            const int prevRowAddr = (currentRowIndex - 1) * 8;

            const uint16_t a_read = ReadWord(currentRowAddr);
            const uint16_t b_read = ReadWord(prevRowAddr);
            const uint16_t c_read = ReadWord(prevRowAddr + 4);

            const uint16_t corruptedWord = b_read | (a_read & c_read);
            WriteWord(currentRowAddr, corruptedWord);

            std::memcpy(&gpu_.oam[currentRowAddr + 2], &gpu_.oam[prevRowAddr + 2], 6);
        }
    } else {
        if (currentRowIndex == 0) return;

        const int currentRowAddr = currentRowIndex * 8;
        const int prevRowAddr = (currentRowIndex - 1) * 8;

        const uint16_t a = ReadWord(currentRowAddr);
        const uint16_t b = ReadWord(prevRowAddr);
        const uint16_t c = ReadWord(prevRowAddr + 4);

        const uint16_t corruptedWord = (type == CorruptionType::Write)
                                           ? ((a ^ c) & (b ^ c)) ^ c
                                           : (b | (a & c));
        WriteWord(currentRowAddr, corruptedWord);
        std::memcpy(&gpu_.oam[currentRowAddr + 2], &gpu_.oam[prevRowAddr + 2], 6);
    }
}
