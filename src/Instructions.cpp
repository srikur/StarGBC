#include "Instructions.h"
#include <cstring>

void Instructions::HandleOAMCorruption(const Gameboy &gameboy, const uint16_t word, const CorruptionType type) const {
    if (!gameboy.IsDMG() || (word < 0xFE00 || word > 0xFEFF) || gameboy.bus->gpu_->stat.mode != GPU::Mode::MODE_2) return;
    if (gameboy.bus->gpu_->scanlineCounter >= 76) return;
    const int currentRowIndex = gameboy.bus->gpu_->GetOAMScanRow();

    auto ReadWord = [&](const int index) -> uint16_t {
        return static_cast<uint16_t>(gameboy.bus->gpu_->oam[index]) << 8 | gameboy.bus->gpu_->oam[index + 1];
    };
    auto WriteWord = [&](const int index, const uint16_t value) {
        gameboy.bus->gpu_->oam[index] = static_cast<uint8_t>(value >> 8);
        gameboy.bus->gpu_->oam[index + 1] = static_cast<uint8_t>(value & 0xFF);
    };

    if (type == CorruptionType::ReadWrite) {
        if (currentRowIndex >= 4 && currentRowIndex < 19) {
            const int row_n_addr = currentRowIndex * 8;
            const int row_n_minus_1_addr = (currentRowIndex - 1) * 8;
            const int row_n_minus_2_addr = (currentRowIndex - 2) * 8;

            const uint16_t a_rw = ReadWord(row_n_minus_2_addr);
            const uint16_t b_rw = ReadWord(row_n_minus_1_addr);
            const uint16_t c_rw = ReadWord(row_n_addr);
            const uint16_t d_rw = ReadWord(row_n_minus_1_addr + 4);

            const uint16_t corrupted_b = (b_rw & (a_rw | c_rw | d_rw)) | (a_rw & c_rw & d_rw);
            WriteWord(row_n_minus_1_addr, corrupted_b);

            uint8_t temp_row[8];
            std::memcpy(temp_row, &gameboy.bus->gpu_->oam[row_n_minus_1_addr], 8);
            std::memcpy(&gameboy.bus->gpu_->oam[row_n_addr], temp_row, 8);
            std::memcpy(&gameboy.bus->gpu_->oam[row_n_minus_2_addr], temp_row, 8);
        }

        if (currentRowIndex > 0) {
            const int currentRowAddr = currentRowIndex * 8;
            const int prevRowAddr = (currentRowIndex - 1) * 8;

            const uint16_t a_read = ReadWord(currentRowAddr);
            const uint16_t b_read = ReadWord(prevRowAddr);
            const uint16_t c_read = ReadWord(prevRowAddr + 4);

            const uint16_t corruptedWord = b_read | (a_read & c_read);
            WriteWord(currentRowAddr, corruptedWord);

            std::memcpy(&gameboy.bus->gpu_->oam[currentRowAddr + 2], &gameboy.bus->gpu_->oam[prevRowAddr + 2], 6);
        }
    } else {
        if (currentRowIndex == 0) return;

        const int currentRowAddr = currentRowIndex * 8;
        const int prevRowAddr = (currentRowIndex - 1) * 8;

        const uint16_t a = ReadWord(currentRowAddr);
        const uint16_t b = ReadWord(prevRowAddr);
        const uint16_t c = ReadWord(prevRowAddr + 4);

        const uint16_t corruptedWord = (type == CorruptionType::Write)
                                           ? ((a ^ c) & (b ^ c)) ^ c
                                           : (b | (a & c));
        WriteWord(currentRowAddr, corruptedWord);
        std::memcpy(&gameboy.bus->gpu_->oam[currentRowAddr + 2], &gameboy.bus->gpu_->oam[prevRowAddr + 2], 6);
    }
}

