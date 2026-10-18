#include "Cartridge.h"
#include "Common.h"

#include <algorithm>
#include <cstring>
#include <span>

Cartridge::Cartridge(RealTimeClock &clock, std::vector<u8> rom) : rtc(clock), game_rom(std::move(rom)) {
    if (game_rom.size() < 0x8000 || game_rom.size() % ROM_BANK_SIZE != 0)
        throw std::invalid_argument("ROM must contain at least two complete 16 KiB banks");
    rtc.recalculate_zero_time();
    rom_bank_count = game_rom.size() / ROM_BANK_SIZE;
    low_rom_mask = std::bit_width(rom_bank_count) - 1;
    DetermineMBC();
    ram_bank_count = game_ram_size / 0x2000;
    multicart = IsLikelyMulticart();
}

bool Cartridge::HasRtc() const { return game_rom[0x147] == 0x0F || game_rom[0x147] == 0x10; }

void Cartridge::LoadBattery(const std::span<const u8> bytes) {
    if (!has_battery || bytes.empty())
        return;
    if (bytes.size() > game_ram_size && (!HasRtc() || bytes.size() < game_ram_size + RealTimeClock::kSaveFooterMinSize ||
                                         bytes.size() > game_ram_size + RealTimeClock::kSaveFooterSize))
        throw std::invalid_argument("Invalid cartridge battery save size");
    std::ranges::fill(game_ram, 0);
    std::copy_n(bytes.begin(), std::min(bytes.size(), static_cast<size_t>(game_ram_size)), game_ram.begin());
    if (HasRtc() && bytes.size() >= game_ram_size + RealTimeClock::kSaveFooterMinSize)
        rtc.LoadSaveFooter(bytes.subspan(game_ram_size));
    AcknowledgeBatterySave();
}

std::vector<u8> Cartridge::SaveBattery() const {
    if (!has_battery)
        return {};
    std::vector<u8> bytes(game_ram.begin(), game_ram.begin() + game_ram_size);
    if (HasRtc()) {
        const auto footer = rtc.SaveFooter();
        bytes.insert(bytes.end(), footer.begin(), footer.end());
    }
    return bytes;
}

void Cartridge::DetermineMBC() {
    auto provisionRam = [&](const u32 size, const bool battery) {
        game_ram_size = size;
        has_battery = battery;
    };

    mbc = [&]() -> MBC {
        using enum MBC;
        switch (game_rom[0x147]) {
            case 0x00:
                return None;

            /* MBC1 */
            case 0x01:
                return MBC1;
            case 0x02: {
                provisionRam(GetRamSize(game_rom[0x149]), false);
                return MBC1;
            }
            case 0x03: {
                provisionRam(GetRamSize(game_rom[0x149]), true);
                return MBC1;
            }

            /* MBC2 */
            case 0x05: {
                provisionRam(0x200, false);
                return MBC2;
            }
            case 0x06: {
                provisionRam(0x200, true);
                return MBC2;
            }

            /* MBC3 */
            case 0x0F: // +Timer +Battery (no RAM)
                has_battery = true;
                return MBC3;
            case 0x11: // plain MBC3
                return MBC3;
            case 0x10: // +Timer +RAM +Battery
            case 0x13: // +RAM +Battery
                provisionRam(GetRamSize(game_rom[0x149]), true);
                return MBC3;
            case 0x12: // +RAM
                provisionRam(GetRamSize(game_rom[0x149]), false);
                return MBC3;

            /* MBC5 */
            case 0x19:
                return MBC5;
            case 0x1A: // +RAM
                provisionRam(GetRamSize(game_rom[0x149]), false);
                return MBC5;
            case 0x1B: // +RAM +Battery
                provisionRam(GetRamSize(game_rom[0x149]), true);
                return MBC5;
            case 0x1C: // +Rumble
                has_rumble = true;
                return MBC5;
            case 0x1D: // +Rumble +RAM
                has_rumble = true;
                provisionRam(GetRamSize(game_rom[0x149]), false);
                return MBC5;
            case 0x1E: // +Rumble +RAM +Battery
                has_rumble = true;
                provisionRam(GetRamSize(game_rom[0x149]), true);
                return MBC5;
            default:
                throw FatalErrorException("Unsupported MBC: " + std::to_string(game_rom[0x147]));
        }
    }();

}

bool Cartridge::IsLikelyMulticart() const {
    if (mbc != MBC::MBC1 || game_rom.size() != 0x1'00'000) {
        return false;
    }

    static constexpr std::array<u8, 48> logo = {
            0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
            0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
            0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
    };

    constexpr std::size_t bankSize = 0x4000;
    constexpr std::size_t bankLogoOfs = 0x0104;
    constexpr std::size_t bank10 = 0x10;
    const u8 *bank10Ptr = game_rom.data() + bank10 * bankSize;

    const bool foundLogo =
            std::equal(logo.begin(), logo.end(), bank10Ptr + bankLogoOfs) || !std::ranges::search(std::span(bank10Ptr, bankSize), logo).empty();

    if (foundLogo) {
        return true;
    }

    constexpr std::size_t blockSize = 0x10 * bankSize; // 16 banks = 256 KiB

    const bool dup1 = std::equal(game_rom.begin() + blockSize, // $10–$1F
                                 game_rom.begin() + 2 * blockSize,
                                 game_rom.begin()); // $00–$0F

    const bool dup2 = std::equal(game_rom.begin() + 3 * blockSize, // $30–$3F
                                 game_rom.end(), // end of ROM
                                 game_rom.begin() + 2 * blockSize); // $20–$2F

    if (dup1 && dup2) {
        return true;
    }

    return false;
}

