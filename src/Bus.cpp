#include "Bus.h"

#include <algorithm>
#include <cstring>
#include <meta>
#include <ranges>
#include <vector>

// TODO: Change to enum
int Bus::DmaBusFor(const uint16_t address) const {
    if (address >= VRAM_BEGIN && address <= VRAM_END)
        return 1; // VRAM bus
    if (address >= WRAM_BEGIN && address <= ERAM_END) {
        // CGB WRAM sits on its own bus; DMG WRAM shares the external bus
        return IsCgb(gpu_.model) ? 2 : 0;
    }
    return 0; // ROM / cart RAM / everything else on the external bus
}

bool Bus::cpu_dma_conflict(const u16 address) const {
    if (!dma_.transferActive || dma_.ticks <= DMA::STARTUP_CYCLES || address >= OAM_BEGIN) {
        return false;
    }
    if (IsCgb(gpu_.model)) {
        if (dma_.startAddress >= ERAM_BEGIN) {
            return DmaBusFor(address) != 1;
        }
        if (address >= WRAM_BEGIN) {
            return DmaBusFor(dma_.startAddress) != 1;
        }
    }
    return DmaBusFor(address) == DmaBusFor(dma_.startAddress);
}

u32 Bus::dma_wram_offset(const u16 address) const { return (address & 0x0FFF) + ((dma_.startAddress & 0x1000) ? 0x1000 * memory_.wramBank_ : 0); }

uint8_t Bus::ReadDMASource(const uint16_t src) {
    const uint8_t page = src >> 8;
    uint8_t returnValue;
    // TODO: Check godbolt to see if IILE can be just as efficient as switch or ifs
    if (IsCgb(gpu_.model) && page >= 0xE0) {
        // DMA reads above 0xDFFF on CGB return FF
        returnValue = 0xFF;
    } else if (page <= 0x7F) {
        returnValue = cartridge_.ReadByte(src);
    } else if (page <= 0x9F) {
        // The DMA unit reads VRAM on its own bus, regardless of PPU mode
        returnValue = gpu_.vram[gpu_.vramBank * 0x2000 + (src - 0x8000)];
    } else if (page <= 0xBF) {
        returnValue = cartridge_.ReadByte(src);
    } else {
        const uint16_t offset = src & 0x1FFF; // C000-DFFF and echo
        returnValue = offset < 0x1000 ? memory_.wram_[offset] : memory_.wram_[(offset & 0x0FFF) + 0x1000 * memory_.wramBank_];
    }
    dmaReadByte = returnValue;
    // TODO: Maybe add directives to remove observation branches from Release builds
    if (observations)
        observations->Emit(diagnostics::Component::Bus, diagnostics::EventKind::BusRead, src, returnValue, 0, diagnostics::Initiator::DMA, false,
                           DebugBank(src));
    return returnValue;
}

uint8_t Bus::ReadHDMASource(uint16_t address) const {
    if (address >= 0xE000)
        address -= 0x4000;
    // Need to research and implement corruption patterns
    if (address >= VRAM_BEGIN && address <= VRAM_END)
        return 0xFF;
    return ReadByte(address, ComponentSource::HDMA);
}

uint8_t Bus::ReadOAM(const uint16_t address, const ComponentSource source) const {
    const bool blocked = source == ComponentSource::CPU ? gpu_.CpuOamReadBlocked() : gpu_.stat.mode == GPUMode::MODE_3;
    if (blocked)
        return 0xFF;
    if (address < 0xFEA0)
        return gpu_.oam[address - 0xFE00];
    uint8_t offset = address & 0xFF;
    switch (UnusableOamFor(gpu_.model)) {
        case UnusableOamBehavior::Zero:
            return 0;
        case UnusableOamBehavior::AddressPattern:
            return (offset & 0xF0) | (offset >> 4);
        case UnusableOamBehavior::EarlyCgbRam:
            offset &= ~0x18;
            break;
        case UnusableOamBehavior::CgbDRam:
            if (offset >= 0xC0)
                offset |= 0xF0;
            break;
    }
    return gpu_.extraOam[offset - 0xA0];
}

