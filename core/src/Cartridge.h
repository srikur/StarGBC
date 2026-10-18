#pragma once
#include <array>
#include <functional>
#include <optional>
#include <span>
#include "../include/RealTimeClock.h"

class Cartridge {
public:
    // Largest RAM any supported mapper addresses (MBC5: 16 banks x 8 KiB)
    static constexpr u32 MAX_RAM_SIZE = 0x400 * 128;

    explicit Cartridge(RealTimeClock &rtc, std::vector<u8> rom);

    static u32 GetRamSize(u8 byte);

    [[nodiscard]] std::vector<u8> SaveBattery() const;

    void LoadBattery(std::span<const u8> bytes);

    [[nodiscard]] bool BatterySavePending() const { return battery_save_pending; }

    void AcknowledgeBatterySave() { ram_dirty = battery_save_pending = false; }

    [[nodiscard]] u8 ReadByte(uint16_t address) const;

    void WriteByte(uint16_t address, u8 value);

    [[nodiscard]] u8 DebugPeek(uint16_t address) const;

    [[nodiscard]] uint16_t DebugBank(uint16_t address) const;

    void DebugPoke(uint16_t address, u8 value);

    [[nodiscard]] std::span<const u8> rom_bytes() const { return game_rom; }

    [[nodiscard]] std::span<const u8> ram_bytes() const { return std::span(game_ram).first(game_ram_size); }

    void restore_ram(std::span<const u8> bytes);

    void MarkRamDirty() {
        ram_dirty = true;
        battery_save_pending = has_battery;
    }

    [[nodiscard]] uint16_t GlobalChecksum() const { return static_cast<uint16_t>(game_rom[0x14E]) << 8 | game_rom[0x14F]; }

    [[nodiscard]] bool BankingStateValid() const { return ram_bank <= 0x0F; }

private:
    [[nodiscard]] std::optional<size_t> DebugRamOffset(uint16_t address) const;

    [[nodiscard]] bool HasRtc() const;

    void DetermineMBC();

    [[nodiscard]] u8 ReadByteNone(uint16_t address) const;

    [[nodiscard]] u8 ReadByteMBC1(uint16_t address) const;

    [[nodiscard]] u8 ReadByteMBC2(uint16_t address) const;

    [[nodiscard]] u8 ReadByteMBC3(uint16_t address) const;

    [[nodiscard]] u8 ReadByteMBC5(uint16_t address) const;

    void WriteByteMBC1(uint16_t address, u8 value);

    void WriteByteMBC2(uint16_t address, u8 value);

    void WriteByteMBC3(uint16_t address, u8 value);

    void WriteByteMBC5(uint16_t address, u8 value);

    [[nodiscard]] u32 HandleRomBank(uint16_t address) const;

    [[nodiscard]] u32 HandleRamBank() const;

    [[nodiscard]] bool IsLikelyMulticart() const;

    [[nodiscard]] u8 BankBitmask() const;

    inline void HandleRamEnableEdge(bool enable);

    enum class MBC { None, MBC1, MBC2, MBC3, MBC5 };

    RealTimeClock &rtc;

    [[= NotStateAware]] std::vector<u8> game_rom;
    std::array<u8, MAX_RAM_SIZE> game_ram{};

    // Derived from the ROM header/contents in the constructor, never mutated
    [[=NotStateAware]] MBC mbc{MBC::None};
    [[= NotStateAware]] u32 game_ram_size{0x00};
    [[= NotStateAware]] u32 rom_bank_count{0x00};
    [[= NotStateAware]] u32 ram_bank_count{0x00};
    u8 rom_bank{0x01};
    u8 ram_bank{0x00};
    u8 bank1{0x01};
    u8 bank2{0x00};
    [[= NotStateAware]] u8 low_rom_mask{0x00}; // derived from ROM size
    u8 mode{0x00};

    bool ram_enabled{false};
    [[=NotStateAware]] bool multicart{false}; // derived from ROM contents
    [[= NotStateAware]] bool has_battery{false};
    [[= NotStateAware]] bool battery_save_pending{false};
    [[= NotStateAware]] bool ram_dirty{false}; // host .sav flush bookkeeping
    bool prev_ram_enable{false};
    [[= NotStateAware]] bool has_rumble{false}; // derived from ROM header
    bool rumble_on{false};
    [[= NotStateAware]] std::function<void(bool)> rumble_callback; // host callback
};
