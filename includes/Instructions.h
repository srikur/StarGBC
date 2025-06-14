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
    static uint8_t DAA(const Gameboy &gameboy);

    static uint8_t RETI(Gameboy &gameboy);

    static uint8_t DI(const Gameboy &gameboy);

    static uint8_t EI(Gameboy &gameboy);

    static uint8_t HALT(Gameboy &gameboy);

    static uint8_t RST(RSTTargets target, Gameboy &gameboy);

    static uint8_t CALL(JumpTest test, Gameboy &gameboy);

    static uint8_t RLCA(const Gameboy &gameboy);

    static uint8_t RLC(ArithmeticSource source, const Gameboy &gameboy);

    static uint8_t RLCAddr(Gameboy &gameboy);

    static uint8_t RLA(const Gameboy &gameboy);

    static uint8_t RL(ArithmeticSource source, const Gameboy &gameboy);

    static uint8_t RLAddr(Gameboy &gameboy);

    static uint8_t CCF(const Gameboy &gameboy);

    static uint8_t CPL(const Gameboy &gameboy);

    static uint8_t SCF(const Gameboy &gameboy);

    static uint8_t RRCA(const Gameboy &gameboy);

    static uint8_t RRC(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t RR(ArithmeticSource source, const Gameboy &gameboy);

    static uint8_t RRAddr(Gameboy &gameboy);

    static uint8_t RRA(const Gameboy &gameboy);

    static uint8_t RET(JumpTest test, Gameboy &gameboy);

    static uint8_t JR(JumpTest test, Gameboy &gameboy);

    static uint8_t JP(JumpTest test, Gameboy &gameboy);

    static uint8_t JPHL(Gameboy &gameboy);

    static uint8_t NOP();

    static uint8_t STOP(Gameboy &gameboy);

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

    static uint8_t SLA(ArithmeticSource source, const Gameboy &gameboy);

    static uint8_t SLAAddr(Gameboy &gameboy);

    static uint8_t SRA(ArithmeticSource source, const Gameboy &gameboy);

    static uint8_t SRAAddr(Gameboy &gameboy);

    static uint8_t SWAP(ArithmeticSource source, const Gameboy &gameboy);

    static uint8_t SWAPAddr(Gameboy &gameboy);

    static uint8_t SRL(ArithmeticSource source, const Gameboy &gameboy);

    static uint8_t SRLAddr(Gameboy &gameboy);

    static uint8_t BIT(uint8_t target, ArithmeticSource source, Gameboy &gameboy);

    static uint8_t RES(uint8_t target, ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SET(uint8_t target, ArithmeticSource source, Gameboy &gameboy);

    static uint8_t SBC(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t ADC(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t ADD16(Arithmetic16Target target, const Gameboy &gameboy);

    static uint8_t ADD(ArithmeticSource source, Gameboy &gameboy);

    static uint8_t ADDSigned(Gameboy &gameboy);

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

    static uint16_t ReadNextWord(Gameboy &gameboy);

    static uint8_t ReadNextByte(Gameboy &gameboy);

    static void WriteByte(Gameboy &gameboy, uint16_t address, uint8_t value);

    static void WriteWord(Gameboy &gameboy, uint16_t address, uint16_t value);

    static uint8_t ReadByte(Gameboy &gameboy, uint16_t address);

    using WrappedFunction = uint8_t(*)(Gameboy &);

    template<typename... Args>
    uint8_t prefixedInstr(const uint8_t opcode, Args &&... args) {
        return prefixedTable[opcode](std::forward<Args>(args)...);
    }

    template<typename... Args>
    uint8_t nonPrefixedInstr(const uint8_t opcode, Args &&... args) {
        return nonPrefixedTable[opcode](std::forward<Args>(args)...);
    }

    static std::string GetMnemonic(uint16_t instruction) {
        const bool prefixed = instruction >> 8 == 0xCB;
        instruction &= 0xFF;
        return prefixed ? prefixedInstructions[instruction] : nonPrefixedInstructions[instruction];
    }

    static constexpr std::array<WrappedFunction, 256> prefixedTable = [] {
        std::array<WrappedFunction, 256> table{};
        table[0x00] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::B, gb); };
        table[0x01] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::C, gb); };
        table[0x02] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::D, gb); };
        table[0x03] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::E, gb); };
        table[0x04] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::H, gb); };
        table[0x05] = [](Gameboy &gb) -> uint8_t { return RLC(ArithmeticSource::L, gb); };
        table[0x06] = [](Gameboy &gb) -> uint8_t { return RLCAddr(gb); };
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
        table[0x16] = [](Gameboy &gb) -> uint8_t { return RLAddr(gb); };
        table[0x17] = [](Gameboy &gb) -> uint8_t { return RL(ArithmeticSource::A, gb); };
        table[0x18] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::B, gb); };
        table[0x19] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::C, gb); };
        table[0x1A] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::D, gb); };
        table[0x1B] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::E, gb); };
        table[0x1C] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::H, gb); };
        table[0x1D] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::L, gb); };
        table[0x1E] = [](Gameboy &gb) -> uint8_t { return RRAddr(gb); };
        table[0x1F] = [](Gameboy &gb) -> uint8_t { return RR(ArithmeticSource::A, gb); };
        table[0x20] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::B, gb); };
        table[0x21] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::C, gb); };
        table[0x22] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::D, gb); };
        table[0x23] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::E, gb); };
        table[0x24] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::H, gb); };
        table[0x25] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::L, gb); };
        table[0x26] = [](Gameboy &gb) -> uint8_t { return SLAAddr(gb); };
        table[0x27] = [](Gameboy &gb) -> uint8_t { return SLA(ArithmeticSource::A, gb); };
        table[0x28] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::B, gb); };
        table[0x29] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::C, gb); };
        table[0x2A] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::D, gb); };
        table[0x2B] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::E, gb); };
        table[0x2C] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::H, gb); };
        table[0x2D] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::L, gb); };
        table[0x2E] = [](Gameboy &gb) -> uint8_t { return SRAAddr(gb); };
        table[0x2F] = [](Gameboy &gb) -> uint8_t { return SRA(ArithmeticSource::A, gb); };
        table[0x30] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::B, gb); };
        table[0x31] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::C, gb); };
        table[0x32] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::D, gb); };
        table[0x33] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::E, gb); };
        table[0x34] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::H, gb); };
        table[0x35] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::L, gb); };
        table[0x36] = [](Gameboy &gb) -> uint8_t { return SWAPAddr(gb); };
        table[0x37] = [](Gameboy &gb) -> uint8_t { return SWAP(ArithmeticSource::A, gb); };
        table[0x38] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::B, gb); };
        table[0x39] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::C, gb); };
        table[0x3A] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::D, gb); };
        table[0x3B] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::E, gb); };
        table[0x3C] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::H, gb); };
        table[0x3D] = [](Gameboy &gb) -> uint8_t { return SRL(ArithmeticSource::L, gb); };
        table[0x3E] = [](Gameboy &gb) -> uint8_t { return SRLAddr(gb); };
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
        table[0x00] = [](Gameboy &) -> uint8_t { return NOP(); };
        table[0x01] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::BC, LoadWordSource::D16, gb); };
        table[0x02] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::BC, LoadByteSource::A, gb); };
        table[0x03] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::BC, gb); };
        table[0x04] = [](Gameboy &gb) -> uint8_t { return INC(IncDecTarget::B, gb); };
        table[0x05] = [](Gameboy &gb) -> uint8_t { return DEC(IncDecTarget::B, gb); };
        table[0x06] = [](Gameboy &gb) -> uint8_t { return LD(LoadByteTarget::B, LoadByteSource::D8, gb); };
        table[0x07] = [](Gameboy &gb) -> uint8_t { return RLCA(gb); };
        table[0x08] = [](Gameboy &gb) -> uint8_t { return LD16(LoadWordTarget::A16, LoadWordSource::SP, gb); };
        table[0x09] = [](Gameboy &gb) -> uint8_t { return ADD16(Arithmetic16Target::BC, gb); };
        table[0x10] = [](Gameboy &gb) -> uint8_t { return STOP(gb); };
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
        table[0xE8] = [](Gameboy &gb) -> uint8_t { return ADDSigned(gb); };
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

    static constexpr std::array<std::string, 256> prefixedInstructions = {
        "RLC B", "RLC C", "RLC D", "RLC E", "RLC H", "RLC L", "RLC (HL)", "RLC A", "RRC B", "RRC C", "RRC D", "RRC E",
        "RRC H", "RRC L", "RRC (HL)", "RRC A", "RL B", "RL C", "RL D", "RL E", "RL H", "RL L", "RL (HL)", "RL A",
        "RR B", "RR C", "RR D", "RR E", "RR H", "RR L", "RR (HL)", "RR A", "SLA B", "SLA C", "SLA D", "SLA E", "SLA H",
        "SLA L", "SLA (HL)", "SLA A", "SRA B", "SRA C", "SRA D", "SRA E", "SRA H", "SRA L", "SRA (HL)", "SRA A",
        "SWAP B", "SWAP C", "SWAP D", "SWAP E", "SWAP H", "SWAP L", "SWAP (HL)", "SWAP A", "SRL B", "SRL C", "SRL D",
        "SRL E", "SRL H", "SRL L", "SRL (HL)", "SRL A", "BIT 0,B", "BIT 0,C", "BIT 0,D", "BIT 0,E", "BIT 0,H",
        "BIT 0,L", "BIT 0,(HL)", "BIT 0,A", "BIT 1,B", "BIT 1,C", "BIT 1,D", "BIT 1,E", "BIT 1,H", "BIT 1,L",
        "BIT 1,(HL)", "BIT 1,A", "BIT 2,B", "BIT 2,C", "BIT 2,D", "BIT 2,E", "BIT 2,H", "BIT 2,L", "BIT 2,(HL)",
        "BIT 2,A", "BIT 3,B", "BIT 3,C", "BIT 3,D", "BIT 3,E", "BIT 3,H", "BIT 3,L", "BIT 3,(HL)", "BIT 3,A", "BIT 4,B",
        "BIT 4,C", "BIT 4,D", "BIT 4,E", "BIT 4,H", "BIT 4,L", "BIT 4,(HL)", "BIT 4,A", "BIT 5,B", "BIT 5,C", "BIT 5,D",
        "BIT 5,E", "BIT 5,H", "BIT 5,L", "BIT 5,(HL)", "BIT 5,A", "BIT 6,B", "BIT 6,C", "BIT 6,D", "BIT 6,E", "BIT 6,H",
        "BIT 6,L", "BIT 6,(HL)", "BIT 6,A", "BIT 7,B", "BIT 7,C", "BIT 7,D", "BIT 7,E", "BIT 7,H", "BIT 7,L",
        "BIT 7,(HL)", "BIT 7,A", "RES 0,B", "RES 0,C", "RES 0,D", "RES 0,E", "RES 0,H", "RES 0,L", "RES 0,(HL)",
        "RES 0,A", "RES 1,B", "RES 1,C", "RES 1,D", "RES 1,E", "RES 1,H", "RES 1,L", "RES 1,(HL)", "RES 1,A", "RES 2,B",
        "RES 2,C", "RES 2,D", "RES 2,E", "RES 2,H", "RES 2,L", "RES 2,(HL)", "RES 2,A", "RES 3,B", "RES 3,C", "RES 3,D",
        "RES 3,E", "RES 3,H", "RES 3,L", "RES 3,(HL)", "RES 3,A", "RES 4,B", "RES 4,C", "RES 4,D", "RES 4,E", "RES 4,H",
        "RES 4,L", "RES 4,(HL)", "RES 4,A", "RES 5,B", "RES 5,C", "RES 5,D", "RES 5,E", "RES 5,H", "RES 5,L",
        "RES 5,(HL)", "RES 5,A", "RES 6,B", "RES 6,C", "RES 6,D", "RES 6,E", "RES 6,H", "RES 6,L", "RES 6,(HL)",
        "RES 6,A", "RES 7,B", "RES 7,C", "RES 7,D", "RES 7,E", "RES 7,H", "RES 7,L", "RES 7,(HL)", "RES 7,A", "SET 0,B",
        "SET 0,C", "SET 0,D", "SET 0,E", "SET 0,H", "SET 0,L", "SET 0,(HL)", "SET 0,A", "SET 1,B", "SET 1,C", "SET 1,D",
        "SET 1,E", "SET 1,H", "SET 1,L", "SET 1,(HL)", "SET 1,A", "SET 2,B", "SET 2,C", "SET 2,D", "SET 2,E", "SET 2,H",
        "SET 2,L", "SET 2,(HL)", "SET 2,A", "SET 3,B", "SET 3,C", "SET 3,D", "SET 3,E", "SET 3,H", "SET 3,L",
        "SET 3,(HL)", "SET 3,A", "SET 4,B", "SET 4,C", "SET 4,D", "SET 4,E", "SET 4,H", "SET 4,L", "SET 4,(HL)",
        "SET 4,A", "SET 5,B", "SET 5,C", "SET 5,D", "SET 5,E", "SET 5,H", "SET 5,L", "SET 5,(HL)", "SET 5,A", "SET 6,B",
        "SET 6,C", "SET 6,D", "SET 6,E", "SET 6,H", "SET 6,L", "SET 6,(HL)", "SET 6,A", "SET 7,B", "SET 7,C", "SET 7,D",
        "SET 7,E", "SET 7,H", "SET 7,L", "SET 7,(HL)", "SET 7,A"
    };

    static constexpr std::array<std::string, 256> nonPrefixedInstructions = {
        "NOP", "LD BC,d16", "LD (BC),A", "INC BC", "INC B", "DEC B", "LD B,d8", "RLCA", "LD (a16),SP", "ADD HL,BC",
        "LD A,(BC)", "DEC BC", "INC C", "DEC C", "LD C,d8", "RRCA", "STOP", "LD DE,d16", "LD (DE),A", "INC DE", "INC D",
        "DEC D", "LD D,d8", "RLA", "JR r8", "ADD HL,DE", "LD A,(DE)", "DEC DE", "INC E", "DEC E", "LD E,d8", "RRA",
        "JR NZ,r8", "LD HL,d16", "LD (HL+),A", "INC HL", "INC H", "DEC H", "LD H,d8", "DAA", "JR Z,r8", "ADD HL,HL",
        "LD A,(HL+)", "DEC HL", "INC L", "DEC L", "LD L,d8", "CPL", "JR NC,r8", "LD SP,d16", "LD (HL-),A", "INC SP",
        "INC (HL)", "DEC (HL)", "LD (HL),d8", "SCF", "JR C,r8", "ADD HL,SP", "LD A,(HL-)", "DEC SP", "INC A", "DEC A",
        "LD A,d8", "CCF", "LD B,B", "LD B,C", "LD B,D", "LD B,E", "LD B,H", "LD B,L", "LD B,(HL)", "LD B,A", "LD C,B",
        "LD C,C", "LD C,D", "LD C,E", "LD C,H", "LD C,L", "LD C,(HL)", "LD C,A", "LD D,B", "LD D,C", "LD D,D", "LD D,E",
        "LD D,H", "LD D,L", "LD D,(HL)", "LD D,A", "LD E,B", "LD E,C", "LD E,D", "LD E,E", "LD E,H", "LD E,L",
        "LD E,(HL)", "LD E,A", "LD H,B", "LD H,C", "LD H,D", "LD H,E", "LD H,H", "LD H,L", "LD H,(HL)", "LD H,A",
        "LD L,B", "LD L,C", "LD L,D", "LD L,E", "LD L,H", "LD L,L", "LD L,(HL)", "LD L,A", "LD (HL),B", "LD (HL),C",
        "LD (HL),D", "LD (HL),E", "LD (HL),H", "LD (HL),L", "HALT", "LD (HL),A", "LD A,B", "LD A,C", "LD A,D", "LD A,E",
        "LD A,H", "LD A,L", "LD A,(HL)", "LD A,A", "ADD A,B", "ADD A,C", "ADD A,D", "ADD A,E", "ADD A,H", "ADD A,L",
        "ADD A,(HL)", "ADD A,A", "ADC A,B", "ADC A,C", "ADC A,D", "ADC A,E", "ADC A,H", "ADC A,L", "ADC A,(HL)",
        "ADC A,A", "SUB B", "SUB C", "SUB D", "SUB E", "SUB H", "SUB L", "SUB (HL)", "SUB A", "SBC A,B", "SBC A,C",
        "SBC A,D", "SBC A,E", "SBC A,H", "SBC A,L", "SBC A,(HL)", "SBC A,A", "AND B", "AND C", "AND D", "AND E",
        "AND H", "AND L", "AND (HL)", "AND A", "XOR B", "XOR C", "XOR D", "XOR E", "XOR H", "XOR L", "XOR (HL)",
        "XOR A", "OR B", "OR C", "OR D", "OR E", "OR H", "OR L", "OR (HL)", "OR A", "CP B", "CP C", "CP D", "CP E",
        "CP H", "CP L", "CP (HL)", "CP A", "RET NZ", "POP BC", "JP NZ,a16", "JP a16", "CALL NZ,a16", "PUSH BC",
        "ADD A,d8", "RST 00H", "RET Z", "RET", "JP Z,a16", "PREFIX", "CALL Z,a16", "CALL a16", "ADC A,d8", "RST 08H",
        "RET NC", "POP DE", "JP NC,a16", "CALL NC,a16", "PUSH DE", "SUB d8", "RST 10H", "RET C", "RETI", "JP C,a16",
        "CALL C,a16", "SBC A,d8", "RST 18H", "LD (FF00+d8),A", "POP HL", "LD (FF00+C),A", "PUSH HL", "AND d8",
        "RST 20H", "ADD SP,d8", "JP (HL)", "LD (a16),A", "XOR d8", "RST 28H", "LD A,(FF00+d8)", "POP AF",
        "LD A,(FF00+C)", "DI", "PUSH AF", "OR d8", "RST 30H", "LD HL,SP+d8", "LD SP,HL", "LD A,(a16)", "EI", "CP d8",
        "RST 38H"
    };
};