void Bus::WriteOAM(const uint16_t address, const uint8_t value, const ComponentSource source) const {
    if (source == ComponentSource::CPU)
        HandleOAMCorruption(address, CorruptionType::Write);
    const bool blocked = source == ComponentSource::CPU ? gpu_.CpuOamWriteBlocked() : gpu_.stat.mode == GPUMode::MODE_3;
    if (blocked)
        return;
    if (address < 0xFEA0) {
        gpu_.oam[address - 0xFE00] = value;
        if (source == ComponentSource::CPU && !IsCgb(gpu_.model) && !gpu_.LCDDisabled() &&
            ((gpu_.stat.mode == GPUMode::MODE_0 && gpu_.currentLine < 143) || gpu_.currentLine == 153) && gpu_.scanlineCounter >= 452) {
            const unsigned row = address & 0xF8;
            const unsigned word = address & 0xFE;
            for (unsigned byte = 0; byte < 2; ++byte) {
                const auto a = gpu_.oam[byte], b = gpu_.oam[row + byte], c = gpu_.oam[word + byte];
                gpu_.oam[byte] = ((a ^ c) & (b ^ c)) ^ c;
            }
            for (unsigned byte = 2; byte < 8; ++byte)
                gpu_.oam[byte] = gpu_.oam[row + byte];
        }
        return;
    }
    uint8_t offset = address & 0xFF;
    switch (UnusableOamFor(gpu_.model)) {
        case UnusableOamBehavior::Zero:
        case UnusableOamBehavior::AddressPattern:
            return;
        case UnusableOamBehavior::EarlyCgbRam:
            offset &= ~0x18;
            break;
        case UnusableOamBehavior::CgbDRam:
            if (offset >= 0xC0)
                offset |= 0xF0;
            break;
    }
    gpu_.extraOam[offset - 0xA0] = value;
}

uint8_t Bus::ReadByte(const uint16_t address, const ComponentSource source) const {
    const auto value = ReadByteValue<false>(address, source);
    if (observations)
        observations->Emit(diagnostics::Component::Bus, diagnostics::EventKind::BusRead, address, value, 0,
                           static_cast<diagnostics::Initiator>(source), AccessBlocked(address, source, false), DebugBank(address));
    return value;
}

uint16_t Bus::DebugBank(const uint16_t address) const {
    if (address < 0x8000) {
        if (bootromRunning && address < bootrom.size() && (address < 0x100 || (IsCgb(gpu_.model) && address >= 0x200)))
            return 0xFFFF;
        return cartridge_.DebugBank(address);
    }
    if (address < 0xA000)
        return gpu_.vramBank;
    if (address < 0xC000)
        return cartridge_.DebugBank(address);
    if ((address >= 0xD000 && address < 0xE000) || (address >= 0xF000 && address < 0xFE00))
        return memory_.wramBank_;
    return 0;
}

bool Bus::AccessBlocked(const uint16_t address, const ComponentSource source, const bool write) const {
    if (address >= 0xFE00 && address <= 0xFEFF && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES)
        return true;
    if (source == ComponentSource::CPU && cpu_dma_conflict(address))
        return true;
    if (address >= 0x8000 && address < 0xA000)
        return source == ComponentSource::CPU ? (write ? gpu_.CpuVramWriteBlocked() : gpu_.CpuVramReadBlocked()) : gpu_.stat.mode == GPUMode::MODE_3;
    if (address >= 0xFE00 && address <= 0xFEFF)
        return source == ComponentSource::CPU ? (write ? gpu_.CpuOamWriteBlocked() : gpu_.CpuOamReadBlocked()) : gpu_.stat.mode == GPUMode::MODE_3;
    if (address == BACKGROUND_PALETTE_DATA || address == OBJ_PALETTE_DATA)
        return (!cgbMode && !bootromRunning) || gpu_.cpu_palette_blocked();
    return false;
}

