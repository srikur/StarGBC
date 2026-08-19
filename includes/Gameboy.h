#pragma once

#include <chrono>
#include <memory>
#include <span>
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

    [[nodiscard]] bool LoadState(std::span<const std::byte> state);

    [[nodiscard]] uint16_t CartChecksum() const {
        return cartridge_.GlobalChecksum();
    }

    [[nodiscard]] Hardware GetHardware() const {
        return gpu_.hardware;
    }

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

    [[nodiscard]] bool LoadedStateValid() const;
};

template<class Base, class T>
static constexpr auto &StateBaseCast(T &obj) {
    if constexpr (std::is_const_v<T>) {
        return static_cast<const Base &>(obj);
    } else {
        return static_cast<Base &>(obj);
    }
}

template<class T, class Visitor>
static constexpr std::size_t ForEachStateLeaf(T &obj, Visitor &&visit, std::size_t offset = 0) {
    using U = std::remove_const_t<T>;
    if constexpr (std::is_array_v<U>) {
        for (auto &element: obj) {
            offset = ForEachStateLeaf(element, visit, offset);
        }
    } else {
        static constexpr auto bases = std::define_static_array(StateBasesOf(^^U));
        static constexpr auto members = std::define_static_array(StateMembersOf(^^U));
        if constexpr (bases.empty() && members.empty()) {
            visit(obj, offset);
            offset += sizeof(U);
        } else {
            template for (constexpr auto base: bases) {
                using BaseT = [:base:];
                offset = ForEachStateLeaf(StateBaseCast<BaseT>(obj), visit, offset);
            }
            template for (constexpr auto m: members) {
                offset = ForEachStateLeaf(obj.[:m:], visit, offset);
            }
        }
    }
    return offset;
}

template<class T>
static constexpr void SerializeInto(const T &obj, std::byte *out) {
    ForEachStateLeaf(obj, [out](const auto &leaf, const std::size_t offset) {
        using Leaf = std::remove_cvref_t<decltype(leaf)>;
        std::ranges::copy(std::bit_cast<std::array<std::byte, sizeof(Leaf)> >(leaf), out + offset);
    });
}

template<class T>
static constexpr void DeserializeFrom(T &obj, const std::byte *in) {
    ForEachStateLeaf(obj, [in](auto &leaf, const std::size_t offset) {
        using Leaf = std::remove_cvref_t<decltype(leaf)>;
        std::array<std::byte, sizeof(Leaf)> bytes;
        std::copy_n(in + offset, sizeof(Leaf), bytes.begin());
        leaf = std::bit_cast<Leaf>(bytes);
    });
}

inline constexpr std::size_t kGameboyStateSize = StateSizeOf(^^Gameboy);

inline auto Gameboy::SaveState() const {
    std::array<std::byte, kGameboyStateSize> out{};
    SerializeInto(*this, out.data());
    return out;
}

inline bool Gameboy::LoadState(const std::span<const std::byte> state) {
    if (state.size() != kGameboyStateSize) return false;
    const auto backup = SaveState();
    DeserializeFrom(*this, state.data());
    if (!LoadedStateValid()) {
        DeserializeFrom(*this, backup.data());
        return false;
    }
    // Fix up some [[=NotStateAware]] members
    rtc_.RecalculateZeroTime();
    cartridge_.MarkRamDirty();
    audio_.ClearBuffer();
    gpu_.frameReady = false;
    return true;
}
