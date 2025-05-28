#include "Gameboy.h"
#include "Instructions.h"

#include <iterator>
#include <map>
#include <thread>
#include <chrono>
#include <bit>

static constexpr uint32_t kFrameCyclesDMG = 70224;

inline uint16_t Gameboy::ReadNextWord() const {
    const uint16_t lower = bus->ReadByte(pc + 1);
    const uint16_t higher = bus->ReadByte(pc + 2);
    return static_cast<uint16_t>(higher << 8 | lower);
}

inline uint8_t Gameboy::ReadNextByte() const {
    return bus->ReadByte(pc + 1);
}

bool Gameboy::CheckVBlank() const {
    const bool value = bus->gpu_->vblank;
    bus->gpu_->vblank = false;
    return value;
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

std::tuple<uint32_t, uint32_t, uint32_t> Gameboy::GetPixel(const uint32_t y, const uint32_t x) const {
    return {
        bus->gpu_->screenData[y][x][0],
        bus->gpu_->screenData[y][x][1],
        bus->gpu_->screenData[y][x][2]
    };
}

uint8_t Gameboy::DecodeInstruction(const uint8_t opcode, const bool prefixed) {
    return prefixed
               ? instructions->prefixedInstr(opcode, *this)
               : instructions->nonPrefixedInstr(opcode, *this);
}

void Gameboy::InitializeBootrom() {
    if (!bus->runBootrom) {
        pc = 0x100;
        InitializeSystem();
        return;
    }
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
    if (!bus->hdmaActive) {
        return 0;
    }

    const bool doubleSpeed = (bus->speed == Bus::Speed::Double);
    const uint32_t cyclesPerBlock = doubleSpeed ? 64 : 32;

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
            if (!bus->gpu_->hblank)
                return 0;

            const uint16_t memSource = bus->hdmaSource;
            for (uint16_t i = 0; i < 0x10; ++i) {
                const uint8_t byte = bus->ReadByte(memSource + i);
                bus->gpu_->WriteVRAM(bus->hdmaDestination + i, byte);
            }
            bus->hdmaSource += 0x10;
            bus->hdmaDestination += 0x10;
            bus->hdmaRemain = (bus->hdmaRemain == 0) ? 0x7F : static_cast<uint8_t>(bus->hdmaRemain - 1);
            if (bus->hdmaRemain == 0x7F)
                bus->hdmaActive = false;
            return cyclesPerBlock;
        }
        default:
            throw UnreachableCodeException("Gameboy::RunHDMA – invalid mode");
    }
}

uint8_t Gameboy::ExecuteInstruction() {
    uint8_t instruction = bus->ReadByte(pc);

    if (haltBug) {
        haltBug = false;
        pc = pc - 1;
        haltBugRun = true;
    }

    const bool prefixed = instruction == 0xCB;
    if (prefixed) {
        instruction = bus->ReadByte(haltBug ? pc : pc + 1);
        currentInstruction = 0xCB | instruction;
    } else {
        currentInstruction = instruction;
    }

    const uint8_t cycleIncrement = DecodeInstruction(instruction, prefixed);
    if (haltBug && haltBugRun) {
        haltBug = false;
        haltBugRun = false;
        pc = pc - 1;
    }
    return cycleIncrement;
}

