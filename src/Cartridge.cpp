#include "Cartridge.h"
#include "Common.h"

#include <algorithm>
#include <fstream>
#include <span>

void Cartridge::ReadFile(const std::string &file) {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open file " + file);
    }
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(ifs), {});
    gameRom_ = std::move(buffer);
}

std::string Cartridge::RemoveExtension(const std::string &filename) {
    const size_t lastdot = filename.find_last_of('.');
    return lastdot == std::string::npos ? filename : filename.substr(0, lastdot);
}

Cartridge::Cartridge(const std::string &romLocation) {
    rtc = std::make_unique<RealTimeClock>();
    rtc->RecalculateZeroTime();
    ReadFile(romLocation);
    romBankCount = gameRom_.size() / 0x4000;
    lowRomMask = std::bit_width(romBankCount) - 1;
    savepath_ = RemoveExtension(romLocation).append(".sav");
    DetermineMBC();
    ramBankCount = gameRam_.size() / 0x2000;
    multicart = IsLikelyMulticart();
}

void Cartridge::LoadRam(const uint16_t size) {
    std::ifstream ifs(savepath_, std::ios::binary);
    if (!ifs.is_open()) {
        gameRam_.resize(size, 0);
        return;
    }
    rtc->Load(ifs);
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(ifs), {});
    gameRam_ = std::move(buffer);
    ifs.close();
}

