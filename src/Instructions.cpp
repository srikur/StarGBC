#include "Instructions.h"

uint16_t Instructions::ReadNextWord(Gameboy &gameboy) {
    const uint16_t lower = ReadNextByte(gameboy);
    const uint16_t higher = ReadNextByte(gameboy);
    const uint16_t word = higher << 8 | lower;
    return word;
}

uint8_t Instructions::ReadNextByte(Gameboy &gameboy) {
    const uint16_t byte = gameboy.bus->ReadByte(gameboy.pc++);
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
    return byte;
}

uint8_t Instructions::ReadByte(Gameboy &gameboy, const uint16_t address) {
    const uint8_t byte = gameboy.bus->ReadByte(address);
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
    return byte;
}

void Instructions::WriteWord(Gameboy &gameboy, const uint16_t address, const uint16_t value) {
    WriteByte(gameboy, address, static_cast<uint8_t>(value & 0xFF));
    WriteByte(gameboy, address + 1, static_cast<uint8_t>(value >> 8));
}

void Instructions::WriteByte(Gameboy &gameboy, const uint16_t address, const uint8_t value) {
    gameboy.bus->WriteByte(address, value);
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
}

uint8_t Instructions::RLCAddr(Gameboy &gameboy) {
    uint8_t byte = ReadByte(gameboy, gameboy.regs->GetHL());
    const uint8_t old = (byte & 0x80) != 0 ? 1 : 0;
    gameboy.regs->SetCarry(old != 0);
    byte = byte << 1 | old;
    WriteByte(gameboy, gameboy.regs->GetHL(), byte);
    gameboy.regs->SetZero(byte == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    return 4;
}

uint8_t Instructions::RLC(const ArithmeticSource source, const Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        using enum ArithmeticSource;
        switch (source) {
            case A: return gameboy.regs->a;
            case B: return gameboy.regs->b;
            case C: return gameboy.regs->c;
            case D: return gameboy.regs->d;
            case E: return gameboy.regs->e;
            case H: return gameboy.regs->h;
            case L: return gameboy.regs->l;
            default: throw UnreachableCodeException("Instructions::RLC -- improper ArithmeticSource");
        }
    }();

    const uint8_t old = (value & 0x80) != 0 ? 1 : 0;
    gameboy.regs->SetCarry(old != 0);
    const uint8_t newValue = (value << 1) | old;
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
        default: throw UnreachableCodeException("Instructions::RLC -- improper ArithmeticSource");
    }

    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    return 2;
}

uint8_t Instructions::DAA(const Gameboy &gameboy) {
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

    return 1;
}


uint8_t Instructions::RETI(Gameboy &gameboy) {
    const uint16_t newPC = pop(gameboy);
    gameboy.pc = newPC;
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
    gameboy.bus->interruptMasterEnable = true;
    return 4;
}

uint8_t Instructions::DI(const Gameboy &gameboy) {
    gameboy.bus->interruptDelay = false;
    gameboy.bus->interruptMasterEnable = false;
    return 1;
}

uint8_t Instructions::EI(Gameboy &gameboy) {
    if (!gameboy.bus->interruptMasterEnable) {
        gameboy.icount = 0;
        gameboy.bus->interruptDelay = true;
        gameboy.bus->interruptMasterEnable = true;
    }
    return 1;
}

uint8_t Instructions::HALT(Gameboy &gameboy) {
    if (const bool bug = (gameboy.bus->interruptEnable & gameboy.bus->interruptFlag & 0x1F) != 0;
        !gameboy.bus->interruptMasterEnable && bug) {
        gameboy.haltBug = true;
        gameboy.halted = false;
    } else {
        gameboy.haltBug = false;
        gameboy.halted = true;
    }

    return 1;
}


uint8_t Instructions::RST(const RSTTargets target, Gameboy &gameboy) {
    const uint16_t location = [&]() -> uint16_t {
        using enum RSTTargets;
        switch (target) {
            case H00: return 0x00;
            case H08: return 0x08;
            case H10: return 0x10;
            case H18: return 0x18;
            case H20: return 0x20;
            case H28: return 0x28;
            case H30: return 0x30;
            case H38: return 0x38;
            default: throw UnreachableCodeException("Instructions::RST -- improper RSTTarget");
        }
    }();

    push(gameboy.pc, gameboy);
    gameboy.pc = location;
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
    return 4;
}

