#include "Audio.h"
#include <doctest/doctest.h>

TEST_CASE("apu: wave RAM remains writable and retained across power cycles") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE, Model::AGBB};
    constexpr std::array optimizations = {false, true};
    for (const auto [model, optimized] : std::views::cartesian_product(models, optimizations)) {
        CAPTURE(ModelName(model));
        CAPTURE(optimized);
        Audio audio(true);
        audio.SetModel(model);
        audio.SetOptimizedExecution(optimized);
        audio.WriteByte(SOUND_CONTROL, 0, false);
        audio.WriteByte(SOUND_PANNING, 0xFF, false);
        CHECK(audio.ReadByte(SOUND_PANNING) == 0);
        for (auto address{0xFF30_u16}; address <= WAVE_RAM_END; ++address)
            audio.WriteByte(address, static_cast<uint8_t>(address * 0x23), false);
        for (auto cycle{0u}; cycle < 2; ++cycle) {
            audio.WriteByte(SOUND_CONTROL, 0x80, false);
            for (auto tick{0u}; tick < 64; ++tick)
                audio.Tick();
            for (auto address{0xFF30_u16}; address <= WAVE_RAM_END; ++address)
                CHECK(audio.ReadByte(address) == static_cast<uint8_t>(address * 0x23));
            audio.WriteByte(SOUND_CONTROL, 0, false);
            for (auto address{0xFF30_u16}; address <= WAVE_RAM_END; ++address)
                CHECK(audio.ReadByte(address) == static_cast<uint8_t>(address * 0x23));
        }
    }
}

TEST_CASE("apu: batching preserves mixer transitions between waveform events") {
    Audio optimized, ordinary;
    ordinary.SetOptimizedExecution(false);
    for (auto *audio : {&optimized, &ordinary}) {
        audio->SetModel(Model::CGBC);
        audio->WriteByte(SOUND_CONTROL, 0x80, false);
        audio->WriteByte(PULSE1_ENVELOPE, 0xF3, false);
        audio->WriteByte(PULSE1_FREQUENCY_HIGH, 0x87, false);
        audio->WriteByte(MASTER_VOLUME, 0x77, false);
        audio->WriteByte(SOUND_PANNING, 0x11, false);
        audio->initialize_post_boot();
    }
    for (auto phase{0u}; phase < 4; ++phase) {
        CAPTURE(phase);
        for (auto *audio : {&optimized, &ordinary}) {
            if (phase == 1)
                audio->WriteByte(SOUND_PANNING, 0x10, false);
            if (phase == 2)
                audio->WriteByte(MASTER_VOLUME, 0x13, false);
            if (phase == 3)
                audio->WriteByte(SOUND_CONTROL, 0, false);
            for (auto tick{0u}; tick < 1000; ++tick)
                audio->Tick();
        }
        std::array<float, 256> left{}, right{};
        const auto count = optimized.ReadSamples(left.data(), 128);
        CHECK(count > 0);
        CHECK(ordinary.ReadSamples(right.data(), 128) == count);
        CHECK(left == right);
    }
}
