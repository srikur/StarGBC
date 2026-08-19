#include "Gameboy.h"

#include <fstream>

static constexpr uint32_t kFrameCyclesCGB = Gameboy::FRAME_CYCLES_DMG * 2;

bool Gameboy::ConsumeFrame() {
    return std::exchange(gpu_.frameReady, false);
}

void Gameboy::Save() const {
    cartridge_.Save();
}

void Gameboy::KeyUp(const Keys key) {
    joypad_.KeyUp(key);
}

void Gameboy::KeyDown(const Keys key) {
    joypad_.KeyDown(key);
}

const uint32_t *Gameboy::GetScreenData() const {
    return gpu_.GetScreenData();
}

bool Gameboy::LoadedStateValid() const {
    return gpu_.backgroundQueue.valid()
           && gpu_.spriteFetchQueue.valid()
           && gpu_.spriteBuffer.valid()
           && gpu_.vramBank <= 1
           && gpu_.currentLine <= 153
           && gpu_.pixelsDrawn <= SCREEN_WIDTH
           && memory_.wramBank_ <= 7
           && cartridge_.BankingStateValid()
           && (!bus_.bootromRunning || !bus_.bootrom.empty());
}

uint32_t Gameboy::AdvanceCycles(const uint32_t maxCycles) {
    if (masterCycles == CGB_CYCLES_PER_SECOND) masterCycles = 0;
    if (cpu_.stopped()) {
        if (bus_.joypad_.KeyPressed()) {
            cpu_.stopped() = false;
        } else {
            masterCycles++;
            return 1;
        }
    }
    if (bus_.speed == Speed::Regular) {
        if (masterCycles % 2 != 0) {
            masterCycles++;
            return 1;
        }
        timer_.Tick(bus_.speed);
        rtc_.Update();
        audio_.Tick();
        serial_.Update();
        bus_.UpdateDMA();
        gpu_.Update();
        bus_.RunHDMA();
        if ((++cpuTickPhase_ & 3) == 0) {
            cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU());
        }
        const uint32_t consumed = bus_.speed == Speed::Regular && !cpu_.stopped() && maxCycles >= 2 ? 2 : 1;
        masterCycles += consumed;
        return consumed;
    }
    const bool evenCycle = masterCycles % 2 == 0;
    timer_.Tick(bus_.speed);
    if (evenCycle) {
        rtc_.Update();
        audio_.Tick();
    }
    serial_.Update();
    bus_.UpdateDMA();
    if (evenCycle) {
        gpu_.Update();
        bus_.RunHDMA();
    }
    if ((++cpuTickPhase_ & 3) == 0) {
        cpu_.ExecuteMicroOp(instructions_, gpu_.hdma.ShouldHaltCPU());
    }
    masterCycles++;
    return 1;
}

void Gameboy::RunFrame() {
    uint32_t remaining = kFrameCyclesCGB;
    while (remaining > 0) {
        remaining -= AdvanceCycles(remaining);
    }
}
