#pragma once

#include <memory>
#include <utility>
#include <fstream>
#include <tuple>

#include "Bus.h"
#include "Registers.h"
#include "Instructions.h"

class Instructions;

enum class Mode {
    None = 0x00,
    GB = 0x01,
    GBC = 0x02,
    GBA = 0x03,
};

struct GameboySettings {
    std::string romName;
    std::string biosPath;
    Mode mode = Mode::None;
    bool runBootrom = false;
    bool debugStart = false;
};

class Gameboy {
    std::string rom_path_;
    std::string bios_path_;
    std::unique_ptr<Registers> regs = std::make_unique<Registers>();
    std::unique_ptr<Bus> bus = std::make_unique<Bus>(rom_path_);
    std::unique_ptr<Instructions> instructions = std::make_unique<Instructions>();
    Mode mode_ = Mode::GB;
    uint16_t currentInstruction = 0x00;

    uint16_t pc = 0x00;
    uint16_t sp = 0x00;
    uint8_t icount = 0;
    bool halted = false;
    bool haltBug = false;
    uint32_t stepCycles = 0;

    uint32_t elapsedCycles = 0;

    bool throttleSpeed = true;
    int speedMultiplier = 1;
    bool paused = false;

    [[nodiscard]] inline uint16_t ReadNextWord() const;

    [[nodiscard]] inline uint8_t ReadNextByte() const;

    uint8_t DecodeInstruction(uint8_t opcode, bool prefixed);

    uint8_t ExecuteInstruction();

    void InitializeBootrom() const;

    void InitializeSystem();

    [[nodiscard]] uint32_t RunHDMA() const;

    uint32_t ProcessInterrupts();

    void PrintCurrentValues() const;

public:
    friend class Instructions;

    explicit Gameboy(std::string rom_path, std::string bios_path, const Mode mode,
                     const bool debugStart) : rom_path_(std::move(rom_path)),
                                              bios_path_(std::move(bios_path)), mode_(mode), paused(debugStart) {
        bus->gpu_->hardware = (mode_ == Mode::GBC) || (bus->cartridge_->ReadByte(0x143) & 0x80) == 0x80
                                  ? GPU::Hardware::CGB
                                  : GPU::Hardware::DMG;
        if (!bios_path_.empty()) {
            bus->bootromRunning = true;
            InitializeBootrom();
        } else {
            pc = 0x100;
            InitializeSystem();
        }
    }

    Gameboy(const Gameboy &other) = delete;

    Gameboy(Gameboy &&other) = delete;

    Gameboy &operator=(const Gameboy &other) = delete;

    Gameboy &operator=(Gameboy &&other) = delete;

    ~Gameboy() = default;

    static std::unique_ptr<Gameboy> init(const GameboySettings &settings) {
        return std::make_unique<Gameboy>(settings.romName, settings.biosPath, settings.mode, settings.debugStart);
    }

    void UpdateEmulator();

    [[nodiscard]] bool CheckVBlank() const;

    void Save() const;

    void KeyUp(Keys key) const;

    void KeyDown(Keys key) const;

    [[nodiscard]] std::tuple<uint32_t, uint32_t, uint32_t> GetPixel(uint32_t y, uint32_t x) const;

    void ToggleSpeed();

    void SetThrottle(bool throttle);

    void AdvanceFrames(uint32_t frameBudget);

    void DebugNextInstruction();

    void SaveState(int slot) const;

    void LoadState(int slot);

    void SetPaused(const bool val) {
        paused = val;
        PrintCurrentValues();
    }

    [[nodiscard]] bool IsPaused() const {
        return paused;
    }

    [[nodiscard]] bool PopSample(StereoSample sample) const;
};
