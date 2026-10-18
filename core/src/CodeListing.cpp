#include "CodeListing.h"

#include <format>

namespace {
    const auto instruction_sizes{[] {
        std::array<uint8_t, 256> sizes{};
        for (auto opcode{0u}; opcode < sizes.size(); ++opcode)
            sizes[opcode] = disassemble(0, {static_cast<uint8_t>(opcode), 0, 0}).size;
        return sizes;
    }()};
}

std::string_view CodeListing::source_label(const CodeSource source) {
    switch (source) {
        case CodeSource::Bios:
            return "BIOS";
        case CodeSource::Cartridge:
            return "ROM";
        case CodeSource::Mapped:
            return "MEM";
    }
    return "MEM";
}

std::span<const uint8_t> CodeListing::bytes() const {
    switch (source_) {
        case CodeSource::Cartridge:
            return machine_.rom_bytes();
        case CodeSource::Bios:
            return machine_.BootromBytes();
        case CodeSource::Mapped:
            return mapped_;
    }
    return {};
}

uint32_t CodeListing::segment_end(const uint32_t offset) const {
    auto end{static_cast<uint32_t>(bytes().size())};
    if (source_ == CodeSource::Cartridge)
        end = std::min(end, (offset / ROM_BANK_SIZE + 1) * ROM_BANK_SIZE);
    if (source_ == CodeSource::Bios && offset < BOOT_ROM_LOW_END)
        end = std::min<uint32_t>(end, BOOT_ROM_LOW_END);
    if (source_ == CodeSource::Mapped) {
        constexpr std::array boundaries{uint32_t{BOOT_ROM_LOW_END}, uint32_t{BOOT_ROM_HIGH_BEGIN}, uint32_t{ROM_BANK_SIZE},  uint32_t{VRAM_BEGIN},
                                        uint32_t{WRAM_BEGIN},       uint32_t{OAM_BEGIN},           uint32_t{HIGH_RAM_BEGIN}, 0x10000u};
        for (const auto boundary : boundaries)
            if (offset < boundary)
                end = std::min(end, boundary);
        const auto boot_end{static_cast<uint32_t>(machine_.BootromBytes().size())};
        if (offset < boot_end)
            end = std::min(end, boot_end);
    }
    return end;
}

const std::vector<uint32_t> &CodeListing::offsets() const {
    switch (source_) {
        case CodeSource::Cartridge:
            return rom_offsets_;
        case CodeSource::Bios:
            return bios_offsets_;
        case CodeSource::Mapped:
            return mapped_offsets_;
    }
    return mapped_offsets_;
}

void CodeListing::index(std::vector<uint32_t> &output) {
    output.clear();
    const auto data{bytes()};
    for (auto offset{0u}; offset < data.size();) {
        if (source_ == CodeSource::Bios && offset >= BOOT_ROM_LOW_END && offset < BOOT_ROM_HIGH_BEGIN) {
            offset = BOOT_ROM_HIGH_BEGIN;
            continue;
        }
        output.push_back(offset);
        const auto length{instruction_sizes[data[offset]]};
        offset += offset + length <= segment_end(offset) ? length : 1;
    }
}

void CodeListing::update(const CodeSource source, const std::optional<uint32_t> anchor) {
    source_ = source;
    if (source == CodeSource::Mapped) {
        for (auto address{0u}; address < mapped_.size(); ++address)
            mapped_[address] = debugger_.read_memory(address);
        index(mapped_offsets_);
    } else if (source == CodeSource::Cartridge && rom_offsets_.empty())
        index(rom_offsets_);
    else if (source == CodeSource::Bios && bios_offsets_.empty())
        index(bios_offsets_);
    anchored_offsets_.clear();
    anchor_begin_ = anchor_end_ = 0;
    if (!anchor || *anchor >= bytes().size() || (source == CodeSource::Bios && *anchor >= BOOT_ROM_LOW_END && *anchor < BOOT_ROM_HIGH_BEGIN))
        return;
    const auto &rows{offsets()};
    anchor_begin_ = anchor_end_ = std::ranges::lower_bound(rows, *anchor) - rows.begin();
    auto offset{*anchor};
    while (offset < bytes().size() && (anchor_end_ == rows.size() || rows[anchor_end_] != offset)) {
        anchored_offsets_.push_back(offset);
        const auto length{instruction_sizes[bytes()[offset]]};
        offset += offset + length <= segment_end(offset) ? length : 1;
        if (source == CodeSource::Bios && offset >= BOOT_ROM_LOW_END && offset < BOOT_ROM_HIGH_BEGIN)
            offset = BOOT_ROM_HIGH_BEGIN;
        while (anchor_end_ < rows.size() && rows[anchor_end_] < offset)
            ++anchor_end_;
    }
}

