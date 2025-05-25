#include "Instructions.h"

uint16_t Instructions::ReadNextWord(const Gameboy &gameboy) {
    const uint16_t lower = gameboy.bus->ReadByte(gameboy.pc + 1);
    const uint16_t higher = gameboy.bus->ReadByte(gameboy.pc + 2);
    const uint16_t word = higher << 8 | lower;
    return word;
}

uint8_t Instructions::ReadNextByte(const Gameboy &gameboy) {
    return gameboy.bus->ReadByte(gameboy.pc + 1);
}

uint8_t Instructions::RLC(const ArithmeticSource source, Gameboy &gameboy) {
    uint8_t old = 0;
    using enum ArithmeticSource;

    if (source == HLAddr) {
        uint8_t byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        old = (byte & 0x80) != 0 ? 1 : 0;
        gameboy.regs->SetCarry(old != 0);
        byte = byte << 1 | old;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.pc += 2;
    } else {
        uint8_t *reg = nullptr;

        switch (source) {
            case A: reg = &gameboy.regs->a;
                break;
            case B: reg = &gameboy.regs->b;
                break;
            case C: reg = &gameboy.regs->c;
                break;
            case D: reg = &gameboy.regs->d;
                break;
            case E: reg = &gameboy.regs->e;
                break;
            case H: reg = &gameboy.regs->h;
                break;
            case L: reg = &gameboy.regs->l;
                break;
            default: throw UnreachableCodeException("Instructions::RLC");
        }

        old = (*reg & 0x80) ? 1 : 0;
        gameboy.regs->SetCarry(old != 0);
        *reg = (*reg << 1) | old;
        gameboy.regs->SetZero(*reg == 0);
        gameboy.pc += 2;
    }

    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    gameboy.pc += 2;
    return 8;
}

uint8_t Instructions::DAA(Gameboy &gameboy) {
    bool carry = false;
    if (!gameboy.regs->FlagSubtract()) {
        if ((gameboy.regs->FlagCarry()) || (gameboy.regs->a > 0x99)) {
            gameboy.regs->a += 0x60;
            carry = true;
        }
        if (gameboy.regs->FlagHalf() || ((gameboy.regs->a & 0x0F) > 0x09)) {
            gameboy.regs->a += 0x06;
        }
    } else if (gameboy.regs->FlagCarry()) {
        carry = true;
        if (gameboy.regs->FlagHalf()) {
            gameboy.regs->a += 0x9A;
        } else {
            gameboy.regs->a += 0xA0;
        }
    } else if (gameboy.regs->FlagHalf()) {
        gameboy.regs->a += 0xFA;
    }

    gameboy.regs->SetCarry(carry);
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetHalf(false);

    gameboy.pc += 1;
    return 4;
}


uint8_t Instructions::RETI(Gameboy &gameboy) {
    gameboy.bus->interruptMasterEnable = true;
    const uint16_t pc = pop(gameboy);
    gameboy.pc = pc;
    return 16;
}