void Cartridge::DetermineMBC() {
    auto provisionRam = [&](const uint16_t sz, const bool load) {
        gameRamSize = sz;
        if (load && gameRamSize) { LoadRam(sz); } else { gameRam_.assign(sz, 0); }
    };

    mbc = [&]() -> MBC {
        using enum MBC;
        switch (gameRom_[0x147]) {
            case 0x00: return None;

            /* MBC1 */
            case 0x01: return MBC1;
            case 0x02: {
                provisionRam(GetRamSize(gameRom_[0x149]), false);
                return MBC1;
            }
            case 0x03: {
                provisionRam(GetRamSize(gameRom_[0x149]), true);
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
            case 0x11: // plain MBC3
                return MBC3;
            case 0x10: // +Timer +RAM +Battery
            case 0x13: // +RAM +Battery
                provisionRam(GetRamSize(gameRom_[0x149]), true);
                return MBC3;
            case 0x12: // +RAM
                provisionRam(GetRamSize(gameRom_[0x149]), false);
                return MBC3;

            /* MBC5 */
            case 0x19: return MBC5;
            case 0x1A: // +RAM
                provisionRam(GetRamSize(gameRom_[0x149]), false);
                return MBC5;
            case 0x1B: // +RAM +Battery
                provisionRam(GetRamSize(gameRom_[0x149]), true);
                return MBC5;
            case 0x1C: // +Rumble
                hasRumble_ = true;
                return MBC5;
            case 0x1D: // +Rumble +RAM
                hasRumble_ = true;
                provisionRam(GetRamSize(gameRom_[0x149]), false);
                return MBC5;
            case 0x1E: // +Rumble +RAM +Battery
                hasRumble_ = true;
                provisionRam(GetRamSize(gameRom_[0x149]), true);
                return MBC5;
            default: throw FatalErrorException("Unsupported MBC: " + std::to_string(gameRom_[0x147]));
        }
    }();

    switch (gameRom_[0x147]) {
        case 0x00:
            mbc = MBC::None;
            break;
        case 0x01:
            mbc = MBC::MBC1;
            break;
        case 0x02: {
            mbc = MBC::MBC1;
            const uint16_t ramSize = GetRamSize(gameRom_[0x149]);
            gameRamSize = ramSize;
            gameRam_ = std::vector<uint8_t>(ramSize);
        }
        case 0x03: {
            mbc = MBC::MBC1;
            const uint16_t ramSize = GetRamSize(gameRom_[0x149]);
            gameRamSize = ramSize;
            gameRam_.reserve(ramSize);
            LoadRam(ramSize);
            break;
        }
        case 0x05:
            mbc = MBC::MBC2;
            gameRamSize = 0x200;
            gameRam_ = std::vector<uint8_t>(0x200);
            break;
        case 0x06:
            mbc = MBC::MBC2;
            gameRamSize = 0x200;
            LoadRam(0x200);
            break;
        case 0x0F:
        case 0x11:
            mbc = MBC::MBC3;
            break;
        case 0x10:
        case 0x13: {
            mbc = MBC::MBC3;
            const uint16_t ramSize = GetRamSize(gameRom_[0x149]);
            gameRamSize = ramSize;
            LoadRam(ramSize);
            break;
        }
        case 0x12: {
            mbc = MBC::MBC3;
            const uint16_t ramSize = GetRamSize(gameRom_[0x149]);
            gameRamSize = ramSize;
            gameRam_ = std::vector<uint8_t>(ramSize);
            break;
        }

        /* MCB5 versions */
        case 0x19:
            mbc = MBC::MBC5;
            break;
        case 0x1A: {
            mbc = MBC::MBC5;
            const uint16_t ramSize = GetRamSize(gameRom_[0x149]);
            gameRamSize = ramSize;
            gameRam_ = std::vector<uint8_t>(ramSize);
            break;
        }
        case 0x1B: {
            mbc = MBC::MBC5;
            const uint16_t ramSize = GetRamSize(gameRom_[0x149]);
            gameRamSize = ramSize;
            LoadRam(ramSize);
            break;
        }
        default:
            throw FatalErrorException("Unsupported MBC: " + std::to_string(gameRom_[0x147]));
    }
}

bool Cartridge::IsLikelyMulticart() const {
    if (mbc != MBC::MBC1 || gameRom_.size() != 0x1'00'000) { return false; }

    static constexpr std::array<uint8_t, 48> logo = {
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
    };

    constexpr std::size_t bankSize = 0x4000;
    constexpr std::size_t bankLogoOfs = 0x0104;
    constexpr std::size_t bank10 = 0x10;
    const uint8_t *bank10Ptr = gameRom_.data() + bank10 * bankSize;

    const bool foundLogo =
            std::equal(logo.begin(), logo.end(), bank10Ptr + bankLogoOfs) ||
            !std::ranges::search(std::span(bank10Ptr, bankSize), logo).empty();

    if (foundLogo) { return true; }

    constexpr std::size_t blockSize = 0x10 * bankSize; // 16 banks = 256 KiB

    const bool dup1 = std::equal(gameRom_.begin() + blockSize, // $10–$1F
                                 gameRom_.begin() + 2 * blockSize,
                                 gameRom_.begin()); // $00–$0F

    const bool dup2 = std::equal(gameRom_.begin() + 3 * blockSize, // $30–$3F
                                 gameRom_.end(), // end of ROM
                                 gameRom_.begin() + 2 * blockSize); // $20–$2F

    if (dup1 && dup2) { return true; }

    return false;
}


uint64_t Cartridge::GetRomSize(const uint8_t byte) {
    constexpr uint64_t bank = 0x4000;
    switch (byte) {
        case 0x00: return bank * 2;
        case 0x01: return bank * 4;
        case 0x02: return bank * 8;
        case 0x03: return bank * 16;
        case 0x04: return bank * 32;
        case 0x05: return bank * 64;
        case 0x06: return bank * 128;
        case 0x07: return bank * 256;
        case 0x08: return bank * 512;
        case 0x52: return bank * 72;
        case 0x53: return bank * 80;
        case 0x54: return bank * 96;
        default: throw FatalErrorException("Unsupported ROM size: " + std::to_string(byte));
    }
}

uint32_t Cartridge::GetRamSize(const uint8_t byte) {
    switch (byte) {
        case 0x00: return 0;
        case 0x01: return 0x400 * 2;
        case 0x02: return 0x400 * 8;
        case 0x03: return 0x400 * 32;
        case 0x04: return 0x400 * 128;
        case 0x05: return 0x400 * 64;
        default: throw FatalErrorException("Unsupported RAM size: " + std::to_string(byte));
    }
}

uint32_t Cartridge::HandleRomBank(const uint16_t address) const {
    if (multicart) {
        return (address < 0x4000)
                   ? (mode == 0 ? 0 : ((bank2 << 4) & BankBitmask()))
                   : (((bank2 << 4) | (bank1 & 0xF)) & BankBitmask());
    }

    if (address < 0x4000) {
        uint32_t bank = (mode == 0) ? 0 : ((bank2 << 5) & BankBitmask());
        if (bank >= romBankCount) bank = 1;
        return bank;
    }

    uint64_t bank = ((bank2 << 5) | bank1) & BankBitmask();
    if (bank >= romBankCount) bank = 1;
    return bank;
}

uint32_t Cartridge::HandleRamBank() const {
    if (mode == 0 || ramBankCount <= 1) return 0;
    return bank2 & 0x03;
}

void Cartridge::Save() const {
    if (mbc == MBC::None || gameRamSize == 0) return;

    std::ofstream file(savepath_, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) throw std::runtime_error("Could not open " + savepath_);

    rtc->Save(file);
    file.write(reinterpret_cast<const char *>(gameRam_.data()), gameRamSize);
}

uint8_t Cartridge::ReadByte(const uint16_t address) const {
    switch (mbc) {
        case MBC::None: return ReadByteNone(address);
        case MBC::MBC1: return ReadByteMBC1(address);
        case MBC::MBC2: return ReadByteMBC2(address);
        case MBC::MBC3: return ReadByteMBC3(address);
        case MBC::MBC5: return ReadByteMBC5(address);
        default: throw UnreachableCodeException("Unsupported MBC");
    }
}

uint8_t Cartridge::ReadByteNone(const uint16_t address) const {
    return gameRom_[address];
}

uint8_t Cartridge::ReadByteMBC1(const uint16_t address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF: return gameRom_[((HandleRomBank(address) * 0x4000) + address) % gameRom_.size()];
        case 0x4000 ... 0x7FFF: return gameRom_[(HandleRomBank(address) * 0x4000) | (address & 0x3FFF)];
        case 0xA000 ... 0xBFFF: {
            if (!ramEnabled || gameRamSize == 0) return 0xFF;
            const size_t bank = HandleRamBank();
            const size_t offset = (address - 0xA000) + bank * 0x2000;
            if (offset >= gameRamSize) return 0xFF;
            return gameRam_[offset];
        }
        default: return 0xFF;
    }
}

