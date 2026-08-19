#pragma once

#include <chrono>
#include <memory>
#include <type_traits>
#include <utility>
#include <numeric>

#include "Common.h"
#include "CPU.h"
#include "Memory.h"

struct GameboySettings {
    std::string romName;
    std::string biosPath;
    Mode mode{Mode::None};
    bool noBootrom{false};
    bool realRTC{false};
};

class Gameboy {
public:
    static constexpr uint32_t DMG_CYCLES_PER_SECOND = 4194304;
    static constexpr uint32_t CGB_CYCLES_PER_SECOND = DMG_CYCLES_PER_SECOND * 2;
    static constexpr uint32_t FRAME_CYCLES_DMG = 70224;
    static constexpr std::chrono::nanoseconds FRAME_PERIOD{
        FRAME_CYCLES_DMG * 1'000'000'000LL / DMG_CYCLES_PER_SECOND
    };

    explicit Gameboy(const GameboySettings &settings) : romPath_(std::move(settings.romName)),
                                                        biosPath_(std::move(settings.biosPath)),
                                                        rtc_(settings.realRTC),
                                                        cartridge_(romPath_, rtc_),
                                                        joypad_(interrupts_), timer_(audio_, interrupts_),
                                                        serial_(interrupts_), gpu_(interrupts_),
                                                        bus_(joypad_, memory_, timer_, cartridge_, serial_, dma_,
                                                             audio_, interrupts_, gpu_),
                                                        cpu_(settings.mode, biosPath_, settings.noBootrom, bus_,
                                                             interrupts_, registers_),
                                                        instructions_(registers_, interrupts_) {
    }

    Gameboy(const Gameboy &other) = delete;

    Gameboy(Gameboy &&other) = delete;

    Gameboy &operator=(const Gameboy &other) = delete;

    Gameboy &operator=(Gameboy &&other) = delete;

    ~Gameboy() = default;

    static std::unique_ptr<Gameboy> init(const GameboySettings &settings) {
        return std::make_unique<Gameboy>(settings);
    }

    void RunFrame();

    [[nodiscard]] bool ConsumeFrame();

    void Save() const;

    void KeyUp(Keys);

    void KeyDown(Keys);

    [[nodiscard]] const uint32_t *GetScreenData() const;

    [[nodiscard]] auto SaveState() const;

    void LoadState();

    [[nodiscard]] size_t GetAudioSamplesAvailable() const {
        return audio_.GetSamplesAvailable();
    }

    size_t ReadAudioSamples(float *output, const size_t numSamples) {
        return audio_.ReadSamples(output, numSamples);
    }

    void ClearAudioBuffer() {
        audio_.ClearBuffer();
    }

private:
    [[=NotStateAware]] std::string romPath_;
    [[=NotStateAware]] std::string biosPath_;

    RealTimeClock rtc_; // init in constructor
    Cartridge cartridge_; // init in constructor
    Interrupts interrupts_{};
    Registers registers_{};
    DMA dma_{};
    Joypad joypad_;
    Audio audio_{};
    Memory memory_{};
    Timer timer_;
    Serial serial_;
    GPU gpu_;
    Bus bus_;
    CPU<Bus> cpu_;
    Instructions<CPU<Bus> > instructions_;

    uint32_t masterCycles{0x00000000};
    uint8_t cpuTickPhase_{0x00};

    uint32_t AdvanceCycles(uint32_t maxCycles);
};

template<class T>
static constexpr void SerializeInto(const T &obj, std::byte *out) {
    if constexpr (std::is_array_v<T>) {
        constexpr std::size_t elementSize = StateSizeOf(^^std::remove_extent_t<T>);
        std::size_t offset = 0;
        for (const auto &element: obj) {
            SerializeInto(element, out + offset);
            offset += elementSize;
        }
    } else {
        static constexpr auto bases = std::define_static_array(StateBasesOf(^^T));
        static constexpr auto members = std::define_static_array(StateMembersOf(^^T));
        if constexpr (bases.empty() && members.empty()) {
            std::ranges::copy(std::bit_cast<std::array<std::byte, sizeof(T)> >(obj), out);
        } else {
            std::size_t offset = 0;
            template for (constexpr auto base: bases) {
                using BaseT = [:base:];
                SerializeInto(static_cast<const BaseT &>(obj), out + offset);
                offset += StateSizeOf(base);
            }
            template for (constexpr auto m: members) {
                SerializeInto(obj.[:m:], out + offset);
                offset += StateSizeOf(std::meta::type_of(m));
            }
        }
    }
}

inline constexpr std::size_t kGameboyStateSize = StateSizeOf(^^Gameboy);

inline auto Gameboy::SaveState() const {
    std::array<std::byte, kGameboyStateSize> out{};
    SerializeInto(*this, out.data());
    return out;
}
