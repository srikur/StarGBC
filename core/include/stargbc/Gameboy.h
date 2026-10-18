#pragma once

#include <chrono>
#include <memory>
#include <numeric>
#include <span>
#include <type_traits>

#include "CPU.h"
#include "stargbc/Common.h"
#include "Execution.h"
#include "Memory.h"
#include "diagnostics/ReplayInput.h"

struct GameboySettings {
    Model model{Model::Auto};
    bool no_bootrom{false};
    bool real_rtc{false};
    bool no_audio{false};
    bool color_correction{true};
    ExecutionPolicy execution_policy{ExecutionPolicy::Optimized};
    DmgPaletteSampling dmg_palette_sampling{DmgPaletteSampling::BitwiseOr};
    bool force_cgb_mode{false};
};

struct GameboyMedia {
    std::vector<u8> rom;
    std::vector<u8> bootrom;
    std::vector<u8> battery;
};

class Gameboy {
public:
    static constexpr uint32_t DMG_CYCLES_PER_SECOND = 4194304;
    static constexpr uint32_t CGB_CYCLES_PER_SECOND = DMG_CYCLES_PER_SECOND * 2;
    static constexpr uint32_t FRAME_CYCLES_DMG = 70224;
    static constexpr std::chrono::nanoseconds FRAME_PERIOD{FRAME_CYCLES_DMG * 1'000'000'000LL / DMG_CYCLES_PER_SECOND};

    explicit Gameboy(const GameboySettings &settings, GameboyMedia media, RealTimeClock::TimeSource time_source = {}) :
        initialSettings_(settings), rtc_(settings.real_rtc, std::move(time_source)), cartridge(rtc_, std::move(media.rom)), joypad_(interrupts_),
        audio_(settings.no_audio), timer_(audio_, interrupts_), serial_(interrupts_), gpu_(interrupts_, settings.color_correction),
        bus_(joypad_, memory_, timer_, cartridge, serial_, dma_, audio_, interrupts_, gpu_),
        cpu_(settings.model, media.bootrom, settings.no_bootrom, bus_, interrupts_, registers_, settings.force_cgb_mode),
        instructions_(registers_, interrupts_), executionPolicy_(settings.execution_policy) {
        cartridge.LoadBattery(media.battery);
        timer_.serial = &serial_;
        gpu_.dmg_palette_sampling = settings.dmg_palette_sampling;
    }

    Gameboy(const Gameboy &other) = delete;

    Gameboy(Gameboy &&other) = delete;

    Gameboy &operator=(const Gameboy &other) = delete;

    Gameboy &operator=(Gameboy &&other) = delete;

    ~Gameboy() = default;

    [[nodiscard]] static std::string_view CoreBuildIdentity();

    [[nodiscard]] const GameboySettings &InitialSettings() const { return initialSettings_; }

    static std::unique_ptr<Gameboy> init(const GameboySettings &settings, GameboyMedia media, RealTimeClock::TimeSource time_source = {}) {
        return std::make_unique<Gameboy>(settings, std::move(media), std::move(time_source));
    }

    void RunFrame();

    [[nodiscard]] ExecutionResult AdvanceTicks(uint64_t ticks, ExecutionPolicy policy = ExecutionPolicy::Ordinary);

    [[nodiscard]] ExecutionResult StepInstruction(uint64_t maxTicks);

    [[nodiscard]] ConditionalExecutionResult RunUntil(std::span<const RunCondition> conditions, uint64_t maxTicks);

    [[nodiscard]] diagnostics::ObservationContext *Observations() const { return observations_; }

    [[nodiscard]] uint64_t TimelineTicks() const { return timelineTicks_; }

    void ObserveWith(diagnostics::ObservationContext *context);

    void RecordPixelsWith(diagnostics::PixelProvenanceContext *context);

    [[nodiscard]] diagnostics::ExecutionPosition ExecutionPosition() const;

    [[nodiscard]] bool ConsumeFrame();

    [[nodiscard]] std::vector<u8> SaveBattery() const { return cartridge.SaveBattery(); }

    void LoadBattery(std::span<const u8> bytes) { cartridge.LoadBattery(bytes); }

    [[nodiscard]] bool BatterySavePending() const { return cartridge.BatterySavePending(); }

    void AcknowledgeBatterySave() { cartridge.AcknowledgeBatterySave(); }

    void KeyUp(Keys);

    void KeyDown(Keys);

    void ApplyButtons(uint8_t buttons);

    void RecordInputsWith(diagnostics::InputSink *sink) { inputSink_ = sink; }

    [[nodiscard]] diagnostics::InputSink *InputRecording() const { return inputSink_; }

    [[nodiscard]] uint64_t InstructionsCompleted() const { return cpu_.InstructionsCompleted(); }

    [[nodiscard]] uint8_t PressedButtons() const { return static_cast<uint8_t>(~joypad_.GetMatrix()); }

    [[nodiscard]] uint8_t DebugPeek(const uint16_t address) const { return bus_.DebugPeek(address); }

    [[nodiscard]] MachineInspection Inspect() const;

    [[nodiscard]] GraphicsInspection inspect_graphics() const;

    [[nodiscard]] uint16_t debug_instruction_address() const {
        const auto cpu{cpu_.Inspect()};
        return cpu.interruptState == InterruptState::M6 ? cpu.pc : cpu_.InstructionAddress();
    }

    [[nodiscard]] uint16_t debug_instruction_bank() const {
        const auto cpu{cpu_.Inspect()};
        return cpu.interruptState == InterruptState::M6 ? bus_.DebugBank(cpu.pc) : cpu_.InstructionBank();
    }

    [[nodiscard]] bool debug_instruction_boundary() const { return cpu_.AtInstructionBoundary(); }

    [[nodiscard]] uint16_t debug_bank(const uint16_t address) const { return bus_.DebugBank(address); }

    [[nodiscard]] CpuInspection InspectCpu() const { return cpu_.Inspect(); }

    [[nodiscard]] RegisterInspection InspectRegister(uint16_t address) const;

    [[nodiscard]] uint8_t InspectCpuRead(const uint16_t address) const { return bus_.InspectRead(address, ComponentSource::CPU); }

    void DebugPoke(const uint16_t address, const uint8_t value) const { bus_.DebugPoke(address, value); }

    [[nodiscard]] uint8_t DebugPeekWram(const uint8_t bank, const uint16_t offset) const {
        if (bank > 7 || offset >= 0x1000 || (bank > 1 && !IsInCgbMode()))
            throw std::out_of_range("invalid WRAM bank or offset");
        return memory_.wram_[bank * 0x1000 + offset];
    }

    [[nodiscard]] const uint32_t *GetScreenData() const;

    [[nodiscard]] auto SaveState() const;

    [[nodiscard]] bool LoadState(std::span<const std::byte> state);

    [[nodiscard]] std::vector<std::byte> SaveDebugState() const;

    [[nodiscard]] bool LoadDebugState(std::span<const std::byte> state);

    [[nodiscard]] std::span<const uint8_t> BootromBytes() const { return bus_.bootrom; }

    [[nodiscard]] std::span<const uint8_t> rom_bytes() const { return cartridge.rom_bytes(); }

    void reset_to_startup(std::span<const std::byte> state);

    [[nodiscard]] bool UsesRealRTC() const { return rtc_.real_rtc; }

    [[nodiscard]] uint16_t CartChecksum() const { return cartridge.GlobalChecksum(); }

    [[nodiscard]] Model GetModel() const { return gpu_.model; }

    [[nodiscard]] bool IsInCgbMode() const { return bus_.cgbMode; }

    [[nodiscard]] size_t GetAudioSamplesAvailable() const { return audio_.GetSamplesAvailable(); }

    size_t ReadAudioSamples(float *output, const size_t numSamples) { return audio_.ReadSamples(output, numSamples); }

    void ClearAudioBuffer() { audio_.ClearBuffer(); }

private:
    [[=NotStateAware]] GameboySettings initialSettings_;

    RealTimeClock rtc_; // init in constructor
    Cartridge cartridge; // init in constructor
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
    Instructions<CPU<Bus>> instructions_;

    uint32_t masterCycles{0x00000000};
    uint8_t cpuTickPhase_{0x00};
    uint64_t timelineTicks_{};
    [[=NotStateAware]] ExecutionPolicy executionPolicy_{ExecutionPolicy::Optimized};
    [[=NotStateAware]] diagnostics::ObservationContext *observations_{};
    [[=NotStateAware]] diagnostics::InputSink *inputSink_{};

    void ObserveInput(uint8_t previous);

    void SynchronizeObservationPosition() const;

    uint32_t AdvanceCycles(uint32_t maxCycles);

    uint32_t AdvanceMCycle();

    uint32_t AdvanceMCycleDouble();

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
        for (auto &element : obj) {
            offset = ForEachStateLeaf(element, visit, offset);
        }
    } else {
        static constexpr auto bases = std::define_static_array(StateBasesOf(^^U));
        static constexpr auto members = std::define_static_array(StateMembersOf(^^U));
        if constexpr (bases.empty() && members.empty()) {
            visit(obj, offset);
            offset += sizeof(U);
        } else {
            template for (constexpr auto base : bases) {
                using BaseT = [:base:];
                offset = ForEachStateLeaf(StateBaseCast<BaseT>(obj), visit, offset);
            }
            template for (constexpr auto m : members) { offset = ForEachStateLeaf(obj.[:m:], visit, offset); }
        }
    }
    return offset;
}