size_t CodeListing::size() const { return offsets().size() - (anchor_end_ - anchor_begin_) + anchored_offsets_.size(); }

size_t CodeListing::find_row(const uint32_t offset) const {
    if (!size())
        return 0;
    const auto &rows{offsets()};
    if (!anchored_offsets_.empty() && offset >= anchored_offsets_.front() && (anchor_end_ == rows.size() || offset < rows[anchor_end_]))
        return std::min(anchor_begin_ + static_cast<size_t>(std::ranges::lower_bound(anchored_offsets_, offset) - anchored_offsets_.begin()),
                        size() - 1);
    auto row{static_cast<size_t>(std::ranges::lower_bound(rows, offset) - rows.begin())};
    if (row >= anchor_end_)
        row = row - (anchor_end_ - anchor_begin_) + anchored_offsets_.size();
    return std::min(row, size() - 1);
}

CodeEntry CodeListing::entry(const size_t row) const {
    if (row >= size())
        throw std::out_of_range("disassembly row outside ROM");
    const auto offset{row < anchor_begin_                              ? offsets()[row]
                      : row < anchor_begin_ + anchored_offsets_.size() ? anchored_offsets_[row - anchor_begin_]
                                                                       : offsets()[row - anchored_offsets_.size() + (anchor_end_ - anchor_begin_)]};
    auto origin{source_};
    auto address{static_cast<uint16_t>(offset)};
    auto bank{BOOT_ROM_BANK};
    if (source_ == CodeSource::Cartridge) {
        bank = offset / ROM_BANK_SIZE;
        auto base{bank == machine_.debug_bank(BOOT_ROM_LOW_END) ? 0u : unsigned{ROM_BANK_SIZE}};
        if (bank == machine_.debug_instruction_bank() && machine_.debug_instruction_address() < VRAM_BEGIN)
            base = machine_.debug_instruction_address() / ROM_BANK_SIZE * ROM_BANK_SIZE;
        address = base + offset % ROM_BANK_SIZE;
    } else if (source_ == CodeSource::Mapped) {
        bank = machine_.debug_bank(address);
        if (address < VRAM_BEGIN)
            origin = bank == BOOT_ROM_BANK ? CodeSource::Bios : CodeSource::Cartridge;
    }
    std::array<uint8_t, 3> instruction_bytes{};
    const auto end{segment_end(offset)};
    for (auto byte{0u}; byte < instruction_bytes.size() && offset + byte < end; ++byte)
        instruction_bytes[byte] = bytes()[offset + byte];
    auto instruction{disassemble(address, instruction_bytes)};
    if (offset + instruction.size > end) {
        instruction.size = 1;
        instruction.call = false;
        instruction.text = std::format("DB ${:02X}", instruction_bytes[0]);
    }
    return {origin, bank, offset, std::move(instruction)};
}

CodeSource CodeListing::execution_source() const {
    if (machine_.debug_instruction_address() >= VRAM_BEGIN)
        return CodeSource::Mapped;
    return machine_.debug_instruction_bank() == BOOT_ROM_BANK ? CodeSource::Bios : CodeSource::Cartridge;
}

uint32_t CodeListing::execution_offset() const {
    if (execution_source() == CodeSource::Cartridge)
        return machine_.debug_instruction_bank() * ROM_BANK_SIZE + machine_.debug_instruction_address() % ROM_BANK_SIZE;
    return machine_.debug_instruction_address();
}

bool CodeListing::is_current(const CodeEntry &entry) const {
    return entry.source == execution_source() && entry.bank == machine_.debug_instruction_bank() &&
           entry.instruction.address == machine_.debug_instruction_address();
}