uint8_t Instructions::CALL(const JumpTest test, Gameboy &gameboy) {
    const bool jumpCondition = [&]() -> bool {
        switch (test) {
            case JumpTest::NotZero: return !gameboy.regs->FlagZero();
            case JumpTest::Zero: return gameboy.regs->FlagZero();
            case JumpTest::Carry: return gameboy.regs->FlagCarry();
            case JumpTest::NotCarry: return !gameboy.regs->FlagCarry();
            case JumpTest::Always: return true;
            default: throw UnreachableCodeException("Instructions::CALL -- improper JumpTest");
        }
    }();

    const uint16_t newPC = ReadNextWord(gameboy);

    if (jumpCondition) {
        push(gameboy.pc, gameboy);
        gameboy.pc = newPC;
        gameboy.TickM(1, true);
        gameboy.cyclesThisInstruction += 1;
    }

    return jumpCondition ? 6 : 3;
}

uint8_t Instructions::RLCA(const Gameboy &gameboy) {
    const uint8_t old = (gameboy.regs->a & 0x80) != 0 ? 1 : 0;
    gameboy.regs->SetCarry(old != 0);
    gameboy.regs->a = gameboy.regs->a << 1 | old;
    gameboy.regs->SetZero(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetSubtract(false);
    return 1;
}

uint8_t Instructions::RLA(const Gameboy &gameboy) {
    const bool flag_c = (gameboy.regs->a & 0x80) >> 7 == 0x01;
    const uint8_t r = (gameboy.regs->a << 1) + static_cast<uint8_t>(gameboy.regs->FlagCarry());
    gameboy.regs->SetCarry(flag_c);
    gameboy.regs->SetZero(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->a = r;
    return 1;
}

uint8_t Instructions::RLAddr(Gameboy &gameboy) {
    uint8_t byte = ReadByte(gameboy, gameboy.regs->GetHL());;
    const uint8_t flag_c = gameboy.regs->FlagCarry() ? 1 : 0;
    gameboy.regs->SetCarry((byte & 0x80) != 0);
    byte = (byte << 1) | flag_c;
    WriteByte(gameboy, gameboy.regs->GetHL(), byte);
    gameboy.regs->SetZero(byte == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    return 4;
}

uint8_t Instructions::RL(const ArithmeticSource source, const Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            default: throw UnreachableCodeException("Instructions::RL -- improper ArithmeticSource");
        }
    }();

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
        default: throw UnreachableCodeException("Instructions::RL -- improper ArithmeticSource");
    }

    return 2;
}

uint8_t Instructions::CCF(const Gameboy &gameboy) {
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetCarry(!gameboy.regs->FlagCarry());
    gameboy.regs->SetHalf(false);
    return 1;
}

uint8_t Instructions::CPL(const Gameboy &gameboy) {
    gameboy.regs->SetHalf(true);
    gameboy.regs->SetSubtract(true);
    gameboy.regs->a = ~gameboy.regs->a;
    return 1;
}

uint8_t Instructions::SCF(const Gameboy &gameboy) {
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(true);
    return 1;
}

uint8_t Instructions::RRCA(const Gameboy &gameboy) {
    gameboy.regs->SetCarry((gameboy.regs->a & 0x01) != 0);
    gameboy.regs->a = gameboy.regs->a >> 1 | (gameboy.regs->a & 0x01) << 7;
    gameboy.regs->SetZero(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    return 1;
}

uint8_t Instructions::RRC(const ArithmeticSource source, Gameboy &gameboy) {
    uint8_t value = [&]() -> uint8_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            case ArithmeticSource::HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());;
            default: throw UnreachableCodeException("Instructions::RRC -- improper ArithmeticSource");
        }
    }();

    if (source == ArithmeticSource::HLAddr) {
        const bool carry = (value & 0x01) == 0x01;
        gameboy.regs->SetCarry(carry);
        if (carry) {
            value = 0x80 | value >> 1;
        } else {
            value = value >> 1;
        }

        WriteByte(gameboy, gameboy.regs->GetHL(), value);
        gameboy.regs->SetZero(value == 0);
        gameboy.regs->SetSubtract(false);
        gameboy.regs->SetHalf(false);
        return 4;
    }

    const bool carry = (value & 0x01) == 0x01;
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
        default: throw UnreachableCodeException("Instructions::RRC -- improper ArithmeticSource");
    }

    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    return 2;
}