template<class T>
static constexpr void SerializeInto(const T &obj, std::byte *out) {
    ForEachStateLeaf(obj, [out]<typename L>(const L &leaf, const std::size_t offset) {
        using Leaf = std::remove_cvref_t<L>;
        std::ranges::copy(std::bit_cast<std::array<std::byte, sizeof(Leaf)>>(leaf), out + offset);
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsfinae-incomplete"
inline auto Gameboy::SaveState() const {
    std::array<std::byte, kGameboyStateSize> out{};
    SerializeInto(*this, out.data());
    return out;
}
#pragma GCC diagnostic pop

inline bool Gameboy::LoadState(const std::span<const std::byte> state) {
    if (state.size() != kGameboyStateSize)
        return false;
    const Model expectedModel = GetModel();
    const auto backup = SaveState();
    DeserializeFrom(*this, state.data());
    if (GetModel() != expectedModel || !LoadedStateValid()) {
        DeserializeFrom(*this, backup.data());
        return false;
    }
    // Fix up some [[=NotStateAware]] members
    rtc_.recalculate_zero_time();
    cartridge.MarkRamDirty();
    audio_.ClearBuffer();
    interrupts_.recompute_pending();
    gpu_.SetModel(gpu_.model);
    gpu_.InvalidateIdle();
    gpu_.RecomputeSpritePending();
    gpu_.frameReady = false;
    if (observations_) {
        observations_->pc = cpu_.InstructionAddress();
        observations_->pcBank = cpu_.InstructionBank();
        observations_->Restore(ExecutionPosition());
    }
    if (gpu_.provenance) {
        gpu_.provenance->ResetPipeline();
        gpu_.provenance->position = ExecutionPosition();
        gpu_.provenance->branch = observations_ ? observations_->branch : gpu_.provenance->branch + 1;
        gpu_.provenance->Pipeline(gpu_.pixelsDrawn, gpu_.currentLine, diagnostics::PipelineReason::RestoredWithoutHistory);
    }
    return true;
}
