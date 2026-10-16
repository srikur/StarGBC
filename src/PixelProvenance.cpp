#include "GPU.h"

void GPU::RecordSpriteCandidate(const unsigned slot, const uint8_t bit, const uint8_t color, const diagnostics::SpriteDecision decision) {
    auto origin = provenance->spriteFetch;
    origin.bit = bit;
    origin.color = color;
    origin.lowData = fetcherTileDataLow_;
    origin.highData = fetcherTileDataHigh_;
    provenance->sprites[(spriteArrayHead_ + slot) & 7].Merge(origin, decision);
}

diagnostics::PixelDecision GPU::ExplainSelection(const Pixel &background, const Pixel &sprite, const bool bgEnable) const {
    using enum diagnostics::PixelDecision;
    if (!bgEnable && (IsDmg(model) || dmg_compat) && (sprite.color == 0 || !Bit<LCDC_OBJ_ENABLE>(lcdc)))
        return BackgroundDisabled;
    if (sprite.color == 0)
        return SpriteTransparent;
    if (!Bit<LCDC_OBJ_ENABLE>(lcdc))
        return ObjectsDisabled;
    if (bgEnable && background.color != 0) {
        if (background.priority)
            return BackgroundPriority;
        if (sprite.priority)
            return SpriteBehindBackground;
    }
    return SpriteForeground;
}

void GPU::RecordPixel(const Pixel &background, const Pixel &sprite, const Pixel &selected, const bool bgEnable, const uint32_t color) {
    diagnostics::PixelExplanation explanation{.emission = provenance->position,
                                              .branch = provenance->branch,
                                              .x = pixelsDrawn,
                                              .y = currentLine,
                                              .background = provenance->PopBackground(),
                                              .sprites = provenance->PopSprites((spriteArrayHead_ + 7) & 7),
                                              .decision = ExplainSelection(background, sprite, bgEnable),
                                              .emittedColor = color,
                                              .finalColor = color,
                                              .finalColorTick = provenance->position.tick,
                                              .lcdc = lcdc,
                                              .backgroundEnabled = bgEnable,
                                              .objectsEnabled = Bit<LCDC_OBJ_ENABLE>(lcdc),
                                              .backgroundPalette = backgroundPalette,
                                              .objectPalette0 = obp0Palette,
                                              .objectPalette1 = obp1Palette};
    explanation.emission.ly = currentLine;
    explanation.emission.dot = scanlineCounter;
    explanation.emission.frame = displayFrames_;
    if (selected.is_sprite) {
        explanation.selected = explanation.sprites.selected;
    } else if (!bgEnable && (IsDmg(model) || dmg_compat)) {
        explanation.selected = {.valid = true, .source = diagnostics::PixelSource::Backdrop};
    } else {
        explanation.selected = explanation.background;
    }
    explanation.emittedSource = explanation.selected.source;
    explanation.emittedDecision = explanation.decision;
    explanation.selected.color = selected.color;
    explanation.selected.palette = is_cgb && !dmg_compat ? selected.cgb_palette : selected.dmg_palette;
    if (provenance->output)
        provenance->output(provenance->owner, explanation);
}

void GPU::RecordRecolor(const uint16_t offset, const uint16_t address, const Pixel &selected, const diagnostics::PixelDecision decision) {
    if (!provenance->recolor)
        return;
    const auto source = selected.is_sprite                                            ? diagnostics::PixelSource::Sprite
                        : decision == diagnostics::PixelDecision::BackgroundDisabled ? diagnostics::PixelSource::Backdrop
                                                                                     : diagnostics::PixelSource::Background;
    auto position = provenance->position;
    position.ly = currentLine;
    position.dot = scanlineCounter;
    position.frame = displayFrames_;
    provenance->recolor(provenance->owner, {.position = position,
                                            .branch = provenance->branch,
                                            .offset = offset,
                                            .registerAddress = address,
                                            .registerValue = address == 0xFF40 ? lcdc : backgroundPalette,
                                            .selectionChanged = address == 0xFF40,
                                            .color = screenData[offset],
                                            .selectedSource = source,
                                            .decision = decision,
                                            .rawColor = selected.color,
                                            .palette = is_cgb && !dmg_compat ? selected.cgb_palette : selected.dmg_palette});
}
