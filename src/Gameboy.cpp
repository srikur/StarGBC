#include "Gameboy.h"
#include "Instructions.h"

#include <iterator>
#include <map>
#include <thread>
#include <chrono>
#include <bit>

static constexpr uint32_t kFrameCyclesDMG = 70224;
static constexpr uint32_t kFrameCyclesCGB = kFrameCyclesDMG * 2;

bool Gameboy::ShouldRender() const {
    // const bool value = bus->gpu_->vblank;
    // bus->gpu_->vblank = false;
    return true;
}

void Gameboy::Save() const {
    bus->cartridge_->Save();
}

void Gameboy::KeyUp(const Keys key) const {
    bus->KeyUp(key);
}

void Gameboy::KeyDown(const Keys key) const {
    bus->KeyDown(key);
}

uint32_t *Gameboy::GetScreenData() const {
    return bus->gpu_->screenData.data();
}

uint8_t Gameboy::DecodeInstruction(const uint8_t opcode, const bool prefixed) {
    // std::fprintf(stderr, "Running instruction %02X %s with mCycleCounter = %d, masterCycles = %d\n", opcode, prefixed ? "(prefixed)" : "", mCycleCounter, masterCycles);
    return prefixed
               ? instructions->prefixedInstr(opcode, *this)
               : instructions->nonPrefixedInstr(opcode, *this);
}

void Gameboy::InitializeBootrom() const {
    std::ifstream file(bios_path_, std::ios::binary);
    file.unsetf(std::ios::skipws);

    file.seekg(0, std::ios::end);
    const std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    bus->bootrom.reserve(fileSize);
    bus->bootrom.insert(bus->bootrom.begin(),
                        std::istream_iterator<uint8_t>(file),
                        std::istream_iterator<uint8_t>());
    file.close();
}

uint32_t Gameboy::RunHDMA() const {
    if (!bus->hdmaActive || bus->gpu_->hardware == GPU::Hardware::DMG) {
        return 0;
    }

    const bool doubleSpeed = (bus->speed == Bus::Speed::Double);
    const uint32_t cyclesPerBlock = doubleSpeed ? 16 : 8;

    switch (bus->gpu_->hdmaMode) {
        case GPU::HDMAMode::GDMA: {
            const uint32_t blocks = static_cast<uint32_t>(bus->hdmaRemain) + 1;
            for (uint32_t unused = 0; unused < blocks; ++unused) {
                const uint16_t memSource = bus->hdmaSource;
                for (uint16_t i = 0; i < 0x10; ++i) {
                    const uint8_t byte = bus->ReadByte(memSource + i);
                    bus->gpu_->WriteVRAM(bus->hdmaDestination + i, byte);
                }
                bus->hdmaSource += 0x10;
                bus->hdmaDestination += 0x10;
                bus->hdmaRemain = (bus->hdmaRemain == 0) ? 0x7F : static_cast<uint8_t>(bus->hdmaRemain - 1);
            }
            bus->hdmaActive = false;
            return blocks * cyclesPerBlock;
        }
        case GPU::HDMAMode::HDMA: {
            if (!bus->gpu_->hblank) return 0;

            const uint16_t memSource = bus->hdmaSource;
            for (uint16_t i = 0; i < 0x10; ++i) {
                const uint8_t byte = bus->ReadByte(memSource + i);
                bus->gpu_->WriteVRAM(bus->hdmaDestination + i, byte);
            }
            bus->hdmaSource += 0x10;
            bus->hdmaDestination += 0x10;
            bus->hdmaRemain = (bus->hdmaRemain == 0) ? 0x7F : static_cast<uint8_t>(bus->hdmaRemain - 1);
            if (bus->hdmaRemain == 0x7F) bus->hdmaActive = false;
            return cyclesPerBlock;
        }
        default: throw UnreachableCodeException("Gameboy::RunHDMA – invalid mode");
    }
}

