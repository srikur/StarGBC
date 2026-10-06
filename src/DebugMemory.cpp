#include "Bus.h"

#include <stdexcept>

std::optional<size_t> Cartridge::DebugRamOffset(const uint16_t address) const {
    if (address < 0xA000 || address > 0xBFFF)
        throw std::invalid_argument("address is not cartridge RAM");
    if (mbc == MBC::MBC3 && ramBank >= 0x08 && ramBank <= 0x0C)
        throw std::invalid_argument("RTC register windows are not supported by debug memory");
    if (!gameRamSize || (mbc != MBC::None && !ramEnabled))
        return std::nullopt;
    size_t bank = 0;
    switch (mbc) {
        case MBC::MBC1:
            bank = HandleRamBank();
            break;
        case MBC::MBC2:
            return (address - 0xA000) % gameRamSize;
        case MBC::MBC3:
            if (ramBank > 3)
                return std::nullopt;
            bank = ramBank;
            break;
        case MBC::MBC5:
            bank = ramBank;
            break;
        case MBC::None:
            break;
    }
    const size_t offset = bank * 0x2000 + address - 0xA000;
    return offset < gameRamSize ? std::optional<size_t>(offset) : std::nullopt;
}

uint8_t Cartridge::DebugPeek(const uint16_t address) const {
    if (address >= 0xA000 && address <= 0xBFFF) {
        const auto offset = DebugRamOffset(address);
        if (!offset)
            return 0xFF;
        return mbc == MBC::MBC2 ? 0xF0 | (gameRam_[*offset] & 0x0F) : gameRam_[*offset];
    }
    if (address >= 0x8000)
        throw std::invalid_argument("unsupported cartridge address");
    size_t bank = address < 0x4000 ? 0 : 1;
    switch (mbc) {
        case MBC::None:
            bank = address / 0x4000;
            break;
        case MBC::MBC1:
            bank = HandleRomBank(address);
            break;
        case MBC::MBC2:
            if (address >= 0x4000)
                bank = bank1 & 0x0F & BankBitmask();
            break;
        case MBC::MBC3:
        case MBC::MBC5:
            if (address >= 0x4000)
                bank = romBank & BankBitmask();
            break;
    }
    const size_t offset = bank * 0x4000 + (address & 0x3FFF);
    return offset < gameRom_.size() ? gameRom_[offset] : 0xFF;
}

void Cartridge::DebugPoke(const uint16_t address, const uint8_t value) {
    const auto offset = DebugRamOffset(address);
    if (!offset)
        throw std::invalid_argument("cartridge RAM is absent, disabled, or unmapped");
    gameRam_[*offset] = mbc == MBC::MBC2 ? value & 0x0F : value;
    MarkRamDirty();
}

uint8_t Bus::DebugPeek(uint16_t address) const {
    if (address < 0x8000) {
        if (bootromRunning && address < bootrom.size() && (address < 0x100 || (IsCgb(gpu_.model) && address >= 0x200)))
            return bootrom[address];
        return cartridge_.DebugPeek(address);
    }
    if (address < 0xA000)
        return gpu_.vram[gpu_.vramBank * 0x2000 + address - 0x8000];
    if (address < 0xC000)
        return cartridge_.DebugPeek(address);
    if (address < 0xFE00) {
        if (address >= 0xE000)
            address -= 0x2000;
        return memory_.wram_[address < 0xD000 ? address - 0xC000 : memory_.wramBank_ * 0x1000 + address - 0xD000];
    }
    if (address < 0xFEA0)
        return gpu_.oam[address - 0xFE00];
    if (address >= 0xFF80 && address < 0xFFFF)
        return memory_.hram_[address - 0xFF80];
    throw std::invalid_argument("debug memory does not support hardware registers or unusable memory");
}

void Bus::DebugPoke(uint16_t address, const uint8_t value) const {
    if (address >= 0x8000 && address < 0xA000) {
        gpu_.vram[gpu_.vramBank * 0x2000 + address - 0x8000] = value;
        gpu_.InvalidateIdle();
    } else if (address >= 0xA000 && address < 0xC000) {
        cartridge_.DebugPoke(address, value);
    } else if (address >= 0xC000 && address < 0xFE00) {
        if (address >= 0xE000)
            address -= 0x2000;
        memory_.wram_[address < 0xD000 ? address - 0xC000 : memory_.wramBank_ * 0x1000 + address - 0xD000] = value;
    } else if (address >= 0xFE00 && address < 0xFEA0) {
        gpu_.oam[address - 0xFE00] = value;
        gpu_.InvalidateIdle();
    } else if (address >= 0xFF80 && address < 0xFFFF) {
        memory_.hram_[address - 0xFF80] = value;
    } else {
        throw std::invalid_argument("debug writes require mapped RAM");
    }
}
