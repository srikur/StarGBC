#pragma once

#include <fstream>
#include <memory>
#include <utility>

#include "Common.h"
#include "CPU.h"

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