uint8_t Cartridge::ReadByteMBC2(const uint16_t address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return gameRom_[address];
        case 0x4000 ... 0x7FFF:
            return gameRom_[(bank1 & 0xF & BankBitmask()) * 0x4000ULL | (address & 0x3FFF)];
        case 0xA000 ... 0xBFFF: {
            if (!ramEnabled || gameRamSize == 0) return 0xFF;
            const uint8_t lower = gameRam_[(address - 0xA000) % gameRam_.size()] & 0x0F;
            return 0xF0 | lower;
        }
        default: return 0xFF;
    }
}

uint8_t Cartridge::ReadByteMBC3(const uint16_t address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return gameRom_[address];
        case 0x4000 ... 0x7FFF:
            return gameRom_[static_cast<uint64_t>(romBank & BankBitmask()) * 0x4000ULL + (address - 0x4000)];
        case 0xA000 ... 0xBFFF: {
            if (!ramEnabled) return 0xFF;
            if (ramBank <= 0x03 && gameRamSize > 0) {
                return gameRam_[static_cast<uint64_t>(ramBank) * 0x2000ULL + (address - 0xA000)];
            }
            return rtc->ReadRTC(ramBank);
        }
        default: return 0xFF;
    }
}

uint8_t Cartridge::ReadByteMBC5(const uint16_t address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF: return gameRom_[address];
        case 0x4000 ... 0x7FFF: return gameRom_[static_cast<uint64_t>(romBank & BankBitmask()) * 0x4000ULL + (address - 0x4000)];
        case 0xA000 ... 0xBFFF: return ramEnabled && gameRamSize > 0
                                           ? gameRam_[static_cast<uint64_t>(ramBank) * 0x2000ULL + (address - 0xA000)]
                                           : 0xFF;
        default: return 0xFF;
    }
}

void Cartridge::WriteByte(const uint16_t address, const uint8_t value) {
    switch (mbc) {
        case MBC::None: break;
        case MBC::MBC1: WriteByteMBC1(address, value);
            break;
        case MBC::MBC2: WriteByteMBC2(address, value);
            break;
        case MBC::MBC3: WriteByteMBC3(address, value);
            break;
        case MBC::MBC5: WriteByteMBC5(address, value);
            break;
    }
}

