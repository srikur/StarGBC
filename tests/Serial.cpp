#include "Serial.h"
#include <doctest/doctest.h>
#include "Audio.h"
#include "Gameboy.h"
#include "TestUtilities.h"
#include "Timer.h"

TEST_CASE("Internal serial clocks use divider edges at both serial rates") {
    for (const bool fast : {false, true}) {
        CAPTURE(fast);
        Interrupts interrupts;
        Serial serial(interrupts);
        serial.write_serial(SERIAL_TRANSFER_DATA, 0, true);
        serial.write_serial(SERIAL_TRANSFER_CONTROL, fast ? 0x83 : 0x81, true);
        const unsigned period = fast ? 16 : 512;
        for (auto tick{1_u32}; tick < period * 8; ++tick) {
            serial.update(tick);
            if (tick % period == 0)
                CHECK(serial.read_serial(SERIAL_TRANSFER_DATA) == (1u << (tick / period)) - 1);
            CHECK_FALSE(interrupts.is_set(InterruptType::Serial));
        }
        serial.update(period * 8);
        CHECK(serial.read_serial(SERIAL_TRANSFER_DATA) == 0xFF);
        CHECK((serial.read_serial(SERIAL_TRANSFER_CONTROL) & 0x80) == 0);
        CHECK(interrupts.is_set(InterruptType::Serial));
    }
}

TEST_CASE("DIV reset advances the serial clock only when its selected divider bit falls") {
    Interrupts interrupts;
    Audio audio(true);
    Timer timer(audio, interrupts);
    Serial serial(interrupts);
    timer.serial = &serial;
    serial.write_serial(SERIAL_TRANSFER_DATA, 0, true);
    serial.write_serial(SERIAL_TRANSFER_CONTROL, 0x83, true);
    timer.divCounter = 4;
    timer.WriteDIV(false);
    CHECK(serial.read_serial(SERIAL_TRANSFER_DATA) == 0);
    timer.WriteDIV(false);
    CHECK(serial.read_serial(SERIAL_TRANSFER_DATA) == 0);
    timer.divCounter = 4;
    timer.WriteDIV(false);
    CHECK(serial.read_serial(SERIAL_TRANSFER_DATA) == 1);
}

TEST_CASE("Clearing serial start or selecting an external clock cancels internal shifting") {
    for (const uint8_t control : {0x01, 0x80}) {
        CAPTURE(control);
        Interrupts interrupts;
        Serial serial(interrupts);
        serial.write_serial(SERIAL_TRANSFER_DATA, 0, true);
        serial.write_serial(SERIAL_TRANSFER_CONTROL, 0x83, true);
        for (auto tick{1_u16}; tick <= 16; ++tick)
            serial.update(tick);
        REQUIRE(serial.read_serial(SERIAL_TRANSFER_DATA) == 1);
        serial.write_serial(SERIAL_TRANSFER_CONTROL, control, true);
        for (auto tick{17_u16}; tick <= 8192; ++tick)
            serial.update(tick);
        CHECK(serial.read_serial(SERIAL_TRANSFER_DATA) == 1);
        CHECK_FALSE(interrupts.is_set(InterruptType::Serial));
    }
}

TEST_CASE("Serial clock phase survives restoring a state during a transfer") {
    for (const bool fast : {false, true}) {
        CAPTURE(fast);
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
        auto &bus = gameboy.[:get_member<Gameboy>("bus_"):];
        gameboy.[:get_member<Gameboy>("cpu_"):].Lock();
        bus.WriteByte(0xFF04, 0, ComponentSource::CPU);
        bus.WriteByte(SERIAL_TRANSFER_DATA, 0, ComponentSource::CPU);
        bus.WriteByte(SERIAL_TRANSFER_CONTROL, fast ? 0x83 : 0x81, ComponentSource::CPU);
        const unsigned period = fast ? 16 : 512;
        static_cast<void>(gameboy.AdvanceTicks(period * 3));
        CHECK(gameboy.InspectCpuRead(SERIAL_TRANSFER_DATA) == 1);
        const auto checkpoint = gameboy.SaveState();
        static_cast<void>(gameboy.AdvanceTicks(period * 13));
        CHECK(gameboy.InspectCpuRead(SERIAL_TRANSFER_DATA) == 0xFF);
        CHECK((gameboy.InspectCpuRead(SERIAL_TRANSFER_CONTROL) & 0x80) == 0);
        const auto completed = gameboy.SaveState();
        REQUIRE(gameboy.LoadState(checkpoint));
        static_cast<void>(gameboy.AdvanceTicks(period * 13));
        CHECK(gameboy.SaveState() == completed);
    }
}
