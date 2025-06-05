#include "Cartridge.h"
#include "Common.h"

#include <algorithm>
#include <fstream>

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
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

Cartridge::Cartridge(const std::string &romLocation) {
    rtc = std::make_unique<RealTimeClock>();
    ReadFile(romLocation);
    savepath_ = RemoveExtension(romLocation).append(".sav");
    DetermineMBC();
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

uint64_t Cartridge::GetRomSize(const uint8_t byte) {
    constexpr uint16_t bank = 0x4000;
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

uint64_t Cartridge::HandleRomBank() const {
    return bankMode == Mode::Rom ? bank & 0x7F : bank & 0x1F;
}

uint64_t Cartridge::HandleRamBank() const {
    return bankMode == Mode::Rom ? 0x00 : (bank & 0x60) >> 5;
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
        case 0x0000 ... 0x3FFF:
            return gameRom_[address];
        case 0x4000 ... 0x7FFF:
            return gameRom_[HandleRomBank() * 0x4000 + address - 0x4000];
        case 0xA000 ... 0xBFFF:
            return ramEnabled ? gameRam_[HandleRamBank() * 0x2000 + address - 0xA000] : 0x00;
        default:
            return 0xFF;
    }
}

uint8_t Cartridge::ReadByteMBC2(const uint16_t address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return gameRom_[address];
        case 0x4000 ... 0x7FFF:
            return gameRom_[static_cast<uint64_t>(romBank) * 0x4000 + address - 0x4000];
        case 0xA000 ... 0xA1FF:
            return ramEnabled ? gameRam_[address - 0xA000] : 0x00;
        default:
            return 0xFF;
    }
}

uint8_t Cartridge::ReadByteMBC3(const uint16_t address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return gameRom_[address];
        case 0x4000 ... 0x7FFF:
            return gameRom_[static_cast<uint64_t>(romBank) * 0x4000 + address - 0x4000];
        case 0xA000 ... 0xBFFF: {
            if (ramEnabled) {
                if (ramBank <= 0x03) {
                    return gameRam_[static_cast<uint64_t>(ramBank) * 0x2000 + address - 0xA000];
                }
                return rtc->ReadRTC(ramBank);
            }
            return 0x00;
        }
        default:
            return 0xFF;
    }
}

uint8_t Cartridge::ReadByteMBC5(const uint16_t address) const {
    switch (address) {
        case 0x0000 ... 0x3FFF:
            return gameRom_[address];
        case 0x4000 ... 0x7FFF: {
            return gameRom_[static_cast<uint64_t>(romBank) * 0x4000 + address - 0x4000];
        }
        case 0xA000 ... 0xBFFF: {
            if (ramEnabled) {
                return gameRam_[static_cast<uint64_t>(ramBank) * 0x2000 + address - 0xA000];
            }
            return 0x00;
        }
        default:
            return 0x00;
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
            uint8_t val = value & 0x1F;
            if (val == 0x00) { val = 0x01; }
            bank = (bank & 0x60) | val;
        }
        break;
        case 0x4000 ... 0x5FFF:
            bank = (bank & 0x9F) | ((value & 0x03) << 5);
            break;
        case 0x6000 ... 0x7FFF:
            switch (value) {
                case 0x00: bankMode = Mode::Rom;
                    break;
                case 0x01: bankMode = Mode::Ram;
                    break;
                default:
                    throw UnreachableCodeException("Invalid bank mode");
            }
            break;
        case 0xA000 ... 0xBFFF:
            if (ramEnabled && (gameRamSize != 0)) {
                gameRam_[HandleRamBank() * 0x2000 + address - 0xA000] = value;
                ramDirty_ = true;
            }
            break;
        default:
            break;
    }
}

void Cartridge::WriteByteMBC2(const uint16_t address, uint8_t value) {
    value &= 0xF;
    switch (address) {
        case 0x0000 ... 0x1FFF:
            if ((address & 0x100) == 0x00) {
                const bool newEnable = value == 0x0A;
                HandleRamEnableEdge(newEnable);
                ramEnabled = newEnable;
            }
            break;
        case 0x2000 ... 0x3FFF:
            if ((address & 0x100) != 0x00) romBank = value;
            break;
        case 0xA000 ... 0xA1FF:
            if (ramEnabled) {
                gameRam_[address - 0xA000] = value;
                ramDirty_ = true;
            }
            break;
        default:
            break;
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
            romBank = value;
            break;
        case 0x4000 ... 0x5FFF:
            ramBank = value & 0x0F;
            break;
        case 0x6000 ... 0x7FFF:
            if ((value & 0x01) != 0) {
                rtc->Tick();
            }
            break;
        case 0xA000 ... 0xBFFF:
            if (ramEnabled) {
                ramDirty_ = true;
                if (ramBank <= 0x03) {
                    gameRam_[static_cast<uint64_t>(ramBank) * 0x2000 + address - 0xA000] = value;
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
            romBank = romBank & 0x0FF | (value & 0x01) << 8;
            break;
        case 0x4000 ... 0x5FFF:
            ramBank = value & 0x0F;
            break;
        case 0xA000 ... 0xBFFF:
            if (ramEnabled) {
                gameRam_[ramBank * 0x2000 + address - 0xA000] = value;
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