uint8_t Instructions::DI(Gameboy &gameboy) {
    gameboy.bus->interruptDelay = false;
    gameboy.bus->interruptMasterEnable = false;
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::EI(Gameboy &gameboy) {
    gameboy.icount = 0;
    gameboy.bus->interruptDelay = true;
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::HALT(Gameboy &gameboy) {
    if (const bool bug = (gameboy.bus->interruptEnable & gameboy.bus->interruptFlag & 0x1F) != 0;
        !gameboy.bus->interruptMasterEnable && bug) {
        gameboy.haltBug = true;
    } else if (!gameboy.bus->interruptMasterEnable && !bug) {
        gameboy.haltBug = false;
        gameboy.halted = true;
    } else {
        gameboy.haltBug = false;
        gameboy.halted = true;
    }

    gameboy.pc += 1;
    return 4;
}


uint8_t Instructions::RST(const RSTTargets target, Gameboy &gameboy) {
    uint16_t location;
    switch (target) {
        case RSTTargets::H00: location = 0x00;
            break;
        case RSTTargets::H08: location = 0x08;
            break;
        case RSTTargets::H10: location = 0x10;
            break;
        case RSTTargets::H18: location = 0x18;
            break;
        case RSTTargets::H20: location = 0x20;
            break;
        case RSTTargets::H28: location = 0x28;
            break;
        case RSTTargets::H30: location = 0x30;
            break;
        case RSTTargets::H38: location = 0x38;
            break;
        default: throw UnreachableCodeException("Instructions::RST");
    }

    push(gameboy.pc + 1, gameboy);
    gameboy.pc = location;
    return 16;
}

uint8_t Instructions::CALL(const JumpTest test, Gameboy &gameboy) {
    bool jumpCondition = false;
    switch (test) {
        case JumpTest::NotZero:
            jumpCondition = !gameboy.regs->FlagZero();
            break;
        case JumpTest::Zero:
            jumpCondition = gameboy.regs->FlagZero();
            break;
        case JumpTest::Carry:
            jumpCondition = gameboy.regs->FlagCarry();
            break;
        case JumpTest::NotCarry:
            jumpCondition = !gameboy.regs->FlagCarry();
            break;
        case JumpTest::Always:
            jumpCondition = true;
            break;
    }

    return call(jumpCondition, gameboy);
}

uint8_t Instructions::RLCA(Gameboy &gameboy) {
    const uint8_t old = (gameboy.regs->a & 0x80) != 0 ? 1 : 0;
    gameboy.regs->SetCarry(old != 0);
    gameboy.regs->a = gameboy.regs->a << 1 | old;
    gameboy.regs->SetZero(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetSubtract(false);
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::RLA(Gameboy &gameboy) {
    const bool flag_c = (gameboy.regs->a & 0x80) >> 7 == 0x01;
    const uint8_t r = (gameboy.regs->a << 1) + static_cast<uint8_t>(gameboy.regs->FlagCarry());
    gameboy.regs->SetCarry(flag_c);
    gameboy.regs->SetZero(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->a = r;
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::RL(const ArithmeticSource source, Gameboy &gameboy) {
    if (source == ArithmeticSource::HLAddr) {
        uint8_t byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        const uint8_t flag_c = gameboy.regs->FlagCarry() ? 1 : 0;
        gameboy.regs->SetCarry((byte & 0x80) != 0);
        byte = (byte << 1) | flag_c;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), byte);
        gameboy.regs->SetZero(byte == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.pc += 2;
        return 16;
    }

    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        default: throw UnreachableCodeException("Instructions::RL");
    }

    const bool flag_c = (value & 0x80) >> 7 == 0x01;
    const uint8_t newValue = value << 1 | static_cast<uint8_t>(gameboy.regs->FlagCarry());
    gameboy.regs->SetCarry(flag_c);
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetSubtract(false);

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a = newValue;
            break;
        case ArithmeticSource::B: gameboy.regs->b = newValue;
            break;
        case ArithmeticSource::C: gameboy.regs->c = newValue;
            break;
        case ArithmeticSource::D: gameboy.regs->d = newValue;
            break;
        case ArithmeticSource::E: gameboy.regs->e = newValue;
            break;
        case ArithmeticSource::H: gameboy.regs->h = newValue;
            break;
        case ArithmeticSource::L: gameboy.regs->l = newValue;
            break;
        default: throw UnreachableCodeException("Instructions::RL");
    }

    gameboy.pc += 2;
    return 8;
}

uint8_t Instructions::CCF(Gameboy &gameboy) {
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetCarry(!gameboy.regs->FlagCarry());
    gameboy.regs->SetHalf(false);
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::CPL(Gameboy &gameboy) {
    gameboy.regs->SetHalf(true);
    gameboy.regs->SetSubtract(true);
    gameboy.regs->a = ~gameboy.regs->a;
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::SCF(Gameboy &gameboy) {
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(true);
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::RRCA(Gameboy &gameboy) {
    gameboy.regs->SetCarry((gameboy.regs->a & 0x01) != 0);
    gameboy.regs->a = gameboy.regs->a >> 1 | (gameboy.regs->a & 0x01) << 7;
    gameboy.regs->SetZero(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::RRC(const ArithmeticSource source, Gameboy &gameboy) {
    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        default: throw UnreachableCodeException("Instructions::RRC");
    }

    if (source == ArithmeticSource::HLAddr) {
        const bool carry = (value & 0x01) == 0x01;
        gameboy.regs->SetCarry(carry);
        if (carry) {
            value = 0x80 | value >> 1;
        } else {
            value = value >> 1;
        }

        gameboy.bus->WriteByte(gameboy.regs->GetHL(), value);
        gameboy.regs->SetZero(value == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.pc += 2;
        return 16;
    }

    bool carry = (value & 0x01) == 0x01;
    gameboy.regs->SetCarry(carry);
    uint8_t newValue = 0;
    if (gameboy.regs->FlagCarry()) {
        newValue = 0x80 | value >> 1;
    } else {
        newValue = value >> 1;
    }
    gameboy.regs->SetZero(newValue == 0);

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a = newValue;
            break;
        case ArithmeticSource::B: gameboy.regs->b = newValue;
            break;
        case ArithmeticSource::C: gameboy.regs->c = newValue;
            break;
        case ArithmeticSource::D: gameboy.regs->d = newValue;
            break;
        case ArithmeticSource::E: gameboy.regs->e = newValue;
            break;
        case ArithmeticSource::H: gameboy.regs->h = newValue;
            break;
        case ArithmeticSource::L: gameboy.regs->l = newValue;
            break;
        default: throw UnreachableCodeException("Instructions::RRC");
    }

    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    gameboy.pc += 2;
    return 8;
}

uint8_t Instructions::RR(const ArithmeticSource source, Gameboy &gameboy) {
    if (source == ArithmeticSource::HLAddr) {
        uint8_t value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        bool carry = (value & 0x01) == 0x01;
        if (gameboy.regs->FlagCarry()) { value = 0x80 | (value >> 1); } else { value = value >> 1; }
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), value);
        gameboy.regs->SetCarry(carry);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.regs->SetZero(value == 0);
        gameboy.pc += 2;
        return 16;
    }

    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        default: throw UnreachableCodeException("Instructions::RR");
    }

    const bool carry = (value & 0x01) == 0x01;
    uint8_t newValue = 0;
    if (gameboy.regs->FlagCarry()) {
        newValue = 0x80 | (value >> 1);
    } else {
        newValue = value >> 1;
    }

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a = newValue;
            break;
        case ArithmeticSource::B: gameboy.regs->b = newValue;
            break;
        case ArithmeticSource::C: gameboy.regs->c = newValue;
            break;
        case ArithmeticSource::D: gameboy.regs->d = newValue;
            break;
        case ArithmeticSource::E: gameboy.regs->e = newValue;
            break;
        case ArithmeticSource::H: gameboy.regs->h = newValue;
            break;
        case ArithmeticSource::L: gameboy.regs->l = newValue;
            break;
        default: throw UnreachableCodeException("Instructions::RR");
    }

    gameboy.regs->SetCarry(carry);
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    gameboy.pc += 2;
    return 8;
}

uint8_t Instructions::RRA(Gameboy &gameboy) {
    const bool carry = (gameboy.regs->a & 0x01) == 0x01;
    uint8_t newValue = 0;
    if (gameboy.regs->FlagCarry()) {
        newValue = 0x80 | (gameboy.regs->a >> 1);
    } else {
        newValue = gameboy.regs->a >> 1;
    }

    gameboy.regs->SetZero(false);
    gameboy.regs->a = newValue;
    gameboy.regs->SetCarry(carry);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::RET(const JumpTest test, Gameboy &gameboy) {
    bool jumpCondition = false;
    switch (test) {
        case JumpTest::NotZero: jumpCondition = !gameboy.regs->FlagZero();
            break;
        case JumpTest::Zero: jumpCondition = gameboy.regs->FlagZero();
            break;
        case JumpTest::Carry: jumpCondition = gameboy.regs->FlagCarry();
            break;
        case JumpTest::NotCarry: jumpCondition = !gameboy.regs->FlagCarry();
            break;
        case JumpTest::Always: jumpCondition = true;
            break;
    }

    return test == JumpTest::Always ? return_(jumpCondition, gameboy) - 4 : return_(jumpCondition, gameboy);
}

uint8_t Instructions::JR(const JumpTest test, Gameboy &gameboy) {
    bool jumpCondition = false;
    switch (test) {
        case JumpTest::NotZero: jumpCondition = !gameboy.regs->FlagZero();
            break;
        case JumpTest::Zero: jumpCondition = gameboy.regs->FlagZero();
            break;
        case JumpTest::Carry: jumpCondition = gameboy.regs->FlagCarry();
            break;
        case JumpTest::NotCarry: jumpCondition = !gameboy.regs->FlagCarry();
            break;
        case JumpTest::Always: jumpCondition = true;
            break;
    }

    return jumpRelative(jumpCondition, gameboy);
}

uint8_t Instructions::JP(const JumpTest test, Gameboy &gameboy) {
    bool jumpCondition = false;
    switch (test) {
        case JumpTest::NotZero: jumpCondition = !gameboy.regs->FlagZero();
            break;
        case JumpTest::Zero: jumpCondition = gameboy.regs->FlagZero();
            break;
        case JumpTest::Carry: jumpCondition = gameboy.regs->FlagCarry();
            break;
        case JumpTest::NotCarry: jumpCondition = !gameboy.regs->FlagCarry();
            break;
        case JumpTest::Always: jumpCondition = true;
            break;
    }

    return jump(jumpCondition, gameboy);
}

uint8_t Instructions::JPHL(Gameboy &gameboy) {
    gameboy.pc = gameboy.regs->GetHL();
    return 4;
}

uint8_t Instructions::NOP(Gameboy &gameboy) {
    gameboy.pc += 1;
    return 4;
}

uint8_t Instructions::DEC(const IncDecTarget target, Gameboy &gameboy) {
    switch (target) {
        case IncDecTarget::A: gameboy.regs->a = decrement(gameboy.regs->a, gameboy);
            break;
        case IncDecTarget::B: gameboy.regs->b = decrement(gameboy.regs->b, gameboy);
            break;
        case IncDecTarget::C: gameboy.regs->c = decrement(gameboy.regs->c, gameboy);
            break;
        case IncDecTarget::D: gameboy.regs->d = decrement(gameboy.regs->d, gameboy);
            break;
        case IncDecTarget::E: gameboy.regs->e = decrement(gameboy.regs->e, gameboy);
            break;
        case IncDecTarget::H: gameboy.regs->h = decrement(gameboy.regs->h, gameboy);
            break;
        case IncDecTarget::L: gameboy.regs->l = decrement(gameboy.regs->l, gameboy);
            break;
        case IncDecTarget::HLAddr: {
            const uint8_t byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            const uint8_t new_value = decrement(byte, gameboy);
            gameboy.bus->WriteByte(gameboy.regs->GetHL(), new_value);
            break;
        }
        case IncDecTarget::HL:
            gameboy.regs->SetHL(gameboy.regs->GetHL() - 1);
            break;
        case IncDecTarget::BC:
            gameboy.regs->SetBC(gameboy.regs->GetBC() - 1);
            break;
        case IncDecTarget::DE:
            gameboy.regs->SetDE(gameboy.regs->GetDE() - 1);
            break;
        case IncDecTarget::SP:
            gameboy.sp -= 1;
            break;
    }

    switch (target) {
        case IncDecTarget::HLAddr:
            gameboy.pc += 1;
            return 12;
            break;
        case IncDecTarget::HL:
        case IncDecTarget::BC:
        case IncDecTarget::DE:
        case IncDecTarget::SP:
            gameboy.pc += 1;
            return 8;
            break;
        default:
            gameboy.pc += 1;
            return 4;
    }
}

uint8_t Instructions::INC(const IncDecTarget target, Gameboy &gameboy) {
    switch (target) {
        case IncDecTarget::A: gameboy.regs->a = increment(gameboy.regs->a, gameboy);
            break;
        case IncDecTarget::B: gameboy.regs->b = increment(gameboy.regs->b, gameboy);
            break;
        case IncDecTarget::C: gameboy.regs->c = increment(gameboy.regs->c, gameboy);
            break;
        case IncDecTarget::D: gameboy.regs->d = increment(gameboy.regs->d, gameboy);
            break;
        case IncDecTarget::E: gameboy.regs->e = increment(gameboy.regs->e, gameboy);
            break;
        case IncDecTarget::H: gameboy.regs->h = increment(gameboy.regs->h, gameboy);
            break;
        case IncDecTarget::L: gameboy.regs->l = increment(gameboy.regs->l, gameboy);
            break;
        case IncDecTarget::HLAddr: {
            const uint8_t byte = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            const uint8_t new_value = increment(byte, gameboy);
            gameboy.bus->WriteByte(gameboy.regs->GetHL(), new_value);
            break;
        }
        case IncDecTarget::HL:
            gameboy.regs->SetHL(gameboy.regs->GetHL() + 1);
            break;
        case IncDecTarget::BC:
            gameboy.regs->SetBC(gameboy.regs->GetBC() + 1);
            break;
        case IncDecTarget::DE:
            gameboy.regs->SetDE(gameboy.regs->GetDE() + 1);
            break;
        case IncDecTarget::SP:
            gameboy.sp += 1;
            break;
    }

    switch (target) {
        case IncDecTarget::HLAddr:
            gameboy.pc += 1;
            return 12;
            break;
        case IncDecTarget::HL:
        case IncDecTarget::BC:
        case IncDecTarget::DE:
        case IncDecTarget::SP:
            gameboy.pc += 1;
            return 8;
            break;
        default:
            gameboy.pc += 1;
            return 4;
    }
}

uint8_t Instructions::LDH(LoadOtherTarget target, LoadOtherSource source, Gameboy &gameboy) {
    if ((target == LoadOtherTarget::A8) && (source == LoadOtherSource::A)) {
        // E0
        uint16_t a = 0xFF00 | static_cast<uint16_t>(ReadNextByte(gameboy));
        gameboy.bus->WriteByte(a, gameboy.regs->a);
        gameboy.pc += 2;
        return 12;
    } else if ((target == LoadOtherTarget::CAddress) && (source == LoadOtherSource::A)) {
        // E2
        uint16_t c = 0xFF00 | static_cast<uint16_t>(gameboy.regs->c);
        gameboy.bus->WriteByte(c, gameboy.regs->a);
        gameboy.pc += 1;
        return 8;
    } else if ((target == LoadOtherTarget::A) && (source == LoadOtherSource::A8)) {
        // F0
        uint16_t a = 0xFF00 | static_cast<uint16_t>(ReadNextByte(gameboy));
        gameboy.regs->a = gameboy.bus->ReadByte(a);
        gameboy.pc += 2;
        return 12;
    } else if ((target == LoadOtherTarget::A) && (source == LoadOtherSource::CAddress)) {
        // F2
        uint16_t a = 0xFF00 | static_cast<uint16_t>(gameboy.regs->c);
        gameboy.regs->a = gameboy.bus->ReadByte(a);
        gameboy.pc += 1;
        return 8;
    }

    return 0;
}

uint8_t Instructions::LD(const LoadByteTarget target, const LoadByteSource source, Gameboy &gameboy) {
    uint8_t sourceValue = 0;
    switch (source) {
        case LoadByteSource::A: sourceValue = gameboy.regs->a;
            break;
        case LoadByteSource::B: sourceValue = gameboy.regs->b;
            break;
        case LoadByteSource::C: sourceValue = gameboy.regs->c;
            break;
        case LoadByteSource::D: sourceValue = gameboy.regs->d;
            break;
        case LoadByteSource::E: sourceValue = gameboy.regs->e;
            break;
        case LoadByteSource::H: sourceValue = gameboy.regs->h;
            break;
        case LoadByteSource::L: sourceValue = gameboy.regs->l;
            break;
        case LoadByteSource::D8: sourceValue = ReadNextByte(gameboy);
            break;
        case LoadByteSource::HL: sourceValue = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case LoadByteSource::BC: sourceValue = gameboy.bus->ReadByte(gameboy.regs->GetBC());
            break;
        case LoadByteSource::DE: sourceValue = gameboy.bus->ReadByte(gameboy.regs->GetDE());
            break;
        case LoadByteSource::HLI: {
            sourceValue = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            gameboy.regs->SetHL(gameboy.regs->GetHL() + 1);
            break;
        }
        case LoadByteSource::HLD: {
            sourceValue = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            gameboy.regs->SetHL(gameboy.regs->GetHL() - 1);
            break;
        }
        case LoadByteSource::A16: sourceValue = gameboy.bus->ReadByte(ReadNextWord(gameboy));
            break;
    }

    switch (target) {
        case LoadByteTarget::A: gameboy.regs->a = sourceValue;
            break;
        case LoadByteTarget::B: gameboy.regs->b = sourceValue;
            break;
        case LoadByteTarget::C: gameboy.regs->c = sourceValue;
            break;
        case LoadByteTarget::D: gameboy.regs->d = sourceValue;
            break;
        case LoadByteTarget::E: gameboy.regs->e = sourceValue;
            break;
        case LoadByteTarget::H: gameboy.regs->h = sourceValue;
            break;
        case LoadByteTarget::L: gameboy.regs->l = sourceValue;
            break;
        case LoadByteTarget::HL: gameboy.bus->WriteByte(gameboy.regs->GetHL(), sourceValue);
            break;
        case LoadByteTarget::BC: gameboy.bus->WriteByte(gameboy.regs->GetBC(), sourceValue);
            break;
        case LoadByteTarget::DE: gameboy.bus->WriteByte(gameboy.regs->GetDE(), sourceValue);
            break;
        case LoadByteTarget::HLI: {
            gameboy.bus->WriteByte(gameboy.regs->GetHL(), sourceValue);
            gameboy.regs->SetHL(gameboy.regs->GetHL() + 1);
            break;
        }
        case LoadByteTarget::HLD: {
            gameboy.bus->WriteByte(gameboy.regs->GetHL(), sourceValue);
            gameboy.regs->SetHL(gameboy.regs->GetHL() - 1);
            break;
        }
        case LoadByteTarget::A16: {
            gameboy.bus->WriteByte(ReadNextWord(gameboy), sourceValue);
            gameboy.pc += 3;
            return 16;
            break;
        }
    }

    if (source == LoadByteSource::D8) {
        if (target == LoadByteTarget::HL) {
            gameboy.pc += 2;
            return 12;
        } else {
            gameboy.pc += 2;
            return 8;
        }
    } else if (source == LoadByteSource::HL) {
        gameboy.pc += 1;
        return 8;
    } else if (target == LoadByteTarget::HL
               || target == LoadByteTarget::HLD
               || target == LoadByteTarget::HLI) {
        gameboy.pc += 1;
        return 8;
    } else if ((target == LoadByteTarget::BC) || (target == LoadByteTarget::DE)) {
        gameboy.pc += 1;
        return 8;
    } else if (source == LoadByteSource::BC
               || (source == LoadByteSource::DE)
               || (source == LoadByteSource::HLD)
               || (source == LoadByteSource::HLI)) {
        gameboy.pc += 1;
        return 8;
    } else if (source == LoadByteSource::A16) {
        gameboy.pc += 3;
        return 16;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}

uint8_t Instructions::LD16(const LoadWordTarget target, const LoadWordSource source, Gameboy &gameboy) {
    uint16_t sourceValue = 0;
    switch (source) {
        case LoadWordSource::D16: sourceValue = ReadNextWord(gameboy);
            break;
        case LoadWordSource::SP: sourceValue = gameboy.sp;
            break;
        case LoadWordSource::HL: sourceValue = gameboy.regs->GetHL();
            break;
        case LoadWordSource::SPr8: sourceValue = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
                                       ReadNextByte(gameboy))));
            break;
    }

    switch (target) {
        case LoadWordTarget::BC: gameboy.regs->SetBC(sourceValue);
            break;
        case LoadWordTarget::DE: gameboy.regs->SetDE(sourceValue);
            break;
        case LoadWordTarget::SP: gameboy.sp = sourceValue;
            break;
        case LoadWordTarget::HL: {
            if (source == LoadWordSource::SPr8) {
                gameboy.regs->SetCarry((gameboy.sp & 0xFF) + (sourceValue & 0xFF) > 0xFF);
                gameboy.regs->SetHalf((gameboy.sp & 0xF) + (sourceValue & 0xF) > 0xF);
                gameboy.regs->SetSubtract(false);
                gameboy.regs->SetZero(false);
                gameboy.regs->SetHL(gameboy.sp + sourceValue);
            } else {
                gameboy.regs->SetHL(sourceValue);
            }
            break;
        }
        case LoadWordTarget::A16: gameboy.bus->WriteWord(ReadNextWord(gameboy), sourceValue);
            break;
    }

    if (source == LoadWordSource::HL) {
        gameboy.pc += 1;
        return 8;
    } else if (source == LoadWordSource::SPr8) {
        gameboy.pc += 2;
        return 12;
    } else if (source == LoadWordSource::D16) {
        gameboy.pc += 3;
        return 12;
    } else if (source == LoadWordSource::SP) {
        gameboy.pc += 3;
        return 20;
    }

    return 0;
}

uint8_t Instructions::PUSH(const StackTarget target, Gameboy &gameboy) {
    uint16_t value = 0;
    switch (target) {
        case StackTarget::BC: value = gameboy.regs->GetBC();
            break;
        case StackTarget::DE: value = gameboy.regs->GetDE();
            break;
        case StackTarget::HL: value = gameboy.regs->GetHL();
            break;
        case StackTarget::AF: value = gameboy.regs->GetAF();
            break;
    }

    push(value, gameboy);
    gameboy.pc += 1;
    return 16;
}

uint8_t Instructions::POP(const StackTarget target, Gameboy &gameboy) {
    const uint16_t result = pop(gameboy);
    switch (target) {
        case StackTarget::BC: gameboy.regs->SetBC(result);
            break;
        case StackTarget::DE: gameboy.regs->SetDE(result);
            break;
        case StackTarget::HL: gameboy.regs->SetHL(result);
            break;
        case StackTarget::AF: gameboy.regs->SetAF(result & 0xFFF0);
            break;
    }

    gameboy.pc += 1;
    return 12;
}

uint8_t Instructions::CP(const ArithmeticSource source, Gameboy &gameboy) {
    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case ArithmeticSource::U8: value = ReadNextByte(gameboy);
            break;
        default: throw UnreachableCodeException("Improper CP source");
    }

    subtract(value, gameboy);

    if (source == ArithmeticSource::U8) {
        gameboy.pc += 2;
        return 8;
    } else if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 1;
        return 8;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}

uint8_t Instructions::OR(const ArithmeticSource source, Gameboy &gameboy) {
    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case ArithmeticSource::U8: value = ReadNextByte(gameboy);
            break;
        default: throw UnreachableCodeException("Improper OR source");
    }

    gameboy.regs->a |= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(false);
    if (source == ArithmeticSource::U8) {
        gameboy.pc += 2;
        return 8;
    } else if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 1;
        return 8;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}