uint8_t Instructions::RRAddr(Gameboy &gameboy) {
    uint8_t value = ReadByte(gameboy, gameboy.regs->GetHL());;
    const bool carry = (value & 0x01) == 0x01;
    if (gameboy.regs->FlagCarry()) { value = 0x80 | (value >> 1); } else { value = value >> 1; }
    WriteByte(gameboy, gameboy.regs->GetHL(), value);
    gameboy.regs->SetCarry(carry);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetZero(value == 0);
    return 4;
}

uint8_t Instructions::RR(const ArithmeticSource source, const Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        using enum ArithmeticSource;
        switch (source) {
            case A: return gameboy.regs->a;
            case B: return gameboy.regs->b;
            case C: return gameboy.regs->c;
            case D: return gameboy.regs->d;
            case E: return gameboy.regs->e;
            case H: return gameboy.regs->h;
            case L: return gameboy.regs->l;
            default: throw UnreachableCodeException("Instructions::OR -- improper ArithmeticSource");
        }
    }();

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
        default: throw UnreachableCodeException("Instructions::OR -- improper ArithmeticSource");
    }

    gameboy.regs->SetCarry(carry);
    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    return 2;
}

uint8_t Instructions::RRA(const Gameboy &gameboy) {
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
    return 1;
}

uint8_t Instructions::RET(const JumpTest test, Gameboy &gameboy) {
    const bool jumpCondition = [&]() -> bool {
        switch (test) {
            case JumpTest::NotZero: return !gameboy.regs->FlagZero();
            case JumpTest::Zero: return gameboy.regs->FlagZero();
            case JumpTest::Carry: return gameboy.regs->FlagCarry();
            case JumpTest::NotCarry: return !gameboy.regs->FlagCarry();
            case JumpTest::Always: return true;
            default: throw UnreachableCodeException("Instructions::RET -- improper JumpTest");
        }
    }();

    if (test != JumpTest::Always) {
        gameboy.TickM(1, true);
        gameboy.cyclesThisInstruction += 1;
    }

    if (!jumpCondition) {
        return 2;
    }

    const uint16_t newPC = pop(gameboy);
    gameboy.pc = newPC;
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
    return test == JumpTest::Always ? 4 : 5;
}

uint8_t Instructions::JR(const JumpTest test, Gameboy &gameboy) {
    const bool jumpCondition = [&]() -> bool {
        switch (test) {
            case JumpTest::NotZero: return !gameboy.regs->FlagZero();
            case JumpTest::Zero: return gameboy.regs->FlagZero();
            case JumpTest::Carry: return gameboy.regs->FlagCarry();
            case JumpTest::NotCarry: return !gameboy.regs->FlagCarry();
            case JumpTest::Always: return true;
            default: throw UnreachableCodeException("Instructions::JR -- improper JumpTest");
        }
    }();

    const uint16_t next = gameboy.pc + 1;
    if (jumpCondition) {
        if (const auto byte = static_cast<int8_t>(ReadByte(gameboy, gameboy.pc)); byte >= 0) {
            gameboy.pc = next + static_cast<uint16_t>(byte);
        } else {
            gameboy.pc = next - static_cast<uint16_t>(abs(byte));
        }
        return 3;
    }
    gameboy.pc = next;
    return 2;
}

uint8_t Instructions::JP(const JumpTest test, Gameboy &gameboy) {
    const bool jumpCondition = [&]() -> bool {
        switch (test) {
            case JumpTest::NotZero: return !gameboy.regs->FlagZero();
            case JumpTest::Zero: return gameboy.regs->FlagZero();
            case JumpTest::Carry: return gameboy.regs->FlagCarry();
            case JumpTest::NotCarry: return !gameboy.regs->FlagCarry();
            case JumpTest::Always: return true;
            default: throw UnreachableCodeException("Instructions::JP -- improper JumpTest");
        }
    }();

    if (jumpCondition) {
        const uint16_t lower_byte = ReadByte(gameboy, gameboy.pc);
        const uint16_t higher_byte = ReadByte(gameboy, gameboy.pc + 1);
        gameboy.pc = (higher_byte << 8) | lower_byte;
        return 4;
    }
    gameboy.pc += 2;
    return 3;
}

uint8_t Instructions::JPHL(Gameboy &gameboy) {
    gameboy.pc = gameboy.regs->GetHL();
    return 1;
}

uint8_t Instructions::NOP() {
    return 1;
}

