#include <array>
#include <doctest/doctest.h>
#include <ranges>
#include "Gameboy.h"
#include "TestUtilities.h"

static constexpr std::string_view kRomName = "roms/acid/cgb-acid2.gbc";

TEST_CASE("OAM DMA: Bus conflicts alter the transferred byte without writing the CPU address") {
    constexpr std::array models = {Model::DMGB, Model::CGBC};
    constexpr std::array<uint16_t, 3> sources = {0x0000, VRAM_BEGIN, WRAM_BEGIN};
    for (const auto [model, source] : std::views::cartesian_product(models, sources)) {
        CAPTURE(model);
        CAPTURE(source);
        Gameboy gameboy(GameboySettings{.romName = std::string{kRomName}, .model = model, .noBootrom = true});
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        bus.gpu_.vram[0] = 0x65;
        bus.memory_.wram_[0] = 0x65;
        bus.gpu_.oam.fill(0x87);
        bus.WriteByte(OAM_DMA_TRANSFER_REG, source >> 8, ComponentSource::CPU);
        for (auto _ : std::views::indices(8))
            bus.UpdateDMA();
        REQUIRE(bus.dma_.currentByte == 1);
        const auto original = bus.gpu_.oam[0];
        const uint16_t address = source + 0x80;
        const auto destination = gameboy.DebugPeek(address);
        bus.WriteByte(address, 0x55, ComponentSource::CPU);
        const auto expected = model == Model::DMGB ? (source == WRAM_BEGIN ? original & 0x55 : 0x55)
                                                   : (source == WRAM_BEGIN   ? original
                                                      : source == VRAM_BEGIN ? 0
                                                                             : 0x55);
        CHECK(bus.gpu_.oam[0] == expected);
        CHECK(bus.gpu_.oam[1] == 0x87);
        CHECK(gameboy.DebugPeek(address) == destination);
        for (auto _ : std::views::indices(4))
            bus.UpdateDMA();
        CHECK(bus.gpu_.oam[0] == expected);
        CHECK(bus.dma_.currentByte == 2);
    }
}

TEST_CASE("CGB OAM DMA selects the work RAM half while the CPU selects its byte") {
    constexpr std::array<uint16_t, 4> sources = {0x0000, 0x1000, 0xE000, 0xF000};
    constexpr std::array<uint16_t, 4> addresses = {0xC123, 0xD123, 0xE123, 0xF123};
    for (const auto [source, address] : std::views::cartesian_product(sources, addresses)) {
        CAPTURE(source);
        CAPTURE(address);
        Gameboy gameboy(GameboySettings{.romName = std::string{kRomName}, .model = Model::CGBC, .noBootrom = true});
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        bus.memory_.wramBank_ = 3;
        bus.memory_.wram_[0x0123] = 0x11;
        bus.memory_.wram_[0x1123] = 0x22;
        bus.memory_.wram_[0x3123] = 0x33;
        bus.WriteByte(OAM_DMA_TRANSFER_REG, source >> 8, ComponentSource::CPU);
        for (auto _ : std::views::indices(8))
            bus.UpdateDMA();
        REQUIRE(bus.dma_.currentByte == 1);
        const bool banked = source & 0x1000;
        const auto before = gameboy.SaveState();
        CHECK(gameboy.InspectCpuRead(address) == (banked ? 0x33 : 0x11));
        CHECK(gameboy.SaveState() == before);
        CHECK(bus.ReadByte(address, ComponentSource::CPU) == (banked ? 0x33 : 0x11));
        const auto transferred = bus.gpu_.oam[0];
        bus.WriteByte(address, 0x55, ComponentSource::CPU);
        CHECK(bus.memory_.wram_[0x0123] == (banked ? 0x11 : 0x55));
        CHECK(bus.memory_.wram_[0x1123] == 0x22);
        CHECK(bus.memory_.wram_[0x3123] == (banked ? 0x55 : 0x33));
        CHECK(bus.gpu_.oam[0] == transferred);
    }
}

