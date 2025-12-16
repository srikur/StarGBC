#pragma once

#include <fstream>
#include <memory>
#include <utility>

#include "Common.h"
#include "CPU.h"
#include "Memory.h"

struct GameboySettings {
    std::string romName;
    std::string biosPath;
    Mode mode{Mode::None};
    bool runBootrom{false};
    bool debugStart{false};
    bool realRTC{false};
    bool unthrottled{false};
};

class Gameboy {
public:
    explicit Gameboy(const GameboySettings &settings) : romPath_(std::move(settings.romName)),
                                                        biosPath_(std::move(settings.biosPath)),
                                                        rtc_(settings.realRTC),
                                                        cartridge_(romPath_, rtc_), joypad_(interrupts_), serial_(interrupts_), gpu_(interrupts_),
                                                        bus_(joypad_, memory_, timer_, cartridge_, serial_, dma_, audio_, interrupts_, gpu_),
                                                        cpu_(settings.mode, biosPath_, bus_, interrupts_),
                                                        throttleSpeed_(!settings.unthrottled),
                                                        paused_(settings.debugStart) {
    }

    Gameboy(const Gameboy &other) = delete;

    Gameboy(Gameboy &&other) = delete;

    Gameboy &operator=(const Gameboy &other) = delete;

    Gameboy &operator=(Gameboy &&other) = delete;

    ~Gameboy() = default;

    static std::unique_ptr<Gameboy> init(const GameboySettings &settings) {
        return std::make_unique<Gameboy>(settings);
    }

    void UpdateEmulator();

    [[nodiscard]] bool ShouldRender() const;

    void Save() const;

    void KeyUp(Keys key);

    void KeyDown(Keys key);

    [[nodiscard]] const uint32_t *GetScreenData() const;

    void ToggleSpeed();

    void SetThrottle(bool throttle);

    void SaveScreen() const;

    void SetPaused(const bool val) {
        paused_ = val;
    }

    [[nodiscard]] bool IsPaused() const {
        return paused_;
    }

private:
    static constexpr uint32_t DMG_CYCLES_PER_SECOND = 4194034;
    static constexpr uint32_t CGB_CYCLES_PER_SECOND = DMG_CYCLES_PER_SECOND * 2;
    static constexpr uint32_t RTC_CLOCK_DIVIDER = 2;
    static constexpr uint32_t AUDIO_CLOCK_DIVIDER = 2;
    static constexpr uint32_t GRAPHICS_CLOCK_DIVIDER = 2;

    std::string romPath_;
    std::string biosPath_;

    RealTimeClock rtc_; // init in constructor
    Cartridge cartridge_; // init in constructor
    Interrupts interrupts_{};
    DMA dma_{};
    Joypad joypad_;
    Audio audio_{};
    Memory memory_{};
    Timer timer_{};
    Serial serial_;
    GPU gpu_;
    Bus bus_;
    CPU<Bus> cpu_;

    uint32_t masterCycles{0x00000000};
    int speedMultiplier_{1};
    bool throttleSpeed_{true};
    bool paused_{false};

    void AdvanceFrame();
};