uint8_t Instructions::STOP(Gameboy &gameboy) {
    const uint8_t key1 = gameboy.bus->ReadByte(0xFF4D);
    const bool speedSwitchRequested = gameboy.bus->prepareSpeedShift && (key1 & 0x01);

    gameboy.bus->WriteByte(0xFF04, 0x00);

    if (speedSwitchRequested) {
        gameboy.TickM(130996, false);
        gameboy.bus->ChangeSpeed();
        gameboy.pc += 1;

        return 1;
    }

    gameboy.stopped = true;
    gameboy.pc += 1;
    return 1;
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
            const uint8_t byte = ReadByte(gameboy, gameboy.regs->GetHL());;
            const uint8_t new_value = decrement(byte, gameboy);
            WriteByte(gameboy, gameboy.regs->GetHL(), new_value);
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
            return 3;
        case IncDecTarget::HL:
        case IncDecTarget::BC:
        case IncDecTarget::DE:
        case IncDecTarget::SP:
            return 2;
        default:
            return 1;
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
            const uint8_t byte = ReadByte(gameboy, gameboy.regs->GetHL());;
            const uint8_t new_value = increment(byte, gameboy);
            WriteByte(gameboy, gameboy.regs->GetHL(), new_value);
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
            return 3;
        case IncDecTarget::HL:
        case IncDecTarget::BC:
        case IncDecTarget::DE:
        case IncDecTarget::SP:
            return 2;
        default:
            return 1;
    }
}

uint8_t Instructions::LDH(const LoadOtherTarget target, const LoadOtherSource source, Gameboy &gameboy) {
    if (target == LoadOtherTarget::A8 && source == LoadOtherSource::A) {
        // E0
        const uint16_t a = 0xFF00 | static_cast<uint16_t>(ReadNextByte(gameboy));
        WriteByte(gameboy, a, gameboy.regs->a);
        return 3;
    }
    if (target == LoadOtherTarget::CAddress && source == LoadOtherSource::A) {
        // E2
        const uint16_t c = 0xFF00 | static_cast<uint16_t>(gameboy.regs->c);
        WriteByte(gameboy, c, gameboy.regs->a);
        return 2;
    }
    if (target == LoadOtherTarget::A && source == LoadOtherSource::A8) {
        // F0
        const uint16_t a = 0xFF00 | static_cast<uint16_t>(ReadNextByte(gameboy));
        gameboy.regs->a = ReadByte(gameboy, a);
        return 3;
    }
    if (target == LoadOtherTarget::A && source == LoadOtherSource::CAddress) {
        // F2
        const uint16_t a = 0xFF00 | static_cast<uint16_t>(gameboy.regs->c);
        gameboy.regs->a = ReadByte(gameboy, a);
        return 2;
    }

    return 0;
}

uint8_t Instructions::LD(const LoadByteTarget target, const LoadByteSource source, Gameboy &gameboy) {
    const uint8_t sourceValue = [&]() -> uint8_t {
        switch (source) {
            case LoadByteSource::A: return gameboy.regs->a;
            case LoadByteSource::B: return gameboy.regs->b;
            case LoadByteSource::C: return gameboy.regs->c;
            case LoadByteSource::D: return gameboy.regs->d;
            case LoadByteSource::E: return gameboy.regs->e;
            case LoadByteSource::H: return gameboy.regs->h;
            case LoadByteSource::L: return gameboy.regs->l;
            case LoadByteSource::D8: return ReadNextByte(gameboy);
            case LoadByteSource::HL: return ReadByte(gameboy, gameboy.regs->GetHL());
            case LoadByteSource::BC: return ReadByte(gameboy, gameboy.regs->GetBC());
            case LoadByteSource::DE: return ReadByte(gameboy, gameboy.regs->GetDE());
            case LoadByteSource::HLI: {
                const auto tmp = ReadByte(gameboy, gameboy.regs->GetHL());
                gameboy.regs->SetHL(gameboy.regs->GetHL() + 1);
                return tmp;
            }
            case LoadByteSource::HLD: {
                const auto tmp = ReadByte(gameboy, gameboy.regs->GetHL());
                gameboy.regs->SetHL(gameboy.regs->GetHL() - 1);
                return tmp;
            }
            case LoadByteSource::A16: return ReadByte(gameboy, ReadNextWord(gameboy));
            default: throw UnreachableCodeException("Instructions::LD -- improper LoadByteSource");
        }
    }();

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
        case LoadByteTarget::HL: WriteByte(gameboy, gameboy.regs->GetHL(), sourceValue);
            break;
        case LoadByteTarget::BC: WriteByte(gameboy, gameboy.regs->GetBC(), sourceValue);
            break;
        case LoadByteTarget::DE: WriteByte(gameboy, gameboy.regs->GetDE(), sourceValue);
            break;
        case LoadByteTarget::HLI: {
            WriteByte(gameboy, gameboy.regs->GetHL(), sourceValue);
            gameboy.regs->SetHL(gameboy.regs->GetHL() + 1);
            break;
        }
        case LoadByteTarget::HLD: {
            WriteByte(gameboy, gameboy.regs->GetHL(), sourceValue);
            gameboy.regs->SetHL(gameboy.regs->GetHL() - 1);
            break;
        }
        case LoadByteTarget::A16: {
            WriteByte(gameboy, ReadNextWord(gameboy), sourceValue);
            return 4;
        }
    }

    if (source == LoadByteSource::D8) {
        return target == LoadByteTarget::HL ? 3 : 2;
    }
    if (target == LoadByteTarget::HL
        || target == LoadByteTarget::HLD
        || target == LoadByteTarget::HLI
        || target == LoadByteTarget::BC
        || target == LoadByteTarget::DE
        || source == LoadByteSource::BC
        || source == LoadByteSource::DE
        || source == LoadByteSource::HLD
        || source == LoadByteSource::HLI
        || source == LoadByteSource::HL) {
        return 2;
    }
    if (source == LoadByteSource::A16) {
        return 4;
    }
    return 1;
}