uint8_t Bus::InspectRead(const uint16_t address, const ComponentSource source) const { return ReadByteValue<true>(address, source); }

template<bool Inspect>
u8 Bus::ReadByteValue(const u16 address, const ComponentSource source) const {
    if (address >= 0xFE00 && address <= 0xFEFF && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES)
        return 0xFF;
    if (source == ComponentSource::CPU && cpu_dma_conflict(address)) {
        if (IsCgb(gpu_.model) && address >= WRAM_BEGIN && (dma_.startAddress < WRAM_BEGIN || dma_.startAddress >= ERAM_BEGIN)) {
            return memory_.wram_[dma_wram_offset(address)];
        }
        const u8 value = gpu_.oam[dma_.currentByte - 1];
        if constexpr (!Inspect) {
            // CGB VRAM — reads clear the destination byte
            if (IsCgb(gpu_.model) && DmaBusFor(dma_.startAddress) == 1) {
                gpu_.oam[dma_.currentByte - 1] = 0;
            }
        }
        return value;
    }
    if (source == ComponentSource::CPU && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES && !dma_.transferComplete && address <= 0xFDFF &&
        DmaBusFor(address) == DmaBusFor(dma_.startAddress)) {
        return dmaReadByte;
    }
    switch (address) {
        case 0x0000 ... 0x7FFF: {
            if (bootromRunning) {
                if (IsCgb(gpu_.model) && address < bootrom.size() && (address < 0x100 || address > 0x1FF)) {
                    return bootrom[address];
                }

                if (IsDmg(gpu_.model) && address < 0x100) {
                    return bootrom[address];
                }
            }
            return cartridge_.ReadByte(address);
        }
        case 0x8000 ... 0x9FFF: {
            if (source == ComponentSource::CPU) {
                return gpu_.CpuVramReadBlocked() ? 0xFF : gpu_.vram[gpu_.vramBank * 0x2000 + address - 0x8000];
            }
            return gpu_.ReadVRAM(address);
        }
        case 0xA000 ... 0xBFFF:
            return cartridge_.ReadByte(address);
        case WRAM_BEGIN ... 0xCFFF:
            return memory_.wram_[address - WRAM_BEGIN];
        case 0xD000 ... 0xDFFF:
            return memory_.wram_[address - 0xD000 + 0x1000 * memory_.wramBank_];
        case ERAM_BEGIN ... 0xEFFF:
            return memory_.wram_[address - ERAM_BEGIN];
        case 0xF000 ... 0xFDFF:
            return memory_.wram_[address - 0xF000 + 0x1000 * memory_.wramBank_];
        case 0xFE00 ... 0xFEFF:
            return ReadOAM(address, source);
        case 0xFF00:
            return joypad_.GetJoypadState() | 0xC0;
        case 0xFF01 ... 0xFF02: {
            uint8_t value = serial_.read_serial(address);
            // The clock-speed bit only exists in CGB mode; it reads 1 otherwise
            if (address == 0xFF02 && !cgbMode)
                value |= 0x02;
            return value;
        }
        case 0xFF04 ... 0xFF07:
            return timer_.ReadByte(address);
        case 0xFF0F:
            return interrupts_.interrupt_flag | interrupts_.interrupt_visible_pending | 0xE0;
        case 0xFF10 ... 0xFF3F:
            if constexpr (Inspect)
                return audio_.InspectRegister(address);
            else
                return audio_.ReadByte(address);
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF4D) {
                if (!cgbMode)
                    return 0xFF;
                const uint8_t first = speed == Speed::Double ? 0x80 : 0x00;
                const uint8_t second = prepareSpeedShift ? 0x01 : 0x00;
                return first | second | 0x7E;
            }
            if (address == 0xFF46) {
                return dma_.writtenValue;
            }
            if (address == 0xFF4C || address == 0xFF4E) {
                return 0xFF;
            }
            return gpu_.ReadRegisters(address);
        }
        case 0xFF50 ... 0xFF55:
            return gpu_.hdma.ReadHDMA(address, cgbMode);
        case 0xFF56:
            return cgbMode ? ((rp & 0xC1) | 0x2E) : 0xFF;
        case 0xFF69:
        case 0xFF6B:
            // Palette data is unmapped in DMG-compat mode once the bootrom hands off
            if (!cgbMode && !bootromRunning)
                return 0xFF;
            return gpu_.ReadRegisters(address);
        case 0xFF68:
        case 0xFF6A:
        case 0xFF6C:
            return gpu_.ReadRegisters(address);
        case 0xFF70:
            return cgbMode ? svbkReg : 0xFF;
        case 0xFF72:
            return IsCgb(gpu_.model) ? psw72 : 0xFF;
        case 0xFF73:
            return IsCgb(gpu_.model) ? psw73 : 0xFF;
        case 0xFF74:
            return cgbMode ? psw74 : 0xFF;
        case 0xFF75:
            return IsCgb(gpu_.model) ? (pgb75 | 0x8F) : 0xFF;
        case 0xFF76:
            if constexpr (Inspect)
                return IsCgb(gpu_.model) ? audio_.InspectRegister(address) : 0xFF;
            else
                return IsCgb(gpu_.model) ? audio_.ReadPCM12() : 0xFF;
        case 0xFF77:
            if constexpr (Inspect)
                return IsCgb(gpu_.model) ? audio_.InspectRegister(address) : 0xFF;
            else
                return IsCgb(gpu_.model) ? audio_.ReadPCM34() : 0xFF;
        case 0xFF80 ... 0xFFFE:
            return memory_.hram_[address - 0xFF80];
        case 0xFFFF:
            return interrupts_.interrupt_enable;
        default:
            return 0xFF;
    }
}

