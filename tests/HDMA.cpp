#include <doctest/doctest.h>
#include "Gameboy.h"
#include "diagnostics/Trace.h"

namespace {
    template<class T>
    consteval std::meta::info dmaTestMember(const std::string_view name) {
        for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::identifier_of(member) == name)
                return member;
        throw std::out_of_range("Unknown DMA test member");
    }
} // namespace

TEST_CASE("GDMA preserves the opcode prefetched before overwriting VRAM") {
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    constexpr std::array speeds = {false, true, true};
    constexpr std::array phases = {0, 0, 1};
    for (const auto [policy, doubleSpeed, masterPhase] : std::views::cartesian_product(policies, speeds, phases)) {
        CAPTURE(policy);
        CAPTURE(doubleSpeed);
        CAPTURE(masterPhase);
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
        auto &bus = gameboy.[:dmaTestMember<Gameboy>("bus_"):];
        auto &cpu = gameboy.[:dmaTestMember<Gameboy>("cpu_"):];
        auto &registers = gameboy.[:dmaTestMember<Gameboy>("registers_"):];
        bus.WriteByte(0xFF40, 0, ComponentSource::CPU);
        if (doubleSpeed) {
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
            gameboy.[:dmaTestMember<Gameboy>("masterCycles"):] = masterPhase;
        }
        registers.a = registers.b = registers.c = 0;
        cpu.currentInstruction = 0;
        cpu.pc(0x8000);

        gameboy.DebugPoke(0x8000, 0xE0); // LDH ($55), A: copy C000..C00F to VRAM
        gameboy.DebugPoke(0x8001, 0x55);
        gameboy.DebugPoke(0x8002, 0x04); // INC B must already be prefetched when DMA starts
        gameboy.DebugPoke(0x8003, 0x76); // HALT
        for (uint16_t i = 0; i < 16; ++i)
            gameboy.DebugPoke(0xC000 + i, 0);
        gameboy.DebugPoke(0xC002, 0x0C); // INC C replaces INC B in memory, but must not execute
        gameboy.DebugPoke(0xC003, 0x76);
        for (const auto [address, value] : std::array<std::pair<uint16_t, uint8_t>, 4>{{{0xFF51, 0xC0}, {0xFF52, 0}, {0xFF53, 0}, {0xFF54, 0}}})
            bus.WriteByte(address, value, ComponentSource::CPU);

        static_cast<void>(gameboy.AdvanceTicks(200, policy));
        CHECK(gameboy.InspectCpu().halted);
        CHECK(gameboy.InspectCpu().bc == 0x0100);
        CHECK(gameboy.DebugPeek(0x8002) == 0x0C);
        CHECK(bus.ReadByte(0xFF55, ComponentSource::CPU) == 0xFF);
    }
}

TEST_CASE("Disabling the LCD during VBlank transfers one pending HDMA block") {
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    constexpr std::array speeds = {false, true};
    for (const auto [policy, doubleSpeed] : std::views::cartesian_product(policies, speeds)) {
        CAPTURE(policy);
        CAPTURE(doubleSpeed);
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
        auto &bus = gameboy.[:dmaTestMember<Gameboy>("bus_"):];
        gameboy.[:dmaTestMember<Gameboy>("cpu_"):].Lock();
        bus.WriteByte(LCD_CONTROL_BIT, 0, ComponentSource::CPU);
        bus.WriteByte(LCD_CONTROL_BIT, 0x91, ComponentSource::CPU);
        if (doubleSpeed) {
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
        }
        static_cast<void>(gameboy.AdvanceTicks(144 * 456 * 2 + 16, policy));
        REQUIRE(bus.gpu_.vblank);
        for (uint16_t i = 0; i < 32; ++i) {
            gameboy.DebugPoke(0xC000 + i, i + 1);
            gameboy.DebugPoke(0x8000 + i, 0);
        }
        for (const auto [address, value] :
             std::array<std::pair<uint16_t, uint8_t>, 5>{{{0xFF51, 0xC0}, {0xFF52, 0}, {0xFF53, 0}, {0xFF54, 0}, {0xFF55, 0x81}}})
            bus.WriteByte(address, value, ComponentSource::CPU);
        static_cast<void>(gameboy.AdvanceTicks(40, policy));
        CHECK(gameboy.DebugPeek(0x8000) == 0);
        CHECK(bus.ReadByte(0xFF55, ComponentSource::CPU) == 1);

        bus.WriteByte(0xFF40, 0, ComponentSource::CPU);
        static_cast<void>(gameboy.AdvanceTicks(200, policy));
        for (uint16_t i = 0; i < 32; ++i)
            CHECK(gameboy.DebugPeek(0x8000 + i) == (i < 16 ? i + 1 : 0));
        CHECK(bus.ReadByte(0xFF55, ComponentSource::CPU) == 0);

        // Another LCDC write with the display still off does not request the remaining block
        bus.WriteByte(0xFF40, 1, ComponentSource::CPU);
        static_cast<void>(gameboy.AdvanceTicks(2000, policy));
        CHECK(gameboy.DebugPeek(0x8010) == 0);
        CHECK(bus.ReadByte(0xFF55, ComponentSource::CPU) == 0);

        // transfer resumes at HBlank after the display is enabled
        bus.WriteByte(0xFF40, 0x91, ComponentSource::CPU);
        static_cast<void>(gameboy.AdvanceTicks(1000, policy));
        for (uint16_t i = 0; i < 32; ++i)
            CHECK(gameboy.DebugPeek(0x8000 + i) == i + 1);
        CHECK(bus.ReadByte(0xFF55, ComponentSource::CPU) == 0xFF);
    }
}