uint8_t Instructions::LD16(const LoadWordTarget target, const LoadWordSource source, Gameboy &gameboy) {
    const uint16_t sourceValue = [&]() -> uint16_t {
        switch (source) {
            case LoadWordSource::D16: return ReadNextWord(gameboy);
            case LoadWordSource::SP: return gameboy.sp;
            case LoadWordSource::HL: return gameboy.regs->GetHL();
            case LoadWordSource::SPr8: return static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
                    ReadNextByte(gameboy))));
            default: throw UnreachableCodeException("Instructions::LD16 -- improper LoadWordSource");
        }
    }();

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
        case LoadWordTarget::A16: {
            const uint16_t word = ReadNextWord(gameboy);
            WriteWord(gameboy, word, sourceValue);
            break;
        }
    }

    if (source == LoadWordSource::HL) {
        return 2;
    }
    if (source == LoadWordSource::SPr8 || source == LoadWordSource::D16) {
        return 3;
    }
    if (source == LoadWordSource::SP) {
        return 5;
    }

    return 0;
}

uint8_t Instructions::PUSH(const StackTarget target, Gameboy &gameboy) {
    const uint16_t value = [&]() -> uint16_t {
        switch (target) {
            case StackTarget::BC: return gameboy.regs->GetBC();
            case StackTarget::DE: return gameboy.regs->GetDE();
            case StackTarget::HL: return gameboy.regs->GetHL();
            case StackTarget::AF: return gameboy.regs->GetAF();
            default: throw UnreachableCodeException("Instructions::PUSH -- improper StackTarget");
        }
    }();

    push(value, gameboy);
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
    return 4;
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

    return 3;
}

uint8_t Instructions::CP(const ArithmeticSource source, Gameboy &gameboy) {
    const auto resolveValue = [&](const ArithmeticSource src) -> uint8_t {
        using enum ArithmeticSource;
        switch (src) {
            case A: return gameboy.regs->a;
            case B: return gameboy.regs->b;
            case C: return gameboy.regs->c;
            case D: return gameboy.regs->d;
            case E: return gameboy.regs->e;
            case H: return gameboy.regs->h;
            case L: return gameboy.regs->l;
            case HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());;
            case U8: return ReadNextByte(gameboy);
            default: throw UnreachableCodeException("Improper CP source");
        }
    };

    const uint8_t value = resolveValue(source);
    subtract(value, gameboy);

    const bool isU8 = source == ArithmeticSource::U8;
    const bool isHL = source == ArithmeticSource::HLAddr;
    return isU8 || isHL ? 2 : 1;
}

uint8_t Instructions::OR(const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        using enum ArithmeticSource;
        switch (source) {
            case A: return gameboy.regs->a;
            case B: return gameboy.regs->b;
            case C: return gameboy.regs->c;
            case D: return gameboy.regs->d;
            case E: return gameboy.regs->e;
            case H: return gameboy.regs->h;
            case L: return gameboy.regs->l;
            case HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());;
            case U8: return ReadNextByte(gameboy);
            default: throw UnreachableCodeException("Instructions::OR -- improper ArithmeticSource");
        }
    }();

    gameboy.regs->a |= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(false);

    const bool isU8 = source == ArithmeticSource::U8;
    const bool isHL = source == ArithmeticSource::HLAddr;
    return isU8 || isHL ? 2 : 1;
}