uint8_t Instructions::XOR(const ArithmeticSource source, Gameboy &gameboy) {
    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case ArithmeticSource::U8: value = ReadNextByte(gameboy);
            break;
        default: throw UnreachableCodeException("Improper XOR source");
    }

    gameboy.regs->a ^= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(false);
    if (source == ArithmeticSource::U8) {
        gameboy.pc += 2;
        return 8;
    } else if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 1;
        return 8;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}

uint8_t Instructions::AND(const ArithmeticSource source, Gameboy &gameboy) {
    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case ArithmeticSource::U8: value = ReadNextByte(gameboy);
            break;
        default: throw UnreachableCodeException("Improper AND source");
    }

    gameboy.regs->a &= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(true);
    gameboy.regs->SetCarry(false);
    if (source == ArithmeticSource::U8) {
        gameboy.pc += 2;
        return 8;
    } else if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 1;
        return 8;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}

uint8_t Instructions::SUB(const ArithmeticSource source, Gameboy &gameboy) {
    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case ArithmeticSource::U8: value = ReadNextByte(gameboy);
            break;
        default: throw UnreachableCodeException("Improper SUB source");
    }

    gameboy.regs->a = subtract(value, gameboy);
    if (source == ArithmeticSource::U8) {
        gameboy.pc += 2;
        return 8;
    } else if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 1;
        return 8;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}

