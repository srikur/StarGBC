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

TEST_CASE("apu: duty writes affect the next pulse step while volume changes use the held sample") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    constexpr std::array second_values = {false, true};
    for (const auto [model, second] : std::views::cartesian_product(models, second_values)) {
        CAPTURE(ModelName(model));
        CAPTURE(second);
        Audio audio(true);
        audio.SetModel(model);
        audio.WriteByte(SOUND_CONTROL, 0x80, false);
        const uint16_t base = second ? PULSE2_REGISTERS_BEGIN : PULSE1_SWEEP;
        audio.WriteByte(base + 1, 0, false);
        audio.WriteByte(base + 2, 0xF0, false);
        audio.WriteByte(base + 3, 0xFE, false);
        audio.WriteByte(base + 4, 0x87, false);
        const auto position = [&] { return second ? audio.ch2.dutyStep : audio.ch1.dutyStep; };
        const auto sample = [&] { return (audio.ReadPCM12() >> (second ? 4 : 0)) & 0xF; };
        for (auto tick{0u}; tick < 100 && position() != 3; ++tick) {
            audio.Tick();
            audio.CatchUp();
        }
        REQUIRE(position() == 3);
        CHECK(sample() == 0);
        audio.WriteByte(base + 1, 0xC0, false);
        audio.WriteByte(base + 2, 0xE0, false);
        audio.WriteByte(base + 4, 0x87, false);
        CHECK(sample() == 0);
        for (auto tick{0u}; tick < 100 && position() == 3; ++tick) {
            audio.Tick();
            audio.CatchUp();
        }
        REQUIRE(position() == 4);
        CHECK(sample() == (HasEarlyCgbPcmGlitch(model) ? 0 : 0xE));
        audio.Tick();
        audio.CatchUp();
        CHECK(sample() == 0xE);
    }
}

TEST_CASE("apu: length expiration clears NR52 independently of the waveform phase") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    constexpr std::array optimizations = {false, true};
    constexpr std::array<unsigned, 4> phases = {0, 1, 2, 3};
    constexpr std::array<uint8_t, 16> length_mask_values = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    for (const auto [model, optimized, phase, length_mask] : std::views::cartesian_product(models, optimizations, phases, length_mask_values)) {
        CAPTURE(ModelName(model));
        CAPTURE(optimized);
        CAPTURE(phase);
        CAPTURE(length_mask);
        Audio audio(true);
        audio.SetModel(model);
        audio.SetOptimizedExecution(optimized);
        audio.WriteByte(SOUND_CONTROL, 0x80, false);
        for (auto tick{0u}; tick < phase; ++tick)
            audio.Tick();
        audio.WriteByte(PULSE1_DUTY_LENGTH, 63, false);
        audio.WriteByte(PULSE2_DUTY_LENGTH, 63, false);
        audio.WriteByte(WAVE_LENGTH, 255, false);
        audio.WriteByte(NOISE_LENGTH, 63, false);
        audio.WriteByte(PULSE1_ENVELOPE, 0xF0, false);
        audio.WriteByte(PULSE2_ENVELOPE, 0xF0, false);
        audio.WriteByte(WAVE_DAC_ENABLE, 0x80, false);
        audio.WriteByte(NOISE_ENVELOPE, 0xF0, false);
        constexpr std::array triggers = {PULSE1_FREQUENCY_HIGH, PULSE2_FREQUENCY_HIGH, WAVE_FREQUENCY_HIGH, NOISE_CONTROL};
        for (auto channel{0u}; channel < triggers.size(); ++channel) {
            audio.WriteByte(triggers[channel], 0x80 | ((length_mask >> channel & 1) << 6), false);
        }
        for (auto tick{0u}; tick < 16; ++tick)
            audio.Tick();
        CHECK(audio.ReadByte(SOUND_CONTROL) == 0xFF);
        audio.TickFrameSequencer();
        CHECK(audio.ReadByte(SOUND_CONTROL) == (0xFF & ~length_mask));
        for (auto tick{0u}; tick < 4; ++tick)
            audio.Tick();
        CHECK(audio.ReadByte(SOUND_CONTROL) == (0xFF & ~length_mask));
    }
}
