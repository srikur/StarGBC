#pragma once
#include <iostream>

#include "RealTimeClock.h"

class Cartridge {
    void ReadFile(const std::string &file);

    void LoadRam(uint16_t size);

    void DetermineMBC();

    [[nodiscard]] uint8_t ReadByteNone(uint16_t address) const;

    [[nodiscard]] uint8_t ReadByteMBC1(uint16_t address) const;

    [[nodiscard]] uint8_t ReadByteMBC2(uint16_t address) const;

    [[nodiscard]] uint8_t ReadByteMBC3(uint16_t address) const;

    [[nodiscard]] uint8_t ReadByteMBC5(uint16_t address) const;

    void WriteByteMBC1(uint16_t address, uint8_t value);

    void WriteByteMBC2(uint16_t address, uint8_t value);

    void WriteByteMBC3(uint16_t address, uint8_t value);

    void WriteByteMBC5(uint16_t address, uint8_t value);

    [[nodiscard]] uint32_t HandleRomBank(uint16_t address) const;

    [[nodiscard]] uint32_t HandleRamBank() const;

    [[nodiscard]] bool IsLikelyMulticart() const;

    [[nodiscard]] uint8_t BankBitmask() const {
        return (lowRomMask >= 8) ? 0xFF : static_cast<uint8_t>((1 << lowRomMask) - 1);
    }

    enum class MBC {
        None, MBC1, MBC2, MBC3, MBC5
    };

    std::string savepath_;
    std::vector<uint8_t> gameRom_;
    std::vector<uint8_t> gameRam_;

    MBC mbc{MBC::None};
    uint32_t gameRamSize{0x00};
    uint32_t romBankCount{0x00};
    uint32_t ramBankCount{0x00};
    uint8_t romBank{0x01};
    uint8_t ramBank{0x00};
    uint8_t bank1{0x01};
    uint8_t bank2{0x00};
    uint8_t lowRomMask{0x00};
    uint8_t mode{0x00};

    bool ramEnabled{false};
    bool multicart{false};
    bool ramDirty_{false};
    bool prevRamEnable_{false};
    bool hasRumble_{false};
    bool rumbleOn_{false};
    std::function<void(bool)> rumbleCallback_;

public:
    explicit Cartridge(const std::string &romLocation);

    Cartridge(const Cartridge &other) = delete;

    Cartridge &operator=(const Cartridge &other) = delete;

    Cartridge(Cartridge &&other) = delete;

    Cartridge &operator=(Cartridge &&other) = delete;

    ~Cartridge() = default;

    static uint64_t GetRomSize(uint8_t byte);

    static uint32_t GetRamSize(uint8_t byte);

    void Save() const;

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    static std::string RemoveExtension(const std::string &filename);

    void WriteByte(uint16_t address, uint8_t value);

    inline void HandleRamEnableEdge(bool enable);

    std::unique_ptr<RealTimeClock> rtc = nullptr;

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(gameRam_.data()), gameRamSize);
            stateFile.write(reinterpret_cast<const char *>(&gameRamSize), sizeof(gameRamSize));
            stateFile.write(reinterpret_cast<const char *>(&ramEnabled), sizeof(ramEnabled));
            stateFile.write(reinterpret_cast<const char *>(&mode), sizeof(mode));
            stateFile.write(reinterpret_cast<const char *>(&romBank), sizeof(romBank));
            stateFile.write(reinterpret_cast<const char *>(&ramBank), sizeof(ramBank));
            stateFile.write(reinterpret_cast<const char *>(&bank1), sizeof(bank1));
            stateFile.write(reinterpret_cast<const char *>(&bank2), sizeof(bank2));
            stateFile.write(reinterpret_cast<const char *>(&mbc), sizeof(mbc));
            stateFile.write(reinterpret_cast<const char *>(&ramDirty_), sizeof(ramDirty_));
            stateFile.write(reinterpret_cast<const char *>(&prevRamEnable_), sizeof(prevRamEnable_));
            rtc->Save(stateFile);
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error saving state: " << e.what() << std::endl;
            return false;
        }
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.read(reinterpret_cast<char *>(gameRam_.data()), gameRamSize);
            stateFile.read(reinterpret_cast<char *>(&gameRamSize), sizeof(gameRamSize));
            stateFile.read(reinterpret_cast<char *>(&ramEnabled), sizeof(ramEnabled));
            stateFile.read(reinterpret_cast<char *>(&mode), sizeof(mode));
            stateFile.read(reinterpret_cast<char *>(&romBank), sizeof(romBank));
            stateFile.read(reinterpret_cast<char *>(&ramBank), sizeof(ramBank));
            stateFile.read(reinterpret_cast<char *>(&bank1), sizeof(bank1));
            stateFile.read(reinterpret_cast<char *>(&mbc), sizeof(mbc));
            stateFile.read(reinterpret_cast<char *>(&ramDirty_), sizeof(ramDirty_));
            stateFile.read(reinterpret_cast<char *>(&prevRamEnable_), sizeof(prevRamEnable_));
            rtc->Load(stateFile);
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading state: " << e.what() << std::endl;
            return false;
        }
    }
};