void Bus::WriteByte(const uint16_t address, const uint8_t value, const ComponentSource source) {
    const bool hdmaOwned = gpu_.hdma.ShouldHaltCPU();
    if (observations) {
        const auto blocked = AccessBlocked(address, source, true);
        observations->Emit(diagnostics::Component::Bus, diagnostics::EventKind::BusWrite, address, value, 0,
                           static_cast<diagnostics::Initiator>(source), blocked, DebugBank(address));
        if (address >= 0xFF00)
            observations->Emit(diagnostics::Component::Bus, diagnostics::EventKind::RegisterWrite, address, value, 0,
                               static_cast<diagnostics::Initiator>(source), blocked);
    }
    WriteByteValue(address, value, source);
    if (observations && hdmaOwned != gpu_.hdma.ShouldHaltCPU())
        observations->Emit(diagnostics::Component::DMA, diagnostics::EventKind::DmaOwnership, 0xFF55, gpu_.hdma.ShouldHaltCPU(), 0,
                           diagnostics::Initiator::HDMA);
    if (observations && address == 0xFF0F)
        observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptVisible, 0xFF0F,
                           interrupts_.interrupt_flag | interrupts_.interrupt_visible_pending | 0xE0);
}

void Bus::WriteByteValue(const uint16_t address, const uint8_t value, const ComponentSource source) {
    if (address >= 0xFE00 && address <= 0xFEFF && dma_.transferActive && dma_.ticks > DMA::STARTUP_CYCLES)
        return;
    if (source == ComponentSource::CPU && cpu_dma_conflict(address)) {
        if (IsCgb(gpu_.model) && address >= WRAM_BEGIN && (dma_.startAddress < WRAM_BEGIN || dma_.startAddress >= ERAM_BEGIN)) {
            memory_.wram_[dma_wram_offset(address)] = value;
            return;
        }
        // appears that writes to the OAM DMA are not discarded, but have corrupted writes to the current OAM byte depending on the model:
        // 1) DMG WRAM — AND of OAM data and value
        // 2) other DMG — bus value
        // 3) CGB VRAM — byte set to 0
        // 4) CGB WRAM — retain DMA value
        // 5) CGB other — bus value
        auto &transferred = gpu_.oam[dma_.currentByte - 1];
        if (IsCgb(gpu_.model)) {
            if (dma_.startAddress < WRAM_BEGIN || dma_.startAddress >= ERAM_BEGIN) {
                transferred = DmaBusFor(dma_.startAddress) == 1 ? 0 : value;
            }
        } else {
            transferred = dma_.startAddress >= WRAM_BEGIN ? transferred & value : value;
        }
        return;
    }
    switch (address) {
        case 0x0000 ... 0x7FFF:
            cartridge_.WriteByte(address, value);
            break;
        case 0x8000 ... 0x9FFF: {
            if (source == ComponentSource::CPU) {
                if (!gpu_.CpuVramWriteBlocked())
                    gpu_.vram[gpu_.vramBank * 0x2000 + address - 0x8000] = value;
            } else {
                gpu_.WriteVRAM(address, value);
            }
            break;
        }
        case 0xA000 ... 0xBFFF:
            cartridge_.WriteByte(address, value);
            break;
        case 0xC000 ... 0xCFFF:
            memory_.wram_[address - 0xC000] = value;
            break;
        case 0xD000 ... 0xDFFF:
            memory_.wram_[address - 0xD000 + 0x1000 * memory_.wramBank_] = value;
            break;
        case 0xE000 ... 0xEFFF:
            memory_.wram_[address - 0xE000] = value;
            break;
        case 0xF000 ... 0xFDFF:
            memory_.wram_[address - 0xF000 + 0x1000 * memory_.wramBank_] = value;
            break;
        case 0xFE00 ... 0xFEFF:
            WriteOAM(address, value, source);
            break;
        case 0xFF00:
            joypad_.SetJoypadState(value);
            break;
        case 0xFF01 ... 0xFF02:
            serial_.write_serial(address, value, cgbMode);
            break;
        case 0xFF04 ... 0xFF07:
            timer_.WriteByte(address, value, speed);
            break;
        case 0xFF0F:
            interrupts_.interrupt_flag = value;
            interrupts_.interrupt_flag_delayed &= value | ~interrupts_.interrupt_visible_pending;
            interrupts_.interrupt_visible_pending &= value;
            break;
        case 0xFF10 ... 0xFF3F:
            audio_.WriteByte(address, value, timer_.divCounter & (speed == Speed::Double ? 0x2000 : 0x1000), speed == Speed::Double);
            break;
        case 0xFF40 ... 0xFF4F: {
            if (address == 0xFF46) {
                dma_.Set(value);
            } else if (address == 0xFF4C) {
                // KEY0: the CGB bootrom writes $04 here for DMG-compat carts;
                // locked once the bootrom hands off
                if (IsCgb(gpu_.model) && bootromRunning) {
                    key0Written = true;
                    cgbMode = (value & 0x0C) == 0;
                    gpu_.dmg_compat = !cgbMode;
                }
            } else if (address == 0xFF4D) {
                if (cgbMode)
                    prepareSpeedShift = (value & 0x01) == 0x01;
            } else if (address == 0xFF4F) {
                if (cgbMode)
                    gpu_.WriteRegisters(address, value);
            } else {
                gpu_.WriteRegisters(address, value);
            }
            break;
        }
        case 0xFF51 ... 0xFF55:
            if (cgbMode) {
                gpu_.hdma.WriteHDMA(address, value, gpu_.LCDDisabled(), gpu_.stat.mode == GPUMode::MODE_0);
            }
            break;
        case 0xFF56:
            if (IsCgb(gpu_.model))
                rp = value;
            break;
        case 0xFF68:
        case 0xFF6A:
            if (IsCgb(gpu_.model))
                gpu_.WriteRegisters(address, value);
            break;
        case 0xFF69:
        case 0xFF6B:
            // Palette data writes are ignored in DMG-compat mode after boot
            if (cgbMode || bootromRunning)
                gpu_.WriteRegisters(address, value);
            break;
        case 0xFF6C:
            // OPRI is only writable while the bootrom runs
            if (IsCgb(gpu_.model) && bootromRunning)
                gpu_.WriteRegisters(address, value);
            break;
        case 0xFF70:
            if (cgbMode || (IsCgb(gpu_.model) && bootromRunning)) {
                memory_.wramBank_ = (value & 0x07) ? (value & 0x07) : 1;
                svbkReg = value | 0xF8;
            }
            break;
        case 0xFF72:
            if (IsCgb(gpu_.model))
                psw72 = value;
            break;
        case 0xFF73:
            if (IsCgb(gpu_.model))
                psw73 = value;
            break;
        case 0xFF74:
            if (IsCgb(gpu_.model))
                psw74 = value;
            break;
        case 0xFF75:
            if (IsCgb(gpu_.model))
                pgb75 = value & 0x70;
            break;
        case 0xFF80 ... 0xFFFE:
            memory_.hram_[address - 0xFF80] = value;
            break;
        case 0xFFFF:
            interrupts_.interrupt_enable = value;
            break;
        default:
            break;
    }
}

