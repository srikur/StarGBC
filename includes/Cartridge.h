#pragma once
#include <array>
#include <functional>
#include "RealTimeClock.h"

class Cartridge {
public:
    // Largest RAM any supported mapper addresses (MBC5: 16 banks x 8 KiB)
    static constexpr uint32_t MAX_RAM_SIZE = 0x400 * 128;

    explicit Cartridge(const std::string &romLocation, RealTimeClock &rtc) : rtc_(rtc) {
        rtc_.RecalculateZeroTime();
        ReadFile(romLocation);
        romBankCount = gameRom_.size() / 0x4000;
        lowRomMask = std::bit_width(romBankCount) - 1;
        savepath_ = RemoveExtension(romLocation).append(".sav");
        DetermineMBC();
        ramBankCount = gameRamSize / 0x2000;
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

    void LoadRam(uint32_t size);

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

    [[=NotStateAware]] std::string savepath_;
    [[=NotStateAware]] std::vector<uint8_t> gameRom_;
    // Battery RAM at the largest supported size; gameRamSize is how much the
    // loaded cart actually maps
    std::array<uint8_t, MAX_RAM_SIZE> gameRam_{};

    // Derived from the ROM header/contents in the constructor, never mutated
    [[=NotStateAware]] MBC mbc{MBC::None};
    [[=NotStateAware]] uint32_t gameRamSize{0x00};
    [[=NotStateAware]] uint32_t romBankCount{0x00};
    [[=NotStateAware]] uint32_t ramBankCount{0x00};
    uint8_t romBank{0x01};
    uint8_t ramBank{0x00};
    uint8_t bank1{0x01};
    uint8_t bank2{0x00};
    [[=NotStateAware]] uint8_t lowRomMask{0x00}; // derived from ROM size
    uint8_t mode{0x00};

    bool ramEnabled{false};
    [[=NotStateAware]] bool multicart{false}; // derived from ROM contents
    [[=NotStateAware]] bool ramDirty_{false}; // host .sav flush bookkeeping
    bool prevRamEnable_{false};
    [[=NotStateAware]] bool hasRumble_{false}; // derived from ROM header
    bool rumbleOn_{false};
    [[=NotStateAware]] std::function<void(bool)> rumbleCallback_; // host callback
};
