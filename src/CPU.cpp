#include "CPU.h"

void CPU::Save() const {
    bus->cartridge_->Save();
}

void CPU::KeyDown(const Keys key) const {
    bus->KeyDown(key);
}

void CPU::KeyUp(const Keys key) const {
    bus->KeyUp(key);
}

uint32_t *CPU::GetScreenData() const {
    return bus->gpu_->screenData.data();
}

void CPU::InitializeBootrom(const std::string &bios_path) const {
    std::ifstream file(bios_path, std::ios::binary);
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

bool CPU::IsDMG() const {
    return bus->gpu_->hardware == GPU::Hardware::DMG;
}

uint8_t CPU::DecodeInstruction(const uint8_t opcode, const bool isPrefixed) {
    return isPrefixed
               ? instructions->prefixedInstr(opcode, *this)
               : instructions->nonPrefixedInstr(opcode, *this);
}
