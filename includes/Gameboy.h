#pragma once

#include <memory>
#include <utility>
#include <fstream>

#include "Bus.h"
#include "Registers.h"
#include "Instructions.h"

class Instructions;

enum class Mode {
    None = 0x00,
    DMG = 0x01,
    MBG = 0x02,
    SGB = 0x03,
    SGB2 = 0x04,
    CGB_DMG = 0x05,
    AGB_DMG = 0x06,
    AGS_DMG = 0x07,
    CGB_GBC = 0x08,
    AGB_GBC = 0x09,
    AGS_GBC = 0x0A,
};

enum class InterruptState {
    M1, M2, M3, M4, M5
};

struct GameboySettings {
    std::string romName;
    std::string biosPath;
    Mode mode = Mode::None;
    bool runBootrom = false;
    bool debugStart = false;
    bool realRTC = false;
};

class Gameboy {
    static constexpr uint32_t CYCLES_PER_SECOND = 8388608;
    static constexpr uint32_t RTC_CLOCK_DIVIDER = 2;
    static constexpr uint32_t AUDIO_CLOCK_DIVIDER = 2;
    static constexpr uint32_t GRAPHICS_CLOCK_DIVIDER = 2;
    std::string rom_path_;
    std::string bios_path_;
    std::unique_ptr<Registers> regs = std::make_unique<Registers>();
    std::unique_ptr<Bus> bus = std::make_unique<Bus>(rom_path_);
    std::unique_ptr<Instructions> instructions = std::make_unique<Instructions>();
    Mode mode_ = Mode::DMG;
    uint16_t currentInstruction = 0x00;

    InterruptState interruptState{InterruptState::M1};
    uint16_t previousPC{0x00};
    uint16_t nextInstruction{0x0000};
    bool runPrefixStall{false};
    bool prefixed{false};
    uint8_t mCycleCounter{0x01};
    uint8_t tCycleCounter{0x00};
    uint32_t masterCycles{0x00000000};
    uint8_t interruptBit{0x00};
    uint8_t interruptMask{0x00};
    uint16_t pc = 0x00;
    uint16_t sp = 0x00;
    uint8_t icount = 0;
    bool halted = false;
    bool haltBug = false;
    uint32_t stepCycles = 0;
    uint32_t cyclesThisInstruction = 0;

    bool throttleSpeed = true;
    int speedMultiplier = 1;
    bool paused = false;
    bool stopped = false;
    bool instrComplete = false;

    void ExecuteMicroOp();

    uint8_t DecodeInstruction(uint8_t opcode, bool prefixed);

    void InitializeBootrom() const;

    void InitializeSystem();

    [[nodiscard]] uint32_t RunHDMA() const;

    bool ProcessInterrupts();

    void PrintCurrentValues() const;

public:
    friend class Instructions;

    explicit Gameboy(std::string rom_path, std::string bios_path, const Mode mode,
                     const bool debugStart, const bool realRTC) : rom_path_(std::move(rom_path)),
                                              bios_path_(std::move(bios_path)), mode_(mode), paused(debugStart) {
        if (mode_ != Mode::None) {
            bus->gpu_->hardware = mode_ == Mode::DMG ? GPU::Hardware::DMG : GPU::Hardware::CGB;
            bus->audio_->SetDMG(bus->gpu_->hardware == GPU::Hardware::DMG);
        } else if ((bus->cartridge_->ReadByte(0x143) & 0x80) == 0x80) {
            mode_ = Mode::CGB_GBC;
            bus->gpu_->hardware = GPU::Hardware::CGB;
        }
        if (!bios_path_.empty()) {
            bus->bootromRunning = true;
            InitializeBootrom();
        } else {
            pc = 0x100;
            InitializeSystem();
        }
        bus->cartridge_->rtc->realRTC = realRTC;
    }

    Gameboy(const Gameboy &other) = delete;

    Gameboy(Gameboy &&other) = delete;

    Gameboy &operator=(const Gameboy &other) = delete;

    Gameboy &operator=(Gameboy &&other) = delete;

    ~Gameboy() = default;

    static std::unique_ptr<Gameboy> init(const GameboySettings &settings) {
        return std::make_unique<Gameboy>(settings.romName, settings.biosPath, settings.mode, settings.debugStart, settings.realRTC);
    }

    void UpdateEmulator();

    [[nodiscard]] bool ShouldRender() const;

    void Save() const;

    void KeyUp(Keys key) const;

    void KeyDown(Keys key) const;

    [[nodiscard]] uint32_t *GetScreenData() const;

    void ToggleSpeed();

    void SetThrottle(bool throttle);

    void AdvanceFrame();

    void SaveState(int slot) const;

    void LoadState(int slot);

    void SetPaused(const bool val) {
        paused = val;
    }

    [[nodiscard]] bool IsPaused() const {
        return paused;
    }
};