uint8_t Instructions::XOR(const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        using enum ArithmeticSource;
        switch (source) {
            case A: return gameboy.regs->a;
            case B: return gameboy.regs->b;
            case C: return gameboy.regs->c;
            case D: return gameboy.regs->d;
            case E: return gameboy.regs->e;
            case H: return gameboy.regs->h;
            case L: return gameboy.regs->l;
            case HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());;
            case U8: return ReadNextByte(gameboy);
            default: throw UnreachableCodeException("Instructions::XOR -- improper ArithmeticSource");
        }
    }();

    gameboy.regs->a ^= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    gameboy.regs->SetCarry(false);

    const bool isU8 = source == ArithmeticSource::U8;
    const bool isHL = source == ArithmeticSource::HLAddr;
    return isU8 || isHL ? 2 : 1;
}

uint8_t Instructions::AND(const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        using enum ArithmeticSource;
        switch (source) {
            case A: return gameboy.regs->a;
            case B: return gameboy.regs->b;
            case C: return gameboy.regs->c;
            case D: return gameboy.regs->d;
            case E: return gameboy.regs->e;
            case H: return gameboy.regs->h;
            case L: return gameboy.regs->l;
            case HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());;
            case U8: return ReadNextByte(gameboy);
            default: throw UnreachableCodeException("Instructions::AND -- improper ArithmeticSource");
        }
    }();

    gameboy.regs->a &= value;
    gameboy.regs->SetZero(gameboy.regs->a == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(true);
    gameboy.regs->SetCarry(false);

    const bool isU8 = source == ArithmeticSource::U8;
    const bool isHL = source == ArithmeticSource::HLAddr;
    return isU8 || isHL ? 2 : 1;
}

uint8_t Instructions::SUB(const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            case ArithmeticSource::HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());;
            case ArithmeticSource::U8: return ReadNextByte(gameboy);
            default: throw UnreachableCodeException("Instructions::SUB -- improper ArithmeticSource");
        }
    }();

    gameboy.regs->a = subtract(value, gameboy);
    const bool isU8 = source == ArithmeticSource::U8;
    const bool isHL = source == ArithmeticSource::HLAddr;
    return isU8 || isHL ? 2 : 1;
}

uint8_t Instructions::SLAAddr(Gameboy &gameboy) {
    uint8_t value = ReadByte(gameboy, gameboy.regs->GetHL());
    gameboy.regs->SetCarry((value & 0x80) != 0);
    value <<= 1;
    WriteByte(gameboy, gameboy.regs->GetHL(), value);
    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    return 4;
}

uint8_t Instructions::SLA(const ArithmeticSource source, const Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            default: throw UnreachableCodeException("Instructions::SLA -- improper ArithmeticSource");
        }
    }();

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
        default: throw UnreachableCodeException("Instructions::SLA -- improper ArithmeticSource");
    }

    gameboy.regs->SetZero(newValue == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    return 2;
}

uint8_t Instructions::SRAAddr(Gameboy &gameboy) {
    uint8_t value = ReadByte(gameboy, gameboy.regs->GetHL());;
    gameboy.regs->SetCarry((value & 0x01) != 0);
    value = (value >> 1) | (value & 0x80);
    WriteByte(gameboy, gameboy.regs->GetHL(), value);
    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    return 4;
}

uint8_t Instructions::SRA(const ArithmeticSource source, const Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            default: throw UnreachableCodeException("Instructions::SLA -- improper ArithmeticSource");
        }
    }();

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

    return 2;
}

uint8_t Instructions::SWAPAddr(Gameboy &gameboy) {
    uint8_t value = ReadByte(gameboy, gameboy.regs->GetHL());;
    value = (value >> 4) | (value << 4);
    WriteByte(gameboy, gameboy.regs->GetHL(), value);
    gameboy.regs->SetCarry(false);
    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    return 4;
}

uint8_t Instructions::SWAP(const ArithmeticSource source, const Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            default: throw UnreachableCodeException("Instructions::SWAP -- improper ArithmeticSource");
        }
    }();

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
        default: throw UnreachableCodeException("Instructions::SWAP -- improper ArithmeticSource");
    }

    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetCarry(false);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    return 2;
}