uint8_t Instructions::SLA(const ArithmeticSource source, Gameboy &gameboy) {
    if (source == ArithmeticSource::HLAddr) {
        uint8_t value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        gameboy.regs->SetCarry((value & 0x80) != 0);
        value <<= 1;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), value);
        gameboy.regs->SetZero(value == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.pc += 2;
        return 16;
    }

    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        default: throw UnreachableCodeException("Improper SLA source");
    }

    gameboy.regs->SetCarry(((value & 0x80) >> 7) == 0x01);
    const uint8_t newValue = value << 1;

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a = newValue;
            break;
        case ArithmeticSource::B: gameboy.regs->b = newValue;
            break;
        case ArithmeticSource::C: gameboy.regs->c = newValue;
            break;
        case ArithmeticSource::D: gameboy.regs->d = newValue;
            break;
        case ArithmeticSource::E: gameboy.regs->e = newValue;
            break;
        case ArithmeticSource::H: gameboy.regs->h = newValue;
            break;
        case ArithmeticSource::L: gameboy.regs->l = newValue;
            break;
        default: throw UnreachableCodeException("Improper SLA source");
    }

    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    gameboy.pc += 2;
    return 8;
}

uint8_t Instructions::SRA(ArithmeticSource source, Gameboy &gameboy) {
    if (source == ArithmeticSource::HLAddr) {
        uint8_t value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        gameboy.regs->SetCarry((value & 0x01) != 0);
        value = (value >> 1) | (value & 0x80);
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), value);
        gameboy.regs->SetZero(value == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.pc += 2;
        return 16;
    }

    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        default: throw UnreachableCodeException("Improper SRA source");
    }

    gameboy.regs->SetCarry((value & 0x01) != 0);
    const uint8_t newValue = (value >> 1) | (value & 0x80);

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a = newValue;
            break;
        case ArithmeticSource::B: gameboy.regs->b = newValue;
            break;
        case ArithmeticSource::C: gameboy.regs->c = newValue;
            break;
        case ArithmeticSource::D: gameboy.regs->d = newValue;
            break;
        case ArithmeticSource::E: gameboy.regs->e = newValue;
            break;
        case ArithmeticSource::H: gameboy.regs->h = newValue;
            break;
        case ArithmeticSource::L: gameboy.regs->l = newValue;
            break;
        default: throw UnreachableCodeException("Improper SRA source");
    }

    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    gameboy.pc += 2;
    return 8;
}