void Gameboy::InitializeSystem() {
    regs->SetStartupValues(static_cast<Registers::Model>(mode_));
    sp = 0xFFFE;

    static const std::map<uint16_t, uint8_t> initialData = {
        {0xFF00, 0xCF}, {0xFF02, 0x7C}, {0xFF03, 0xFF}, {0xFF04, 0x1E}, {0xFF07, 0xF8}, {0xFF08, 0xFF}, {0xFF09, 0xFF},
        {0xFF0A, 0xFF}, {0xFF0B, 0xFF}, {0xFF0C, 0xFF}, {0xFF0D, 0xFF}, {0xFF0E, 0xFF}, {0xFF0F, 0xE1}, {0xFF10, 0x80},
        {0xFF11, 0xBF}, {0xFF12, 0xF3}, {0xFF13, 0xFF}, {0xFF14, 0xBF}, {0xFF15, 0xFF}, {0xFF16, 0x3F}, {0xFF18, 0xFF},
        {0xFF19, 0xBF}, {0xFF1A, 0x7F}, {0xFF1B, 0xFF}, {0xFF1C, 0x9F}, {0xFF1D, 0xFF}, {0xFF1E, 0xBF}, {0xFF1F, 0xFF},
        {0xFF20, 0xFF}, {0xFF23, 0xBF}, {0xFF24, 0x77}, {0xFF25, 0xF3}, {0xFF26, 0xF1}, {0xFF27, 0xFF}, {0xFF28, 0xFF},
        {0xFF29, 0xFF}, {0xFF2A, 0xFF}, {0xFF2B, 0xFF}, {0xFF2C, 0xFF}, {0xFF2D, 0xFF}, {0xFF2E, 0xFF}, {0xFF2F, 0xFF},
        {0xFF31, 0xFF}, {0xFF33, 0xFF}, {0xFF35, 0xFF}, {0xFF37, 0xFF}, {0xFF39, 0xFF}, {0xFF3B, 0xFF}, {0xFF3D, 0xFF},
        {0xFF3F, 0xFF}, {0xFF40, 0x91}, {0xFF41, 0x81}, {0xFF47, 0xFC}, {0xFF4C, 0xFF}, {0xFF4D, 0x7E}, {0xFF4E, 0xFF},
        {0xFF4F, 0xFE}, {0xFF50, 0xFF}, {0xFF51, 0xFF}, {0xFF52, 0xFF}, {0xFF53, 0xFF}, {0xFF54, 0xFF}, {0xFF55, 0xFF},
        {0xFF56, 0xFF}, {0xFF57, 0xFF}, {0xFF58, 0xFF}, {0xFF59, 0xFF}, {0xFF5A, 0xFF}, {0xFF5B, 0xFF}, {0xFF5C, 0xFF},
        {0xFF5D, 0xFF}, {0xFF5E, 0xFF}, {0xFF5F, 0xFF}, {0xFF60, 0xFF}, {0xFF61, 0xFF}, {0xFF62, 0xFF}, {0xFF63, 0xFF},
        {0xFF64, 0xFF}, {0xFF65, 0xFF}, {0xFF66, 0xFF}, {0xFF67, 0xFF}, {0xFF68, 0xC0}, {0xFF69, 0xFF}, {0xFF6A, 0xC1},
        {0xFF6B, 0x90}, {0xFF6C, 0xFE}, {0xFF6D, 0xFF}, {0xFF6E, 0xFF}, {0xFF6F, 0xFF}, {0xFF70, 0xF8}, {0xFF71, 0xFF},
        {0xFF75, 0x8F}, {0xFF78, 0xFF}, {0xFF79, 0xFF}, {0xFF7A, 0xFF}, {0xFF7B, 0xFF}, {0xFF7C, 0xFF}, {0xFF7D, 0xFF},
        {0xFF7E, 0xFF}, {0xFF7F, 0xFF},
    };

    for (const auto &[address, value]: initialData) {
        bus->WriteByte(address, value);
    }
}

void Gameboy::ToggleSpeed() {
    speedMultiplier = (speedMultiplier == 1) ? 4 : 1;
}

void Gameboy::SetThrottle(const bool throttle) {
    throttleSpeed = throttle;
}

void Gameboy::AdvanceFrame() {
    const uint32_t speedDivider = bus->speed == Bus::Speed::Regular ? 2 : 1;
    if (masterCycles == CGB_CYCLES_PER_SECOND) masterCycles = 0;
    if (masterCycles % speedDivider == 0) ExecuteMicroOp();
    if (masterCycles % speedDivider == 0) bus->UpdateTimers();
    if (masterCycles % RTC_CLOCK_DIVIDER == 0) bus->UpdateRTC();
    if (masterCycles % AUDIO_CLOCK_DIVIDER == 0) bus->audio_->Tick();
    if (masterCycles % speedDivider == 0) bus->UpdateSerial();
    if (masterCycles % speedDivider == 0) bus->UpdateDMA();
    if (masterCycles % GRAPHICS_CLOCK_DIVIDER == 0) bus->UpdateGraphics();
    // if (interruptState == InterruptState::M1 && instrComplete && tCycleCounter == 0) PrintCurrentValues();
    masterCycles++;
    // need to add hdma tick
}

