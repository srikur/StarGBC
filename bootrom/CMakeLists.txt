# Bootrom: assembles dmg.asm (and later cgb.asm) and embeds the binaries
# into includes/EmbeddedBootroms.h. If RGBDS or python3 isn't available the
# step is skipped and the checked-in header is used as-is.

set(BOOTROM_HEADER ${CMAKE_SOURCE_DIR}/includes/EmbeddedBootroms.h)

find_program(RGBASM_EXECUTABLE  rgbasm)
find_program(RGBLINK_EXECUTABLE rgblink)
find_program(PYTHON3_EXECUTABLE python3)

if(RGBASM_EXECUTABLE AND RGBLINK_EXECUTABLE AND PYTHON3_EXECUTABLE)
    set(BOOTROM_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
    set(DMG_BIN  ${BOOTROM_BUILD_DIR}/dmg.bin)
    set(DMG_OBJ  ${BOOTROM_BUILD_DIR}/dmg.o)
    set(DMG_ASM  ${CMAKE_CURRENT_SOURCE_DIR}/dmg.asm)
    set(CGB_BIN  ${BOOTROM_BUILD_DIR}/cgb.bin)
    set(CGB_OBJ  ${BOOTROM_BUILD_DIR}/cgb.o)
    set(CGB_ASM  ${CMAKE_CURRENT_SOURCE_DIR}/cgb.asm)
    set(EMBED_PY ${CMAKE_CURRENT_SOURCE_DIR}/embed.py)

    add_custom_command(
        OUTPUT  ${DMG_BIN}
        COMMAND ${RGBASM_EXECUTABLE}  -o ${DMG_OBJ} ${DMG_ASM}
        COMMAND ${RGBLINK_EXECUTABLE} -x -o ${DMG_BIN} ${DMG_OBJ}
        DEPENDS ${DMG_ASM}
        COMMENT "Assembling StarGBC DMG bootrom"
        VERBATIM
    )

    add_custom_command(
        OUTPUT  ${CGB_BIN}
        COMMAND ${RGBASM_EXECUTABLE}  -o ${CGB_OBJ} ${CGB_ASM}
        COMMAND ${RGBLINK_EXECUTABLE} -x -o ${CGB_BIN} ${CGB_OBJ}
        DEPENDS ${CGB_ASM}
        COMMENT "Assembling StarGBC CGB bootrom"
        VERBATIM
    )

    add_custom_command(
        OUTPUT  ${BOOTROM_HEADER}
        COMMAND ${PYTHON3_EXECUTABLE} ${EMBED_PY} ${BOOTROM_HEADER} ${DMG_BIN} ${CGB_BIN}
        DEPENDS ${DMG_BIN} ${CGB_BIN} ${EMBED_PY}
        COMMENT "Embedding bootrom binaries into EmbeddedBootroms.h"
        VERBATIM
    )

    add_custom_target(stargbc_bootroms ALL DEPENDS ${BOOTROM_HEADER})
else()
    message(STATUS "RGBDS or python3 not found — skipping bootrom regeneration; checked-in EmbeddedBootroms.h will be used.")
endif()
