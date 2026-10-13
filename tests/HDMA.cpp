#include <doctest/doctest.h>
#include "Gameboy.h"

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