u8 Cartridge::BankBitmask() const { return low_rom_mask >= 8 ? 0xFF : static_cast<u8>((1 << low_rom_mask) - 1); }

u32 Cartridge::GetRamSize(const u8 byte) {
    switch (byte) {
        case 0x00:
            return 0;
        case 0x01:
            return 0x400 * 2;
        case 0x02:
            return 0x400 * 8;
        case 0x03:
            return 0x400 * 32;
        case 0x04:
            return 0x400 * 128;
        case 0x05:
            return 0x400 * 64;
        default:
            throw FatalErrorException("Unsupported RAM size: " + std::to_string(byte));
    }
}

u32 Cartridge::HandleRomBank(const u16 address) const {
    if (multicart) {
        return (address < 0x4000) ? (mode == 0 ? 0 : ((bank2 << 4) & BankBitmask())) : (((bank2 << 4) | (bank1 & 0xF)) & BankBitmask());
    }

    if (address < 0x4000) {
        u32 bank = (mode == 0) ? 0 : ((bank2 << 5) & BankBitmask());
        if (bank >= rom_bank_count)
            bank = 1;
        return bank;
    }

    u64 bank = ((bank2 << 5) | bank1) & BankBitmask();
    if (bank >= rom_bank_count)
        bank = 1;
    return bank;
}

u32 Cartridge::HandleRamBank() const {
    if (mode == 0 || ram_bank_count <= 1)
        return 0;
    return bank2 & 0x03;
}

u8 Cartridge::ReadByte(const u16 address) const {
    switch (mbc) {
        case MBC::None:
            return ReadByteNone(address);
        case MBC::MBC1:
            return ReadByteMBC1(address);
        case MBC::MBC2:
            return ReadByteMBC2(address);
        case MBC::MBC3:
            return ReadByteMBC3(address);
        case MBC::MBC5:
            return ReadByteMBC5(address);
        default:
            throw UnreachableCodeException("Unsupported MBC");
    }
}

u8 Cartridge::ReadByteNone(const u16 address) const { return address < 0x8000 && address < game_rom.size() ? game_rom[address] : 0xFF; }

u8 Cartridge::ReadByteMBC1(const u16 address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return game_rom[((HandleRomBank(address) * 0x4000) + address) % game_rom.size()];
        case 0x4000 ... 0x7FFF:
            return game_rom[(HandleRomBank(address) * 0x4000) | (address & 0x3FFF)];
        case 0xA000 ... 0xBFFF: {
            if (!ram_enabled || game_ram_size == 0)
                return 0xFF;
            const size_t bank = HandleRamBank();
            const size_t offset = (address - 0xA000) + bank * 0x2000;
            if (offset >= game_ram_size)
                return 0xFF;
            return game_ram[offset];
        }
        default:
            return 0xFF;
    }
}

u8 Cartridge::ReadByteMBC2(const u16 address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return game_rom[address];
        case 0x4000 ... 0x7FFF:
            return game_rom[(bank1 & 0xF & BankBitmask()) * 0x4000ULL | (address & 0x3FFF)];
        case 0xA000 ... 0xBFFF: {
            if (!ram_enabled || game_ram_size == 0)
                return 0xFF;
            const u8 lower = game_ram[(address - 0xA000) % game_ram_size] & 0x0F;
            return 0xF0 | lower;
        }
        default:
            return 0xFF;
    }
}

u8 Cartridge::ReadByteMBC3(const u16 address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return game_rom[address];
        case 0x4000 ... 0x7FFF:
            return game_rom[static_cast<u64>(rom_bank & BankBitmask()) * 0x4000ULL + (address - 0x4000)];
        case 0xA000 ... 0xBFFF: {
            if (!ram_enabled)
                return 0xFF;
            if (ram_bank <= 0x03) {
                if (!game_ram_size)
                    return 0xFF;
                const u64 ramAddr = static_cast<u64>(ram_bank) * 0x2000ULL + (address - 0xA000);
                return game_ram[ramAddr & (game_ram_size - 1)];
            }
            return HasRtc() ? rtc.read_rtc(ram_bank) : 0xFF;
        }
        default:
            return 0xFF;
    }
}

u8 Cartridge::ReadByteMBC5(const u16 address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return game_rom[address];
        case 0x4000 ... 0x7FFF:
            return game_rom[static_cast<u64>(rom_bank & BankBitmask()) * 0x4000ULL + (address - 0x4000)];
        case 0xA000 ... 0xBFFF: {
            if (!ram_enabled || game_ram_size == 0)
                return 0xFF;
            // magentests "OOB" RAM access — supposedly should use the mapped address bits?
            const u64 ramAddr = static_cast<u64>(ram_bank) * 0x2000ULL + (address - 0xA000);
            return game_ram[ramAddr & (game_ram_size - 1)];
        }
        default:
            return 0xFF;
    }
}

