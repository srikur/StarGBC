#pragma once

#include "Debugger.h"

enum class CodeSource { Mapped, Cartridge, Bios };

struct CodeEntry {
    CodeSource source{};
    uint16_t bank{};
    uint32_t offset{};
    DisassembledInstruction instruction;
};

class CodeListing {
public:
    CodeListing(Gameboy &machine, Debugger &debugger) : machine_(machine), debugger_(debugger) {}

    void update(CodeSource source, std::optional<uint32_t> anchor = {});
    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t find_row(uint32_t offset) const;
    [[nodiscard]] CodeEntry entry(size_t row) const;
    [[nodiscard]] CodeSource execution_source() const;
    [[nodiscard]] uint32_t execution_offset() const;
    [[nodiscard]] bool is_current(const CodeEntry &entry) const;
    [[nodiscard]] static std::string_view source_label(CodeSource source);

private:
    [[nodiscard]] std::span<const uint8_t> bytes() const;
    [[nodiscard]] uint32_t segment_end(uint32_t offset) const;
    [[nodiscard]] const std::vector<uint32_t> &offsets() const;
    void index(std::vector<uint32_t> &output);

    Gameboy &machine_;
    Debugger &debugger_;
    CodeSource source_{CodeSource::Mapped};
    std::array<uint8_t, 0x10000> mapped_{};
    std::vector<uint32_t> mapped_offsets_;
    std::vector<uint32_t> rom_offsets_;
    std::vector<uint32_t> bios_offsets_;
    std::vector<uint32_t> anchored_offsets_;
    size_t anchor_begin_{};
    size_t anchor_end_{};
};
