#include <string.h>
#define GB_INTERNAL
#include <Core/gb.h>

uint64_t StarGBC_SameBoyRunTicks(const GB_gameboy_t *gb) {
    return gb->cycles_since_run;
}

bool StarGBC_SameBoyBootromActive(const GB_gameboy_t *gb) {
    return !gb->boot_rom_finished;
}

void StarGBC_SameBoyInitializeRtc(GB_gameboy_t *gb) {
    GB_set_rtc_mode(gb, GB_RTC_MODE_ACCURATE);
    memset(&gb->rtc_real, 0, sizeof(gb->rtc_real));
    memset(&gb->rtc_latched, 0, sizeof(gb->rtc_latched));
    gb->rtc_cycles = 0;
    gb->last_rtc_second = 0;
}