void Bus::UpdateDMAWork() {
    if (gpu_.cpu_halted && !dma_.halt_step_pending)
        return;
    dma_.halt_step_pending = false;
    if (dma_.transferComplete) {
        dma_.transferActive = false;
        dma_.transferComplete = false;
        if (observations)
            observations->Emit(diagnostics::Component::DMA, diagnostics::EventKind::DmaOwnership, dma_.startAddress, false, 0,
                               diagnostics::Initiator::DMA);
        dma_.ticks = 0;
        dma_.currentByte = 0;
    }
    if (!dma_.transferActive) {
        return;
    }
    if (dma_.restartPending && --dma_.restartCountdown == 0) {
        dma_.restartPending = false;
        dma_.startAddress = dma_.pendingStart;
        dma_.currentByte = 0;
        dma_.ticks = 1;
    }

    ++dma_.ticks;
    if (dma_.ticks <= DMA::STARTUP_CYCLES)
        return; // OAM still accessible here

    if (observations && dma_.ticks == DMA::STARTUP_CYCLES + 1)
        observations->Emit(diagnostics::Component::DMA, diagnostics::EventKind::DmaOwnership, dma_.startAddress, true, 0,
                           diagnostics::Initiator::DMA);
    gpu_.oam[dma_.currentByte] = ReadDMASource(dma_.startAddress + dma_.currentByte);
    if (observations)
        observations->Emit(diagnostics::Component::DMA, diagnostics::EventKind::DmaTransfer, 0xFE00 + dma_.currentByte, gpu_.oam[dma_.currentByte],
                           dma_.startAddress + dma_.currentByte, diagnostics::Initiator::DMA);
    ++dma_.currentByte;
    if (dma_.currentByte == DMA::TOTAL_BYTES) {
        dma_.transferComplete = true;
    }
}