TEST_CASE("An HDMA block started at the end of HBlank completes in the next scanline") {
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    constexpr std::array speeds = {false, true};
    for (const auto [policy, doubleSpeed] : std::views::cartesian_product(policies, speeds)) {
        CAPTURE(policy);
        CAPTURE(doubleSpeed);
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
        auto &bus = gameboy.[:dmaTestMember<Gameboy>("bus_"):];
        gameboy.[:dmaTestMember<Gameboy>("cpu_"):].Lock();
        bus.WriteByte(LCD_CONTROL_BIT, 0, ComponentSource::CPU);
        bus.WriteByte(LCD_CONTROL_BIT, 0x91, ComponentSource::CPU);
        if (doubleSpeed) {
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
        }
        for (auto i{0_u16}; i < 32; ++i) {
            gameboy.DebugPoke(0xC000 + i, i + 1);
            gameboy.DebugPoke(0x8000 + i, 0);
        }

        // Start on a steady scanline, after the special LCD-enable line.
        static_cast<void>(gameboy.AdvanceTicks(456 * 2, policy));
        for (auto dot{0u}; dot < 456 && !(bus.gpu_.currentLine == 1 && bus.gpu_.scanlineCounter == 452); ++dot)
            static_cast<void>(gameboy.AdvanceTicks(2, policy));
        REQUIRE(bus.gpu_.currentLine == 1);
        REQUIRE(bus.gpu_.scanlineCounter == 452);
        REQUIRE(bus.gpu_.hblank);
        for (const auto [address, value] :
             std::array<std::pair<uint16_t, uint8_t>, 5>{{{0xFF51, 0xC0}, {0xFF52, 0}, {0xFF53, 0}, {0xFF54, 0}, {0xFF55, 0x81}}})
            bus.WriteByte(address, value, ComponentSource::CPU);

        static_cast<void>(gameboy.AdvanceTicks(128, policy));
        REQUIRE(bus.gpu_.currentLine == 2);
        REQUIRE(bus.gpu_.stat.mode == GPUMode::MODE_2);
        for (auto i{0_u16}; i < 32; ++i)
            CHECK(gameboy.DebugPeek(0x8000 + i) == (i < 16 ? i + 1 : 0));
        CHECK(bus.ReadByte(0xFF55, ComponentSource::CPU) == 0);
        CHECK_FALSE(bus.gpu_.hdma.ShouldHaltCPU());

        // The next block still needs a separate HBlank request.
        static_cast<void>(gameboy.AdvanceTicks(456 * 2, policy));
        for (auto i{0_u16}; i < 32; ++i)
            CHECK(gameboy.DebugPeek(0x8000 + i) == i + 1);
        CHECK(bus.ReadByte(0xFF55, ComponentSource::CPU) == 0xFF);
    }
}

