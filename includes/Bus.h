#pragma once
#include <string>

#include "Auxiliary.h"
#include "Cartridge.h"
#include "GPU.h"

/* Memory */
struct Memory {
    static constexpr uint16_t WRAM_BEGIN = 0xA000;
    static constexpr uint16_t WRAM_END = 0xBFFF;
    static constexpr uint16_t HRAM_BEGIN = 0xFF80;
    static constexpr uint16_t HRAM_END = 0xFFFE;

    std::array<uint8_t, 0x8000> wram_ = {};
    std::array<uint8_t, 0x80> hram_ = {};
    uint8_t wramBank_ = 0x01;

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(wram_.data()), 0x8000);
            stateFile.write(reinterpret_cast<const char *>(hram_.data()), 0x80);
            stateFile.write(reinterpret_cast<const char *>(&wramBank_), sizeof(wramBank_));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error saving Memory state: " << e.what() << std::endl;
            return false;
        }
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.read(reinterpret_cast<char *>(wram_.data()), 0x8000);
            stateFile.read(reinterpret_cast<char *>(hram_.data()), 0x80);
            stateFile.read(reinterpret_cast<char *>(&wramBank_), sizeof(wramBank_));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading Memory state: " << e.what() << std::endl;
            return false;
        }
    }
};

class Bus {
public:
    enum class Speed {
        Regular = 0x01,
        Double = 0x02
    };

    explicit Bus(const std::string &romLocation);

    [[nodiscard]] uint8_t ReadByte(uint16_t address) const;

    void WriteByte(uint16_t address, uint8_t value);

    void KeyDown(Keys key);

    void KeyUp(Keys key);

    void UpdateGraphics(uint32_t cycles);

    void UpdateTimers(uint32_t cycles);

    void WriteWord(uint16_t address, uint16_t value);

    [[nodiscard]] uint8_t ReadHDMA(uint16_t address) const;

    void WriteHDMA(uint16_t address, uint8_t value);

    void ChangeSpeed();

    bool SaveState(std::ofstream &stateFile) const;

    void LoadState(std::ifstream &stateFile);

    std::unique_ptr<Cartridge> cartridge_ = nullptr;
    std::unique_ptr<GPU> gpu_ = std::make_unique<GPU>();

    Joypad joypad_ = {};
    Memory memory_ = {};
    Timer timer_ = {};
    Serial serial_ = {};
    std::unique_ptr<Audio> audio_ = std::make_unique<Audio>();

    // Bootrom
    std::vector<uint8_t> bootrom;
    bool bootromRunning = false;

    // GBC
    Speed speed = Speed::Regular;
    bool speedShift = false;
    uint16_t hdmaSource;
    uint16_t hdmaDestination;
    bool hdmaActive;
    uint8_t hdmaRemain;

    //Interrupts
    enum class InterruptType {
        VBlank,
        LCDStat,
        Timer,
        Joypad,
    };

    uint8_t interruptEnable = 0x00;
    uint8_t interruptFlag = 0xE1;
    bool interruptMasterEnable = false;
    bool interruptDelay = false;

    void SetInterrupt(InterruptType interrupt);

    [[nodiscard]] uint32_t AdjustedCycles(const uint32_t cycles) const {
        return cycles << (speed == Speed::Double ? 1 : 0);
    }
};
