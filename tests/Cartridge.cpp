#include <doctest/doctest.h>
#include "Cartridge.h"

TEST_CASE (
"cartridge: ROM-only unmapped reads do not access host memory"
)
 {
    RealTimeClock rtc(false);
    Cartridge cartridge("roms/gambatte/dma/dma_hiram_read_result_cgb04c_out1.gbc", rtc);
    CHECK(cartridge.ReadByte(0x100) == cartridge.DebugPeek(0x100));
    CHECK(cartridge.ReadByte(0x7FFF) == cartridge.DebugPeek(0x7FFF));
    for (const uint16_t address : {0x8000, 0x9FFF, 0xA000, 0xBFFF, 0xFFFF})
        CHECK(cartridge.ReadByte(address) == 0xFF);
}
