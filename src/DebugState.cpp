#include "Gameboy.h"

void Cartridge::restore_ram(const std::span<const uint8_t> bytes) {
    if (bytes.size() != game_ram_size)
        throw std::invalid_argument("cartridge RAM size mismatch");
    std::ranges::copy(bytes, game_ram.begin());
    MarkRamDirty();
}

void Gameboy::reset_to_startup(const std::span<const std::byte> state) {
    if (inputSink_)
        throw std::logic_error("Stop input recording before resetting the ROM");
    const std::vector ram(cartridge.ram_bytes().begin(), cartridge.ram_bytes().end());
    const auto clock{rtc_};
    if (!LoadState(state))
        throw std::invalid_argument("invalid startup state");
    cartridge.restore_ram(ram);
    rtc_ = clock;
    cpu_.RestoreInstructionCount(0);
    gpu_.screenData.fill(0);
    gpu_.frameReady = true;
}

namespace {
    struct DebugSupplement {
        std::array<uint32_t, SCREEN_HEIGHT * SCREEN_WIDTH * 3> screen;
        bool frameReady{};
        uint64_t instructions{};
        AudioDebugOutput audio;
    };

    constexpr auto SupplementSize = StateSizeOf(^^DebugSupplement);
} // namespace

AudioDebugOutput Audio::CaptureDebugOutput() const {
    AudioDebugOutput state{
            .sampleCount = sampleBuffer.size(),
            .writePosition = bufferWritePos,
            .readPosition = bufferReadPos,
            .available = samplesAvailable,
            .sampleAccumulator = sampleAcc_,
            .resampler = bandLimited,
            .highpassLeft = highpassLeft,
            .highpassRight = highpassRight,
            .lastMixRegisters = lastMixRegs_,
            .lastMixOutputs = lastMixOutputs_,
            .mixLevel = mixLevel_,
            .mixChanges = mixLevelChanges_,
            .pendingTicks = pendingTicks_,
            .pendingFirstTick = pendingFirstTick_,
            .emissionTick = emissionTick_
    };
    std::ranges::copy(sampleBuffer, state.samples.begin());
    return state;
}

bool Audio::ValidDebugOutput(const AudioDebugOutput &state) const {
    return state.sampleCount == sampleBuffer.size() && state.writePosition < AUDIO_BUFFER_SIZE && state.readPosition < AUDIO_BUFFER_SIZE &&
           state.available <= AUDIO_BUFFER_SIZE && state.sampleAccumulator < APU_CLOCK_RATE_INT &&
           std::ranges::all_of(state.resampler, [](const auto &channel) { return channel.pos >= 0 && channel.pos < BL_BUFFER_SIZE; });
}

void Audio::RestoreDebugOutput(const AudioDebugOutput &state) {
    std::ranges::copy(std::span(state.samples).first(state.sampleCount), sampleBuffer.begin());
    bufferWritePos = state.writePosition;
    bufferReadPos = state.readPosition;
    samplesAvailable = state.available;
    sampleAcc_ = state.sampleAccumulator;
    bandLimited = state.resampler;
    highpassLeft = state.highpassLeft;
    highpassRight = state.highpassRight;
    lastMixRegs_ = state.lastMixRegisters;
    lastMixOutputs_ = state.lastMixOutputs;
    mixLevel_ = state.mixLevel;
    mixLevelChanges_ = state.mixChanges;
    pendingTicks_ = state.pendingTicks;
    pendingFirstTick_ = state.pendingFirstTick;
    emissionTick_ = state.emissionTick;
}

std::vector<std::byte> Gameboy::SaveDebugState() const {
    if (UsesRealRTC())
        throw std::invalid_argument("Deterministic checkpoints require virtual RTC");
    std::vector<std::byte> output(kGameboyStateSize + SupplementSize);
    SerializeInto(*this, output.data());
    const DebugSupplement supplement{gpu_.screenData, gpu_.frameReady, cpu_.InstructionsCompleted(), audio_.CaptureDebugOutput()};
    SerializeInto(supplement, output.data() + kGameboyStateSize);
    return output;
}

bool Gameboy::LoadDebugState(const std::span<const std::byte> state) {
    if (UsesRealRTC() || state.size() != kGameboyStateSize + SupplementSize)
        return false;
    DebugSupplement supplement;
    DeserializeFrom(supplement, state.data() + kGameboyStateSize);
    if (!audio_.ValidDebugOutput(supplement.audio) || !LoadState(state.first(kGameboyStateSize)))
        return false;
    gpu_.screenData = supplement.screen;
    gpu_.frameReady = supplement.frameReady;
    cpu_.RestoreInstructionCount(supplement.instructions);
    audio_.RestoreDebugOutput(supplement.audio);
    return true;
}