uint8_t Instructions::SRLAddr(Gameboy &gameboy) {
    uint8_t value = ReadByte(gameboy, gameboy.regs->GetHL());;
    gameboy.regs->SetCarry((value & 0x01) != 0);
    value >>= 1;
    WriteByte(gameboy, gameboy.regs->GetHL(), value);
    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);
    return 4;
}

uint8_t Instructions::SRL(const ArithmeticSource source, const Gameboy &gameboy) {
    uint8_t value = [&]() -> uint8_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            default: throw UnreachableCodeException("Instructions::SRL -- improper ArithmeticSource");
        }
    }();

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
        default: throw UnreachableCodeException("Instructions::SRL -- improper ArithmeticSource");
    }

    gameboy.regs->SetZero(value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf(false);

    return 2;
}

uint8_t Instructions::BIT(const uint8_t target, const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = 1 << target;
    const bool zero = [&]() -> bool {
        switch (source) {
            case ArithmeticSource::A: return (gameboy.regs->a & value) == 0;
            case ArithmeticSource::B: return (gameboy.regs->b & value) == 0;
            case ArithmeticSource::C: return (gameboy.regs->c & value) == 0;
            case ArithmeticSource::D: return (gameboy.regs->d & value) == 0;
            case ArithmeticSource::E: return (gameboy.regs->e & value) == 0;
            case ArithmeticSource::H: return (gameboy.regs->h & value) == 0;
            case ArithmeticSource::L: return (gameboy.regs->l & value) == 0;
            case ArithmeticSource::HLAddr: return (ReadByte(gameboy, gameboy.regs->GetHL()) & value) == 0;
            default: throw UnreachableCodeException("Instructions::BIT -- improper ArithmeticSource");
        }
    }();

    gameboy.regs->SetZero(zero);
    gameboy.regs->SetHalf(true);
    gameboy.regs->SetSubtract(false);

    return source == ArithmeticSource::HLAddr ? 3 : 2;
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
        case ArithmeticSource::HLAddr: {
            const uint8_t byte = ReadByte(gameboy, gameboy.regs->GetHL());
            WriteByte(gameboy, gameboy.regs->GetHL(), byte & ~value);
            break;
        }
        default: throw UnreachableCodeException("Instructions::RES -- improper ArithmeticSource");
    }

    return source == ArithmeticSource::HLAddr ? 4 : 2;
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
        case ArithmeticSource::HLAddr: {
            const uint8_t byte = ReadByte(gameboy, gameboy.regs->GetHL());
            WriteByte(gameboy, gameboy.regs->GetHL(), byte | value);
            break;
        }
        default: throw UnreachableCodeException("Instructions::SET -- improper ArithmeticSource");
    }

    return source == ArithmeticSource::HLAddr ? 4 : 2;
}

uint8_t Instructions::SBC(const ArithmeticSource source, Gameboy &gameboy) {
    const uint16_t value = [&]() -> uint16_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            case ArithmeticSource::HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());
            case ArithmeticSource::U8: return ReadNextByte(gameboy);
            default: throw UnreachableCodeException("Instructions::SBC -- improper ArithmeticSource");
        }
    }();

    const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
    const uint8_t r = gameboy.regs->a - value - flag_carry;
    gameboy.regs->SetCarry(gameboy.regs->a < value + static_cast<uint16_t>(flag_carry));
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) < (value & 0xF) + flag_carry);
    gameboy.regs->SetSubtract(true);
    gameboy.regs->SetZero(r == 0x0);
    gameboy.regs->a = r;

    const bool isU8 = source == ArithmeticSource::U8;
    const bool isHL = source == ArithmeticSource::HLAddr;
    return isU8 || isHL ? 2 : 1;
}

