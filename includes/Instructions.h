#pragma once

#include "Gameboy.h"

class Gameboy;

enum class JumpTest {
    NotZero, Zero, NotCarry, Carry, Always
};

enum class RSTTargets {
    H00, H10, H20, H30, H08, H18, H28, H38,
};

enum class IncDecTarget {
    A, B, C, D, E, H, L, HL, HLAddr, BC, DE, SP,
};

enum class Arithmetic16Target {
    HL, BC, DE, SP,
};

enum class ArithmeticTarget {
    A, SP,
};

enum class ArithmeticSource {
    A, B, C, D, E, H, L, U8, HLAddr, I8
};

enum class LoadByteTarget {
    A, B, C, D, E, H, L, HLI, HLD, HL, BC, DE, A16,
};

enum class LoadByteSource {
    A, B, C, D, E, H, L, D8, HL, HLI, HLD, A16, BC, DE,
};

enum class LoadWordSource {
    D16, HL, SP, SPr8,
};

enum class LoadWordTarget {
    BC, DE, HL, SP, A16,
};

enum class LoadOtherTarget {
    A, A8, CAddress,
};

enum class LoadOtherSource {
    A, A8, CAddress,
};

enum class StackTarget {
    BC, DE, HL, AF,
};

class Instructions {
public:
    static uint8_t DAA(Gameboy &gameboy);

    static uint8_t RETI(Gameboy &gameboy);

    static uint8_t DI(Gameboy &gameboy);

    static uint8_t EI(Gameboy &gameboy);

    static uint8_t HALT(Gameboy &gameboy);

    static uint8_t RST(RSTTargets target, Gameboy &gameboy);

    static uint8_t CALL(JumpTest test, Gameboy &gameboy);

    static uint8_t RLCA(Gameboy &gameboy);

