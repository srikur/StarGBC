#include "Gameboy.h"
#include <doctest/doctest.h>

namespace {
template<class T>
consteval std::meta::info member(const std::string_view name) {
    for (const auto m : std::meta::members_of(^^T, std::meta::access_context::unchecked())) {
        if (std::meta::has_identifier(m) && std::meta::identifier_of(m) == name) return m;
    }
    throw "Test member not found";
}

constexpr auto busMember = member<Gameboy>("bus_");
constexpr auto regsMember = member<Gameboy>("registers_");

GameboySettings settings(const Model model, const bool color = false) {
    return {
        .romName = color ? "roms/acid/cgb-acid2.gbc" : "roms/mooneye/acceptance/boot_regs-dmgABC.gb",
        .model = model,
        .noBootrom = true,
    };
}
}

TEST_CASE("models: configuration resolves a concrete revision") {
    for (unsigned i = 0; i < ModelNames.size(); ++i) {
        const auto model = static_cast<Model>(i);
        CHECK(ParseModel(ModelName(model)) == model);
    }
    CHECK(ParseModel("dmg") == Model::DMGB);
    CHECK(ParseModel("cgb") == Model::CGBE);
    CHECK(ParseModel("agb") == Model::AGBA);
    CHECK(ParseModel("ags") == Model::AGBB);
    CHECK_FALSE(ParseModel("cgbf").has_value());
    CHECK_FALSE(ParseModel("").has_value());
    Gameboy monochrome(settings(Model::Auto));
    Gameboy color(settings(Model::Auto, true));
    CHECK(monochrome.GetModel() == Model::DMGB);
    CHECK(color.GetModel() == Model::CGBE);
    CHECK_THROWS_AS(Gameboy(settings(static_cast<Model>(255))), std::invalid_argument);
}

TEST_CASE("cpu: illegal opcodes lock execution while peripherals keep running") {
    for (const uint8_t opcode : {0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD}) {
        CAPTURE(opcode);
        Gameboy gameboy(settings(Model::DMGB));
        auto &cpu = gameboy.[:member<Gameboy>("cpu_"):];
        auto &bus = gameboy.[:busMember:];
        auto &interrupts = gameboy.[:member<Gameboy>("interrupts_"):];
        bus.WriteByte(0xC000, opcode, ComponentSource::CPU);
        cpu.pc(0xC001);
        cpu.currentInstruction = opcode;
        gameboy.RunFrame();
        REQUIRE(cpu.locked());
        CHECK(cpu.pc() == 0xC001);

        interrupts.interruptEnable = interrupts.interruptFlag = 0x1F;
        interrupts.interruptMasterEnable = true;
        const auto div = bus.timer_.divCounter;
        gameboy.RunFrame();
        CHECK(cpu.pc() == 0xC001);
        CHECK(bus.timer_.divCounter != div);
        const auto state = gameboy.SaveState();
        REQUIRE(gameboy.LoadState(state));
        CHECK(cpu.locked());
    }
}

TEST_CASE("models: cartridge compatibility preserves the silicon revision") {
    for (const auto model : {Model::CGB0, Model::CGBB, Model::CGBD, Model::CGBE, Model::AGB0, Model::AGBBE}) {
        CAPTURE(ModelName(model));
        Gameboy dmgCart(settings(model));
        Gameboy cgbCart(settings(model, true));
        CHECK(dmgCart.GetModel() == model);
        CHECK(cgbCart.GetModel() == model);
        CHECK_FALSE(dmgCart.IsInCgbMode());
        CHECK(cgbCart.IsInCgbMode());
        CHECK(dmgCart.[:busMember:].gpu_.dmgCompat);
        CHECK_FALSE(cgbCart.[:busMember:].gpu_.dmgCompat);
        CHECK(dmgCart.[:regsMember:].b == (IsAgb(model) ? 1 : 0));
        CHECK(cgbCart.[:regsMember:].b == (IsAgb(model) ? 1 : 0));
    }
    Gameboy dmgHardware(settings(Model::DMGB, true));
    CHECK_FALSE(dmgHardware.IsInCgbMode());
}

TEST_CASE("models: unused OAM follows revision-specific address decoding") {
    for (const auto model : {Model::DMGB, Model::MGB, Model::CGB0, Model::CGBB,
                             Model::CGBC, Model::CGBD, Model::CGBE, Model::AGBA}) {
        CAPTURE(ModelName(model));
        Gameboy gameboy(settings(model)); // Includes CGB hardware in DMG compatibility mode.
        auto &bus = gameboy.[:busMember:];
        bus.WriteByte(0xFF40, 0, ComponentSource::CPU);
        const auto read = [&](uint16_t address) { return bus.ReadByte(address, ComponentSource::CPU); };
        const auto write = [&](uint16_t address, uint8_t value) { bus.WriteByte(address, value, ComponentSource::CPU); };
        write(0xFEA0, 0x12);
        write(0xFEB8, 0x34);
        write(0xFEC0, 0x56);
        if (IsDmg(model)) {
            CHECK(read(0xFEA0) == 0);
            CHECK(read(0xFEB8) == 0);
        } else if (model == Model::CGBE || IsAgb(model)) {
            CHECK(read(0xFEA0) == 0xAA);
            CHECK(read(0xFEB8) == 0xBB);
            CHECK(read(0xFEF3) == 0xFF);
        } else if (model == Model::CGBD) {
            CHECK(read(0xFEA0) == 0x12);
            CHECK(read(0xFEB8) == 0x34);
            CHECK(read(0xFEF0) == 0x56);
            write(0xFED0, 0x78);
            CHECK(read(0xFEC0) == 0x78);
        } else {
            CHECK(read(0xFEA0) == 0x34);
            CHECK(read(0xFEA8) == 0x34);
            CHECK(read(0xFED8) == 0x56);
        }
        bus.gpu_.lcdc = 0x80;
        bus.gpu_.stat.mode = GPUMode::MODE_3;
        CHECK(read(0xFEA0) == 0xFF);
        write(0xFEA0, 0x99);
        bus.gpu_.stat.mode = GPUMode::MODE_0;
        CHECK(read(0xFEA0) != 0x99);
        bus.dma_.transferActive = true;
        bus.dma_.ticks = DMA::STARTUP_CYCLES + 1;
        CHECK(read(0xFEA0) == 0xFF);
    }
}