uint8_t Instructions::SWAP(ArithmeticSource source, Gameboy &gameboy) {
    if (source == ArithmeticSource::HLAddr) {
        uint8_t value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        value = (value >> 4) | (value << 4);
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), value);
        gameboy.regs->SetCarry(false);
        gameboy.regs->SetZero(value == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.pc += 2;
        return 16;
    }

    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        default: throw UnreachableCodeException("Improper SWAP source");
    }

    const uint8_t newValue = value >> 4 | value << 4;

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a = newValue;
            break;
        case ArithmeticSource::B: gameboy.regs->b = newValue;
            break;
        case ArithmeticSource::C: gameboy.regs->c = newValue;
            break;
        case ArithmeticSource::D: gameboy.regs->d = newValue;
            break;
        case ArithmeticSource::E: gameboy.regs->e = newValue;
            break;
        case ArithmeticSource::H: gameboy.regs->h = newValue;
            break;
        case ArithmeticSource::L: gameboy.regs->l = newValue;
            break;
        default: throw UnreachableCodeException("Improper SWAP source");
    }

    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetCarry(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    gameboy.pc += 2;
    return 8;
}

uint8_t Instructions::SRL(ArithmeticSource source, Gameboy &gameboy) {
    if (source == ArithmeticSource::HLAddr) {
        uint8_t value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
        gameboy.regs->SetCarry((value & 0x01) != 0);
        value >>= 1;
        gameboy.bus->WriteByte(gameboy.regs->GetHL(), value);
        gameboy.regs->SetZero(value == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        gameboy.pc += 2;
        return 16;
    }

    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        default: throw UnreachableCodeException("Improper SRL source");
    }

    gameboy.regs->SetCarry((value & 0x01) != 0);
    value = value >> 1;

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a = value;
            break;
        case ArithmeticSource::B: gameboy.regs->b = value;
            break;
        case ArithmeticSource::C: gameboy.regs->c = value;
            break;
        case ArithmeticSource::D: gameboy.regs->d = value;
            break;
        case ArithmeticSource::E: gameboy.regs->e = value;
            break;
        case ArithmeticSource::H: gameboy.regs->h = value;
            break;
        case ArithmeticSource::L: gameboy.regs->l = value;
            break;
        default: throw UnreachableCodeException("Improper SRL source");
    }

    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    gameboy.pc += 2;
    return 8;
}