bool Bus::hdma_hblank_active() const {
    return gpu_.hdma.hdmaActive && gpu_.hdma.hdmaMode == HDMAMode::HDMA && gpu_.hblank && !gpu_.vblank &&
           gpu_.scanlineCounter > gpu_.mode3EndDot_ + 1u && gpu_.scanlineCounter < (gpu_.doubleSpeed ? 454u : 456u);
}

void Bus::set_cpu_halted(const bool value, const bool preserve_dma_request, const bool immediate_wake) const {
    auto &hdma = gpu_.hdma;
    if (gpu_.cpu_halted == value)
        return;
    const bool hblank_active = value || immediate_wake ? hdma_hblank_active() : hdma.hdmaActive && hdma.wake_hblank_sample;
    gpu_.cpu_halted = value;
    dma_.halt_step_pending = false;
    if (value) {
        hdma.halt_hblank_active = hblank_active;
        hdma.halt_request_pending = hdma.block_requested && !preserve_dma_request;
        hdma.block_requested &= preserve_dma_request;
    } else {
        if (hdma.halt_request_pending || (hblank_active && !hdma.halt_hblank_active))
            hdma.block_requested = true;
        else if (hdma.halt_hblank_active && hdma.hdmaActive && gpu_.hblank && !gpu_.vblank)
            hdma.hblankBlockFinished = true;
        hdma.halt_hblank_active = false;
    }
}