TEST_CASE("models: CGB-D latches the background row across both bitplanes") {
    for (const auto model : {Model::DMGB, Model::CGB0, Model::CGBB, Model::CGBC,
                             Model::CGBD, Model::CGBE, Model::AGBA}) {
        CAPTURE(ModelName(model));
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.model = model;
        gpu.lcdc = 0x91;
        gpu.vram[0] = 0x55; // row 0, low plane
        gpu.vram[1] = 0xAA; // row 0, high plane
        gpu.vram[2] = 0x33; // row 1, low plane
        gpu.vram[3] = 0xCC; // row 1, high plane
        gpu.TickMode3(); // tile number and (on late CGB) row
        gpu.scrollY = 1;
        gpu.TickMode3(); // wait
        gpu.TickMode3(); // low bitplane
        gpu.scrollY = 0;
        gpu.TickMode3(); // wait
        gpu.TickMode3(); // high bitplane
        const bool latched = model == Model::CGBD || model == Model::CGBE || model == Model::AGBA;
        CHECK(gpu.fetcherTileDataLow_ == (latched ? 0x55 : 0x33));
        CHECK(gpu.fetcherTileDataHigh_ == 0xAA);
    }
}

TEST_CASE("models: early CGB length writes differ from CGB-C and later") {
    for (const auto model : {Model::DMGB, Model::CGB0, Model::CGBA, Model::CGBB,
                             Model::CGBC, Model::CGBD, Model::CGBE, Model::AGBA}) {
        CAPTURE(ModelName(model));
        Audio audio;
        audio.SetModel(model);
        audio.WriteByte(0xFF26, 0x80, true); // Odd DIV-APU step.
        audio.ch1.lengthTimer.lengthTimer = 62;
        audio.ch2.lengthTimer.lengthTimer = 62;
        audio.ch4.lengthTimer.lengthTimer = 62;
        for (const uint16_t address : {0xFF14, 0xFF19, 0xFF23}) audio.WriteByte(address, 0, false);
        const bool early = model == Model::CGB0 || model == Model::CGBA || model == Model::CGBB;
        CHECK(audio.ch1.lengthTimer.lengthTimer == (early ? 63 : 62));
        CHECK(audio.ch2.lengthTimer.lengthTimer == (early ? 63 : 62));
        CHECK(audio.ch4.lengthTimer.lengthTimer == (early ? 63 : 62));
        // Enabling length still clocks it once on every model.
        audio.WriteByte(0xFF14, 0x40, false);
        CHECK(audio.ch1.lengthTimer.lengthTimer == (early ? 64 : 63));
    }
}

TEST_CASE("models: save states retain extra OAM and reject a different revision") {
    Gameboy early(settings(Model::CGBB));
    Gameboy late(settings(Model::CGBE));
    auto &bus = early.[:busMember:];
    bus.WriteByte(0xFF40, 0, ComponentSource::CPU);
    bus.WriteByte(0xFEA0, 0x65, ComponentSource::CPU);
    const auto state = early.SaveState();
    bus.WriteByte(0xFEA0, 0x12, ComponentSource::CPU);
    REQUIRE(early.LoadState(state));
    CHECK(bus.ReadByte(0xFEB8, ComponentSource::CPU) == 0x65);
    const auto lateBefore = late.SaveState();
    CHECK_FALSE(late.LoadState(state));
    CHECK(late.GetModel() == Model::CGBE);
    CHECK(late.SaveState() == lateBefore);
}

TEST_CASE("models: early CGB envelope writes pass through an intermediate value") {
    for (const auto model : {Model::CGB0, Model::CGBB, Model::CGBC, Model::CGBD, Model::CGBE, Model::AGBA}) {
        CAPTURE(ModelName(model));
        Audio audio;
        audio.SetModel(model);
        audio.WriteByte(0xFF26, 0x80, false);
        const auto prepare = [](auto &channel) {
            channel.enabled = true;
            channel.envelope.Write(0x20);
            channel.envelope.currentVolume = 2;
        };
        prepare(audio.ch1);
        prepare(audio.ch2);
        prepare(audio.ch4);
        for (const uint16_t address : {0xFF12, 0xFF17, 0xFF21}) audio.WriteByte(address, 0x20, false);
        const bool early = model == Model::CGB0 || model == Model::CGBB || model == Model::CGBC;
        CHECK(audio.ch1.envelope.currentVolume == (early ? 3 : 2));
        CHECK(audio.ch2.envelope.currentVolume == (early ? 3 : 2));
        CHECK(audio.ch4.envelope.currentVolume == (early ? 3 : 2));
    }
}