uint8_t Instructions::ADC(const ArithmeticSource source, Gameboy &gameboy) {
    const uint16_t value = [&]() -> uint16_t {
        switch (source) {
            case ArithmeticSource::A: return gameboy.regs->a;
            case ArithmeticSource::B: return gameboy.regs->b;
            case ArithmeticSource::C: return gameboy.regs->c;
            case ArithmeticSource::D: return gameboy.regs->d;
            case ArithmeticSource::E: return gameboy.regs->e;
            case ArithmeticSource::H: return gameboy.regs->h;
            case ArithmeticSource::L: return gameboy.regs->l;
            case ArithmeticSource::HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());
            case ArithmeticSource::U8: return ReadNextByte(gameboy);
            default: throw UnreachableCodeException("Improper ADC source");
        }
    }();

    const uint8_t flag_carry = gameboy.regs->FlagCarry() ? 1 : 0;
    const uint8_t r = gameboy.regs->a + value + flag_carry;
    gameboy.regs->SetCarry(static_cast<uint16_t>(gameboy.regs->a) + value + static_cast<uint16_t>(flag_carry) > 0xFF);
    gameboy.regs->SetHalf(((gameboy.regs->a & 0xF) + (value & 0xF) + (flag_carry & 0xF)) > 0xF);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetZero(r == 0x0);
    gameboy.regs->a = r;

    const bool isU8 = source == ArithmeticSource::U8;
    const bool isHL = source == ArithmeticSource::HLAddr;
    return isU8 || isHL ? 2 : 1;
}

uint8_t Instructions::ADD16(const Arithmetic16Target target, const Gameboy &gameboy) {
    const uint16_t sourceValue = [&]() -> uint16_t {
        switch (target) {
            case Arithmetic16Target::BC: return gameboy.regs->GetBC();
            case Arithmetic16Target::DE: return gameboy.regs->GetDE();
            case Arithmetic16Target::HL: return gameboy.regs->GetHL();
            case Arithmetic16Target::SP: return gameboy.sp;
            default: throw UnreachableCodeException("Instructions::ADD16 -- improper Arithmetic16Target");
        }
    }();

    const uint16_t reg = gameboy.regs->GetHL();
    const uint16_t sum = reg + sourceValue;

    gameboy.regs->SetCarry(reg > 0xFFFF - sourceValue);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf((reg & 0x07FF) + (sourceValue & 0x07FF) > 0x07FF);
    gameboy.regs->SetHL(sum);

    return 2;
}

uint8_t Instructions::ADD(const ArithmeticSource source, Gameboy &gameboy) {
    const uint8_t value = [&]() -> uint8_t {
        using enum ArithmeticSource;
        switch (source) {
            case A: return gameboy.regs->a;
            case B: return gameboy.regs->b;
            case C: return gameboy.regs->c;
            case D: return gameboy.regs->d;
            case E: return gameboy.regs->e;
            case H: return gameboy.regs->h;
            case L: return gameboy.regs->l;
            case HLAddr: return ReadByte(gameboy, gameboy.regs->GetHL());
            case U8: return ReadNextByte(gameboy);
            default: throw UnreachableCodeException("Instructions::ADD -- improper ArithmeticSource");
        }
    }();

    const uint8_t a = gameboy.regs->a;
    const uint8_t new_value = a + value;
    gameboy.regs->SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(value) > 0xFF);
    gameboy.regs->SetZero(new_value == 0);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetHalf((gameboy.regs->a & 0xF) + (value & 0xF) > 0xF);

    gameboy.regs->a = new_value;

    const bool isU8 = source == ArithmeticSource::U8;
    const bool isHL = source == ArithmeticSource::HLAddr;
    return isU8 || isHL ? 2 : 1;
}

uint8_t Instructions::ADDSigned(Gameboy &gameboy) {
    const auto source_value = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
        ReadNextByte(gameboy))));
    const uint16_t sp_value = gameboy.sp;
    gameboy.regs->SetCarry(((sp_value & 0xFF) + (source_value & 0xFF)) > 0xFF);
    gameboy.regs->SetHalf(((sp_value & 0xF) + (source_value & 0xF)) > 0xF);
    gameboy.regs->SetSubtract(false);
    gameboy.regs->SetZero(false);
    gameboy.sp = sp_value + source_value;
    return 4;
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

void Instructions::push(const uint16_t value, Gameboy &gameboy) {
    gameboy.sp -= 1;
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
    gameboy.bus->WriteByte(gameboy.sp, (value & 0xFF00) >> 8);
    gameboy.sp -= 1;
    gameboy.TickM(1, true);
    gameboy.cyclesThisInstruction += 1;
    gameboy.bus->WriteByte(gameboy.sp, value & 0xFF);
}

uint16_t Instructions::pop(Gameboy &gameboy) {
    const uint16_t lsb = ReadByte(gameboy, gameboy.sp);
    gameboy.sp += 1;
    const uint16_t msb = ReadByte(gameboy, gameboy.sp);
    gameboy.sp += 1;

    return msb << 8 | lsb;
}
