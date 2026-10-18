#include "Memory.h"

void Memory::initialize_power_on(const Model model) {
    if (model < Model::DMG0 || model > Model::DMGC)
        return;
    for (auto offset{0u}; offset < 0x2000; ++offset)
        wram_[offset] = ((offset >> 8) ^ (offset >> 11)) & 1 ? 0xFF : 0;
}