TEST_CASE("An accepted HDMA request survives cancellation and waits for the instruction boundary") {
    constexpr std::array speeds = {false, true};
    for (const auto double_speed : speeds) {
        CAPTURE(double_speed);
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
        auto &bus = gameboy.[:dmaTestMember<Gameboy>("bus_"):];
        bus.WriteByte(LCD_CONTROL_BIT, 0, ComponentSource::CPU);
        if (double_speed) {
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
        }
        for (auto offset{0_u16}; offset < 32; ++offset) {
            gameboy.DebugPoke(WRAM_BEGIN + offset, offset + 1);
            gameboy.DebugPoke(VRAM_BEGIN + offset, 0);
        }
        for (const auto [address, value] :
             std::array<std::pair<uint16_t, uint8_t>, 5>{{{HDMA_SOURCE_HIGH, 0xC0}, {HDMA_SOURCE_LOW, 0}, {HDMA_DESTINATION_HIGH, 0}, {HDMA_DESTINATION_LOW, 0}, {HDMA_CONTROL, 0x81}}})
            bus.WriteByte(address, value, ComponentSource::CPU);
        for (auto dot{0u}; dot < 8; ++dot)
            bus.RunHDMA(false);
        REQUIRE(bus.gpu_.hdma.block_requested);
        CHECK_FALSE(bus.gpu_.hdma.ShouldHaltCPU());
        CHECK(gameboy.DebugPeek(VRAM_BEGIN) == 0);
        bus.WriteByte(HDMA_CONTROL, 0, ComponentSource::CPU);
        REQUIRE_FALSE(bus.gpu_.hdma.hdmaActive);
        CHECK(bus.ReadByte(HDMA_CONTROL, ComponentSource::CPU) == 0x80);
        for (auto dot{0u}; dot < 32; ++dot)
            bus.RunHDMA(true);
        for (auto offset{0_u16}; offset < 32; ++offset)
            CHECK(gameboy.DebugPeek(VRAM_BEGIN + offset) == (offset < 16 ? offset + 1 : 0));
        CHECK(bus.ReadByte(HDMA_CONTROL, ComponentSource::CPU) == 0xFF);
        REQUIRE(bus.gpu_.hdma.ShouldHaltCPU());
        for (auto dot{0u}; dot < (double_speed ? 2u : 4u); ++dot) {
            bus.RunHDMA(true);
            CHECK(bus.gpu_.hdma.ShouldHaltCPU());
        }
        bus.RunHDMA(true);
        CHECK_FALSE(bus.gpu_.hdma.ShouldHaltCPU());
    }
}

TEST_CASE("HALT suspends HBlank DMA until the CPU wakes") {
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    constexpr std::array speeds = {false, true};
    for (const auto [policy, double_speed] : std::views::cartesian_product(policies, speeds)) {
        CAPTURE(policy);
        CAPTURE(double_speed);
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
        auto &bus = gameboy.[:dmaTestMember<Gameboy>("bus_"):];
        auto &cpu = gameboy.[:dmaTestMember<Gameboy>("cpu_"):];
        auto &registers = gameboy.[:dmaTestMember<Gameboy>("registers_"):];
        auto &interrupts = gameboy.[:dmaTestMember<Gameboy>("interrupts_"):];
        bus.WriteByte(LCD_CONTROL_BIT, 0, ComponentSource::CPU);
        if (double_speed) {
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
        }
        registers.a = 0;
        cpu.currentInstruction = 0x76;
        cpu.pc(0xC101);
        gameboy.DebugPoke(0xC101, 0x3C);
        gameboy.DebugPoke(0xC102, 0x18);
        gameboy.DebugPoke(0xC103, 0xFE);
        for (auto offset{0_u16}; offset < 16; ++offset) {
            gameboy.DebugPoke(WRAM_BEGIN + offset, offset + 1);
            gameboy.DebugPoke(VRAM_BEGIN + offset, 0);
        }
        bus.WriteByte(LCD_CONTROL_BIT, 0x91, ComponentSource::CPU);
        for (const auto [address, value] :
             std::array<std::pair<uint16_t, uint8_t>, 5>{{{HDMA_SOURCE_HIGH, 0xC0}, {HDMA_SOURCE_LOW, 0}, {HDMA_DESTINATION_HIGH, 0}, {HDMA_DESTINATION_LOW, 0}, {HDMA_CONTROL, 0x80}}})
            bus.WriteByte(address, value, ComponentSource::CPU);
        interrupts.interrupt_enable = 1;
        interrupts.interrupt_flag = 0;
        interrupts.interrupt_master_enable = false;
        static_cast<void>(gameboy.AdvanceTicks(3000, policy));
        REQUIRE(cpu.IsHalted());
        CHECK(registers.a == 0);
        CHECK(bus.ReadByte(HDMA_CONTROL, ComponentSource::CPU) == 0);
        for (auto offset{0_u16}; offset < 16; ++offset)
            CHECK(gameboy.DebugPeek(VRAM_BEGIN + offset) == 0);
        interrupts.set(InterruptType::VBlank, false);
        static_cast<void>(gameboy.AdvanceTicks(1000, policy));
        CHECK_FALSE(cpu.IsHalted());
        CHECK(registers.a == 1);
        CHECK(bus.ReadByte(HDMA_CONTROL, ComponentSource::CPU) == 0xFF);
        for (auto offset{0_u16}; offset < 16; ++offset)
            CHECK(gameboy.DebugPeek(VRAM_BEGIN + offset) == offset + 1);
    }
}