uint8_t Instructions::BIT(const uint8_t target, const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = 1 << target;

    bool zero = true;
    switch (source) {
        case ArithmeticSource::A: zero = (gameboy.regs->a & value) == 0;
            break;
        case ArithmeticSource::B: zero = (gameboy.regs->b & value) == 0;
            break;
        case ArithmeticSource::C: zero = (gameboy.regs->c & value) == 0;
            break;
        case ArithmeticSource::D: zero = (gameboy.regs->d & value) == 0;
            break;
        case ArithmeticSource::E: zero = (gameboy.regs->e & value) == 0;
            break;
        case ArithmeticSource::H: zero = (gameboy.regs->h & value) == 0;
            break;
        case ArithmeticSource::L: zero = (gameboy.regs->l & value) == 0;
            break;
        case ArithmeticSource::HLAddr: zero = (gameboy.bus->ReadByte(gameboy.regs->GetHL()) & value) == 0;
            break;
        default: throw UnreachableCodeException("Invalid source");
    }

    gameboy.regs->SetZero(zero);
    gameboy.regs->SetHalf(true);
    gameboy.regs->SetSubtract(false);

    if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 2;
        return 12;
    } else {
        gameboy.pc += 2;
        return 8;
    }
}

uint8_t Instructions::RES(const uint8_t target, const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = 1 << target;

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a &= ~value;
            break;
        case ArithmeticSource::B: gameboy.regs->b &= ~value;
            break;
        case ArithmeticSource::C: gameboy.regs->c &= ~value;
            break;
        case ArithmeticSource::D: gameboy.regs->d &= ~value;
            break;
        case ArithmeticSource::E: gameboy.regs->e &= ~value;
            break;
        case ArithmeticSource::H: gameboy.regs->h &= ~value;
            break;
        case ArithmeticSource::L: gameboy.regs->l &= ~value;
            break;
        case ArithmeticSource::HLAddr: gameboy.bus->WriteByte(gameboy.regs->GetHL(),
                                                              gameboy.bus->ReadByte(gameboy.regs->GetHL()) & ~value);
            break;
        default: throw UnreachableCodeException("Invalid RES source");
    }

    gameboy.pc += 2;
    return source == ArithmeticSource::HLAddr ? 16 : 8;
}

