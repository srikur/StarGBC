#include "Gameboy.h"
#include "Instructions.h"

#include <iterator>
#include <map>

inline uint16_t Gameboy::ReadNextWord() const {
    const uint16_t lower = bus->ReadByte(pc + 1);
    const uint16_t higher = bus->ReadByte(pc + 2);
    const uint16_t word = higher << 8 | lower;
    return word;
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

std::tuple<uint32_t, uint32_t, uint32_t> Gameboy::GetPixel(const uint32_t x, const uint32_t y) const {
    return {bus->gpu_->screenData[x][y][0], bus->gpu_->screenData[x][y][1], bus->gpu_->screenData[x][y][2]};
}

uint8_t Gameboy::DecodeInstruction(const uint8_t opcode, const bool prefixed) {
    const uint8_t cycleIncrement = prefixed
                                       ? instructions->prefixedInstr(opcode, *this)
                                       : instructions->nonPrefixedInstr(opcode, *this);
    return cycleIncrement;
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

void Gameboy::RunBootrom() {
    if (!bus->runBootrom) {
        return;
    }

    stepCycles = 0;

    while (stepCycles < Timer::MAX_CYCLES) {
        if (bus->interruptDelay) {
            icount += 1;
            if (icount == 2) {
                bus->interruptDelay = false;
                bus->interruptMasterEnable = true;
            }
        }

        uint32_t cycles = ProcessInterrupts();

        if (cycles) {
            stepCycles += cycles;
        } else if (halted) {
            stepCycles += 4;
        } else {
            cycles = ExecuteInstruction();
            stepCycles += cycles;
        }

        const uint8_t hdmaCycles = RunHDMA();
        bus->UpdateTimers(cycles + static_cast<int>(bus->speed) * hdmaCycles);
        bus->UpdateGraphics(cycles + 4);

        if ((bus->gpu_->hardware == GPU::Hardware::CGB && pc != 0x100) || (
                bus->gpu_->hardware == GPU::Hardware::DMG && pc <= 0xFF)) {
            continue;
        }

        bus->runBootrom = false;
        pc = 0x100;
        InitializeSystem();
        return;
    }
}

uint32_t Gameboy::RunHDMA() const {
    if (!bus->hdmaActive) {
        return 0;
    }

    switch (bus->gpu_->hdmaMode) {
        case GPU::HDMAMode::GDMA: {
            const uint32_t length = static_cast<uint32_t>(bus->hdmaRemain) + 1;
            for (uint32_t unused = 0; unused < length; unused++) {
                const uint16_t memSource = bus->hdmaSource;
                for (uint16_t i = 0; i < 0x10; i++) {
                    const uint8_t byte = bus->ReadByte(memSource + i);
                    bus->gpu_->WriteVRAM(bus->hdmaDestination + i, byte);
                }
                bus->hdmaSource += 0x10;
                bus->hdmaDestination += 0x10;
                if (bus->hdmaRemain == 0) {
                    bus->hdmaRemain = 0x7F;
                } else {
                    bus->hdmaRemain -= 1;
                }
            }
            bus->hdmaActive = false;
            return length * 8 * 4;
        }
        case GPU::HDMAMode::HDMA: {
            if (!bus->gpu_->hblank) {
                return 0;
            }
            const uint16_t memSource = bus->hdmaSource;
            for (uint16_t i = 0; i < 0x10; i++) {
                const uint8_t byte = bus->ReadByte(memSource + i);
                bus->gpu_->WriteVRAM(bus->hdmaDestination + i, byte);
            }
            bus->hdmaSource += 0x10;
            bus->hdmaDestination += 0x10;
            if (bus->hdmaRemain == 0) {
                bus->hdmaRemain = 0x7F;
            } else {
                bus->hdmaRemain -= 1;
            }
            if (bus->hdmaRemain == 0x7F) {
                bus->hdmaActive = false;
            }
            return 32;
        }
        default: throw UnreachableCodeException("Gameboy::RunHDMA");
    }
}

uint8_t Gameboy::ExecuteInstruction() {
    uint8_t instruction = bus->ReadByte(pc);

    if (haltBug) {
        haltBug = false;
        pc = pc - 1;
    }

    const bool prefixed = instruction == 0xCB;
    if (prefixed) {
        instruction = bus->ReadByte(pc + 1);
    }

    const uint8_t cycleIncrement = DecodeInstruction(instruction, prefixed);
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

    static std::map<const uint16_t, const uint8_t> initialData = {
        {0xFF05, 0x00},
        {0xFF06, 0x00},
        {0xFF07, 0x00},
        {0xFF10, 0x80},
        {0xFF11, 0xBF},
        {0xFF12, 0xF3},
        {0xFF14, 0xBF},
        {0xFF16, 0x3F},
        {0xFF17, 0x00},
        {0xFF19, 0xBF},
        {0xFF1A, 0x7F},
        {0xFF1B, 0xFF},
        {0xFF1C, 0x9F},
        {0xFF1E, 0xBF},
        {0xFF20, 0xFF},
        {0xFF21, 0x00},
        {0xFF22, 0x00},
        {0xFF23, 0xBF},
        {0xFF24, 0x77},
        {0xFF25, 0xF3},
        {0xFF26, 0xF1},
        {0xFF40, 0x91},
        {0xFF42, 0x00},
        {0xFF43, 0x00},
        {0xFF45, 0x00},
        {0xFF47, 0xFC},
        {0xFF48, 0xFF},
        {0xFF49, 0xFF},
        {0xFF4A, 0x00},
        {0xFF4B, 0x00},
        {0xFFFF, 0x00},
    };

    for (const auto &[address, value]: initialData) {
        bus->WriteByte(address, value);
    }
}

void Gameboy::UpdateEmulator() {
    stepCycles = 0;

    while (stepCycles < Timer::MAX_CYCLES) {
        if (pc == 0x10) {
            bus->ChangeSpeed();
        }

        if (bus->interruptDelay) {
            icount += 1;
            if (icount == 2) {
                bus->interruptDelay = false;
                bus->interruptMasterEnable = true;
            }
        }

        uint32_t cycles = ProcessInterrupts();

        if (cycles) {
            stepCycles += cycles;
        } else if (halted) {
            stepCycles += 4;
        } else {
            cycles = ExecuteInstruction();
            stepCycles += cycles;
        }

        const uint8_t hdmaCycles = RunHDMA();
        bus->UpdateTimers(cycles + static_cast<int>(bus->speed) * hdmaCycles);
        bus->UpdateGraphics(cycles + 8);
    }
}

uint32_t Gameboy::ProcessInterrupts() {
    if (!halted && !bus->interruptMasterEnable) {
        return 0;
    }
    const uint8_t fired = bus->interruptEnable & bus->interruptFlag;
    if (fired == 0x00) {
        return 0;
    }
    halted = false;
    if (!bus->interruptMasterEnable) {
        return 0;
    }
    bus->interruptMasterEnable = false;

    const uint8_t shift = 1 << trailingZeros(fired);
    const uint8_t flip = ~shift;
    const uint8_t flag = (bus->interruptFlag) & flip;

    bus->interruptFlag = flag;

    bus->WriteByte(sp - 1, pc >> 8);
    bus->WriteByte(sp - 2, pc & 0xFF);
    sp -= 2;

    pc = 0x40 | trailingZeros(fired) << 3;
    return 16;
}