void Bus::RunHDMAWork(const bool cpuAtInstructionBoundary) const {
    if (gpu_.hdma.release_delay) {
        if (--gpu_.hdma.release_delay == 0)
            gpu_.hdma.transferringBlock = false;
        return;
    }
    const uint16_t destination = VRAM_BEGIN | (gpu_.hdma.hdmaDestination & 0x1FFF);
    switch (gpu_.hdma.hdmaMode) {
        case HDMAMode::GDMA: {
            if (!gpu_.hdma.transferringBlock) {
                if (!cpuAtInstructionBoundary)
                    return;
                gpu_.hdma.transferringBlock = true;
            }
            if (gpu_.hdma.step == HDMAStep::Read) {
                gpu_.hdma.byte = ReadHDMASource(gpu_.hdma.hdmaSource);
                gpu_.hdma.step = HDMAStep::Write;
            } else {
                gpu_.WriteVRAM(destination, gpu_.hdma.byte);
                if (observations)
                    observations->Emit(diagnostics::Component::DMA, diagnostics::EventKind::DmaTransfer, destination, gpu_.hdma.byte,
                                       gpu_.hdma.hdmaSource, diagnostics::Initiator::HDMA, false, gpu_.vramBank);
                gpu_.hdma.bytesThisBlock++;
                gpu_.hdma.step = HDMAStep::Read;
                gpu_.hdma.hdmaSource++;
                gpu_.hdma.hdmaDestination++;
                if (gpu_.hdma.bytesThisBlock == 0x10) {
                    gpu_.hdma.bytesThisBlock = 0;
                    gpu_.hdma.hdmaRemain -= 1;
                    gpu_.hdma.hdma5 = gpu_.hdma.hdmaRemain > 0 ? (gpu_.hdma.hdmaRemain - 1) : 0xFF;
                    if (gpu_.hdma.hdmaDestination == 0) {
                        gpu_.hdma.hdmaActive = false;
                        gpu_.hdma.hdma5 |= 0x80;
                    }
                }
            }
            if (gpu_.hdma.hdmaRemain == 0 || !gpu_.hdma.hdmaActive) {
                gpu_.hdma.hdmaActive = false;
                gpu_.hdma.release_delay = gpu_.doubleSpeed ? 3 : 5;
            }
            return;
        }
        case HDMAMode::HDMA: {
            if (!gpu_.hdma.transferringBlock) {
                if (!gpu_.hdma.block_requested) {
                    if (gpu_.cpu_halted)
                        return;
                    if (gpu_.hdma.hdmaStartDelay > 0) {
                        --gpu_.hdma.hdmaStartDelay;
                        return;
                    }
                    if (!gpu_.hblank || gpu_.vblank || speedShiftActive)
                        return;
                    if (!gpu_.LCDDisabled()) {
                        if (gpu_.hdma.hblankBlockFinished || gpu_.scanlineCounter <= gpu_.mode3EndDot_ + 1u || gpu_.scanlineCounter >= 455)
                            return;
                    } else if (!gpu_.hdma.singleBlockTransfer) {
                        return;
                    }
                    gpu_.hdma.block_requested = true;
                }
                if (!cpuAtInstructionBoundary || speedShiftActive)
                    return;
                gpu_.hdma.block_requested = false;
                gpu_.hdma.transferringBlock = true;
            }

            if (gpu_.hdma.step == HDMAStep::Read) {
                gpu_.hdma.byte = ReadHDMASource(gpu_.hdma.hdmaSource);
                gpu_.hdma.step = HDMAStep::Write;
            } else {
                gpu_.WriteVRAM(destination, gpu_.hdma.byte);
                if (observations)
                    observations->Emit(diagnostics::Component::DMA, diagnostics::EventKind::DmaTransfer, destination, gpu_.hdma.byte,
                                       gpu_.hdma.hdmaSource, diagnostics::Initiator::HDMA, false, gpu_.vramBank);
                gpu_.hdma.bytesThisBlock++;
                gpu_.hdma.step = HDMAStep::Read;
                gpu_.hdma.hdmaSource++;
                gpu_.hdma.hdmaDestination++;
                if (gpu_.hdma.bytesThisBlock == 0x10) {
                    gpu_.hdma.singleBlockTransfer = false;
                    gpu_.hdma.hblankBlockFinished = true;
                    gpu_.hdma.release_delay = gpu_.hdma.halt_request_pending ? 1 : (gpu_.doubleSpeed ? 3 : 5);
                    gpu_.hdma.halt_request_pending = false;
                    gpu_.hdma.bytesThisBlock = 0;
                    if (gpu_.cpu_halted) {
                        gpu_.hdma.hdmaActive = false;
                        gpu_.hdma.hdma5 |= 0x80;
                    } else {
                        gpu_.hdma.hdmaRemain -= 1;
                        gpu_.hdma.hdma5 = gpu_.hdma.hdmaRemain > 0 ? (gpu_.hdma.hdmaRemain - 1) | (gpu_.hdma.hdmaActive ? 0 : 0x80) : 0xFF;
                    }
                    if (gpu_.hdma.hdmaDestination == 0) {
                        gpu_.hdma.hdmaActive = false;
                        gpu_.hdma.hdma5 |= 0x80;
                    }
                }
            }
            if (gpu_.hdma.hdmaRemain == 0x00)
                gpu_.hdma.hdmaActive = false;
            return;
        }
        default:
            return;
    }
}