uint8_t Instructions::SET(const uint8_t target, const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = 1 << target;

    switch (source) {
        case ArithmeticSource::A: gameboy.regs->a |= value;
            break;
        case ArithmeticSource::B: gameboy.regs->b |= value;
            break;
        case ArithmeticSource::C: gameboy.regs->c |= value;
            break;
        case ArithmeticSource::D: gameboy.regs->d |= value;
            break;
        case ArithmeticSource::E: gameboy.regs->e |= value;
            break;
        case ArithmeticSource::H: gameboy.regs->h |= value;
            break;
        case ArithmeticSource::L: gameboy.regs->l |= value;
            break;
        case ArithmeticSource::HLAddr: gameboy.bus->WriteByte(gameboy.regs->GetHL(),
                                                              gameboy.bus->ReadByte(gameboy.regs->GetHL()) | value);
            break;
        default: throw UnreachableCodeException("Improper SET source");
    }

    gameboy.pc += 2;
    return source == ArithmeticSource::HLAddr ? 16 : 8;
}

uint8_t Instructions::SBC(ArithmeticSource source, Gameboy &gameboy) {
    uint16_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case ArithmeticSource::U8: value = ReadNextByte(gameboy);
            break;
        default: throw UnreachableCodeException("Improper SBC source");
    }

    const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
    const uint8_t r = gameboy.regs->a - value - flag_carry;
    gameboy.regs->SetCarry(gameboy.regs->a < value + static_cast<uint16_t>(flag_carry));
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (value & 0xF) + flag_carry);
    gameboy.regs->SetSubtract(true);
    gameboy.regs->SetZero(r == 0x0);
    gameboy.regs->a = r;

    if (source == ArithmeticSource::U8) {
        gameboy.pc += 2;
        return 8;
    } else if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 1;
        return 8;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}

uint8_t Instructions::ADC(ArithmeticSource source, Gameboy &gameboy) {
    uint16_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case ArithmeticSource::U8: value = ReadNextByte(gameboy);
            break;
        default: throw UnreachableCodeException("Improper ADC source");
    }

    const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
    const uint8_t r = gameboy.regs->a + value + flag_carry;
    gameboy.regs->SetCarry(static_cast<uint16_t>(gameboy.regs->a) + value + static_cast<uint16_t>(flag_carry) > 0xFF);
    gameboy.regs->SetHalf(((gameboy.regs->a & 0xF) + (value & 0xF) + (flag_carry & 0xF)) > 0xF);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetZero(r == 0x0);
    gameboy.regs->a = r;

    if (source == ArithmeticSource::U8) {
        gameboy.pc += 2;
        return 8;
    } else if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 1;
        return 8;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}

uint8_t Instructions::ADD16(Arithmetic16Target target, Gameboy &gameboy) {
    uint16_t sourceValue = 0;
    switch (target) {
        case Arithmetic16Target::BC: sourceValue = gameboy.regs->GetBC();
            break;
        case Arithmetic16Target::DE: sourceValue = gameboy.regs->GetDE();
            break;
        case Arithmetic16Target::HL: sourceValue = gameboy.regs->GetHL();
            break;
        case Arithmetic16Target::SP: sourceValue = gameboy.sp;
            break;
    }

    const uint16_t reg = gameboy.regs->GetHL();
    const uint16_t sum = reg + sourceValue;
    gameboy.regs->SetCarry(reg > 0xFFFF - sourceValue);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf((reg & 0x07FF) + (sourceValue & 0x07FF) > 0x07FF);
    gameboy.regs->SetHL(sum);
    gameboy.pc += 1;
    return 8;
}