void Cartridge::WriteByteMBC1(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0x0000 ... 0x1FFF: {
            const bool newEnable = (value & 0x0F) == 0x0A;
            HandleRamEnableEdge(newEnable);
            ramEnabled = newEnable;
        }
        break;
        case 0x2000 ... 0x3FFF: {
            bank1 = value & 0x1F;
            if (bank1 == 0) bank1 = 1;
        }
        break;
        case 0x4000 ... 0x5FFF:
            bank2 = value & 0x03;
            break;
        case 0x6000 ... 0x7FFF:
            mode = value & 0x01;
            break;
        case 0xA000 ... 0xBFFF:
            if (ramEnabled && gameRamSize > 0) {
                gameRam_[(HandleRamBank() * 0x2000ULL + (address - 0xA000))] = value;
                ramDirty_ = true;
            }
            break;
        default:
            break;
    }
}

void Cartridge::WriteByteMBC2(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0x0000 ... 0x3FFF: {
            if ((address & 0x100) == 0x00) {
                const bool newEnable = (value & 0xF) == 0x0A;
                HandleRamEnableEdge(newEnable);
                ramEnabled = newEnable;
            } else {
                bank1 = value & 0x0F; // romb analogous to bank1
                if (bank1 == 0) bank1 = 1;
            }
            break;
        }
        case 0xA000 ... 0xBFFF: {
            if (ramEnabled && gameRamSize > 0) {
                gameRam_[(address - 0xA000) % gameRam_.size()] = value & 0xF;
                ramDirty_ = true;
            }
        }
        break;
        default: break;
    }
}

void Cartridge::WriteByteMBC3(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0x0000 ... 0x1FFF: {
            const bool newEnable = (value & 0x0F) == 0x0A;
            HandleRamEnableEdge(newEnable);
            ramEnabled = newEnable;
        }
        break;
        case 0x2000 ... 0x3FFF:
            romBank = value ? value : 1;
            break;
        case 0x4000 ... 0x5FFF:
            ramBank = value & 0x0F;
            break;
        case 0x6000 ... 0x7FFF:
            std::memcpy(&rtc->latchedClock, &rtc->realClock, sizeof(rtc->latchedClock));
            break;
        case 0xA000 ... 0xBFFF:
            if (ramEnabled) {
                ramDirty_ = true;
                if (ramBank <= 0x03 && gameRamSize > 0) {
                    gameRam_[static_cast<uint64_t>(ramBank) * 0x2000ULL + (address - 0xA000)] = value;
                } else {
                    rtc->WriteRTC(ramBank, value);
                }
            }
            break;
        default:
            break;
    }
}

void Cartridge::WriteByteMBC5(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0x0000 ... 0x1FFF: {
            const bool newEnable = (value & 0x0F) == 0x0A;
            HandleRamEnableEdge(newEnable);
            ramEnabled = newEnable;
        }
        break;
        case 0x2000 ... 0x2FFF:
            romBank = romBank & 0x100 | value;
            break;
        case 0x3000 ... 0x3FFF:
            romBank = romBank & 0xFF | (value & 0x01) << 8;
            break;
        case 0x4000 ... 0x5FFF: {
            const bool rumbleRequest = (value & 0x10) != 0;
            ramBank = value & 0x0F;
            if (hasRumble_ && rumbleRequest != rumbleOn_) {
                rumbleOn_ = rumbleRequest;
                if (rumbleCallback_) rumbleCallback_(rumbleOn_);
            }
        }
        break;
        case 0xA000 ... 0xBFFF:
            if (ramEnabled && gameRamSize != 0) {
                gameRam_[ramBank * 0x2000ULL + address - 0xA000] = value;
                ramDirty_ = true;
            }
            break;
        default: break;
    }
}

inline void Cartridge::HandleRamEnableEdge(const bool enable) {
    if (prevRamEnable_ && !enable && ramDirty_) {
        Save();
        ramDirty_ = false;
    }
    prevRamEnable_ = enable;
}
