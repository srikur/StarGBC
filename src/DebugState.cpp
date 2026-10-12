#include "Gameboy.h"

namespace {
    struct DebugSupplement {
        std::array<uint32_t, SCREEN_HEIGHT * SCREEN_WIDTH * 3> screen;
        bool frameReady{};
        uint64_t instructions{};
        AudioDebugOutput audio;
    };

    constexpr auto SupplementSize = StateSizeOf(^^DebugSupplement);
}

AudioDebugOutput Audio::CaptureDebugOutput() const {
    AudioDebugOutput state{
            .sampleCount = sampleBuffer.size(), .writePosition = bufferWritePos, .readPosition = bufferReadPos,
            .available = samplesAvailable, .sampleAccumulator = sampleAcc_, .resampler = bandLimited,
            .highpassLeft = highpassLeft, .highpassRight = highpassRight, .lastMixRegisters = lastMixRegs_,
            .lastMixOutputs = lastMixOutputs_, .mixLevel = mixLevel_, .mixChanges = mixLevelChanges_,
            .pendingTicks = pendingTicks_, .pendingFirstTick = pendingFirstTick_, .emissionTick = emissionTick_};
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
