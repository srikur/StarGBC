#include "Bus.h"

#include <algorithm>

uint8_t Bus::ReadDMASource(const uint16_t src) {
    const uint8_t page = src >> 8;
    uint8_t returnValue{};
    if (page <= 0x7F) returnValue = cartridge_.ReadByte(src);
    if (page <= 0x9F) returnValue = gpu_.ReadVRAM(src);
    if (page <= 0xBF) returnValue = cartridge_.ReadByte(src);
    if (page <= 0xFF) returnValue = memory_.wram_[src & 0x1FFF];
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

uint8_t Bus::ReadByte(const uint16_t address, ComponentSource source) const {
    if (address >= 0xFE00 && address <= 0xFE9F && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES) return 0xFF;
    if (source == ComponentSource::CPU && dma_.transferActive && (address < 0xFF80 || address > 0xFFFE)) return dmaReadByte;
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
        case 0xFF01 ... 0xFF02: return serial_.ReadSerial(address);
        case 0xFF04 ... 0xFF07: return timer_.ReadByte(address);
        case 0xFF0F: return interrupts_.interruptFlag | 0xE0;
        case 0xFF10 ... 0xFF3F: return audio_.ReadByte(address);
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF4D) {
                if (gpu_.hardware == Hardware::DMG) return 0xFF;
                const uint8_t first = speed == Speed::Double ? 0x80 : 0x00;
                const uint8_t second = prepareSpeedShift ? 0x01 : 0x00;
                return first | second | 0x7E;
            }
            if (address == 0xFF46) { return dma_.writtenValue; }
            if (address == 0xFF4C || address == 0xFF4E) { return 0xFF; }
            return gpu_.ReadRegisters(address);
        }
        case 0xFF50 ... 0xFF55: return gpu_.hdma.ReadHDMA(address, gpu_.hardware == Hardware::CGB);
        case 0xFF68 ... 0xFF6C: return gpu_.ReadRegisters(address);
        case 0xFF70: return gpu_.hardware == Hardware::CGB ? memory_.wramBank_ : 0xFF;
        case 0xFF80 ... 0xFFFE: return memory_.hram_[address - 0xFF80];
        case 0xFFFF: return interrupts_.interruptEnable;
        default: return 0xFF;
    }
}

void Bus::WriteByte(const uint16_t address, const uint8_t value, ComponentSource source) {
    if (address >= 0xFE00 && address <= 0xFE9F && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES) return;
    if (source == ComponentSource::CPU && dma_.transferActive && (address < 0xFF80 || address > 0xFFFE)) return;
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
        case 0xFF01 ... 0xFF02: serial_.WriteSerial(address, value, speed == Speed::Double, gpu_.hardware == Hardware::CGB);
            break;
        case 0xFF04 ... 0xFF07: timer_.WriteByte(address, value);
            break;
        case 0xFF0F: interrupts_.interruptFlag = value;
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
        case 0xFFFF: interrupts_.interruptEnable = value;
            break;
        default: break;
    }
}

void Bus::UpdateTimers() const {
    const int frameSeqBit = audio_.IsDMG() || speed == Speed::Regular ? 12 : 13;

    timer_.reloadActive = false;
    if (timer_.overflowPending && --timer_.overflowDelay == 0) {
        timer_.tima = timer_.tma;
        interrupts_.Set(InterruptType::Timer, false);
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

        gpu_.oam[dma_.currentByte++] = ReadDMASource(dma_.startAddress + dma_.currentByte);
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
                }
            }
            if (gpu_.hdma.hdmaRemain == 0) gpu_.hdma.hdmaActive = false;
            return;
        }
        case HDMAMode::HDMA: {
            // Delay is always 4 cycles whether in single or double speed mode
            if (gpu_.hdma.hdmaStartDelay-- > 0) return;
            // HDMA copy won't happen if the CPU is in HALT or STOP mode, or during a speed shift
            if (!gpu_.hblank || gpu_.vblank || speedShiftActive) return;
            if (gpu_.hdma.hblankBlockFinished) return; // Copies one block per H-Blank

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
                    gpu_.hdma.hblankBlockFinished = true;
                    gpu_.hdma.bytesThisBlock = 0;
                    gpu_.hdma.hdmaRemain -= 1;
                    gpu_.hdma.hdma5 |= gpu_.hdma.hdmaRemain;
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
    if ((gpu_.hardware != Hardware::DMG) || (location < 0xFE00 || location > 0xFEFF) || gpu_.stat.mode != GPUMode::MODE_2) return;
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

bool Bus::SaveState(std::ofstream &stateFile) const {
    try {
        stateFile.write(reinterpret_cast<const char *>(&speed), sizeof(speed));
        stateFile.write(reinterpret_cast<const char *>(&prepareSpeedShift), sizeof(prepareSpeedShift));
        stateFile.write(reinterpret_cast<const char *>(&bootromRunning), sizeof(bootromRunning));
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

        cartridge_.LoadState(stateFile);
        gpu_.LoadState(stateFile);
        joypad_.LoadState(stateFile);
        memory_.LoadState(stateFile);
        timer_.LoadState(stateFile);
        serial_.LoadState(stateFile);
    } catch ([[maybe_unused]] const std::exception &e) {
    }
}