TEST_CASE("dma: VRAM address wrapping retains the selected bank and full destination counter") {
    constexpr std::array policies = {ExecutionPolicy::Ordinary, ExecutionPolicy::Optimized};
    constexpr std::array speeds = {false, true};
    constexpr std::array modes = {HDMAMode::GDMA, HDMAMode::HDMA};
    constexpr std::array<uint8_t, 2> bank_values = {0, 1};
    constexpr std::array<uint16_t, 4> start_values = {0x1FF0, 0x9FF0, 0xDFF0, 0xFFF0};
    for (const auto [policy, double_speed, mode, bank, start] : std::views::cartesian_product(policies, speeds, modes, bank_values, start_values)) {
        CAPTURE(policy);
        CAPTURE(double_speed);
        CAPTURE(mode);
        CAPTURE(bank);
        CAPTURE(start);
        const unsigned length = start == 0xFFF0 ? 16 : 32;
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
        auto &bus = gameboy.[:dmaTestMember<Gameboy>("bus_"):];
        gameboy.[:dmaTestMember<Gameboy>("cpu_"):].Lock();
        bus.WriteByte(LCD_CONTROL_BIT, 0, ComponentSource::CPU);
        if (double_speed) {
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
        }
        bus.WriteByte(VRAM_BANK, bank, ComponentSource::CPU);
        bus.gpu_.vram.fill(0xA5);
        for (auto i{0u}; i < 32; ++i)
            gameboy.DebugPoke(WRAM_BEGIN + i, i + 1);
        if (mode == HDMAMode::HDMA)
            bus.WriteByte(LCD_CONTROL_BIT, 0x91, ComponentSource::CPU);
        diagnostics::TraceRecorder recorder(64, {
            .components = uint64_t{1} << static_cast<unsigned>(diagnostics::Component::DMA),
            .kinds = uint64_t{1} << static_cast<unsigned>(diagnostics::EventKind::DmaTransfer)
        });
        gameboy.ObserveWith(&recorder.Context());
        for (const auto [address, value] : std::array<std::pair<uint16_t, uint8_t>, 5>{{
                 {HDMA_SOURCE_HIGH, 0xC0}, {HDMA_SOURCE_LOW, 0}, {HDMA_DESTINATION_HIGH, static_cast<uint8_t>(start >> 8)}, {HDMA_DESTINATION_LOW, 0xF0},
                 {HDMA_CONTROL, mode == HDMAMode::HDMA ? 0x81 : 0x01}}})
            bus.WriteByte(address, value, ComponentSource::CPU);
        static_cast<void>(gameboy.AdvanceTicks(3 * 912, policy));
        REQUIRE_FALSE(bus.gpu_.hdma.hdmaActive);
        CHECK_FALSE(bus.gpu_.hdma.ShouldHaltCPU());
        CHECK(bus.gpu_.hdma.hdmaSource == WRAM_BEGIN + length);
        CHECK(bus.gpu_.hdma.hdmaDestination == static_cast<uint16_t>(start + length));
        CHECK(gameboy.InspectCpuRead(HDMA_CONTROL) == (length == 16 ? 0x80 : 0xFF));
        const auto events = recorder.Events();
        REQUIRE(events.size() == length);
        for (auto i{0u}; i < length; ++i) {
            const uint16_t address = i < 16 ? 0x9FF0 + i : VRAM_BEGIN + i - 16;
            CAPTURE(i);
            REQUIRE(gameboy.DebugPeek(address) == i + 1);
            CHECK(events[i].address == address);
            CHECK(events[i].bank == bank);
            CHECK(events[i].detail == WRAM_BEGIN + i);
            CHECK(events[i].value == i + 1);
        }
        if (length == 16)
            CHECK(gameboy.DebugPeek(VRAM_BEGIN) == 0xA5);
        bus.WriteByte(VRAM_BANK, bank ^ 1, ComponentSource::CPU);
        for (auto address{VRAM_BEGIN}; address < 0xA000; ++address)
            REQUIRE(gameboy.DebugPeek(address) == 0xA5);
        gameboy.ObserveWith(nullptr);
    }
}