void Gameboy::InitializeSystem() {
    if (bus->gpu_->hardware == GPU::Hardware::CGB) {
        regs->a = 0x11;
    } else {
        regs->a = 0x01;
    }
    regs->f = 0xB0;
    regs->SetBC(0x0013);
    regs->SetDE(0x00D8);
    regs->SetHL(0x014D);
    sp = 0xFFFE;

    static const std::map<uint16_t, uint8_t> initialData = {
        {0xFF05, 0x00}, {0xFF06, 0x00}, {0xFF07, 0x00}, {0xFF10, 0x80},
        {0xFF11, 0xBF}, {0xFF12, 0xF3}, {0xFF14, 0xBF}, {0xFF16, 0x3F},
        {0xFF17, 0x00}, {0xFF19, 0xBF}, {0xFF1A, 0x7F}, {0xFF1B, 0xFF},
        {0xFF1C, 0x9F}, {0xFF1E, 0xBF}, {0xFF20, 0xFF}, {0xFF21, 0x00},
        {0xFF22, 0x00}, {0xFF23, 0xBF}, {0xFF24, 0x77}, {0xFF25, 0xF3},
        {0xFF26, 0xF1}, {0xFF40, 0x91}, {0xFF42, 0x00}, {0xFF43, 0x00},
        {0xFF45, 0x00}, {0xFF47, 0xFC}, {0xFF48, 0xFF}, {0xFF49, 0xFF},
        {0xFF4A, 0x00}, {0xFF4B, 0x00}, {0xFFFF, 0x00},
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

void Gameboy::AdvanceFrames(const uint32_t frameBudget) {
    stepCycles = 0;
    while (stepCycles < frameBudget) {
        if (pc == 0x10) {
            bus->ChangeSpeed();
        }
        if (bus->interruptDelay && ++icount == 2) {
            bus->interruptDelay = false;
            bus->interruptMasterEnable = true;
            icount = 0;
        }

        uint32_t cycles = ProcessInterrupts();
        if (!cycles) {
            cycles = halted ? 4 : ExecuteInstruction();
        }

        const uint32_t hdmaCycles = RunHDMA();
        const uint32_t total = cycles + hdmaCycles;

        bus->UpdateTimers(total);
        bus->UpdateGraphics(total);

        stepCycles += total;
    }
}

void Gameboy::UpdateEmulator() {
    if (paused) {
        return;
    }

    using clock = std::chrono::steady_clock;
    static constexpr auto kFramePeriod = std::chrono::microseconds{16'667}; // ≈ 60 FPS (16.667 ms)
    const auto frameStart = clock::now();

    const bool doubleSpeed = (bus->speed == Bus::Speed::Double);
    const uint32_t frameBudget = kFrameCyclesDMG * (doubleSpeed ? 2u : 1u);
    AdvanceFrames(frameBudget);

    const auto elapsed = clock::now() - frameStart;
    if (const auto effectiveFrameTime = kFramePeriod / speedMultiplier; throttleSpeed && elapsed < effectiveFrameTime)
        std::this_thread::sleep_for(effectiveFrameTime - elapsed);
}

void Gameboy::DebugNextInstruction() {
    AdvanceFrames(1);
    PrintCurrentValues();
}

void Gameboy::PrintCurrentValues() const {
    const std::string text = std::format(
        "----------------------------------------------\n"
        "PC: {:#06x}  SP: {:#06x}  OP: {:#04x} — {}\n"
        "A:{:02X}  F: Z{} N{} H{} C{}\n"
        "B:{:02X}  C:{:02X}  D:{:02X}  E:{:02X}\n"
        "H:{:02X}  L:{:02X}\n\n"
        "DIV:{:02X} TIMA:{:02X} TMA:{:02X} TAC:{:02X}"
        "\n----------------------------------------------\n",
        pc, sp, currentInstruction, instructions->GetMnemonic(currentInstruction),
        regs->a,
        regs->FlagZero() ? 1 : 0, regs->FlagSubtract() ? 1 : 0,
        regs->FlagHalf() ? 1 : 0, regs->FlagCarry() ? 1 : 0,
        regs->b, regs->c, regs->d, regs->e,
        regs->h, regs->l,
        bus->timer_.divCounter, bus->timer_.tima, bus->timer_.tma, bus->timer_.tac);
    std::printf("%s", text.c_str());
    std::fflush(stdout);
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

uint32_t Gameboy::ProcessInterrupts() {
    const uint8_t pending = bus->interruptEnable & bus->interruptFlag & 0x1F;
    if (pending == 0) {
        return 0;
    }
    if (halted && !bus->interruptMasterEnable) {
        halted = false;
        return 4;
    }

    if (!bus->interruptMasterEnable)
        return 0;

    halted = false;
    bus->interruptMasterEnable = false;

    const auto bit = static_cast<uint8_t>(std::countr_zero(pending));
    const auto mask = static_cast<uint8_t>(1u << bit);
    bus->interruptFlag &= static_cast<uint8_t>(~mask);

    sp -= 2;
    bus->WriteByte(sp + 1, static_cast<uint8_t>(pc >> 8));
    bus->WriteByte(sp, static_cast<uint8_t>(pc & 0xFF));

    pc = interruptAddress(bit);
    return 20;
}