void Gameboy::ExecuteMicroOp() {
    if ((++tCycleCounter % 4) == 0) {
        tCycleCounter = 0;
        if (!instrRunning && ProcessInterrupts()) return;
        if (halted) {
            return;
        }
        mCycleCounter++;
        if (bus->bootromRunning && pc == 0x100) {
            bus->bootromRunning = false;
        }
        instrRunning = true;
        if (DecodeInstruction(currentInstruction, prefixed)) {
            previousPC = pc - 1;
            instrComplete = true;
            previousPrefixed = prefixed;
            prefixed = (currentInstruction >> 8) == 0xCB;
            previousInstruction = currentInstruction;
            currentInstruction = nextInstruction;
            mCycleCounter = 1;
            if (haltBug) {
                haltBug = false;
                pc -= 1;
            }
            instructions->word2 = instructions->word = instructions->byte = 0;
            instructions->jumpCondition = false;
            instrRunning = false;
        } else instrComplete = false;
    }
}

void Gameboy::UpdateEmulator() {
    if (paused) {
        return;
    }

    using clock = std::chrono::steady_clock;
    static constexpr auto kFramePeriod = std::chrono::microseconds{16'667}; // ≈ 60 FPS (16.667 ms)
    const auto frameStart = clock::now();

    for (uint32_t i = 0; i < kFrameCyclesCGB; i++) {
        AdvanceFrame();
    }

    const auto elapsed = clock::now() - frameStart;
    if (const auto effectiveFrameTime = kFramePeriod / speedMultiplier; throttleSpeed && elapsed < effectiveFrameTime)
        std::this_thread::sleep_for(effectiveFrameTime - elapsed);
}

void Gameboy::PrintCurrentValues() {
    if ((previousInstruction & 0xFF00) >> 8 == 0xCB) return;
    if (previousPrefixed) previousInstruction = 0xCB00 | (previousInstruction & 0x00FF);
    const std::string text = std::format(
        "----------------------------------------------\n"
        "PC: {:#06x}  SP: {:#06x}  OP: {:#04x} — {}\n"
        "A:{:02X}  F: Z{} N{} H{} C{}\n"
        "B:{:02X}  C:{:02X}\n"
        "D:{:02X}  E:{:02X}\n"
        "H:{:02X}  L:{:02X}\n\n"
        "LCDC:{:02X} STAT:{:02X} LY:{:02X} CNT:{:02X}\n"
        "IE:{:02X} IF:{:02X} IME:{:02X}\n"
        "DIV:{:02X} TIMA:{:02X} TMA:{:02X} TAC:{:02X}"
        "\n----------------------------------------------\n",
        previousPC, sp, previousInstruction, instructions->GetMnemonic(previousPrefixed ? 0xCB00 | previousInstruction : previousInstruction),
        regs->a,
        regs->FlagZero() ? 1 : 0, regs->FlagSubtract() ? 1 : 0,
        regs->FlagHalf() ? 1 : 0, regs->FlagCarry() ? 1 : 0,
        regs->b, regs->c, regs->d, regs->e,
        regs->h, regs->l,
        bus->gpu_->lcdc, bus->gpu_->stat.value(), bus->gpu_->currentLine, bus->gpu_->scanlineCounter,
        bus->interruptEnable, bus->interruptFlag, bus->interruptMasterEnable ? 1 : 0,
        bus->timer_.divCounter, bus->timer_.tima, bus->timer_.tma, bus->timer_.tac);
    // const std::string text = std::format("{:#04x} - {}\n", currentInstruction, instructions->GetMnemonic(prefixed ? 0xCB00 | currentInstruction : currentInstruction));
    // print to file 'debug.txt' -- append
    // std::printf("%s", text.c_str());
    // std::fflush(stdout);
    // std::ofstream file("debug.txt", std::ios_base::app);
    // file << text;
    // file.close();
}

inline uint8_t interruptAddress(const uint8_t bit) {
    switch (bit) {
        case 0: return 0x40; // VBlank
        case 1: return 0x48; // LCD STAT
        case 2: return 0x50; // Timer
        case 3: return 0x58; // Serial
        case 4: return 0x60; // Joypad
        default: return 0;
    }
}

bool Gameboy::ProcessInterrupts() {
    using enum InterruptState;
    switch (interruptState) {
        case M1: {
            if (bus->interruptDelay && ++icount == 2) {
                bus->interruptDelay = false;
                bus->interruptMasterEnable = true;
                icount = 0;
            }
            const uint8_t pending = bus->interruptEnable & bus->interruptFlag & 0x1F;
            if (pending == 0) {
                return false;
            }
            if (halted && !bus->interruptMasterEnable) {
                halted = false;
                return false;
            }

            if (bus->interruptDelay || !bus->interruptMasterEnable) {
                return false;
            }

            interruptState = M2;
            halted = false;
            bus->interruptMasterEnable = false;

            interruptBit = static_cast<uint8_t>(std::countr_zero(pending));
            interruptMask = static_cast<uint8_t>(1u << interruptBit);

            pc -= 1;
            return true;
        }
        case M2: {
            sp -= 1;
            bus->WriteByte(sp, static_cast<uint8_t>(pc >> 8));
            interruptState = M3;
            return true;
        }
        case M3: {
            if (const uint8_t newPending = bus->interruptEnable & bus->interruptFlag & 0x1F; !(newPending & interruptMask)) {
                if (!newPending) {
                    sp--;
                    bus->WriteByte(sp, pc & 0xFF);
                    pc = 0x0000;
                    interruptState = M4;
                    return true;
                } else {
                    interruptBit = std::countr_zero(newPending);
                    interruptMask = 1u << interruptBit;
                }
            }

            sp -= 1;
            bus->WriteByte(sp, static_cast<uint8_t>(pc & 0xFF));
            bus->interruptFlag &= ~interruptMask;
            pc = interruptAddress(interruptBit);

            interruptState = M4;
            return true;
        }
        case M4: {
            interruptState = M5;
            return true;
        }
        case M5: {
            interruptState = M6;
            return true;
        }
        case M6: {
            previousPC = pc;
            prefixed = false;
            currentInstruction = bus->ReadByte(pc++);
            interruptState = M1;
            mCycleCounter = 1;
            return false;
        }
    }
    return true;
}

void Gameboy::SaveState(int slot) const {
    if (pc < 0x100) {
        std::printf("Error: Cannot save state while bootrom is running\n");
        return;
    }

    bus->cartridge_->Save();

    std::ofstream stateFile(std::format("{}.sgm{}", Cartridge::RemoveExtension(rom_path_), slot), std::ios::binary);
    if (!stateFile.is_open()) {
        std::printf("Error: Could not open state file for saving\n");
        return;
    }

    stateFile.write(reinterpret_cast<const char *>(&pc), sizeof(pc));
    stateFile.write(reinterpret_cast<const char *>(&sp), sizeof(sp));
    stateFile.write(reinterpret_cast<const char *>(&icount), sizeof(icount));
    stateFile.write(reinterpret_cast<const char *>(&halted), sizeof(halted));
    stateFile.write(reinterpret_cast<const char *>(&haltBug), sizeof(haltBug));
    stateFile.write(reinterpret_cast<const char *>(&currentInstruction), sizeof(currentInstruction));

    regs->SaveState(stateFile);
    bus->SaveState(stateFile);

    stateFile.close();
}

void Gameboy::LoadState(int slot) {
    if (pc < 0x100) {
        std::printf("Error: Cannot load state while bootrom is running\n");
        return;
    }

    bus->cartridge_->Save();
    paused = true;
    std::ifstream stateFile(std::format("{}.sgm{}", Cartridge::RemoveExtension(rom_path_), slot), std::ios::binary);
    if (!stateFile.is_open()) {
        std::printf("Error: Could not open state file for loading\n");
        return;
    }

    stateFile.read(reinterpret_cast<char *>(&pc), sizeof(pc));
    stateFile.read(reinterpret_cast<char *>(&sp), sizeof(sp));
    stateFile.read(reinterpret_cast<char *>(&icount), sizeof(icount));
    stateFile.read(reinterpret_cast<char *>(&halted), sizeof(halted));
    stateFile.read(reinterpret_cast<char *>(&haltBug), sizeof(haltBug));
    stateFile.read(reinterpret_cast<char *>(&currentInstruction), sizeof(currentInstruction));

    regs->LoadState(stateFile);
    bus->LoadState(stateFile);

    stateFile.close();
    paused = false;
}
