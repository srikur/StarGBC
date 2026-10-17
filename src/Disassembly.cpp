#include "Disassembly.h"

#include <format>
#include <string_view>

DisassembledInstruction disassemble(const uint16_t address, const std::array<uint8_t, 3> bytes) {
    constexpr std::array<std::string_view, 8> registers{"B", "C", "D", "E", "H", "L", "(HL)", "A"};
    constexpr std::array<std::string_view, 4> pairs{"BC", "DE", "HL", "SP"};
    constexpr std::array<std::string_view, 4> stack_pairs{"BC", "DE", "HL", "AF"};
    constexpr std::array<std::string_view, 4> conditions{"NZ", "Z", "NC", "C"};
    constexpr std::array<std::string_view, 8> arithmetic{"ADD A,", "ADC A,", "SUB", "SBC A,", "AND", "XOR", "OR", "CP"};
    constexpr std::array<std::string_view, 8> rotations{"RLC", "RRC", "RL", "RR", "SLA", "SRA", "SWAP", "SRL"};
    constexpr std::array<std::string_view, 8> misc{"RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF"};
    DisassembledInstruction result{.address = address, .bytes = bytes};
    const auto opcode{bytes[0]};
    const auto x{opcode >> 6};
    const auto y{(opcode >> 3) & 7};
    const auto z{opcode & 7};
    const auto p{y >> 1};
    const auto q{y & 1};
    const auto byte{std::format("${:02X}", bytes[1])};
    const auto word{std::format("${:04X}", bytes[1] | bytes[2] << 8)};
    const auto relative{std::format("${:04X}", static_cast<uint16_t>(address + 2 + static_cast<int8_t>(bytes[1])))};
    if (opcode == 0xCB) {
        result.size = 2;
        const auto group{bytes[1] >> 6};
        const auto bit{(bytes[1] >> 3) & 7};
        const auto target{registers[bytes[1] & 7]};
        constexpr std::array<std::string_view, 4> operations{"", "BIT", "RES", "SET"};
        result.text = group ? std::format("{} {}, {}", operations[group], bit, target) : std::format("{} {}", rotations[bit], target);
    } else if (x == 1) {
        result.text = opcode == 0x76 ? "HALT" : std::format("LD {}, {}", registers[y], registers[z]);
    } else if (x == 2) {
        result.text = std::format("{} {}", arithmetic[y], registers[z]);
    } else if (x == 0) {
        switch (z) {
            case 0:
                if (y == 0)
                    result.text = "NOP";
                else if (y == 1) {
                    result.size = 3;
                    result.text = std::format("LD ({}), SP", word);
                } else {
                    result.size = 2;
                    result.text = y == 2 ? "STOP" : y == 3 ? std::format("JR {}", relative) : std::format("JR {}, {}", conditions[y - 4], relative);
                }
                break;
            case 1:
                result.size = q ? 1 : 3;
                result.text = q ? std::format("ADD HL, {}", pairs[p]) : std::format("LD {}, {}", pairs[p], word);
                break;
            case 2: {
                constexpr std::array<std::string_view, 4> indirect{"(BC)", "(DE)", "(HL+)", "(HL-)"};
                result.text = q ? std::format("LD A, {}", indirect[p]) : std::format("LD {}, A", indirect[p]);
                break;
            }
            case 3:
                result.text = std::format("{} {}", q ? "DEC" : "INC", pairs[p]);
                break;
            case 4:
            case 5:
                result.text = std::format("{} {}", z == 4 ? "INC" : "DEC", registers[y]);
                break;
            case 6:
                result.size = 2;
                result.text = std::format("LD {}, {}", registers[y], byte);
                break;
            case 7:
                result.text = misc[y];
                break;
        }
    } else {
        switch (z) {
            case 0:
                if (y < 4)
                    result.text = std::format("RET {}", conditions[y]);
                else {
                    result.size = 2;
                    const auto offset{static_cast<int8_t>(bytes[1])};
                    switch (y) {
                        case 4:
                            result.text = std::format("LDH ($FF00+{}), A", byte);
                            break;
                        case 5:
                            result.text = std::format("ADD SP, {:+}", offset);
                            break;
                        case 6:
                            result.text = std::format("LDH A, ($FF00+{})", byte);
                            break;
                        case 7:
                            result.text = std::format("LD HL, SP{:+}", offset);
                            break;
                    }
                }
                break;
            case 1:
                if (!q)
                    result.text = std::format("POP {}", stack_pairs[p]);
                else {
                    constexpr std::array<std::string_view, 4> operations{"RET", "RETI", "JP HL", "LD SP, HL"};
                    result.text = operations[p];
                }
                break;
            case 2:
                result.size = y == 4 || y == 6 ? 1 : 3;
                if (y < 4)
                    result.text = std::format("JP {}, {}", conditions[y], word);
                else if (y == 4)
                    result.text = "LD ($FF00+C), A";
                else if (y == 6)
                    result.text = "LD A, ($FF00+C)";
                else
                    result.text = y == 5 ? std::format("LD ({}), A", word) : std::format("LD A, ({})", word);
                break;
            case 3:
                if (y == 0) {
                    result.size = 3;
                    result.text = std::format("JP {}", word);
                } else if (y == 6 || y == 7)
                    result.text = y == 6 ? "DI" : "EI";
                break;
            case 4:
                if (y < 4) {
                    result.size = 3;
                    result.call = true;
                    result.text = std::format("CALL {}, {}", conditions[y], word);
                }
                break;
            case 5:
                if (!q)
                    result.text = std::format("PUSH {}", stack_pairs[p]);
                else if (p == 0) {
                    result.size = 3;
                    result.call = true;
                    result.text = std::format("CALL {}", word);
                }
                break;
            case 6:
                result.size = 2;
                result.text = std::format("{} {}", arithmetic[y], byte);
                break;
            case 7:
                result.call = true;
                result.text = std::format("RST ${:02X}", y * 8);
                break;
        }
    }
    if (result.text.empty())
        result.text = std::format("LOCK ${:02X}", opcode);
    return result;
}
