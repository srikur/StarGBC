#pragma once

#include <memory>
#include <utility>
#include <fstream>

#include "Bus.h"
#include "Registers.h"
#include "Instructions.h"

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
    M1, M2, M3, M4, M5, M6
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
    std::string romPath_;
    std::string biosPath_;
    std::unique_ptr<CPU> cpu = nullptr;

    bool throttleSpeed = true;
    int speedMultiplier = 1;
    bool paused = false;

public:
    static constexpr uint32_t DMG_CYCLES_PER_SECOND = 4194034;
    static constexpr uint32_t CGB_CYCLES_PER_SECOND = DMG_CYCLES_PER_SECOND * 2;
    static constexpr uint32_t RTC_CLOCK_DIVIDER = 2;
    static constexpr uint32_t AUDIO_CLOCK_DIVIDER = 2;
    static constexpr uint32_t GRAPHICS_CLOCK_DIVIDER = 2;

    explicit Gameboy(std::string romPath, std::string biosPath, const Mode mode,
                     const bool debugStart, const bool realRTC) : romPath_(std::move(romPath)),
                                                                  biosPath_(std::move(biosPath)),
                                                                  paused(debugStart) {
        cpu = std::make_unique<CPU>(mode, biosPath_, realRTC);
    }

    Gameboy(const Gameboy &other) = delete;

    Gameboy(Gameboy &&other) = delete;

    Gameboy &operator=(const Gameboy &other) = delete;

    Gameboy &operator=(Gameboy &&other) = delete;

    ~Gameboy() = default;

    static std::unique_ptr<Gameboy> init(const GameboySettings &settings) {
        return std::make_unique<Gameboy>(settings.romName, settings.biosPath, settings.mode, settings.debugStart, settings.realRTC);
    }

    void UpdateEmulator() const;

    [[nodiscard]] bool ShouldRender() const;

    void Save() const;

    void KeyUp(Keys key) const;

    void KeyDown(Keys key) const;

    [[nodiscard]] uint32_t *GetScreenData() const;

    void ToggleSpeed();

    void SetThrottle(bool throttle);

    void SaveScreen() const {
        try {
            std::ofstream file(romPath_ + ".screen", std::ios::binary | std::ios::trunc);
            if (!file.is_open()) throw std::runtime_error("Could not open " + romPath_ + ".screen");
            file.write(reinterpret_cast<const char *>(GetScreenData()), 160 * 144 * 4);
            std::fprintf(stderr, "Saved screen to %s.screen\n", romPath_.c_str());
        } catch (const std::exception &e) {
            std::fprintf(stderr, "Failed to save screen: %s\n", e.what());
        }
    }

    void SetPaused(const bool val) {
        paused = val;
    }

    [[nodiscard]] bool IsPaused() const {
        return paused;
    }
};