uint8_t Instructions::ADD(const ArithmeticSource source, Gameboy &gameboy) {
    if (source == ArithmeticSource::I8) {
        const auto source_value = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
            ReadNextByte(gameboy))));
        const uint16_t sp_value = gameboy.sp;
        gameboy.regs->SetCarry(((sp_value & 0xFF) + (source_value & 0xFF)) > 0xFF);
        gameboy.regs->SetHalf(((sp_value & 0xF) + (source_value & 0xF)) > 0xF);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetZero(false);
        gameboy.sp = sp_value + source_value;
        gameboy.pc += 2;
        return 16;
    }

    uint8_t value = 0;
    switch (source) {
        case ArithmeticSource::A: value = gameboy.regs->a;
            break;
        case ArithmeticSource::B: value = gameboy.regs->b;
            break;
        case ArithmeticSource::C: value = gameboy.regs->c;
            break;
        case ArithmeticSource::D: value = gameboy.regs->d;
            break;
        case ArithmeticSource::E: value = gameboy.regs->e;
            break;
        case ArithmeticSource::H: value = gameboy.regs->h;
            break;
        case ArithmeticSource::L: value = gameboy.regs->l;
            break;
        case ArithmeticSource::HLAddr: value = gameboy.bus->ReadByte(gameboy.regs->GetHL());
            break;
        case ArithmeticSource::U8: value = ReadNextByte(gameboy);
            break;
        default: throw UnreachableCodeException("Invalid source");
    }

    gameboy.regs->a = addition(value, gameboy);

    if (source == ArithmeticSource::U8) {
        gameboy.pc += 2;
        return 8;
    } else if (source == ArithmeticSource::HLAddr) {
        gameboy.pc += 1;
        return 8;
    } else {
        gameboy.pc += 1;
        return 4;
    }
}


uint8_t Instructions::increment(const uint8_t reg, const Gameboy &gameboy) {
    gameboy.regs->SetHalf((reg & 0xF) == 0xF);
    const uint8_t new_value = reg + 1;
    gameboy.regs->SetZero(new_value == 0);
    gameboy.regs->SetSubtract(false);
    return new_value;
}

uint8_t Instructions::decrement(const uint8_t reg, const Gameboy &gameboy) {
    gameboy.regs->SetHalf((reg & 0xF) == 0x00);
    const uint8_t new_value = reg - 1;
    gameboy.regs->SetZero(new_value == 0);
    gameboy.regs->SetSubtract(true);
    return new_value;
}

uint8_t Instructions::subtract(const uint8_t value, const Gameboy &gameboy) {
    const uint8_t new_value = gameboy.regs->a - value;
    gameboy.regs->SetCarry(gameboy.regs->a < value);
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (value & 0xF));
    gameboy.regs->SetSubtract(true);
    gameboy.regs->SetZero(new_value == 0x0);
    return new_value;
}

uint8_t Instructions::addition(const uint8_t value, const Gameboy &gameboy) {
    const uint8_t a = gameboy.regs->a;
    const uint8_t new_value = a + value;
    gameboy.regs->SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(value) > 0xFF);
    gameboy.regs->SetZero(new_value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) + (value & 0xF) > 0xF);
    return new_value;
}

uint8_t Instructions::jump(const bool shouldJump, Gameboy &gameboy) {
    if (shouldJump) {
        const uint16_t lower_byte = gameboy.bus->ReadByte(gameboy.pc + 1);
        const uint16_t higher_byte = gameboy.bus->ReadByte(gameboy.pc + 2);
        gameboy.pc = (higher_byte << 8) | lower_byte;
        return 16;
    }
    gameboy.pc += 3;
    return 12;
}

uint8_t Instructions::jumpRelative(const bool shouldJump, Gameboy &gameboy) {
    const uint16_t next = gameboy.pc + 2;
    if (shouldJump) {
        if (const auto byte = static_cast<int8_t>(gameboy.bus->ReadByte(gameboy.pc + 1)); byte >= 0) {
            gameboy.pc = next + static_cast<uint16_t>(byte);
        } else {
            gameboy.pc = next - static_cast<uint16_t>(abs(byte));
        }

        return 12;
    }
    gameboy.pc = next;
    return 8;
}

void Instructions::push(const uint16_t value, Gameboy &gameboy) {
    gameboy.sp -= 1;
    gameboy.bus->WriteByte(gameboy.sp, (value & 0xFF00) >> 8);
    gameboy.sp -= 1;
    gameboy.bus->WriteByte(gameboy.sp, value & 0xFF);
}

uint16_t Instructions::pop(Gameboy &gameboy) {
    const uint16_t lsb = gameboy.bus->ReadByte(gameboy.sp);
    gameboy.sp += 1;
    const uint16_t msb = gameboy.bus->ReadByte(gameboy.sp);
    gameboy.sp += 1;

    return msb << 8 | lsb;
}

uint8_t Instructions::call(const bool shouldJump, Gameboy &gameboy) {
    const uint16_t next = gameboy.pc + 3;
    if (shouldJump) {
        push(next, gameboy);
        gameboy.pc = ReadNextWord(gameboy);
        return 24;
    }
    gameboy.pc = next;
    return 12;
}

uint8_t Instructions::return_(const bool shouldJump, Gameboy &gameboy) {
    if (shouldJump) {
        gameboy.pc = pop(gameboy);
        return 20;
    }
    gameboy.pc += 1;
    return 8;
}
