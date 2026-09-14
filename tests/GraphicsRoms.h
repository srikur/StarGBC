#pragma once
#include "TestRoms.h"

// Reference images are bundled upstream screenshots; see expected/README.md.

TEST_CASE("rom: mealybug CGB-C m3_lcdc_bg_en_change2") {
    CHECK(runRomTest("roms/mealybug-tearoom-tests/ppu/m3_lcdc_bg_en_change2.gb",
        "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_bg_en_change2.cgb-c.screen", bootroms.cgbBootrom, Model::CGBC, false));
}

TEST_CASE("rom: mealybug CGB-C m3_lcdc_bg_map_change2") {
    CHECK(runRomTest("roms/mealybug-tearoom-tests/ppu/m3_lcdc_bg_map_change2.gb",
        "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_bg_map_change2.cgb-c.screen", bootroms.cgbBootrom, Model::CGBC, false));
}

TEST_CASE("rom: mealybug CGB-C m3_lcdc_tile_sel_change2") {
    CHECK(runRomTest("roms/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_change2.gb",
        "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_change2.cgb-c.screen", bootroms.cgbBootrom, Model::CGBC, false));
}

TEST_CASE("rom: mealybug CGB-C m3_lcdc_tile_sel_win_change2") {
    CHECK(runRomTest("roms/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_win_change2.gb",
        "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_win_change2.cgb-c.screen", bootroms.cgbBootrom, Model::CGBC, false));
}

TEST_CASE("rom: mealybug CGB-C m3_lcdc_win_map_change2") {
    CHECK(runRomTest("roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_map_change2.gb",
        "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_win_map_change2.cgb-c.screen", bootroms.cgbBootrom, Model::CGBC, false));
}

TEST_CASE("rom: mealybug CGB-C m3_scx_high_5_bits_change2") {
    CHECK(runRomTest("roms/mealybug-tearoom-tests/ppu/m3_scx_high_5_bits_change2.gb",
        "tests/expected/mealybug-tearoom-tests/ppu/m3_scx_high_5_bits_change2.cgb-c.screen", bootroms.cgbBootrom, Model::CGBC, false));
}

TEST_CASE("rom: mealybug CGB-C m3_scy_change2") {
    CHECK(runRomTest("roms/mealybug-tearoom-tests/ppu/m3_scy_change2.gb",
        "tests/expected/mealybug-tearoom-tests/ppu/m3_scy_change2.cgb-c.screen", bootroms.cgbBootrom, Model::CGBC, false));
}

TEST_CASE("rom: turtle-tests window_y_trigger") {
    CHECK(runRomTest("roms/turtle-tests/window_y_trigger/window_y_trigger.gb",
        "tests/expected/turtle-tests/window_y_trigger.screen", bootroms.dmgBootrom, Model::DMGB, false));
}

TEST_CASE("rom: mealybug win_without_bg") {
    CHECK(runRomTest("roms/mealybug-tearoom-tests/ppu/win_without_bg.gb",
        "tests/expected/mealybug-tearoom-tests/ppu/win_without_bg.screen", bootroms.dmgBootrom, Model::DMGB, false));
}

TEST_CASE("rom: turtle-tests window_y_trigger_wx_offscreen") {
    CHECK(runRomTest("roms/turtle-tests/window_y_trigger_wx_offscreen/window_y_trigger_wx_offscreen.gb",
        "tests/expected/turtle-tests/window_y_trigger_wx_offscreen.screen", bootroms.dmgBootrom, Model::DMGB, false));
}