    static uint8_t RLC(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t RLA(Gameboy &gameboy);

    static uint8_t RL(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t CCF(Gameboy &gameboy);

    static uint8_t CPL(Gameboy &gameboy);

    static uint8_t SCF(Gameboy &gameboy);

    static uint8_t RRCA(Gameboy &gameboy);

    static uint8_t RRC(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t RR(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t RRA(Gameboy &gameboy);

    static uint8_t RET(JumpTest test, Gameboy &gameboy);

    static uint8_t JR(JumpTest test, Gameboy &gameboy);

    static uint8_t JP(JumpTest test, Gameboy &gameboy);

    static uint8_t JPHL(Gameboy &gameboy);

    static uint8_t NOP(Gameboy &gameboy);

    static uint8_t DEC(IncDecTarget target, Gameboy &gameboy);

    static uint8_t INC(IncDecTarget target, Gameboy &gameboy);

    static uint8_t LDH(LoadOtherTarget target, LoadOtherSource source, Gameboy &gameboy);

    static uint8_t LD(LoadByteTarget target, LoadByteSource source, Gameboy &gameboy);

    static uint8_t LD16(LoadWordTarget target, LoadWordSource source, Gameboy &gameboy);

    static uint8_t PUSH(StackTarget target, Gameboy &gameboy);

    static uint8_t POP(StackTarget target, Gameboy &gameboy);

    static uint8_t CP(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t OR(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t XOR(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t AND(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SUB(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SLA(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SRA(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SWAP(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SRL(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t BIT(uint8_t target, ArithmeticSource source, Gameboy &gameboy);

    static uint8_t RES(uint8_t target, ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SET(uint8_t target, ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SBC(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t ADC(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t ADD16(Arithmetic16Target target, Gameboy &gameboy);

    static uint8_t ADD(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t increment(uint8_t reg, const Gameboy &gameboy);

    static uint8_t decrement(uint8_t reg, const Gameboy &gameboy);

    static uint8_t subtract(uint8_t value, const Gameboy &gameboy);

    static uint8_t addition(uint8_t value, const Gameboy &gameboy);

    static uint8_t jump(bool shouldJump, Gameboy &gameboy);

    static uint8_t jumpRelative(bool shouldJump, Gameboy &gameboy);

    static void push(uint16_t value, Gameboy &gameboy);

    static uint16_t pop(Gameboy &gameboy);

    static uint8_t call(bool shouldJump, Gameboy &gameboy);

    static uint8_t return_(bool shouldJump, Gameboy &gameboy);

    static uint16_t ReadNextWord(const Gameboy &gameboy);

    static uint8_t ReadNextByte(const Gameboy &gameboy);

    using WrappedFunction = uint8_t(*)(Gameboy &);

    template<typename... Args>
    uint8_t prefixedInstr(const uint8_t opcode, Args &&... args) {
        return prefixedTable[opcode](std::forward<Args>(args)...);
    }

    template<typename... Args>
    uint8_t nonPrefixedInstr(const uint8_t opcode, Args &&... args) {
        return nonPrefixedTable[opcode](std::forward<Args>(args)...);
    }

    static constexpr std::array<WrappedFunction, 256> prefixedTable = [] {
        std::array<WrappedFunction, 256> table{};
        table[0x00] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::B, gb); };
        table[0x01] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::C, gb); };
        table[0x02] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::D, gb); };
        table[0x03] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::E, gb); };
        table[0x04] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::H, gb); };
        table[0x05] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::L, gb); };
        table[0x06] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::HLAddr, gb); };
        table[0x07] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::A, gb); };
        table[0x08] = [](Gameboy &gb) -> uint8_t { return RRC(ArithmeticSource::B, gb); };
        table[0x09] = [](Gameboy &gb) -> uint8_t { return RRC(ArithmeticSource::C, gb); };
        table[0x0A] = [](Gameboy &gb) -> uint8_t { return RRC(ArithmeticSource::D, gb); };
        table[0x0B] = [](Gameboy &gb) -> uint8_t { return RRC(ArithmeticSource::E, gb); };
        table[0x0C] = [](Gameboy &gb) -> uint8_t { return RRC(ArithmeticSource::H, gb); };
        table[0x0D] = [](Gameboy &gb) -> uint8_t { return RRC(ArithmeticSource::L, gb); };
        table[0x0E] = [](Gameboy &gb) -> uint8_t { return RRC(ArithmeticSource::HLAddr, gb); };
        table[0x0F] = [](Gameboy &gb) -> uint8_t { return RRC(ArithmeticSource::A, gb); };
        table[0x10] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::B, gb); };
        table[0x11] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::C, gb); };
        table[0x12] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::D, gb); };
        table[0x13] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::E, gb); };
        table[0x14] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::H, gb); };
        table[0x15] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::L, gb); };
        table[0x16] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::HLAddr, gb); };
        table[0x17] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::A, gb); };
        table[0x18] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::B, gb); };
        table[0x19] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::C, gb); };
        table[0x1A] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::D, gb); };
        table[0x1B] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::E, gb); };
        table[0x1C] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::H, gb); };
        table[0x1D] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::L, gb); };
        table[0x1E] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::HLAddr, gb); };
        table[0x1F] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::A, gb); };
        table[0x20] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::B, gb); };
        table[0x21] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::C, gb); };
        table[0x22] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::D, gb); };
        table[0x23] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::E, gb); };
        table[0x24] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::H, gb); };
        table[0x25] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::L, gb); };
        table[0x26] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::HLAddr, gb); };
        table[0x27] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::A, gb); };
        table[0x28] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::B, gb); };
        table[0x29] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::C, gb); };
        table[0x2A] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::D, gb); };
        table[0x2B] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::E, gb); };
        table[0x2C] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::H, gb); };
        table[0x2D] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::L, gb); };
        table[0x2E] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::HLAddr, gb); };
        table[0x2F] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::A, gb); };
        table[0x30] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::B, gb); };
        table[0x31] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::C, gb); };
        table[0x32] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::D, gb); };
        table[0x33] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::E, gb); };
        table[0x34] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::H, gb); };
        table[0x35] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::L, gb); };
        table[0x36] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::HLAddr, gb); };
        table[0x37] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::A, gb); };
        table[0x38] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::B, gb); };
        table[0x39] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::C, gb); };
        table[0x3A] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::D, gb); };
        table[0x3B] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::E, gb); };
        table[0x3C] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::H, gb); };
        table[0x3D] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::L, gb); };
        table[0x3E] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::HLAddr, gb); };
        table[0x3F] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::A, gb); };
        table[0x40] = [](Gameboy &gb) -> uint8_t { return BIT(0, ArithmeticSource::B, gb); };
        table[0x41] = [](Gameboy &gb) -> uint8_t { return BIT(0, ArithmeticSource::C, gb); };
        table[0x42] = [](Gameboy &gb) -> uint8_t { return BIT(0, ArithmeticSource::D, gb); };
        table[0x43] = [](Gameboy &gb) -> uint8_t { return BIT(0, ArithmeticSource::E, gb); };
        table[0x44] = [](Gameboy &gb) -> uint8_t { return BIT(0, ArithmeticSource::H, gb); };
        table[0x45] = [](Gameboy &gb) -> uint8_t { return BIT(0, ArithmeticSource::L, gb); };
        table[0x46] = [](Gameboy &gb) -> uint8_t { return BIT(0, ArithmeticSource::HLAddr, gb); };
        table[0x47] = [](Gameboy &gb) -> uint8_t { return BIT(0, ArithmeticSource::A, gb); };
        table[0x48] = [](Gameboy &gb) -> uint8_t { return BIT(1, ArithmeticSource::B, gb); };
        table[0x49] = [](Gameboy &gb) -> uint8_t { return BIT(1, ArithmeticSource::C, gb); };
        table[0x4A] = [](Gameboy &gb) -> uint8_t { return BIT(1, ArithmeticSource::D, gb); };
        table[0x4B] = [](Gameboy &gb) -> uint8_t { return BIT(1, ArithmeticSource::E, gb); };
        table[0x4C] = [](Gameboy &gb) -> uint8_t { return BIT(1, ArithmeticSource::H, gb); };
        table[0x4D] = [](Gameboy &gb) -> uint8_t { return BIT(1, ArithmeticSource::L, gb); };
        table[0x4E] = [](Gameboy &gb) -> uint8_t { return BIT(1, ArithmeticSource::HLAddr, gb); };
        table[0x4F] = [](Gameboy &gb) -> uint8_t { return BIT(1, ArithmeticSource::A, gb); };
        table[0x50] = [](Gameboy &gb) -> uint8_t { return BIT(2, ArithmeticSource::B, gb); };
        table[0x51] = [](Gameboy &gb) -> uint8_t { return BIT(2, ArithmeticSource::C, gb); };
        table[0x52] = [](Gameboy &gb) -> uint8_t { return BIT(2, ArithmeticSource::D, gb); };
        table[0x53] = [](Gameboy &gb) -> uint8_t { return BIT(2, ArithmeticSource::E, gb); };
        table[0x54] = [](Gameboy &gb) -> uint8_t { return BIT(2, ArithmeticSource::H, gb); };
        table[0x55] = [](Gameboy &gb) -> uint8_t { return BIT(2, ArithmeticSource::L, gb); };
        table[0x56] = [](Gameboy &gb) -> uint8_t { return BIT(2, ArithmeticSource::HLAddr, gb); };
        table[0x57] = [](Gameboy &gb) -> uint8_t { return BIT(2, ArithmeticSource::A, gb); };
        table[0x58] = [](Gameboy &gb) -> uint8_t { return BIT(3, ArithmeticSource::B, gb); };
        table[0x59] = [](Gameboy &gb) -> uint8_t { return BIT(3, ArithmeticSource::C, gb); };
        table[0x5A] = [](Gameboy &gb) -> uint8_t { return BIT(3, ArithmeticSource::D, gb); };
        table[0x5B] = [](Gameboy &gb) -> uint8_t { return BIT(3, ArithmeticSource::E, gb); };
        table[0x5C] = [](Gameboy &gb) -> uint8_t { return BIT(3, ArithmeticSource::H, gb); };
        table[0x5D] = [](Gameboy &gb) -> uint8_t { return BIT(3, ArithmeticSource::L, gb); };
        table[0x5E] = [](Gameboy &gb) -> uint8_t { return BIT(3, ArithmeticSource::HLAddr, gb); };
        table[0x5F] = [](Gameboy &gb) -> uint8_t { return BIT(3, ArithmeticSource::A, gb); };
        table[0x60] = [](Gameboy &gb) -> uint8_t { return BIT(4, ArithmeticSource::B, gb); };
        table[0x61] = [](Gameboy &gb) -> uint8_t { return BIT(4, ArithmeticSource::C, gb); };
        table[0x62] = [](Gameboy &gb) -> uint8_t { return BIT(4, ArithmeticSource::D, gb); };
        table[0x63] = [](Gameboy &gb) -> uint8_t { return BIT(4, ArithmeticSource::E, gb); };
        table[0x64] = [](Gameboy &gb) -> uint8_t { return BIT(4, ArithmeticSource::H, gb); };
        table[0x65] = [](Gameboy &gb) -> uint8_t { return BIT(4, ArithmeticSource::L, gb); };
        table[0x66] = [](Gameboy &gb) -> uint8_t { return BIT(4, ArithmeticSource::HLAddr, gb); };
        table[0x67] = [](Gameboy &gb) -> uint8_t { return BIT(4, ArithmeticSource::A, gb); };
        table[0x68] = [](Gameboy &gb) -> uint8_t { return BIT(5, ArithmeticSource::B, gb); };
        table[0x69] = [](Gameboy &gb) -> uint8_t { return BIT(5, ArithmeticSource::C, gb); };
        table[0x6A] = [](Gameboy &gb) -> uint8_t { return BIT(5, ArithmeticSource::D, gb); };
        table[0x6B] = [](Gameboy &gb) -> uint8_t { return BIT(5, ArithmeticSource::E, gb); };
        table[0x6C] = [](Gameboy &gb) -> uint8_t { return BIT(5, ArithmeticSource::H, gb); };
        table[0x6D] = [](Gameboy &gb) -> uint8_t { return BIT(5, ArithmeticSource::L, gb); };
        table[0x6E] = [](Gameboy &gb) -> uint8_t { return BIT(5, ArithmeticSource::HLAddr, gb); };
        table[0x6F] = [](Gameboy &gb) -> uint8_t { return BIT(5, ArithmeticSource::A, gb); };
        table[0x70] = [](Gameboy &gb) -> uint8_t { return BIT(6, ArithmeticSource::B, gb); };
        table[0x71] = [](Gameboy &gb) -> uint8_t { return BIT(6, ArithmeticSource::C, gb); };
        table[0x72] = [](Gameboy &gb) -> uint8_t { return BIT(6, ArithmeticSource::D, gb); };
        table[0x73] = [](Gameboy &gb) -> uint8_t { return BIT(6, ArithmeticSource::E, gb); };
        table[0x74] = [](Gameboy &gb) -> uint8_t { return BIT(6, ArithmeticSource::H, gb); };
        table[0x75] = [](Gameboy &gb) -> uint8_t { return BIT(6, ArithmeticSource::L, gb); };
        table[0x76] = [](Gameboy &gb) -> uint8_t { return BIT(6, ArithmeticSource::HLAddr, gb); };
        table[0x77] = [](Gameboy &gb) -> uint8_t { return BIT(6, ArithmeticSource::A, gb); };
        table[0x78] = [](Gameboy &gb) -> uint8_t { return BIT(7, ArithmeticSource::B, gb); };
        table[0x79] = [](Gameboy &gb) -> uint8_t { return BIT(7, ArithmeticSource::C, gb); };
        table[0x7A] = [](Gameboy &gb) -> uint8_t { return BIT(7, ArithmeticSource::D, gb); };
        table[0x7B] = [](Gameboy &gb) -> uint8_t { return BIT(7, ArithmeticSource::E, gb); };
        table[0x7C] = [](Gameboy &gb) -> uint8_t { return BIT(7, ArithmeticSource::H, gb); };
        table[0x7D] = [](Gameboy &gb) -> uint8_t { return BIT(7, ArithmeticSource::L, gb); };
        table[0x7E] = [](Gameboy &gb) -> uint8_t { return BIT(7, ArithmeticSource::HLAddr, gb); };
        table[0x7F] = [](Gameboy &gb) -> uint8_t { return BIT(7, ArithmeticSource::A, gb); };
        table[0x80] = [](Gameboy &gb) -> uint8_t { return RES(0, ArithmeticSource::B, gb); };
        table[0x81] = [](Gameboy &gb) -> uint8_t { return RES(0, ArithmeticSource::C, gb); };
        table[0x82] = [](Gameboy &gb) -> uint8_t { return RES(0, ArithmeticSource::D, gb); };
        table[0x83] = [](Gameboy &gb) -> uint8_t { return RES(0, ArithmeticSource::E, gb); };
        table[0x84] = [](Gameboy &gb) -> uint8_t { return RES(0, ArithmeticSource::H, gb); };
        table[0x85] = [](Gameboy &gb) -> uint8_t { return RES(0, ArithmeticSource::L, gb); };
        table[0x86] = [](Gameboy &gb) -> uint8_t { return RES(0, ArithmeticSource::HLAddr, gb); };
        table[0x87] = [](Gameboy &gb) -> uint8_t { return RES(0, ArithmeticSource::A, gb); };
        table[0x88] = [](Gameboy &gb) -> uint8_t { return RES(1, ArithmeticSource::B, gb); };
        table[0x89] = [](Gameboy &gb) -> uint8_t { return RES(1, ArithmeticSource::C, gb); };
        table[0x8A] = [](Gameboy &gb) -> uint8_t { return RES(1, ArithmeticSource::D, gb); };
        table[0x8B] = [](Gameboy &gb) -> uint8_t { return RES(1, ArithmeticSource::E, gb); };
        table[0x8C] = [](Gameboy &gb) -> uint8_t { return RES(1, ArithmeticSource::H, gb); };
        table[0x8D] = [](Gameboy &gb) -> uint8_t { return RES(1, ArithmeticSource::L, gb); };
        table[0x8E] = [](Gameboy &gb) -> uint8_t { return RES(1, ArithmeticSource::HLAddr, gb); };
        table[0x8F] = [](Gameboy &gb) -> uint8_t { return RES(1, ArithmeticSource::A, gb); };
        table[0x90] = [](Gameboy &gb) -> uint8_t { return RES(2, ArithmeticSource::B, gb); };
        table[0x91] = [](Gameboy &gb) -> uint8_t { return RES(2, ArithmeticSource::C, gb); };
        table[0x92] = [](Gameboy &gb) -> uint8_t { return RES(2, ArithmeticSource::D, gb); };
        table[0x93] = [](Gameboy &gb) -> uint8_t { return RES(2, ArithmeticSource::E, gb); };
        table[0x94] = [](Gameboy &gb) -> uint8_t { return RES(2, ArithmeticSource::H, gb); };
        table[0x95] = [](Gameboy &gb) -> uint8_t { return RES(2, ArithmeticSource::L, gb); };
        table[0x96] = [](Gameboy &gb) -> uint8_t { return RES(2, ArithmeticSource::HLAddr, gb); };
        table[0x97] = [](Gameboy &gb) -> uint8_t { return RES(2, ArithmeticSource::A, gb); };
        table[0x98] = [](Gameboy &gb) -> uint8_t { return RES(3, ArithmeticSource::B, gb); };
        table[0x99] = [](Gameboy &gb) -> uint8_t { return RES(3, ArithmeticSource::C, gb); };
        table[0x9A] = [](Gameboy &gb) -> uint8_t { return RES(3, ArithmeticSource::D, gb); };
        table[0x9B] = [](Gameboy &gb) -> uint8_t { return RES(3, ArithmeticSource::E, gb); };
        table[0x9C] = [](Gameboy &gb) -> uint8_t { return RES(3, ArithmeticSource::H, gb); };
        table[0x9D] = [](Gameboy &gb) -> uint8_t { return RES(3, ArithmeticSource::L, gb); };
        table[0x9E] = [](Gameboy &gb) -> uint8_t { return RES(3, ArithmeticSource::HLAddr, gb); };
        table[0x9F] = [](Gameboy &gb) -> uint8_t { return RES(3, ArithmeticSource::A, gb); };
        table[0xA0] = [](Gameboy &gb) -> uint8_t { return RES(4, ArithmeticSource::B, gb); };
        table[0xA1] = [](Gameboy &gb) -> uint8_t { return RES(4, ArithmeticSource::C, gb); };
        table[0xA2] = [](Gameboy &gb) -> uint8_t { return RES(4, ArithmeticSource::D, gb); };
        table[0xA3] = [](Gameboy &gb) -> uint8_t { return RES(4, ArithmeticSource::E, gb); };
        table[0xA4] = [](Gameboy &gb) -> uint8_t { return RES(4, ArithmeticSource::H, gb); };
        table[0xA5] = [](Gameboy &gb) -> uint8_t { return RES(4, ArithmeticSource::L, gb); };
        table[0xA6] = [](Gameboy &gb) -> uint8_t { return RES(4, ArithmeticSource::HLAddr, gb); };
        table[0xA7] = [](Gameboy &gb) -> uint8_t { return RES(4, ArithmeticSource::A, gb); };
        table[0xA8] = [](Gameboy &gb) -> uint8_t { return RES(5, ArithmeticSource::B, gb); };
        table[0xA9] = [](Gameboy &gb) -> uint8_t { return RES(5, ArithmeticSource::C, gb); };
        table[0xAA] = [](Gameboy &gb) -> uint8_t { return RES(5, ArithmeticSource::D, gb); };
        table[0xAB] = [](Gameboy &gb) -> uint8_t { return RES(5, ArithmeticSource::E, gb); };
        table[0xAC] = [](Gameboy &gb) -> uint8_t { return RES(5, ArithmeticSource::H, gb); };
        table[0xAD] = [](Gameboy &gb) -> uint8_t { return RES(5, ArithmeticSource::L, gb); };
        table[0xAE] = [](Gameboy &gb) -> uint8_t { return RES(5, ArithmeticSource::HLAddr, gb); };
        table[0xAF] = [](Gameboy &gb) -> uint8_t { return RES(5, ArithmeticSource::A, gb); };
        table[0xB0] = [](Gameboy &gb) -> uint8_t { return RES(6, ArithmeticSource::B, gb); };
        table[0xB1] = [](Gameboy &gb) -> uint8_t { return RES(6, ArithmeticSource::C, gb); };
        table[0xB2] = [](Gameboy &gb) -> uint8_t { return RES(6, ArithmeticSource::D, gb); };
        table[0xB3] = [](Gameboy &gb) -> uint8_t { return RES(6, ArithmeticSource::E, gb); };
        table[0xB4] = [](Gameboy &gb) -> uint8_t { return RES(6, ArithmeticSource::H, gb); };
        table[0xB5] = [](Gameboy &gb) -> uint8_t { return RES(6, ArithmeticSource::L, gb); };
        table[0xB6] = [](Gameboy &gb) -> uint8_t { return RES(6, ArithmeticSource::HLAddr, gb); };
        table[0xB7] = [](Gameboy &gb) -> uint8_t { return RES(6, ArithmeticSource::A, gb); };
        table[0xB8] = [](Gameboy &gb) -> uint8_t { return RES(7, ArithmeticSource::B, gb); };
        table[0xB9] = [](Gameboy &gb) -> uint8_t { return RES(7, ArithmeticSource::C, gb); };
        table[0xBA] = [](Gameboy &gb) -> uint8_t { return RES(7, ArithmeticSource::D, gb); };
        table[0xBB] = [](Gameboy &gb) -> uint8_t { return RES(7, ArithmeticSource::E, gb); };
        table[0xBC] = [](Gameboy &gb) -> uint8_t { return RES(7, ArithmeticSource::H, gb); };
        table[0xBD] = [](Gameboy &gb) -> uint8_t { return RES(7, ArithmeticSource::L, gb); };
        table[0xBE] = [](Gameboy &gb) -> uint8_t { return RES(7, ArithmeticSource::HLAddr, gb); };
        table[0xBF] = [](Gameboy &gb) -> uint8_t { return RES(7, ArithmeticSource::A, gb); };
        table[0xC0] = [](Gameboy &gb) -> uint8_t { return SET(0, ArithmeticSource::B, gb); };
        table[0xC1] = [](Gameboy &gb) -> uint8_t { return SET(0, ArithmeticSource::C, gb); };
        table[0xC2] = [](Gameboy &gb) -> uint8_t { return SET(0, ArithmeticSource::D, gb); };
        table[0xC3] = [](Gameboy &gb) -> uint8_t { return SET(0, ArithmeticSource::E, gb); };
        table[0xC4] = [](Gameboy &gb) -> uint8_t { return SET(0, ArithmeticSource::H, gb); };
        table[0xC5] = [](Gameboy &gb) -> uint8_t { return SET(0, ArithmeticSource::L, gb); };
        table[0xC6] = [](Gameboy &gb) -> uint8_t { return SET(0, ArithmeticSource::HLAddr, gb); };
        table[0xC7] = [](Gameboy &gb) -> uint8_t { return SET(0, ArithmeticSource::A, gb); };
        table[0xC8] = [](Gameboy &gb) -> uint8_t { return SET(1, ArithmeticSource::B, gb); };
        table[0xC9] = [](Gameboy &gb) -> uint8_t { return SET(1, ArithmeticSource::C, gb); };
        table[0xCA] = [](Gameboy &gb) -> uint8_t { return SET(1, ArithmeticSource::D, gb); };
        table[0xCB] = [](Gameboy &gb) -> uint8_t { return SET(1, ArithmeticSource::E, gb); };
        table[0xCC] = [](Gameboy &gb) -> uint8_t { return SET(1, ArithmeticSource::H, gb); };
        table[0xCD] = [](Gameboy &gb) -> uint8_t { return SET(1, ArithmeticSource::L, gb); };
        table[0xCE] = [](Gameboy &gb) -> uint8_t { return SET(1, ArithmeticSource::HLAddr, gb); };
        table[0xCF] = [](Gameboy &gb) -> uint8_t { return SET(1, ArithmeticSource::A, gb); };
        table[0xD0] = [](Gameboy &gb) -> uint8_t { return SET(2, ArithmeticSource::B, gb); };
        table[0xD1] = [](Gameboy &gb) -> uint8_t { return SET(2, ArithmeticSource::C, gb); };
        table[0xD2] = [](Gameboy &gb) -> uint8_t { return SET(2, ArithmeticSource::D, gb); };
        table[0xD3] = [](Gameboy &gb) -> uint8_t { return SET(2, ArithmeticSource::E, gb); };
        table[0xD4] = [](Gameboy &gb) -> uint8_t { return SET(2, ArithmeticSource::H, gb); };
        table[0xD5] = [](Gameboy &gb) -> uint8_t { return SET(2, ArithmeticSource::L, gb); };
        table[0xD6] = [](Gameboy &gb) -> uint8_t { return SET(2, ArithmeticSource::HLAddr, gb); };
        table[0xD7] = [](Gameboy &gb) -> uint8_t { return SET(2, ArithmeticSource::A, gb); };
        table[0xD8] = [](Gameboy &gb) -> uint8_t { return SET(3, ArithmeticSource::B, gb); };
        table[0xD9] = [](Gameboy &gb) -> uint8_t { return SET(3, ArithmeticSource::C, gb); };
        table[0xDA] = [](Gameboy &gb) -> uint8_t { return SET(3, ArithmeticSource::D, gb); };
        table[0xDB] = [](Gameboy &gb) -> uint8_t { return SET(3, ArithmeticSource::E, gb); };
        table[0xDC] = [](Gameboy &gb) -> uint8_t { return SET(3, ArithmeticSource::H, gb); };
        table[0xDD] = [](Gameboy &gb) -> uint8_t { return SET(3, ArithmeticSource::L, gb); };
        table[0xDE] = [](Gameboy &gb) -> uint8_t { return SET(3, ArithmeticSource::HLAddr, gb); };
        table[0xDF] = [](Gameboy &gb) -> uint8_t { return SET(3, ArithmeticSource::A, gb); };
        table[0xE0] = [](Gameboy &gb) -> uint8_t { return SET(4, ArithmeticSource::B, gb); };
        table[0xE1] = [](Gameboy &gb) -> uint8_t { return SET(4, ArithmeticSource::C, gb); };
        table[0xE2] = [](Gameboy &gb) -> uint8_t { return SET(4, ArithmeticSource::D, gb); };
        table[0xE3] = [](Gameboy &gb) -> uint8_t { return SET(4, ArithmeticSource::E, gb); };
        table[0xE4] = [](Gameboy &gb) -> uint8_t { return SET(4, ArithmeticSource::H, gb); };
        table[0xE5] = [](Gameboy &gb) -> uint8_t { return SET(4, ArithmeticSource::L, gb); };
        table[0xE6] = [](Gameboy &gb) -> uint8_t { return SET(4, ArithmeticSource::HLAddr, gb); };
        table[0xE7] = [](Gameboy &gb) -> uint8_t { return SET(4, ArithmeticSource::A, gb); };
        table[0xE8] = [](Gameboy &gb) -> uint8_t { return SET(5, ArithmeticSource::B, gb); };
        table[0xE9] = [](Gameboy &gb) -> uint8_t { return SET(5, ArithmeticSource::C, gb); };
        table[0xEA] = [](Gameboy &gb) -> uint8_t { return SET(5, ArithmeticSource::D, gb); };
        table[0xEB] = [](Gameboy &gb) -> uint8_t { return SET(5, ArithmeticSource::E, gb); };
        table[0xEC] = [](Gameboy &gb) -> uint8_t { return SET(5, ArithmeticSource::H, gb); };
        table[0xED] = [](Gameboy &gb) -> uint8_t { return SET(5, ArithmeticSource::L, gb); };
        table[0xEE] = [](Gameboy &gb) -> uint8_t { return SET(5, ArithmeticSource::HLAddr, gb); };
        table[0xEF] = [](Gameboy &gb) -> uint8_t { return SET(5, ArithmeticSource::A, gb); };
        table[0xF0] = [](Gameboy &gb) -> uint8_t { return SET(6, ArithmeticSource::B, gb); };
        table[0xF1] = [](Gameboy &gb) -> uint8_t { return SET(6, ArithmeticSource::C, gb); };
        table[0xF2] = [](Gameboy &gb) -> uint8_t { return SET(6, ArithmeticSource::D, gb); };
        table[0xF3] = [](Gameboy &gb) -> uint8_t { return SET(6, ArithmeticSource::E, gb); };
        table[0xF4] = [](Gameboy &gb) -> uint8_t { return SET(6, ArithmeticSource::H, gb); };
        table[0xF5] = [](Gameboy &gb) -> uint8_t { return SET(6, ArithmeticSource::L, gb); };
        table[0xF6] = [](Gameboy &gb) -> uint8_t { return SET(6, ArithmeticSource::HLAddr, gb); };
        table[0xF7] = [](Gameboy &gb) -> uint8_t { return SET(6, ArithmeticSource::A, gb); };
        table[0xF8] = [](Gameboy &gb) -> uint8_t { return SET(7, ArithmeticSource::B, gb); };
        table[0xF9] = [](Gameboy &gb) -> uint8_t { return SET(7, ArithmeticSource::C, gb); };
        table[0xFA] = [](Gameboy &gb) -> uint8_t { return SET(7, ArithmeticSource::D, gb); };
        table[0xFB] = [](Gameboy &gb) -> uint8_t { return SET(7, ArithmeticSource::E, gb); };
        table[0xFC] = [](Gameboy &gb) -> uint8_t { return SET(7, ArithmeticSource::H, gb); };
        table[0xFD] = [](Gameboy &gb) -> uint8_t { return SET(7, ArithmeticSource::L, gb); };
        table[0xFE] = [](Gameboy &gb) -> uint8_t { return SET(7, ArithmeticSource::HLAddr, gb); };
        table[0xFF] = [](Gameboy &gb) -> uint8_t { return SET(7, ArithmeticSource::A, gb); };
        return table;
    }();

    static constexpr std::array<WrappedFunction, 256> nonPrefixedTable = [] {
        std::array<WrappedFunction, 256> table{};
        table[0x00] = [](Gameboy &gb) -> uint8_t { return NOP(gb); };
        table[0x01] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::BC, LoadWordSource::D16, gb); };
        table[0x02] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::BC, LoadByteSource::A, gb); };
        table[0x03] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::BC, gb); };
        table[0x04] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::B, gb); };
        table[0x05] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::B, gb); };
        table[0x06] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::D8, gb); };
        table[0x07] = [](Gameboy &gb) -> uint8_t { return RLCA(gb); };
        table[0x08] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::A16, LoadWordSource::SP, gb); };
        table[0x09] = [](Gameboy &gb) -> uint8_t { return ADD16(Arithmetic16Target::BC, gb); };
        table[0x10] = [](Gameboy &gb) -> uint8_t { return NOP(gb); };
        table[0x0A] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::BC, gb); };
        table[0x0B] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::BC, gb); };
        table[0x0C] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::C, gb); };
        table[0x0D] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::C, gb); };
        table[0x0E] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::D8, gb); };
        table[0x0F] = [](Gameboy &gb) -> uint8_t { return RRCA(gb); };
        table[0x11] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::DE, LoadWordSource::D16, gb); };
        table[0x12] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::DE, LoadByteSource::A, gb); };
        table[0x13] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::DE, gb); };
        table[0x14] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::D, gb); };
        table[0x15] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::D, gb); };
        table[0x16] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::D8, gb); };
        table[0x17] = [](Gameboy &gb) -> uint8_t { return RLA(gb); };
        table[0x18] = [](Gameboy &gb) -> uint8_t { return JR(JumpTest::Always, gb); };
        table[0x19] = [](Gameboy &gb) -> uint8_t { return ADD16(Arithmetic16Target::DE, gb); };
        table[0x1A] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::DE, gb); };
        table[0x1B] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::DE, gb); };
        table[0x1C] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::E, gb); };
        table[0x1D] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::E, gb); };
        table[0x1E] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::D8, gb); };
        table[0x1F] = [](Gameboy &gb) -> uint8_t { return RRA(gb); };
        table[0x20] = [](Gameboy &gb) -> uint8_t { return JR(JumpTest::NotZero, gb); };
        table[0x21] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::HL, LoadWordSource::D16, gb); };
        table[0x22] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HLI, LoadByteSource::A, gb); };
        table[0x23] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::HL, gb); };
        table[0x24] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::H, gb); };
        table[0x25] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::H, gb); };
        table[0x26] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::D8, gb); };
        table[0x27] = [](Gameboy &gb) -> uint8_t { return DAA(gb); };
        table[0x28] = [](Gameboy &gb) -> uint8_t { return JR(JumpTest::Zero, gb); };
        table[0x29] = [](Gameboy &gb) -> uint8_t { return ADD16(Arithmetic16Target::HL, gb); };
        table[0x2A] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::HLI, gb); };
        table[0x2B] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::HL, gb); };
        table[0x2C] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::L, gb); };
        table[0x2D] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::L, gb); };
        table[0x2E] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::D8, gb); };
        table[0x2F] = [](Gameboy &gb) -> uint8_t { return CPL(gb); };
        table[0x30] = [](Gameboy &gb) -> uint8_t { return JR(JumpTest::NotCarry, gb); };
        table[0x31] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::SP, LoadWordSource::D16, gb); };
        table[0x32] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HLD, LoadByteSource::A, gb); };
        table[0x33] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::SP, gb); };
        table[0x34] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::HLAddr, gb); };
        table[0x35] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::HLAddr, gb); };
        table[0x36] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HL, LoadByteSource::D8, gb); };
        table[0x37] = [](Gameboy &gb) -> uint8_t { return SCF(gb); };
        table[0x38] = [](Gameboy &gb) -> uint8_t { return JR(JumpTest::Carry, gb); };
        table[0x39] = [](Gameboy &gb) -> uint8_t { return ADD16(Arithmetic16Target::SP, gb); };
        table[0x3A] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::HLD, gb); };
        table[0x3B] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::SP, gb); };
        table[0x3C] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::A, gb); };
        table[0x3D] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::A, gb); };
        table[0x3E] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::D8, gb); };
        table[0x3F] = [](Gameboy &gb) -> uint8_t { return CCF(gb); };
        table[0x40] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::B, gb); };
        table[0x41] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::C, gb); };
        table[0x42] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::D, gb); };
        table[0x43] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::E, gb); };
        table[0x44] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::H, gb); };
        table[0x45] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::L, gb); };
        table[0x46] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::HL, gb); };
        table[0x47] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::A, gb); };
        table[0x48] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::B, gb); };
        table[0x49] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::C, gb); };
        table[0x4A] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::D, gb); };
        table[0x4B] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::E, gb); };
        table[0x4C] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::H, gb); };
        table[0x4D] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::L, gb); };
        table[0x4E] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::HL, gb); };
        table[0x4F] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::C, LoadByteSource::A, gb); };
        table[0x50] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::B, gb); };
        table[0x51] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::C, gb); };
        table[0x52] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::D, gb); };
        table[0x53] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::E, gb); };
        table[0x54] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::H, gb); };
        table[0x55] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::L, gb); };
        table[0x56] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::HL, gb); };
        table[0x57] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::D, LoadByteSource::A, gb); };
        table[0x58] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::B, gb); };
        table[0x59] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::C, gb); };
        table[0x5A] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::D, gb); };
        table[0x5B] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::E, gb); };
        table[0x5C] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::H, gb); };
        table[0x5D] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::L, gb); };
        table[0x5E] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::HL, gb); };
        table[0x5F] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::E, LoadByteSource::A, gb); };
        table[0x60] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::B, gb); };
        table[0x61] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::C, gb); };
        table[0x62] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::D, gb); };
        table[0x63] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::E, gb); };
        table[0x64] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::H, gb); };
        table[0x65] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::L, gb); };
        table[0x66] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::HL, gb); };
        table[0x67] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::H, LoadByteSource::A, gb); };
        table[0x68] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::B, gb); };
        table[0x69] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::C, gb); };
        table[0x6A] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::D, gb); };
        table[0x6B] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::E, gb); };
        table[0x6C] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::H, gb); };
        table[0x6D] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::L, gb); };
        table[0x6E] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::HL, gb); };
        table[0x6F] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::L, LoadByteSource::A, gb); };
        table[0x70] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HL, LoadByteSource::B, gb); };
        table[0x71] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HL, LoadByteSource::C, gb); };
        table[0x72] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HL, LoadByteSource::D, gb); };
        table[0x73] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HL, LoadByteSource::E, gb); };
        table[0x74] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HL, LoadByteSource::H, gb); };
        table[0x75] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HL, LoadByteSource::L, gb); };
        table[0x76] = [](Gameboy &gb) -> uint8_t { return HALT(gb); };
        table[0x77] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::HL, LoadByteSource::A, gb); };
        table[0x78] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::B, gb); };
        table[0x79] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::C, gb); };
        table[0x7A] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::D, gb); };
        table[0x7B] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::E, gb); };
        table[0x7C] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::H, gb); };
        table[0x7D] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::L, gb); };
        table[0x7E] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::HL, gb); };
        table[0x7F] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::A, gb); };
        table[0x80] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::B, gb); };
        table[0x81] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::C, gb); };
        table[0x82] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::D, gb); };
        table[0x83] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::E, gb); };
        table[0x84] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::H, gb); };
        table[0x85] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::L, gb); };
        table[0x86] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::HLAddr, gb); };
        table[0x87] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::A, gb); };
        table[0x88] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::B, gb); };
        table[0x89] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::C, gb); };
        table[0x8A] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::D, gb); };
        table[0x8B] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::E, gb); };
        table[0x8C] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::H, gb); };
        table[0x8D] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::L, gb); };
        table[0x8E] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::HLAddr, gb); };
        table[0x8F] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::A, gb); };
        table[0x90] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::B, gb); };
        table[0x91] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::C, gb); };
        table[0x92] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::D, gb); };
        table[0x93] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::E, gb); };
        table[0x94] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::H, gb); };
        table[0x95] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::L, gb); };
        table[0x96] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::HLAddr, gb); };
        table[0x97] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::A, gb); };
        table[0x98] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::B, gb); };
        table[0x99] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::C, gb); };
        table[0x9A] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::D, gb); };
        table[0x9B] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::E, gb); };
        table[0x9C] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::H, gb); };
        table[0x9D] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::L, gb); };
        table[0x9E] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::HLAddr, gb); };
        table[0x9F] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::A, gb); };
        table[0xA0] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::B, gb); };
        table[0xA1] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::C, gb); };
        table[0xA2] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::D, gb); };
        table[0xA3] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::E, gb); };
        table[0xA4] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::H, gb); };
        table[0xA5] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::L, gb); };
        table[0xA6] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::HLAddr, gb); };
        table[0xA7] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::A, gb); };
        table[0xA8] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::B, gb); };
        table[0xA9] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::C, gb); };
        table[0xAA] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::D, gb); };
        table[0xAB] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::E, gb); };
        table[0xAC] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::H, gb); };
        table[0xAD] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::L, gb); };
        table[0xAE] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::HLAddr, gb); };
        table[0xAF] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::A, gb); };
        table[0xB0] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::B, gb); };
        table[0xB1] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::C, gb); };
        table[0xB2] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::D, gb); };
        table[0xB3] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::E, gb); };
        table[0xB4] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::H, gb); };
        table[0xB5] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::L, gb); };
        table[0xB6] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::HLAddr, gb); };
        table[0xB7] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::A, gb); };
        table[0xB8] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::B, gb); };
        table[0xB9] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::C, gb); };
        table[0xBA] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::D, gb); };
        table[0xBB] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::E, gb); };
        table[0xBC] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::H, gb); };
        table[0xBD] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::L, gb); };
        table[0xBE] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::HLAddr, gb); };
        table[0xBF] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::A, gb); };
        table[0xC0] = [](Gameboy &gb) -> uint8_t { return RET(JumpTest::NotZero, gb); };
        table[0xC1] = [](Gameboy &gb) -> uint8_t { return POP(StackTarget::BC, gb); };
        table[0xC2] = [](Gameboy &gb) -> uint8_t { return JP(JumpTest::NotZero, gb); };
        table[0xC3] = [](Gameboy &gb) -> uint8_t { return JP(JumpTest::Always, gb); };
        table[0xC4] = [](Gameboy &gb) -> uint8_t { return CALL(JumpTest::NotZero, gb); };
        table[0xC5] = [](Gameboy &gb) -> uint8_t { return PUSH(StackTarget::BC, gb); };
        table[0xC6] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::U8, gb); };
        table[0xC7] = [](Gameboy &gb) -> uint8_t { return RST(RSTTargets::H00, gb); };
        table[0xC8] = [](Gameboy &gb) -> uint8_t { return RET(JumpTest::Zero, gb); };
        table[0xC9] = [](Gameboy &gb) -> uint8_t { return RET(JumpTest::Always, gb); };
        table[0xCA] = [](Gameboy &gb) -> uint8_t { return JP(JumpTest::Zero, gb); };
        table[0xCC] = [](Gameboy &gb) -> uint8_t { return CALL(JumpTest::Zero, gb); };
        table[0xCD] = [](Gameboy &gb) -> uint8_t { return CALL(JumpTest::Always, gb); };
        table[0xCE] = [](Gameboy &gb) -> uint8_t { return ADC(ArithmeticSource::U8, gb); };
        table[0xCF] = [](Gameboy &gb) -> uint8_t { return RST(RSTTargets::H08, gb); };
        table[0xD0] = [](Gameboy &gb) -> uint8_t { return RET(JumpTest::NotCarry, gb); };
        table[0xD1] = [](Gameboy &gb) -> uint8_t { return POP(StackTarget::DE, gb); };
        table[0xD2] = [](Gameboy &gb) -> uint8_t { return JP(JumpTest::NotCarry, gb); };
        table[0xD4] = [](Gameboy &gb) -> uint8_t { return CALL(JumpTest::NotCarry, gb); };
        table[0xD5] = [](Gameboy &gb) -> uint8_t { return PUSH(StackTarget::DE, gb); };
        table[0xD6] = [](Gameboy &gb) -> uint8_t { return SUB(ArithmeticSource::U8, gb); };
        table[0xD7] = [](Gameboy &gb) -> uint8_t { return RST(RSTTargets::H10, gb); };
        table[0xD8] = [](Gameboy &gb) -> uint8_t { return RET(JumpTest::Carry, gb); };
        table[0xD9] = [](Gameboy &gb) -> uint8_t { return RETI(gb); };
        table[0xDA] = [](Gameboy &gb) -> uint8_t { return JP(JumpTest::Carry, gb); };
        table[0xDC] = [](Gameboy &gb) -> uint8_t { return CALL(JumpTest::Carry, gb); };
        table[0xDE] = [](Gameboy &gb) -> uint8_t { return SBC(ArithmeticSource::U8, gb); };
        table[0xDF] = [](Gameboy &gb) -> uint8_t { return RST(RSTTargets::H18, gb); };
        table[0xE0] = [](Gameboy &gb) -> uint8_t { return LDH(LoadOtherTarget::A8, LoadOtherSource::A, gb); };
        table[0xE1] = [](Gameboy &gb) -> uint8_t { return POP(StackTarget::HL, gb); };
        table[0xE2] = [](Gameboy &gb) -> uint8_t { return LDH(LoadOtherTarget::CAddress, LoadOtherSource::A, gb); };
        table[0xE5] = [](Gameboy &gb) -> uint8_t { return PUSH(StackTarget::HL, gb); };
        table[0xE6] = [](Gameboy &gb) -> uint8_t { return AND(ArithmeticSource::U8, gb); };
        table[0xE7] = [](Gameboy &gb) -> uint8_t { return RST(RSTTargets::H20, gb); };
        table[0xE8] = [](Gameboy &gb) -> uint8_t { return ADD(ArithmeticSource::I8, gb); };
        table[0xE9] = [](Gameboy &gb) -> uint8_t { return JPHL(gb); };
        table[0xEA] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A16, LoadByteSource::A, gb); };
        table[0xEE] = [](Gameboy &gb) -> uint8_t { return XOR(ArithmeticSource::U8, gb); };
        table[0xEF] = [](Gameboy &gb) -> uint8_t { return RST(RSTTargets::H28, gb); };
        table[0xF0] = [](Gameboy &gb) -> uint8_t { return LDH(LoadOtherTarget::A, LoadOtherSource::A8, gb); };
        table[0xF1] = [](Gameboy &gb) -> uint8_t { return POP(StackTarget::AF, gb); };
        table[0xF2] = [](Gameboy &gb) -> uint8_t { return LDH(LoadOtherTarget::A, LoadOtherSource::CAddress, gb); };
        table[0xF3] = [](Gameboy &gb) -> uint8_t { return DI(gb); };
        table[0xF5] = [](Gameboy &gb) -> uint8_t { return PUSH(StackTarget::AF, gb); };
        table[0xF6] = [](Gameboy &gb) -> uint8_t { return OR(ArithmeticSource::U8, gb); };
        table[0xF7] = [](Gameboy &gb) -> uint8_t { return RST(RSTTargets::H30, gb); };
        table[0xF8] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::HL, LoadWordSource::SPr8, gb); };
        table[0xF9] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::SP, LoadWordSource::HL, gb); };
        table[0xFA] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::A, LoadByteSource::A16, gb); };
        table[0xFB] = [](Gameboy &gb) -> uint8_t { return EI(gb); };
        table[0xFE] = [](Gameboy &gb) -> uint8_t { return CP(ArithmeticSource::U8, gb); };
        table[0xFF] = [](Gameboy &gb) -> uint8_t { return RST(RSTTargets::H38, gb); };
        return table;
    }();
};