TEST_CASE("OAM DMA sources above work RAM are disconnected on CGB and mirrored on DMG") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    constexpr std::array<uint8_t, 5> pages = {0xE0, 0xEF, 0xF0, 0xFE, 0xFF};
    for (const auto [model, page] : std::views::cartesian_product(models, pages)) {
        CAPTURE(model);
        CAPTURE(page);
        Gameboy gameboy(GameboySettings{.romName = std::string{kRomName}, .model = model, .noBootrom = true});
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        bus.memory_.wram_.fill(0x65);
        bus.WriteByte(OAM_DMA_TRANSFER_REG, page, ComponentSource::CPU);
        for (auto _ : std::views::indices(8))
            bus.UpdateDMA();
        REQUIRE(bus.dma_.currentByte == 1);
        CHECK(bus.gpu_.oam[0] == (model == Model::DMGB ? 0x65 : 0xFF));
        CHECK(bus.ReadByte(0x100, ComponentSource::CPU) == (model == Model::DMGB ? 0x65 : 0xFF));
    }
}

TEST_CASE("Reading the VRAM DMA bus clears the CGB OAM byte") {
    for (const auto model : {Model::DMGB, Model::CGBC}) {
        CAPTURE(model);
        Gameboy gameboy(GameboySettings{.romName = std::string{kRomName}, .model = model, .noBootrom = true});
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        bus.gpu_.vram[0] = 0x65;
        bus.gpu_.vram[1] = 0x76;
        bus.gpu_.oam.fill(0x87);
        bus.WriteByte(OAM_DMA_TRANSFER_REG, 0x80, ComponentSource::CPU);
        for (auto _ : std::views::indices(8))
            bus.UpdateDMA();
        REQUIRE(bus.dma_.currentByte == 1);
        const auto before = gameboy.SaveState();
        for (auto _ : std::views::indices(3))
            CHECK(gameboy.InspectCpuRead(0x9000) == 0x65);
        CHECK(gameboy.SaveState() == before);
        CHECK(bus.ReadByte(0x9000, ComponentSource::CPU) == 0x65);
        CHECK(bus.gpu_.oam[0] == (model == Model::DMGB ? 0x65 : 0));
        CHECK(bus.gpu_.oam[1] == 0x87);
        CHECK(bus.gpu_.vram[0] == 0x65);
        CHECK(gameboy.InspectCpuRead(0x9000) == (model == Model::DMGB ? 0x65 : 0));
        for (auto _ : std::views::indices(4))
            bus.UpdateDMA();
        CHECK(gameboy.InspectCpuRead(0x9000) == 0x76);
        CHECK(bus.gpu_.oam[1] == 0x76);
    }
}

TEST_CASE("OAM DMA retains source bus ownership through the last byte machine cycle") {
    for (const auto model : {Model::DMGB, Model::CGBC}) {
        CAPTURE(model);
        Gameboy gameboy({.romName = std::string{kRomName}, .model = model, .noBootrom = true});
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        bus.WriteByte(LCD_CONTROL_BIT, 0, ComponentSource::CPU);
        const auto romByte = gameboy.DebugPeek(0);
        bus.WriteByte(OAM_DMA_TRANSFER_REG, 0, ComponentSource::CPU);
        // TODO: Create RunFor(uz, lambda) utility
        for (auto _ : std::views::indices(644))
            bus.UpdateDMA();
        REQUIRE(bus.dma_.currentByte == 160);
        REQUIRE(bus.dma_.transferComplete);
        const auto lastByte = bus.gpu_.oam[159];
        CHECK(gameboy.InspectCpuRead(0) == lastByte);
        bus.WriteByte(0, 0x5A, ComponentSource::CPU);
        CHECK(bus.gpu_.oam[159] == 0x5A);
        for (auto _ : std::views::indices(3)) {
            bus.UpdateDMA();
            CHECK(gameboy.InspectCpuRead(0) == 0x5A);
            CHECK(gameboy.InspectCpuRead(0xFE9F) == 0xFF);
        }
        bus.UpdateDMA();
        CHECK_FALSE(bus.dma_.transferActive);
        CHECK(gameboy.InspectCpuRead(0) == romByte);
        CHECK(gameboy.InspectCpuRead(0xFE9F) == 0x5A);
    }
}
