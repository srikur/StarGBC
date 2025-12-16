#pragma once
#include <functional>
#include "RealTimeClock.h"

class Cartridge {
public:
    explicit Cartridge(const std::string &romLocation, RealTimeClock &rtc) : rtc_(rtc) {
        rtc_.RecalculateZeroTime();
        ReadFile(romLocation);
        romBankCount = gameRom_.size() / 0x4000;
        lowRomMask = std::bit_width(romBankCount) - 1;
        savepath_ = RemoveExtension(romLocation).append(".sav");
        DetermineMBC();
        ramBankCount = gameRam_.size() / 0x2000;
        multicart = IsLikelyMulticart();
    }

    static uint32_t GetRamSize(uint8_t byte);

    void Save() const;

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    void WriteByte(uint16_t address, uint8_t value);

    bool SaveState(std::ofstream &stateFile) const;

    bool LoadState(std::ifstream &stateFile);

private:
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

    [[nodiscard]] uint8_t BankBitmask() const;

    inline void HandleRamEnableEdge(bool enable);

    static std::string RemoveExtension(const std::string &filename);

    enum class MBC {
        None, MBC1, MBC2, MBC3, MBC5
    };

    RealTimeClock& rtc_;

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
};
