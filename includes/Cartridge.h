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

    void writeByteMBC1(uint16_t address, uint8_t value);

    void writeByteMBC2(uint16_t address, uint8_t value);

    void writeByteMBC3(uint16_t address, uint8_t value);

    void writeByteMBC5(uint16_t address, uint8_t value);

    [[nodiscard]] uint64_t HandleRomBank() const;

    [[nodiscard]] uint64_t HandleRamBank() const;

    enum class Mode {
        Rom, Ram
    };

    enum class MBC {
        None, MBC1, MBC2, MBC3, MBC5
    };

    std::string savepath_;
    std::vector<uint8_t> gameRom_;
    std::vector<uint8_t> gameRam_;
    uint32_t gameRamSize = 0;
    bool ramEnabled = false;
    Mode bankMode = Mode::Rom;
    std::unique_ptr<RealTimeClock> rtc = nullptr;
    uint8_t romBank = 0x01;
    uint8_t ramBank = 0x00;
    uint8_t bank = 0x01;
    MBC mbc = MBC::None;

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

    void writeByte(uint16_t address, uint8_t value);

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(gameRam_.data()), gameRamSize);
            stateFile.write(reinterpret_cast<const char *>(&gameRamSize), sizeof(gameRamSize));
            stateFile.write(reinterpret_cast<const char *>(&ramEnabled), sizeof(ramEnabled));
            stateFile.write(reinterpret_cast<const char *>(&bankMode), sizeof(bankMode));
            stateFile.write(reinterpret_cast<const char *>(&romBank), sizeof(romBank));
            stateFile.write(reinterpret_cast<const char *>(&ramBank), sizeof(ramBank));
            stateFile.write(reinterpret_cast<const char *>(&bank), sizeof(bank));
            stateFile.write(reinterpret_cast<const char *>(&mbc), sizeof(mbc));

            rtc->Save();
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
            stateFile.read(reinterpret_cast<char *>(&bankMode), sizeof(bankMode));
            stateFile.read(reinterpret_cast<char *>(&romBank), sizeof(romBank));
            stateFile.read(reinterpret_cast<char *>(&ramBank), sizeof(ramBank));
            stateFile.read(reinterpret_cast<char *>(&bank), sizeof(bank));
            stateFile.read(reinterpret_cast<char *>(&mbc), sizeof(mbc));

            // rtc->LoadRTC();
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading state: " << e.what() << std::endl;
            return false;
        }
    }
};