template<Register source>
bool Instructions::RLC(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const uint8_t old = value & 0x80 ? 1 : 0;
    gameboy.regs->SetCarry(old != 0);
    const uint8_t newValue = (value << 1) | old;
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs.get()->*sourceReg = newValue;
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::RLCAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t old = byte & 0x80 ? 1 : 0;
        gameboy.regs->SetCarry(old != 0);
        byte = byte << 1 | old;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::DAA(Gameboy &gameboy) const {
    uint8_t adjust = 0;
    bool carry = gameboy.regs->FlagCarry();

    if (!gameboy.regs->FlagSubtract()) {
        if (gameboy.regs->FlagHalf() || (gameboy.regs->a & 0x0F) > 0x09)
            adjust |= 0x06;

        if (gameboy.regs->FlagCarry() || gameboy.regs->a > 0x99) {
            adjust |= 0x60;
            carry = true;
        }

        gameboy.regs->a += adjust;
    } else {
        if (gameboy.regs->FlagHalf()) { adjust |= 0x06; }
        if (gameboy.regs->FlagCarry()) { adjust |= 0x60; }
        gameboy.regs->a -= adjust;
    }

    gameboy.regs->SetCarry(carry);
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::RETI(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.sp);
        gameboy.sp += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.sp)) << 8;
        gameboy.sp += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.pc = word;
        gameboy.bus->interruptMasterEnable = true;
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::DI(Gameboy &gameboy) const {
    gameboy.bus->interruptDelay = false;
    gameboy.bus->interruptMasterEnable = false;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::EI(Gameboy &gameboy) const {
    if (!gameboy.bus->interruptMasterEnable) {
        gameboy.icount = 0;
        gameboy.bus->interruptDelay = true;
        gameboy.bus->interruptMasterEnable = true;
    }
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::HALT(Gameboy &gameboy) const {
    if (const bool bug = (gameboy.bus->interruptEnable & gameboy.bus->interruptFlag & 0x1F) != 0; !gameboy.bus->interruptMasterEnable && bug) {
        gameboy.haltBug = true;
        gameboy.halted = false;
    } else {
        gameboy.haltBug = false;
        gameboy.halted = true;
    }
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

template<RSTTarget target>
bool Instructions::RST(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        gameboy.sp -= 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.bus->WriteByte(gameboy.sp, (gameboy.pc & 0xFF00) >> 8);
        gameboy.sp -= 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.bus->WriteByte(gameboy.sp, gameboy.pc & 0xFF);
        constexpr auto location = GetRSTAddress<target>();
        if constexpr (target == RSTTarget::H38) {
            std::fprintf(stderr, "RST 38\n");
        }
        gameboy.pc = location;
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::CALLUnconditional(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.pc);
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc)) << 8;
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.sp -= 1;
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.bus->WriteByte(gameboy.sp, (gameboy.pc & 0xFF00) >> 8);
        gameboy.sp -= 1;
        return false;
    }
    if (gameboy.mCycleCounter == 6) {
        gameboy.bus->WriteByte(gameboy.sp, gameboy.pc & 0xFF);
        gameboy.pc = word;
        return false;
    }
    if (gameboy.mCycleCounter == 7) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::CALL(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.pc);
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        if constexpr (test == JumpTest::NotZero) jumpCondition = !gameboy.regs->FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = gameboy.regs->FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = gameboy.regs->FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !gameboy.regs->FlagCarry();
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc)) << 8;
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        if (jumpCondition) {
            gameboy.sp -= 1;
            return false;
        }

        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.bus->WriteByte(gameboy.sp, (gameboy.pc & 0xFF00) >> 8);
        gameboy.sp -= 1;
        return false;
    }
    if (gameboy.mCycleCounter == 6) {
        gameboy.bus->WriteByte(gameboy.sp, gameboy.pc & 0xFF);
        gameboy.pc = word;
        return false;
    }
    if (gameboy.mCycleCounter == 7) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::RLCA(Gameboy &gameboy) const {
    const uint8_t old = (gameboy.regs->a & 0x80) != 0 ? 1 : 0;
    gameboy.regs->SetCarry(old != 0);
    gameboy.regs->a = gameboy.regs->a << 1 | old;
    gameboy.regs->SetZero(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetSubtract(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::RLA(Gameboy &gameboy) const {
    const bool flag_c = (gameboy.regs->a & 0x80) >> 7 == 0x01;
    const uint8_t r = (gameboy.regs->a << 1) + static_cast<uint8_t>(gameboy.regs->FlagCarry());
    gameboy.regs->SetCarry(flag_c);
    gameboy.regs->SetZero(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->a = r;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

template<Register source>
bool Instructions::RL(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const bool flag_c = (value & 0x80) >> 7 == 0x01;
    const uint8_t newValue = value << 1 | static_cast<uint8_t>(gameboy.regs->FlagCarry());
    gameboy.regs->SetCarry(flag_c);
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs.get()->*sourceReg = newValue;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::RLAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t oldCarry = gameboy.regs->FlagCarry() ? 1 : 0;
        gameboy.regs->SetCarry((byte & 0x80) != 0);
        byte = (byte << 1) | oldCarry;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::CCF(Gameboy &gameboy) const {
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetCarry(!gameboy.regs->FlagCarry());
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::CPL(Gameboy &gameboy) const {
    gameboy.regs->SetHalf(true);
    gameboy.regs->SetSubtract(true);
    gameboy.regs->a = ~gameboy.regs->a;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::SCF(Gameboy &gameboy) const {
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(true);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::RRCA(Gameboy &gameboy) const {
    gameboy.regs->SetCarry((gameboy.regs->a & 0x01) != 0);
    gameboy.regs->a = gameboy.regs->a >> 1 | (gameboy.regs->a & 0x01) << 7;
    gameboy.regs->SetZero(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

template<Register source>
bool Instructions::RRC(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const bool carry = (value & 0x01) == 0x01;
    gameboy.regs->SetCarry(carry);
    const uint8_t newValue = gameboy.regs->FlagCarry() ? 0x80 | value >> 1 : value >> 1;
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs.get()->*sourceReg = newValue;
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::RRCAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->SetCarry(byte & 0x01);
        byte = gameboy.regs->FlagCarry() ? 0x80 | byte >> 1 : byte >> 1;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::RRAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word = (byte & 0x01) == 0x01; // hack for storing carry
        byte = gameboy.regs->FlagCarry() ? 0x80 | (byte >> 1) : byte >> 1;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetCarry(word);
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::RR(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const bool carry = (value & 0x01) == 0x01;
    uint8_t newValue = gameboy.regs->FlagCarry() ? 0x80 | (value >> 1) : value >> 1;
    gameboy.regs.get()->*sourceReg = newValue;
    gameboy.regs->SetCarry(carry);
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::RRA(Gameboy &gameboy) const {
    const bool carry = (gameboy.regs->a & 0x01) == 0x01;
    const uint8_t newValue = gameboy.regs->FlagCarry() ? 0x80 | gameboy.regs->a >> 1 : gameboy.regs->a >> 1;
    gameboy.regs->SetZero(false);
    gameboy.regs->a = newValue;
    gameboy.regs->SetCarry(carry);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::RETUnconditional(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.sp);
        gameboy.sp += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.sp)) << 8;
        gameboy.sp += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.pc = word;
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::RETConditional(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        if constexpr (test == JumpTest::NotZero) jumpCondition = !gameboy.regs->FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = gameboy.regs->FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = gameboy.regs->FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !gameboy.regs->FlagCarry();
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        if (jumpCondition) {
            word = gameboy.bus->ReadByte(gameboy.sp);
            gameboy.sp += 1;
            return false;
        }

        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    if (gameboy.mCycleCounter == 4) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.sp)) << 8;
        gameboy.sp += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.pc = word;
        return false;
    }
    if (gameboy.mCycleCounter == 6) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::JRUnconditional(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        signedByte = std::bit_cast<int8_t>(gameboy.bus->ReadByte(gameboy.pc));
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint16_t next = gameboy.pc;
        if (signedByte >= 0) {
            gameboy.pc = next + static_cast<uint16_t>(signedByte);
        } else {
            gameboy.pc = next - static_cast<uint16_t>(abs(signedByte));
        }
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::JR(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        signedByte = std::bit_cast<int8_t>(gameboy.bus->ReadByte(gameboy.pc));
        gameboy.pc += 1;
        if constexpr (test == JumpTest::NotZero) jumpCondition = !gameboy.regs->FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = gameboy.regs->FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = gameboy.regs->FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !gameboy.regs->FlagCarry();
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint16_t next = gameboy.pc;
        if (jumpCondition) {
            if (signedByte >= 0) {
                gameboy.pc = next + static_cast<uint16_t>(signedByte);
            } else {
                gameboy.pc = next - static_cast<uint16_t>(abs(signedByte));
            }
            return false;
        }
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::JPUnconditional(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.pc);
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc)) << 8;
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.pc = word;
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::JP(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.pc);
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc)) << 8;
        gameboy.pc += 1;
        if constexpr (test == JumpTest::NotZero) jumpCondition = !gameboy.regs->FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = gameboy.regs->FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = gameboy.regs->FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !gameboy.regs->FlagCarry();
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        if (jumpCondition) {
            gameboy.pc = word;
            return false;
        }
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::JPHL(Gameboy &gameboy) const {
    gameboy.pc = gameboy.regs->GetHL();
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::NOP(Gameboy &gameboy) const {
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::PREFIX(Gameboy &gameboy) const {
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    gameboy.currentInstruction = 0xCB00 | gameboy.nextInstruction;
    gameboy.prefixed = true;
    return true;
}

bool Instructions::STOP(Gameboy &gameboy) const {
    const uint8_t key1 = gameboy.bus->ReadByte(0xFF4D);
    const bool speedSwitchRequested = gameboy.bus->prepareSpeedShift && (key1 & 0x01);
    gameboy.bus->WriteByte(0xFF04, 0x00);

    if (speedSwitchRequested) {
        gameboy.bus->ChangeSpeed();
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }

    gameboy.stopped = true;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

template<Register source>
bool Instructions::DECRegister(Gameboy &gameboy) const {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = gameboy.regs.get()->*sourceReg;
    gameboy.regs->SetHalf((sourceValue & 0xF) == 0x00);
    const uint8_t newValue = sourceValue - 1;
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(true);
    gameboy.regs.get()->*sourceReg = newValue;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::DECIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->SetHalf((byte & 0xF) == 0x00);
        const uint8_t newValue = byte - 1;
        gameboy.regs->SetZero(newValue == 0);
        gameboy.regs->SetSubtract(true);
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), newValue);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::DEC16(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) {
            word = gameboy.regs->GetBC();
            HandleOAMCorruption(gameboy, word, CorruptionType::Write);
            gameboy.regs->SetBC(word - 1);
        } else if constexpr (target == Arithmetic16Target::DE) {
            word = gameboy.regs->GetDE();
            HandleOAMCorruption(gameboy, word, CorruptionType::Write);
            gameboy.regs->SetDE(word - 1);
        } else if constexpr (target == Arithmetic16Target::HL) {
            word = gameboy.regs->GetHL();
            gameboy.regs->SetHL(word - 1);
            HandleOAMCorruption(gameboy, word, CorruptionType::Write);
        } else if constexpr (target == Arithmetic16Target::SP) {
            HandleOAMCorruption(gameboy, gameboy.sp, CorruptionType::Write);
            gameboy.sp -= 1;
        }
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::INCRegister(Gameboy &gameboy) const {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = gameboy.regs.get()->*sourceReg;
    gameboy.regs->SetHalf((sourceValue & 0xF) == 0xF);
    const uint8_t newValue = sourceValue + 1;
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs.get()->*sourceReg = newValue;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::INCIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->SetHalf((byte & 0xF) == 0xF);
        const uint8_t newValue = byte + 1;
        gameboy.regs->SetZero(newValue == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), newValue);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::INC16(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) {
            word = gameboy.regs->GetBC();
            HandleOAMCorruption(gameboy, word, CorruptionType::Write);
            gameboy.regs->SetBC(word + 1);
        } else if constexpr (target == Arithmetic16Target::DE) {
            word = gameboy.regs->GetDE();
            HandleOAMCorruption(gameboy, word, CorruptionType::Write);
            gameboy.regs->SetDE(word + 1);
        } else if constexpr (target == Arithmetic16Target::HL) {
            word = gameboy.regs->GetHL();
            HandleOAMCorruption(gameboy, word, CorruptionType::Write);
            gameboy.regs->SetHL(word + 1);
        } else if constexpr (target == Arithmetic16Target::SP) {
            HandleOAMCorruption(gameboy, gameboy.sp, CorruptionType::Write);
            gameboy.sp += 1;
        }
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

// 0xE0
bool Instructions::LoadFromAccumulatorDirectA(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc));
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.bus->WriteByte(static_cast<uint16_t>(0xFF00) | word, gameboy.regs->a);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

// 0xE2
bool Instructions::LoadFromAccumulatorIndirectC(Gameboy &gameboy) const {
    if (gameboy.mCycleCounter == 2) {
        const uint16_t c = 0xFF00 | static_cast<uint16_t>(gameboy.regs->c);
        gameboy.bus->WriteByte(c, gameboy.regs->a);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

// 0xF0
bool Instructions::LoadAccumulatorA(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word = 0xFF00 | static_cast<uint16_t>(byte);
        byte = gameboy.bus->ReadByte(word);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        gameboy.regs->a = byte;
        return true;
    }
    return false;
}

// 0xF2
bool Instructions::LoadAccumulatorIndirectC(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(0xFF00 | static_cast<uint16_t>(gameboy.regs->c));
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a = byte;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

/* M2 -- one cycle */
template<Register target, Register source>
bool Instructions::LDRegister(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    constexpr auto targetReg = GetRegisterPtr<target>();
    const uint8_t sourceValue = gameboy.regs.get()->*sourceReg;
    gameboy.regs.get()->*targetReg = sourceValue;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

template<Register source>
bool Instructions::LDRegisterImmediate(Gameboy &gameboy) {
    constexpr auto targetReg = GetRegisterPtr<source>();
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs.get()->*targetReg = byte;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::LDRegisterIndirect(Gameboy &gameboy) {
    constexpr auto targetReg = GetRegisterPtr<source>();
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs.get()->*targetReg = byte;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::LDAddrRegister(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    if (gameboy.mCycleCounter == 2) {
        const uint8_t sourceValue = gameboy.regs.get()->*sourceReg;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), sourceValue);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAddrImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorBC(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetBC());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a = byte;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorDE(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetDE());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a = byte;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccBC(Gameboy &gameboy) const {
    if (gameboy.mCycleCounter == 2) {
        gameboy.bus->WriteByte(gameboy.regs->GetBC(), gameboy.regs->a);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccDE(Gameboy &gameboy) const {
    if (gameboy.mCycleCounter == 2) {
        gameboy.bus->WriteByte(gameboy.regs->GetDE(), gameboy.regs->a);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorDirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.pc);
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc)) << 8;
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->a = gameboy.bus->ReadByte(word);
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorDirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.pc);
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc)) << 8;
        gameboy.pc += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.bus->WriteByte(word, gameboy.regs->a);
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc);
        gameboy.pc += 1;
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorIndirectDec(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        HandleOAMCorruption(gameboy, gameboy.regs->GetHL(), CorruptionType::ReadWrite);
        gameboy.regs->SetHL(gameboy.regs->GetHL() - 1);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a = byte;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorIndirectDec(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.regs->GetHL();
        HandleOAMCorruption(gameboy, word, CorruptionType::Write);
        gameboy.bus->WriteByte(word, gameboy.regs->a);
        HandleOAMCorruption(gameboy, word, CorruptionType::Write);
        gameboy.regs->SetHL(word - 1);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorIndirectInc(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        HandleOAMCorruption(gameboy, gameboy.regs->GetHL(), CorruptionType::ReadWrite);
        gameboy.regs->SetHL(gameboy.regs->GetHL() + 1);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a = byte;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorIndirectInc(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.regs->GetHL();
        HandleOAMCorruption(gameboy, word, CorruptionType::Write);
        gameboy.bus->WriteByte(word, gameboy.regs->a);
        HandleOAMCorruption(gameboy, word, CorruptionType::Write);
        gameboy.regs->SetHL(word + 1);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16FromStack(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc++)) << 8;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.bus->WriteByte(word, gameboy.sp & 0xFF);
        word += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.bus->WriteByte(word, gameboy.sp >> 8);
        return false;
    }
    if (gameboy.mCycleCounter == 6) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<LoadWordTarget target>
bool Instructions::LD16Register(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.pc++)) << 8;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        if constexpr (target == LoadWordTarget::HL) gameboy.regs->SetHL(word);
        else if constexpr (target == LoadWordTarget::SP) gameboy.sp = word;
        else if constexpr (target == LoadWordTarget::BC) gameboy.regs->SetBC(word);
        else if constexpr (target == LoadWordTarget::DE) gameboy.regs->SetDE(word);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16Stack(Gameboy &gameboy) const {
    if (gameboy.mCycleCounter == 2) {
        gameboy.sp = gameboy.regs->GetHL();
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16StackAdjusted(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
            gameboy.bus->ReadByte(gameboy.pc++))));
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->SetCarry((gameboy.sp & 0xFF) + (word & 0xFF) > 0xFF);
        gameboy.regs->SetHalf((gameboy.sp & 0xF) + (word & 0xF) > 0xF);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetZero(false);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetHL(gameboy.sp + word);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<StackTarget target>
bool Instructions::PUSH(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        HandleOAMCorruption(gameboy, gameboy.sp, CorruptionType::Write);
        gameboy.sp -= 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        HandleOAMCorruption(gameboy, gameboy.sp, CorruptionType::Write);
        if constexpr (target == StackTarget::BC) word = gameboy.regs->GetBC();
        if constexpr (target == StackTarget::DE) word = gameboy.regs->GetDE();
        if constexpr (target == StackTarget::HL) word = gameboy.regs->GetHL();
        if constexpr (target == StackTarget::AF) word = gameboy.regs->GetAF();
        gameboy.bus->WriteByte(gameboy.sp, (word & 0xFF00) >> 8);
        gameboy.sp -= 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        HandleOAMCorruption(gameboy, gameboy.sp, CorruptionType::Write);
        gameboy.bus->WriteByte(gameboy.sp, word & 0xFF);
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<StackTarget target>
bool Instructions::POP(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        HandleOAMCorruption(gameboy, gameboy.sp, CorruptionType::ReadWrite);
        word = gameboy.bus->ReadByte(gameboy.sp);
        gameboy.sp += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        HandleOAMCorruption(gameboy, gameboy.sp, CorruptionType::Read);
        word |= static_cast<uint16_t>(gameboy.bus->ReadByte(gameboy.sp)) << 8;
        gameboy.sp += 1;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        if constexpr (target == StackTarget::BC) gameboy.regs->SetBC(word);
        if constexpr (target == StackTarget::DE) gameboy.regs->SetDE(word);
        if constexpr (target == StackTarget::HL) gameboy.regs->SetHL(word);
        if constexpr (target == StackTarget::AF) gameboy.regs->SetAF(word & 0xFFF0);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::CPRegister(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const uint8_t new_value = gameboy.regs->a - value;
    gameboy.regs->SetCarry(gameboy.regs->a < value);
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (value & 0xF));
    gameboy.regs->SetSubtract(true);
    gameboy.regs->SetZero(new_value == 0x0);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::CPIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t new_value = gameboy.regs->a - byte;
        gameboy.regs->SetCarry(gameboy.regs->a < byte);
        gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (byte & 0xF));
        gameboy.regs->SetSubtract(true);
        gameboy.regs->SetZero(new_value == 0x0);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::CPImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t new_value = gameboy.regs->a - byte;
        gameboy.regs->SetCarry(gameboy.regs->a < byte);
        gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (byte & 0xF));
        gameboy.regs->SetSubtract(true);
        gameboy.regs->SetZero(new_value == 0x0);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return true;
}

template<Register source>
bool Instructions::ORRegister(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    gameboy.regs->a |= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::ORIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a |= byte;
        gameboy.regs->SetZero(gameboy.regs->a == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.regs->SetCarry(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::ORImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a |= byte;
        gameboy.regs->SetZero(gameboy.regs->a == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.regs->SetCarry(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::XORRegister(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    gameboy.regs->a ^= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::XORIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a ^= byte;
        gameboy.regs->SetZero(gameboy.regs->a == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.regs->SetCarry(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::XORImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a ^= byte;
        gameboy.regs->SetZero(gameboy.regs->a == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.regs->SetCarry(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::AND(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    gameboy.regs->a &= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(true);
    gameboy.regs->SetCarry(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::ANDImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a &= byte;
        gameboy.regs->SetZero(gameboy.regs->a == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(true);
        gameboy.regs->SetCarry(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::ANDIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->a &= byte;
        gameboy.regs->SetZero(gameboy.regs->a == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(true);
        gameboy.regs->SetCarry(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SUB(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const uint8_t new_value = gameboy.regs->a - value;
    gameboy.regs->SetCarry(gameboy.regs->a < value);
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (value & 0xF));
    gameboy.regs->SetSubtract(true);
    gameboy.regs->SetZero(new_value == 0x0);
    gameboy.regs->a = new_value;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::SUBImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t new_value = gameboy.regs->a - byte;
        gameboy.regs->SetCarry(gameboy.regs->a < byte);
        gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (byte & 0xF));
        gameboy.regs->SetSubtract(true);
        gameboy.regs->SetZero(new_value == 0x0);
        gameboy.regs->a = new_value;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::SUBIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t new_value = gameboy.regs->a - byte;
        gameboy.regs->SetCarry(gameboy.regs->a < byte);
        gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (byte & 0xF));
        gameboy.regs->SetSubtract(true);
        gameboy.regs->SetZero(new_value == 0x0);
        gameboy.regs->a = new_value;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SLA(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    gameboy.regs->SetCarry(((value & 0x80) >> 7) == 0x01);
    const uint8_t newValue = value << 1;
    gameboy.regs.get()->*sourceReg = newValue;
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::SLAAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->SetCarry((byte & 0x80) != 0);
        byte <<= 1;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SRA(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const uint8_t newValue = (value >> 1) | (value & 0x80);
    gameboy.regs.get()->*sourceReg = newValue;
    gameboy.regs->SetCarry((value & 0x01) != 0);
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::SRAAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->SetCarry((byte & 0x01) != 0);
        byte = (byte >> 1) | (byte & 0x80);
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SWAP(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t value = gameboy.regs.get()->*sourceReg;
    value = (value >> 4) | (value << 4);
    gameboy.regs.get()->*sourceReg = value;
    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetCarry(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::SWAPAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        byte = (byte >> 4) | (byte << 4);
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetCarry(false);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SRL(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t value = gameboy.regs.get()->*sourceReg;
    gameboy.regs->SetCarry((value & 0x01) != 0);
    value = value >> 1;
    gameboy.regs.get()->*sourceReg = value;
    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::SRLAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        gameboy.regs->SetCarry((byte & 0x01) != 0);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        byte = byte >> 1;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

/* M3 -- prefixed, so only one */
template<Register source, int bit>
bool Instructions::BIT(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = gameboy.regs.get()->*sourceReg;
    gameboy.regs->SetZero((sourceValue & (1 << bit)) == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(true);
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

/* M4 -- prefixed, M3 fetches byte */
template<int bit>
bool Instructions::BITAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        gameboy.regs->SetZero((byte & (1 << bit)) == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(true);
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source, int bit>
bool Instructions::RES(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t sourceValue = gameboy.regs.get()->*sourceReg;
    sourceValue &= ~(1 << bit);
    gameboy.regs.get()->*sourceReg = sourceValue;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

template<int bit>
bool Instructions::RESAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        byte &= ~(1 << bit);
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source, int bit>
bool Instructions::SET(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t sourceValue = gameboy.regs.get()->*sourceReg;
    sourceValue |= 1 << bit;
    gameboy.regs.get()->*sourceReg = sourceValue;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

template<int bit>
bool Instructions::SETAddr(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        byte |= 1 << bit;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SBCRegister(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
    const uint8_t r = gameboy.regs->a - value - flag_carry;
    gameboy.regs->SetCarry(gameboy.regs->a < value + static_cast<uint16_t>(flag_carry));
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (value & 0xF) + flag_carry);
    gameboy.regs->SetSubtract(true);
    gameboy.regs->SetZero(r == 0x0);
    gameboy.regs->a = r;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::SBCIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
        const uint8_t r = gameboy.regs->a - byte - flag_carry;
        gameboy.regs->SetCarry(gameboy.regs->a < byte + static_cast<uint16_t>(flag_carry));
        gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (byte & 0xF) + flag_carry);
        gameboy.regs->SetSubtract(true);
        gameboy.regs->SetZero(r == 0x0);
        gameboy.regs->a = r;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::SBCImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
        const uint8_t r = gameboy.regs->a - byte - flag_carry;
        gameboy.regs->SetCarry(gameboy.regs->a < byte + static_cast<uint16_t>(flag_carry));
        gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (byte & 0xF) + flag_carry);
        gameboy.regs->SetSubtract(true);
        gameboy.regs->SetZero(r == 0x0);
        gameboy.regs->a = r;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::ADCRegister(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
    const uint8_t r = gameboy.regs->a + value + flag_carry;
    gameboy.regs->SetCarry(static_cast<uint16_t>(gameboy.regs->a) + value + static_cast<uint16_t>(flag_carry) > 0xFF);
    gameboy.regs->SetHalf(((gameboy.regs->a & 0xF) + (value & 0xF) + (flag_carry & 0xF)) > 0xF);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetZero(r == 0x0);
    gameboy.regs->a = r;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::ADCIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
        const uint8_t r = gameboy.regs->a + byte + flag_carry;
        gameboy.regs->SetCarry(static_cast<uint16_t>(gameboy.regs->a) + byte + static_cast<uint16_t>(flag_carry) > 0xFF);
        gameboy.regs->SetHalf(((gameboy.regs->a & 0xF) + (byte & 0xF) + (flag_carry & 0xF)) > 0xF);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetZero(r == 0x0);
        gameboy.regs->a = r;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::ADCImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
        const uint8_t r = gameboy.regs->a + byte + flag_carry;
        gameboy.regs->SetCarry(static_cast<uint16_t>(gameboy.regs->a) + byte + static_cast<uint16_t>(flag_carry) > 0xFF);
        gameboy.regs->SetHalf(((gameboy.regs->a & 0xF) + (byte & 0xF) + (flag_carry & 0xF)) > 0xF);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetZero(r == 0x0);
        gameboy.regs->a = r;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::ADD16(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) word = gameboy.regs->GetBC();
        if constexpr (target == Arithmetic16Target::DE) word = gameboy.regs->GetDE();
        if constexpr (target == Arithmetic16Target::HL) word = gameboy.regs->GetHL();
        if constexpr (target == Arithmetic16Target::SP) word = gameboy.sp;
        return false;
    }

    if (gameboy.mCycleCounter == 3) {
        const uint16_t reg = gameboy.regs->GetHL();
        const uint16_t sum = reg + word;

        gameboy.regs->SetCarry(reg > 0xFFFF - word);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf((reg & 0x07FF) + (word & 0x07FF) > 0x07FF);
        gameboy.regs->SetHL(sum);

        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::ADDRegister(Gameboy &gameboy) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = gameboy.regs.get()->*sourceReg;
    const uint8_t a = gameboy.regs->a;
    const uint8_t new_value = a + value;
    gameboy.regs->SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(value) > 0xFF);
    gameboy.regs->SetZero(new_value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) + (value & 0xF) > 0xF);
    gameboy.regs->a = new_value;
    gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
    return true;
}

bool Instructions::ADDIndirect(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t a = gameboy.regs->a;
        const uint8_t new_value = a + byte;
        gameboy.regs->SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(byte) > 0xFF);
        gameboy.regs->SetZero(new_value == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf((gameboy.regs->a & 0xF) + (byte & 0xF) > 0xF);
        gameboy.regs->a = new_value;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

bool Instructions::ADDImmediate(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        byte = gameboy.bus->ReadByte(gameboy.pc++);
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        const uint8_t a = gameboy.regs->a;
        const uint8_t new_value = a + byte;
        gameboy.regs->SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(byte) > 0xFF);
        gameboy.regs->SetZero(new_value == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf((gameboy.regs->a & 0xF) + (byte & 0xF) > 0xF);
        gameboy.regs->a = new_value;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

// Not exactly M-cycle accurate (GBCTR page 80)
bool Instructions::ADDSigned(Gameboy &gameboy) {
    if (gameboy.mCycleCounter == 2) {
        word = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
            gameboy.bus->ReadByte(gameboy.pc++))));
        return false;
    }
    if (gameboy.mCycleCounter == 3) {
        word2 = gameboy.sp;
        return false;
    }
    if (gameboy.mCycleCounter == 4) {
        gameboy.regs->SetCarry(((word2 & 0xFF) + (word & 0xFF)) > 0xFF);
        gameboy.regs->SetHalf(((word2 & 0xF) + (word & 0xF)) > 0xF);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetZero(false);
        return false;
    }
    if (gameboy.mCycleCounter == 5) {
        gameboy.sp = word2 + word;
        gameboy.nextInstruction = gameboy.bus->ReadByte(gameboy.pc++);
        return true;
    }
    return false;
}

// template function definitions to solve linker errors
template bool Instructions::RST<RSTTarget::H00>(Gameboy &gameboy);

template bool Instructions::RST<RSTTarget::H08>(Gameboy &gameboy);

template bool Instructions::RST<RSTTarget::H10>(Gameboy &gameboy);

template bool Instructions::RST<RSTTarget::H18>(Gameboy &gameboy);

template bool Instructions::RST<RSTTarget::H20>(Gameboy &gameboy);

template bool Instructions::RST<RSTTarget::H28>(Gameboy &gameboy);

template bool Instructions::RST<RSTTarget::H30>(Gameboy &gameboy);

template bool Instructions::RST<RSTTarget::H38>(Gameboy &gameboy);

template bool Instructions::CALL<JumpTest::NotZero>(Gameboy &gameboy);

template bool Instructions::CALL<JumpTest::Zero>(Gameboy &gameboy);

template bool Instructions::CALL<JumpTest::NotCarry>(Gameboy &gameboy);

template bool Instructions::CALL<JumpTest::Carry>(Gameboy &gameboy);

template bool Instructions::RETConditional<JumpTest::NotZero>(Gameboy &gameboy);

template bool Instructions::RETConditional<JumpTest::Zero>(Gameboy &gameboy);

template bool Instructions::RETConditional<JumpTest::NotCarry>(Gameboy &gameboy);

template bool Instructions::RETConditional<JumpTest::Carry>(Gameboy &gameboy);

template bool Instructions::JR<JumpTest::NotZero>(Gameboy &gameboy);

template bool Instructions::JR<JumpTest::Zero>(Gameboy &gameboy);

template bool Instructions::JR<JumpTest::NotCarry>(Gameboy &gameboy);

template bool Instructions::JR<JumpTest::Carry>(Gameboy &gameboy);

template bool Instructions::JP<JumpTest::NotZero>(Gameboy &gameboy);

template bool Instructions::JP<JumpTest::Zero>(Gameboy &gameboy);

template bool Instructions::JP<JumpTest::NotCarry>(Gameboy &gameboy);

template bool Instructions::JP<JumpTest::Carry>(Gameboy &gameboy);

template bool Instructions::DECRegister<Register::A>(Gameboy &gameboy) const;

template bool Instructions::DECRegister<Register::B>(Gameboy &gameboy) const;

template bool Instructions::DECRegister<Register::C>(Gameboy &gameboy) const;

template bool Instructions::DECRegister<Register::D>(Gameboy &gameboy) const;

template bool Instructions::DECRegister<Register::E>(Gameboy &gameboy) const;

template bool Instructions::DECRegister<Register::H>(Gameboy &gameboy) const;

template bool Instructions::DECRegister<Register::L>(Gameboy &gameboy) const;

template bool Instructions::DEC16<Arithmetic16Target::BC>(Gameboy &gameboy);

template bool Instructions::DEC16<Arithmetic16Target::DE>(Gameboy &gameboy);

template bool Instructions::DEC16<Arithmetic16Target::HL>(Gameboy &gameboy);

template bool Instructions::DEC16<Arithmetic16Target::SP>(Gameboy &gameboy);

template bool Instructions::INCRegister<Register::A>(Gameboy &gameboy) const;

template bool Instructions::INCRegister<Register::B>(Gameboy &gameboy) const;

template bool Instructions::INCRegister<Register::C>(Gameboy &gameboy) const;

template bool Instructions::INCRegister<Register::D>(Gameboy &gameboy) const;

template bool Instructions::INCRegister<Register::E>(Gameboy &gameboy) const;

template bool Instructions::INCRegister<Register::H>(Gameboy &gameboy) const;

template bool Instructions::INCRegister<Register::L>(Gameboy &gameboy) const;

template bool Instructions::INC16<Arithmetic16Target::BC>(Gameboy &gameboy);

template bool Instructions::INC16<Arithmetic16Target::DE>(Gameboy &gameboy);

template bool Instructions::INC16<Arithmetic16Target::HL>(Gameboy &gameboy);

template bool Instructions::INC16<Arithmetic16Target::SP>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::A, Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::A, Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::A, Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::A, Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::A, Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::A, Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::A, Register::L>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::B, Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::B, Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::B, Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::B, Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::B, Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::B, Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::B, Register::L>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::C, Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::C, Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::C, Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::C, Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::C, Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::C, Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::C, Register::L>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::D, Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::D, Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::D, Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::D, Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::D, Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::D, Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::D, Register::L>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::E, Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::E, Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::E, Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::E, Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::E, Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::E, Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::E, Register::L>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::H, Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::H, Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::H, Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::H, Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::H, Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::H, Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::H, Register::L>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::L, Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::L, Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::L, Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::L, Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::L, Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::L, Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegister<Register::L, Register::L>(Gameboy &gameboy);

template bool Instructions::LDRegisterImmediate<Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegisterImmediate<Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegisterImmediate<Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegisterImmediate<Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegisterImmediate<Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegisterImmediate<Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegisterImmediate<Register::L>(Gameboy &gameboy);

template bool Instructions::LDRegisterIndirect<Register::A>(Gameboy &gameboy);

template bool Instructions::LDRegisterIndirect<Register::B>(Gameboy &gameboy);

template bool Instructions::LDRegisterIndirect<Register::C>(Gameboy &gameboy);

template bool Instructions::LDRegisterIndirect<Register::D>(Gameboy &gameboy);

template bool Instructions::LDRegisterIndirect<Register::E>(Gameboy &gameboy);

template bool Instructions::LDRegisterIndirect<Register::H>(Gameboy &gameboy);

template bool Instructions::LDRegisterIndirect<Register::L>(Gameboy &gameboy);

template bool Instructions::LDAddrRegister<Register::A>(Gameboy &gameboy);

template bool Instructions::LDAddrRegister<Register::B>(Gameboy &gameboy);

template bool Instructions::LDAddrRegister<Register::C>(Gameboy &gameboy);

template bool Instructions::LDAddrRegister<Register::D>(Gameboy &gameboy);

template bool Instructions::LDAddrRegister<Register::E>(Gameboy &gameboy);

template bool Instructions::LDAddrRegister<Register::H>(Gameboy &gameboy);

template bool Instructions::LDAddrRegister<Register::L>(Gameboy &gameboy);

template bool Instructions::LD16Register<LoadWordTarget::BC>(Gameboy &gameboy);

template bool Instructions::LD16Register<LoadWordTarget::DE>(Gameboy &gameboy);

template bool Instructions::LD16Register<LoadWordTarget::HL>(Gameboy &gameboy);

template bool Instructions::LD16Register<LoadWordTarget::SP>(Gameboy &gameboy);

template bool Instructions::PUSH<StackTarget::BC>(Gameboy &gameboy);

template bool Instructions::PUSH<StackTarget::AF>(Gameboy &gameboy);

template bool Instructions::PUSH<StackTarget::DE>(Gameboy &gameboy);

template bool Instructions::PUSH<StackTarget::HL>(Gameboy &gameboy);

template bool Instructions::POP<StackTarget::BC>(Gameboy &gameboy);

template bool Instructions::POP<StackTarget::AF>(Gameboy &gameboy);

template bool Instructions::POP<StackTarget::DE>(Gameboy &gameboy);

template bool Instructions::POP<StackTarget::HL>(Gameboy &gameboy);

template bool Instructions::CPRegister<Register::A>(Gameboy &gameboy);

template bool Instructions::CPRegister<Register::B>(Gameboy &gameboy);

template bool Instructions::CPRegister<Register::C>(Gameboy &gameboy);

template bool Instructions::CPRegister<Register::D>(Gameboy &gameboy);

template bool Instructions::CPRegister<Register::E>(Gameboy &gameboy);

template bool Instructions::CPRegister<Register::H>(Gameboy &gameboy);

template bool Instructions::CPRegister<Register::L>(Gameboy &gameboy);

template bool Instructions::ORRegister<Register::A>(Gameboy &gameboy);

template bool Instructions::ORRegister<Register::B>(Gameboy &gameboy);

template bool Instructions::ORRegister<Register::C>(Gameboy &gameboy);

template bool Instructions::ORRegister<Register::D>(Gameboy &gameboy);

template bool Instructions::ORRegister<Register::E>(Gameboy &gameboy);

template bool Instructions::ORRegister<Register::H>(Gameboy &gameboy);

template bool Instructions::ORRegister<Register::L>(Gameboy &gameboy);

template bool Instructions::XORRegister<Register::A>(Gameboy &gameboy);

template bool Instructions::XORRegister<Register::B>(Gameboy &gameboy);

template bool Instructions::XORRegister<Register::C>(Gameboy &gameboy);

template bool Instructions::XORRegister<Register::D>(Gameboy &gameboy);

template bool Instructions::XORRegister<Register::E>(Gameboy &gameboy);

template bool Instructions::XORRegister<Register::H>(Gameboy &gameboy);

template bool Instructions::XORRegister<Register::L>(Gameboy &gameboy);

template bool Instructions::AND<Register::A>(Gameboy &gameboy);

template bool Instructions::AND<Register::B>(Gameboy &gameboy);

template bool Instructions::AND<Register::C>(Gameboy &gameboy);

template bool Instructions::AND<Register::D>(Gameboy &gameboy);

template bool Instructions::AND<Register::E>(Gameboy &gameboy);

template bool Instructions::AND<Register::H>(Gameboy &gameboy);

template bool Instructions::AND<Register::L>(Gameboy &gameboy);

template bool Instructions::SUB<Register::A>(Gameboy &gameboy);

template bool Instructions::SUB<Register::B>(Gameboy &gameboy);

template bool Instructions::SUB<Register::C>(Gameboy &gameboy);

template bool Instructions::SUB<Register::D>(Gameboy &gameboy);

template bool Instructions::SUB<Register::E>(Gameboy &gameboy);

template bool Instructions::SUB<Register::H>(Gameboy &gameboy);

template bool Instructions::SUB<Register::L>(Gameboy &gameboy);

template bool Instructions::RRC<Register::A>(Gameboy &gameboy);

template bool Instructions::RRC<Register::B>(Gameboy &gameboy);

template bool Instructions::RRC<Register::C>(Gameboy &gameboy);

template bool Instructions::RRC<Register::D>(Gameboy &gameboy);

template bool Instructions::RRC<Register::E>(Gameboy &gameboy);

template bool Instructions::RRC<Register::H>(Gameboy &gameboy);

template bool Instructions::RRC<Register::L>(Gameboy &gameboy);

template bool Instructions::RLC<Register::A>(Gameboy &gameboy);

template bool Instructions::RLC<Register::B>(Gameboy &gameboy);

template bool Instructions::RLC<Register::C>(Gameboy &gameboy);

template bool Instructions::RLC<Register::D>(Gameboy &gameboy);

template bool Instructions::RLC<Register::E>(Gameboy &gameboy);

template bool Instructions::RLC<Register::H>(Gameboy &gameboy);

template bool Instructions::RLC<Register::L>(Gameboy &gameboy);

template bool Instructions::RR<Register::A>(Gameboy &gameboy);

template bool Instructions::RR<Register::B>(Gameboy &gameboy);

template bool Instructions::RR<Register::C>(Gameboy &gameboy);

template bool Instructions::RR<Register::D>(Gameboy &gameboy);

template bool Instructions::RR<Register::E>(Gameboy &gameboy);

template bool Instructions::RR<Register::H>(Gameboy &gameboy);

template bool Instructions::RR<Register::L>(Gameboy &gameboy);

template bool Instructions::RL<Register::A>(Gameboy &gameboy);

template bool Instructions::RL<Register::B>(Gameboy &gameboy);

template bool Instructions::RL<Register::C>(Gameboy &gameboy);

template bool Instructions::RL<Register::D>(Gameboy &gameboy);

template bool Instructions::RL<Register::E>(Gameboy &gameboy);

template bool Instructions::RL<Register::H>(Gameboy &gameboy);

template bool Instructions::RL<Register::L>(Gameboy &gameboy);

template bool Instructions::SLA<Register::A>(Gameboy &gameboy);

template bool Instructions::SLA<Register::B>(Gameboy &gameboy);

template bool Instructions::SLA<Register::C>(Gameboy &gameboy);

template bool Instructions::SLA<Register::D>(Gameboy &gameboy);

template bool Instructions::SLA<Register::E>(Gameboy &gameboy);

template bool Instructions::SLA<Register::H>(Gameboy &gameboy);

template bool Instructions::SLA<Register::L>(Gameboy &gameboy);

template bool Instructions::SRA<Register::A>(Gameboy &gameboy);

template bool Instructions::SRA<Register::B>(Gameboy &gameboy);

template bool Instructions::SRA<Register::C>(Gameboy &gameboy);

template bool Instructions::SRA<Register::D>(Gameboy &gameboy);

template bool Instructions::SRA<Register::E>(Gameboy &gameboy);

template bool Instructions::SRA<Register::H>(Gameboy &gameboy);

template bool Instructions::SRA<Register::L>(Gameboy &gameboy);

template bool Instructions::SWAP<Register::A>(Gameboy &gameboy);

template bool Instructions::SWAP<Register::B>(Gameboy &gameboy);

template bool Instructions::SWAP<Register::C>(Gameboy &gameboy);

template bool Instructions::SWAP<Register::D>(Gameboy &gameboy);

template bool Instructions::SWAP<Register::E>(Gameboy &gameboy);

template bool Instructions::SWAP<Register::H>(Gameboy &gameboy);

template bool Instructions::SWAP<Register::L>(Gameboy &gameboy);

template bool Instructions::SRL<Register::A>(Gameboy &gameboy);

template bool Instructions::SRL<Register::B>(Gameboy &gameboy);

template bool Instructions::SRL<Register::C>(Gameboy &gameboy);

template bool Instructions::SRL<Register::D>(Gameboy &gameboy);

template bool Instructions::SRL<Register::E>(Gameboy &gameboy);

template bool Instructions::SRL<Register::H>(Gameboy &gameboy);

template bool Instructions::SRL<Register::L>(Gameboy &gameboy);

template bool Instructions::BIT<Register::A, 0>(Gameboy &gameboy);

template bool Instructions::BIT<Register::B, 0>(Gameboy &gameboy);

template bool Instructions::BIT<Register::C, 0>(Gameboy &gameboy);

template bool Instructions::BIT<Register::D, 0>(Gameboy &gameboy);

template bool Instructions::BIT<Register::E, 0>(Gameboy &gameboy);

template bool Instructions::BIT<Register::H, 0>(Gameboy &gameboy);

template bool Instructions::BIT<Register::L, 0>(Gameboy &gameboy);

template bool Instructions::BIT<Register::A, 1>(Gameboy &gameboy);

template bool Instructions::BIT<Register::B, 1>(Gameboy &gameboy);

template bool Instructions::BIT<Register::C, 1>(Gameboy &gameboy);

template bool Instructions::BIT<Register::D, 1>(Gameboy &gameboy);

template bool Instructions::BIT<Register::E, 1>(Gameboy &gameboy);

template bool Instructions::BIT<Register::H, 1>(Gameboy &gameboy);

template bool Instructions::BIT<Register::L, 1>(Gameboy &gameboy);

template bool Instructions::BIT<Register::A, 2>(Gameboy &gameboy);

template bool Instructions::BIT<Register::B, 2>(Gameboy &gameboy);

template bool Instructions::BIT<Register::C, 2>(Gameboy &gameboy);

template bool Instructions::BIT<Register::D, 2>(Gameboy &gameboy);

template bool Instructions::BIT<Register::E, 2>(Gameboy &gameboy);

template bool Instructions::BIT<Register::H, 2>(Gameboy &gameboy);

template bool Instructions::BIT<Register::L, 2>(Gameboy &gameboy);

template bool Instructions::BIT<Register::A, 3>(Gameboy &gameboy);

template bool Instructions::BIT<Register::B, 3>(Gameboy &gameboy);

template bool Instructions::BIT<Register::C, 3>(Gameboy &gameboy);

template bool Instructions::BIT<Register::D, 3>(Gameboy &gameboy);

template bool Instructions::BIT<Register::E, 3>(Gameboy &gameboy);

template bool Instructions::BIT<Register::H, 3>(Gameboy &gameboy);

template bool Instructions::BIT<Register::L, 3>(Gameboy &gameboy);

template bool Instructions::BIT<Register::A, 4>(Gameboy &gameboy);

template bool Instructions::BIT<Register::B, 4>(Gameboy &gameboy);

template bool Instructions::BIT<Register::C, 4>(Gameboy &gameboy);

template bool Instructions::BIT<Register::D, 4>(Gameboy &gameboy);

template bool Instructions::BIT<Register::E, 4>(Gameboy &gameboy);

template bool Instructions::BIT<Register::H, 4>(Gameboy &gameboy);

template bool Instructions::BIT<Register::L, 4>(Gameboy &gameboy);

template bool Instructions::BIT<Register::A, 5>(Gameboy &gameboy);

template bool Instructions::BIT<Register::B, 5>(Gameboy &gameboy);

template bool Instructions::BIT<Register::C, 5>(Gameboy &gameboy);

template bool Instructions::BIT<Register::D, 5>(Gameboy &gameboy);

template bool Instructions::BIT<Register::E, 5>(Gameboy &gameboy);

template bool Instructions::BIT<Register::H, 5>(Gameboy &gameboy);

template bool Instructions::BIT<Register::L, 5>(Gameboy &gameboy);

template bool Instructions::BIT<Register::A, 6>(Gameboy &gameboy);

template bool Instructions::BIT<Register::B, 6>(Gameboy &gameboy);

template bool Instructions::BIT<Register::C, 6>(Gameboy &gameboy);

template bool Instructions::BIT<Register::D, 6>(Gameboy &gameboy);

template bool Instructions::BIT<Register::E, 6>(Gameboy &gameboy);

template bool Instructions::BIT<Register::H, 6>(Gameboy &gameboy);

template bool Instructions::BIT<Register::L, 6>(Gameboy &gameboy);

template bool Instructions::BIT<Register::A, 7>(Gameboy &gameboy);

template bool Instructions::BIT<Register::B, 7>(Gameboy &gameboy);

template bool Instructions::BIT<Register::C, 7>(Gameboy &gameboy);

template bool Instructions::BIT<Register::D, 7>(Gameboy &gameboy);

template bool Instructions::BIT<Register::E, 7>(Gameboy &gameboy);

template bool Instructions::BIT<Register::H, 7>(Gameboy &gameboy);

template bool Instructions::BIT<Register::L, 7>(Gameboy &gameboy);

template bool Instructions::RES<Register::A, 0>(Gameboy &gameboy);

template bool Instructions::RES<Register::B, 0>(Gameboy &gameboy);

template bool Instructions::RES<Register::C, 0>(Gameboy &gameboy);

template bool Instructions::RES<Register::D, 0>(Gameboy &gameboy);

template bool Instructions::RES<Register::E, 0>(Gameboy &gameboy);

template bool Instructions::RES<Register::H, 0>(Gameboy &gameboy);

template bool Instructions::RES<Register::L, 0>(Gameboy &gameboy);

template bool Instructions::RES<Register::A, 1>(Gameboy &gameboy);

template bool Instructions::RES<Register::B, 1>(Gameboy &gameboy);

template bool Instructions::RES<Register::C, 1>(Gameboy &gameboy);

template bool Instructions::RES<Register::D, 1>(Gameboy &gameboy);

template bool Instructions::RES<Register::E, 1>(Gameboy &gameboy);

template bool Instructions::RES<Register::H, 1>(Gameboy &gameboy);

template bool Instructions::RES<Register::L, 1>(Gameboy &gameboy);

template bool Instructions::RES<Register::A, 2>(Gameboy &gameboy);

template bool Instructions::RES<Register::B, 2>(Gameboy &gameboy);

template bool Instructions::RES<Register::C, 2>(Gameboy &gameboy);

template bool Instructions::RES<Register::D, 2>(Gameboy &gameboy);

template bool Instructions::RES<Register::E, 2>(Gameboy &gameboy);

template bool Instructions::RES<Register::H, 2>(Gameboy &gameboy);

template bool Instructions::RES<Register::L, 2>(Gameboy &gameboy);

template bool Instructions::RES<Register::A, 3>(Gameboy &gameboy);

template bool Instructions::RES<Register::B, 3>(Gameboy &gameboy);

template bool Instructions::RES<Register::C, 3>(Gameboy &gameboy);

template bool Instructions::RES<Register::D, 3>(Gameboy &gameboy);

template bool Instructions::RES<Register::E, 3>(Gameboy &gameboy);

template bool Instructions::RES<Register::H, 3>(Gameboy &gameboy);

template bool Instructions::RES<Register::L, 3>(Gameboy &gameboy);

template bool Instructions::RES<Register::A, 4>(Gameboy &gameboy);

template bool Instructions::RES<Register::B, 4>(Gameboy &gameboy);

template bool Instructions::RES<Register::C, 4>(Gameboy &gameboy);

template bool Instructions::RES<Register::D, 4>(Gameboy &gameboy);

template bool Instructions::RES<Register::E, 4>(Gameboy &gameboy);

template bool Instructions::RES<Register::H, 4>(Gameboy &gameboy);

template bool Instructions::RES<Register::L, 4>(Gameboy &gameboy);

template bool Instructions::RES<Register::A, 5>(Gameboy &gameboy);

template bool Instructions::RES<Register::B, 5>(Gameboy &gameboy);

template bool Instructions::RES<Register::C, 5>(Gameboy &gameboy);

template bool Instructions::RES<Register::D, 5>(Gameboy &gameboy);

template bool Instructions::RES<Register::E, 5>(Gameboy &gameboy);

template bool Instructions::RES<Register::H, 5>(Gameboy &gameboy);

template bool Instructions::RES<Register::L, 5>(Gameboy &gameboy);

template bool Instructions::RES<Register::A, 6>(Gameboy &gameboy);

template bool Instructions::RES<Register::B, 6>(Gameboy &gameboy);

template bool Instructions::RES<Register::C, 6>(Gameboy &gameboy);

template bool Instructions::RES<Register::D, 6>(Gameboy &gameboy);

template bool Instructions::RES<Register::E, 6>(Gameboy &gameboy);

template bool Instructions::RES<Register::H, 6>(Gameboy &gameboy);

template bool Instructions::RES<Register::L, 6>(Gameboy &gameboy);

template bool Instructions::RES<Register::A, 7>(Gameboy &gameboy);

template bool Instructions::RES<Register::B, 7>(Gameboy &gameboy);

template bool Instructions::RES<Register::C, 7>(Gameboy &gameboy);

template bool Instructions::RES<Register::D, 7>(Gameboy &gameboy);

template bool Instructions::RES<Register::E, 7>(Gameboy &gameboy);

template bool Instructions::RES<Register::H, 7>(Gameboy &gameboy);

template bool Instructions::RES<Register::L, 7>(Gameboy &gameboy);

template bool Instructions::SET<Register::A, 0>(Gameboy &gameboy);

template bool Instructions::SET<Register::B, 0>(Gameboy &gameboy);

template bool Instructions::SET<Register::C, 0>(Gameboy &gameboy);

template bool Instructions::SET<Register::D, 0>(Gameboy &gameboy);

template bool Instructions::SET<Register::E, 0>(Gameboy &gameboy);

template bool Instructions::SET<Register::H, 0>(Gameboy &gameboy);

template bool Instructions::SET<Register::L, 0>(Gameboy &gameboy);

template bool Instructions::SET<Register::A, 1>(Gameboy &gameboy);

template bool Instructions::SET<Register::B, 1>(Gameboy &gameboy);

template bool Instructions::SET<Register::C, 1>(Gameboy &gameboy);

template bool Instructions::SET<Register::D, 1>(Gameboy &gameboy);

template bool Instructions::SET<Register::E, 1>(Gameboy &gameboy);

template bool Instructions::SET<Register::H, 1>(Gameboy &gameboy);

template bool Instructions::SET<Register::L, 1>(Gameboy &gameboy);

template bool Instructions::SET<Register::A, 2>(Gameboy &gameboy);

template bool Instructions::SET<Register::B, 2>(Gameboy &gameboy);

template bool Instructions::SET<Register::C, 2>(Gameboy &gameboy);

template bool Instructions::SET<Register::D, 2>(Gameboy &gameboy);

template bool Instructions::SET<Register::E, 2>(Gameboy &gameboy);

template bool Instructions::SET<Register::H, 2>(Gameboy &gameboy);

template bool Instructions::SET<Register::L, 2>(Gameboy &gameboy);

template bool Instructions::SET<Register::A, 3>(Gameboy &gameboy);

template bool Instructions::SET<Register::B, 3>(Gameboy &gameboy);

template bool Instructions::SET<Register::C, 3>(Gameboy &gameboy);

template bool Instructions::SET<Register::D, 3>(Gameboy &gameboy);

template bool Instructions::SET<Register::E, 3>(Gameboy &gameboy);

template bool Instructions::SET<Register::H, 3>(Gameboy &gameboy);

template bool Instructions::SET<Register::L, 3>(Gameboy &gameboy);

template bool Instructions::SET<Register::A, 4>(Gameboy &gameboy);

template bool Instructions::SET<Register::B, 4>(Gameboy &gameboy);

template bool Instructions::SET<Register::C, 4>(Gameboy &gameboy);

template bool Instructions::SET<Register::D, 4>(Gameboy &gameboy);

template bool Instructions::SET<Register::E, 4>(Gameboy &gameboy);

template bool Instructions::SET<Register::H, 4>(Gameboy &gameboy);

template bool Instructions::SET<Register::L, 4>(Gameboy &gameboy);

template bool Instructions::SET<Register::A, 5>(Gameboy &gameboy);

template bool Instructions::SET<Register::B, 5>(Gameboy &gameboy);

template bool Instructions::SET<Register::C, 5>(Gameboy &gameboy);

template bool Instructions::SET<Register::D, 5>(Gameboy &gameboy);

template bool Instructions::SET<Register::E, 5>(Gameboy &gameboy);

template bool Instructions::SET<Register::H, 5>(Gameboy &gameboy);

template bool Instructions::SET<Register::L, 5>(Gameboy &gameboy);

template bool Instructions::SET<Register::A, 6>(Gameboy &gameboy);

template bool Instructions::SET<Register::B, 6>(Gameboy &gameboy);

template bool Instructions::SET<Register::C, 6>(Gameboy &gameboy);

template bool Instructions::SET<Register::D, 6>(Gameboy &gameboy);

template bool Instructions::SET<Register::E, 6>(Gameboy &gameboy);

template bool Instructions::SET<Register::H, 6>(Gameboy &gameboy);

template bool Instructions::SET<Register::L, 6>(Gameboy &gameboy);

template bool Instructions::SET<Register::A, 7>(Gameboy &gameboy);

template bool Instructions::SET<Register::B, 7>(Gameboy &gameboy);

template bool Instructions::SET<Register::C, 7>(Gameboy &gameboy);

template bool Instructions::SET<Register::D, 7>(Gameboy &gameboy);

template bool Instructions::SET<Register::E, 7>(Gameboy &gameboy);

template bool Instructions::SET<Register::H, 7>(Gameboy &gameboy);

template bool Instructions::SET<Register::L, 7>(Gameboy &gameboy);

template bool Instructions::BITAddr<0>(Gameboy &gameboy);

template bool Instructions::BITAddr<1>(Gameboy &gameboy);

template bool Instructions::BITAddr<2>(Gameboy &gameboy);

template bool Instructions::BITAddr<3>(Gameboy &gameboy);

template bool Instructions::BITAddr<4>(Gameboy &gameboy);

template bool Instructions::BITAddr<5>(Gameboy &gameboy);

template bool Instructions::BITAddr<6>(Gameboy &gameboy);

template bool Instructions::BITAddr<7>(Gameboy &gameboy);

template bool Instructions::RESAddr<0>(Gameboy &gameboy);

template bool Instructions::RESAddr<1>(Gameboy &gameboy);

template bool Instructions::RESAddr<2>(Gameboy &gameboy);

template bool Instructions::RESAddr<3>(Gameboy &gameboy);

template bool Instructions::RESAddr<4>(Gameboy &gameboy);

template bool Instructions::RESAddr<5>(Gameboy &gameboy);

template bool Instructions::RESAddr<6>(Gameboy &gameboy);

template bool Instructions::RESAddr<7>(Gameboy &gameboy);

template bool Instructions::SETAddr<0>(Gameboy &gameboy);

template bool Instructions::SETAddr<1>(Gameboy &gameboy);

template bool Instructions::SETAddr<2>(Gameboy &gameboy);

template bool Instructions::SETAddr<3>(Gameboy &gameboy);

template bool Instructions::SETAddr<4>(Gameboy &gameboy);

template bool Instructions::SETAddr<5>(Gameboy &gameboy);

template bool Instructions::SETAddr<6>(Gameboy &gameboy);

template bool Instructions::SETAddr<7>(Gameboy &gameboy);

template bool Instructions::SBCRegister<Register::A>(Gameboy &gameboy);

template bool Instructions::SBCRegister<Register::B>(Gameboy &gameboy);

template bool Instructions::SBCRegister<Register::C>(Gameboy &gameboy);

template bool Instructions::SBCRegister<Register::D>(Gameboy &gameboy);

template bool Instructions::SBCRegister<Register::E>(Gameboy &gameboy);

template bool Instructions::SBCRegister<Register::H>(Gameboy &gameboy);

template bool Instructions::SBCRegister<Register::L>(Gameboy &gameboy);

template bool Instructions::ADCRegister<Register::A>(Gameboy &gameboy);

template bool Instructions::ADCRegister<Register::B>(Gameboy &gameboy);

template bool Instructions::ADCRegister<Register::C>(Gameboy &gameboy);

template bool Instructions::ADCRegister<Register::D>(Gameboy &gameboy);

template bool Instructions::ADCRegister<Register::E>(Gameboy &gameboy);

template bool Instructions::ADCRegister<Register::H>(Gameboy &gameboy);

template bool Instructions::ADCRegister<Register::L>(Gameboy &gameboy);

template bool Instructions::ADD16<Arithmetic16Target::BC>(Gameboy &gameboy);

template bool Instructions::ADD16<Arithmetic16Target::DE>(Gameboy &gameboy);

template bool Instructions::ADD16<Arithmetic16Target::HL>(Gameboy &gameboy);

template bool Instructions::ADD16<Arithmetic16Target::SP>(Gameboy &gameboy);

template bool Instructions::ADDRegister<Register::A>(Gameboy &gameboy);

template bool Instructions::ADDRegister<Register::B>(Gameboy &gameboy);

template bool Instructions::ADDRegister<Register::C>(Gameboy &gameboy);

template bool Instructions::ADDRegister<Register::D>(Gameboy &gameboy);

template bool Instructions::ADDRegister<Register::E>(Gameboy &gameboy);

template bool Instructions::ADDRegister<Register::H>(Gameboy &gameboy);

template bool Instructions::ADDRegister<Register::L>(Gameboy &gameboy);