void Cartridge::WriteByte(const u16 address, const u8 value) {
    switch (mbc) {
        case MBC::None:
            break;
        case MBC::MBC1:
            WriteByteMBC1(address, value);
            break;
        case MBC::MBC2:
            WriteByteMBC2(address, value);
            break;
        case MBC::MBC3:
            WriteByteMBC3(address, value);
            break;
        case MBC::MBC5:
            WriteByteMBC5(address, value);
            break;
    }
}

void Cartridge::WriteByteMBC1(const u16 address, const u8 value) {
    switch (address) {
        case 0x0000 ... 0x1FFF: {
            const bool newEnable = (value & 0x0F) == 0x0A;
            HandleRamEnableEdge(newEnable);
            ram_enabled = newEnable;
        }
        break;
        case 0x2000 ... 0x3FFF: {
            bank1 = value & 0x1F;
            if (bank1 == 0)
                bank1 = 1;
        }
        break;
        case 0x4000 ... 0x5FFF:
            bank2 = value & 0x03;
            break;
        case 0x6000 ... 0x7FFF:
            mode = value & 0x01;
            break;
        case 0xA000 ... 0xBFFF:
            if (ram_enabled && game_ram_size > 0) {
                game_ram[(HandleRamBank() * 0x2000ULL + (address - 0xA000))] = value;
                ram_dirty = true;
            }
            break;
        default:
            break;
    }
}

void Cartridge::WriteByteMBC2(const u16 address, const u8 value) {
    switch (address) {
        case 0x0000 ... 0x3FFF: {
            if ((address & 0x100) == 0x00) {
                const bool newEnable = (value & 0xF) == 0x0A;
                HandleRamEnableEdge(newEnable);
                ram_enabled = newEnable;
            } else {
                bank1 = value & 0x0F; // romb analogous to bank1
                if (bank1 == 0)
                    bank1 = 1;
            }
            break;
        }
        case 0xA000 ... 0xBFFF: {
            if (ram_enabled && game_ram_size > 0) {
                game_ram[(address - 0xA000) % game_ram_size] = value & 0xF;
                ram_dirty = true;
            }
        }
        break;
        default:
            break;
    }
}

void Cartridge::WriteByteMBC3(const u16 address, const u8 value) {
    switch (address) {
        case 0x0000 ... 0x1FFF: {
            const bool newEnable = (value & 0x0F) == 0x0A;
            HandleRamEnableEdge(newEnable);
            ram_enabled = newEnable;
        }
        break;
        case 0x2000 ... 0x3FFF:
            rom_bank = value ? value : 1;
            break;
        case 0x4000 ... 0x5FFF:
            ram_bank = value & 0x0F;
            break;
        case 0x6000 ... 0x7FFF:
            std::memcpy(&rtc.latchedClock, &rtc.realClock, sizeof(rtc.latchedClock));
            break;
        case 0xA000 ... 0xBFFF:
            if (ram_enabled) {
                ram_dirty = true;
                if (ram_bank <= 0x03 && game_ram_size > 0) {
                    const u64 ramAddr = static_cast<u64>(ram_bank) * 0x2000ULL + (address - 0xA000);
                    game_ram[ramAddr & (game_ram_size - 1)] = value;
                } else if (HasRtc()) {
                    rtc.write_rtc(ram_bank, value);
                }
            }
            break;
        default:
            break;
    }
}

void Cartridge::WriteByteMBC5(const u16 address, const u8 value) {
    switch (address) {
        case 0x0000 ... 0x1FFF: {
            const bool newEnable = (value & 0x0F) == 0x0A;
            HandleRamEnableEdge(newEnable);
            ram_enabled = newEnable;
        }
        break;
        case 0x2000 ... 0x2FFF:
            rom_bank = (rom_bank & 0x100) | value;
            break;
        case 0x3000 ... 0x3FFF:
            rom_bank = (rom_bank & 0xFF) | (value & 0x01) << 8;
            break;
        case 0x4000 ... 0x5FFF: {
            const bool rumbleRequest = (value & 0x10) != 0;
            ram_bank = value & 0x0F;
            if (has_rumble && rumbleRequest != rumble_on) {
                rumble_on = rumbleRequest;
                if (rumble_callback)
                    rumble_callback(rumble_on);
            }
        }
        break;
        case 0xA000 ... 0xBFFF:
            if (ram_enabled && game_ram_size != 0) {
                const u64 ramAddr = static_cast<u64>(ram_bank) * 0x2000ULL + (address - 0xA000);
                game_ram[ramAddr & (game_ram_size - 1)] = value;
                ram_dirty = true;
            }
            break;
        default:
            break;
    }
}

inline void Cartridge::HandleRamEnableEdge(const bool enable) {
    if (prev_ram_enable && !enable && ram_dirty) {
        battery_save_pending = has_battery;
    }
    prev_ram_enable = enable;
}
