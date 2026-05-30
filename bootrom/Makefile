RGBASM  ?= rgbasm
RGBLINK ?= rgblink
PYTHON  ?= python3

BOOTROM_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
OUT_DIR     ?= $(BOOTROM_DIR)build
EMBED_OUT   ?= $(BOOTROM_DIR)../includes/EmbeddedBootroms.h

ROMS := $(OUT_DIR)/dmg.bin

.PHONY: all clean
all: $(EMBED_OUT)

$(OUT_DIR):
	@mkdir -p $(OUT_DIR)

$(OUT_DIR)/%.o: $(BOOTROM_DIR)%.asm | $(OUT_DIR)
	$(RGBASM) -o $@ $<

$(OUT_DIR)/%.bin: $(OUT_DIR)/%.o
	$(RGBLINK) -x -o $@ $<

$(EMBED_OUT): $(ROMS) $(BOOTROM_DIR)embed.py
	$(PYTHON) $(BOOTROM_DIR)embed.py $@ $(ROMS)

clean:
	rm -rf $(OUT_DIR)
	rm -f $(EMBED_OUT)