void Bus::ChangeSpeed() {
    if (prepareSpeedShift) {
        speed = speed == Speed::Regular ? Speed::Double : Speed::Regular;
        prepareSpeedShift = false;
        gpu_.doubleSpeed = speed == Speed::Double;
        gpu_.InvalidateIdle();
        audio_.OnSpeedSwitch(gpu_.doubleSpeed);
        if (gpu_.doubleSpeed && !gpu_.LCDDisabled())
            gpu_.clockPause_ = 1;
    }
}

void Bus::HandleOAMCorruption(const uint16_t location, const CorruptionType type) const {
    if (IsCgb(gpu_.model) || location < 0xFE00 || location > 0xFEFF || gpu_.LCDDisabled())
        return;
    // The scan's OAM bus activity leads the visible mode-2 window by 4 dots: it
    // starts during the last 4 dots of the previous line (where the CPU-side
    // OAM read lock already asserts) and the row advances on that shifted grid
    // (blargg oam_bug scanline timing vs mooneye lcdon_timing-GS)
    int currentRowIndex;
    if (gpu_.stat.mode == GPUMode::MODE_2 && gpu_.scanlineCounter < 76) {
        currentRowIndex = static_cast<int>(gpu_.scanlineCounter + 4) / 4;
    } else if (gpu_.stat.mode == GPUMode::MODE_0 && gpu_.currentLine < 143 && gpu_.scanlineCounter >= 452) {
        currentRowIndex = 0;
    } else {
        return;
    }

    auto ReadWord = [&](const int index) -> uint16_t { return static_cast<uint16_t>(gpu_.oam[index]) << 8 | gpu_.oam[index + 1]; };
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
        if (currentRowIndex == 0)
            return;

        const int currentRowAddr = currentRowIndex * 8;
        const int prevRowAddr = (currentRowIndex - 1) * 8;

        const uint16_t a = ReadWord(currentRowAddr);
        const uint16_t b = ReadWord(prevRowAddr);
        const uint16_t c = ReadWord(prevRowAddr + 4);

        const uint16_t corruptedWord = (type == CorruptionType::Write) ? ((a ^ c) & (b ^ c)) ^ c : (b | (a & c));
        WriteWord(currentRowAddr, corruptedWord);
        std::memcpy(&gpu_.oam[currentRowAddr + 2], &gpu_.oam[prevRowAddr + 2], 6);
    }
}
