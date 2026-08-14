#include <list>

#include "GPU.h"
#include "Common.h"

static constexpr uint8_t expand5(const uint8_t c) noexcept {
    return static_cast<uint8_t>(c << 3 | c >> 2);
}

bool GPU::LCDDisabled() const {
    return !Bit<LCDC_ENABLE_BIT>(lcdc);
}

bool GPU::StatLineHigh() const {
    // The DMG early OAM assert counts as the next line's mode 2 condition
    // already being high
    return (stat.enableLYInterrupt && stat.coincidenceFlag) ||
           (stat.enableM0Interrupt && stat.mode == GPUMode::MODE_0) ||
           (stat.enableM1Interrupt && stat.mode == GPUMode::MODE_1) ||
           (stat.enableM2Interrupt && (stat.mode == GPUMode::MODE_2 || m2IrqRaisedEarly));
}

void GPU::ResetScanlineState(const bool clearBuffer) {
    backgroundQueue.clear();
    spriteArray.fill({.isPlaceholder = true});
    if (clearBuffer) spriteBuffer.clear();
    fetcherTileX_ = 0;
    spriteFetchActive_ = false;
    isFetchingWindow_ = false;
    fetcherState_ = FetcherState::GetTile;
    firstScanlineDataHigh = true;
    pixelsDrawn = 0;
    fetcherDelay_ = 0;
    spriteFetchQueue.clear();
    spriteFetchWait_ = 0;
    spriteMergeDelay_ = 0;
    spriteFetchAbort_ = false;
    spriteFetchedThisLine_ = false;
    windowMatchLatch_ = false;
    windowWasActiveThisLine_ = false;
    windowEndPending_ = false;
    windowEndStage_ = 0;
    windowActivatePending_ = false;
    windowPixel0Triggered_ = false;
}

uint8_t GPU::GetOAMScanRow() const {
    return scanlineCounter / 4;
}

void GPU::Update() {
    if (interrupts_.interruptSetDelay > 0) {
        interrupts_.interruptSetDelay--;
        if (interrupts_.interruptSetDelay == 0) {
            interrupts_.interruptFlag = interrupts_.interruptFlagDelayed;
            interrupts_.interruptFlagDelayed = 0;
        }
    }

    // DMG BGP write glitch: a mode-3 write reaches the pixel pipe late — one dot where the palette reads as old|new, then the new value
    if (bgpWriteStage > 0) {
        bgpWriteStage--;
        if (bgpWriteStage == 1) {
            backgroundPalette |= bgpPending;
        } else if (bgpWriteStage == 0) {
            backgroundPalette = bgpPending;
        }
    }

    // DMG: a mode-3 LCDC write reaches the fetcher two dots late, same latency class as BGP
    if (lcdcWriteStage > 0) {
        lcdcWriteStage--;
        if (lcdcWriteStage == 0) ApplyLCDC(lcdcPending);
    }

    // DMG: a mode-3 WX write reaches the window comparator with the same
    // latency, so a trigger racing the write still sees the old value
    if (wxWriteStage > 0) {
        wxWriteStage--;
        if (wxWriteStage == 0) windowX = wxPending;
    }

    // DMG: mode-3 SCX/SCY writes reach the fetcher's reads two dots late,
    // same latency class as BGP. The warmup fetch's reads run just ahead of
    // the apply point, so a value landing on the read dot is still seen old
    if (scxWriteStage > 0) scxWriteStage--;
    scyJustApplied_ = false;
    if (scyWriteStage > 0) {
        scyWriteStage--;
        if (scyWriteStage == 0) scyJustApplied_ = true;
    }

    if (windowEndStage_ > 0) {
        windowEndStage_--;
        if (windowEndStage_ == 0) windowEndPending_ = true;
    }

    // DMG: OBP0/OBP1 mode-3 writes share BGP's latency, glitch dot included
    if (obp0WriteStage > 0) {
        obp0WriteStage--;
        if (obp0WriteStage == 1) {
            obp0Palette |= obp0Pending;
        } else if (obp0WriteStage == 0) {
            obp0Palette = obp0Pending;
        }
    }
    if (obp1WriteStage > 0) {
        obp1WriteStage--;
        if (obp1WriteStage == 1) {
            obp1Palette |= obp1Pending;
        } else if (obp1WriteStage == 0) {
            obp1Palette = obp1Pending;
        }
    }

    if (LCDDisabled()) {
        return;
    }

    if (currentLine == lyc) {
        stat.coincidenceFlag = true;
        if (stat.enableLYInterrupt && !statTriggered) {
            interrupts_.Set(InterruptType::LCDStat, true);
            statTriggered = true;
        }
    } else {
        stat.coincidenceFlag = false;
    }

    switch (stat.mode) {
        case GPUMode::MODE_0:
            if (stat.enableM0Interrupt && !statTriggered) {
                interrupts_.Set(InterruptType::LCDStat, true);
                statTriggered = true;
            }
            break;
        case GPUMode::MODE_1:
            // The mode 1 STAT condition asserts as soon as vblank starts; the
            // halt-wake dispatch penalty accounts for the extra cycle mooneye's
            // intr_1_2_timing observes, so no delayed set here
            if (stat.enableM1Interrupt && !statTriggered) {
                interrupts_.Set(InterruptType::LCDStat, false);
                statTriggered = true;
            }
            break;
        case GPUMode::MODE_2:
            if (stat.enableM2Interrupt && !statTriggered) {
                interrupts_.Set(InterruptType::LCDStat, hardware == Hardware::CGB);
                statTriggered = true;
            }
            TickOAMScan();
            break;
        case GPUMode::MODE_3: {
            TickMode3();
            if (pixelsDrawn == SCREEN_WIDTH) {
                stat.mode = GPUMode::MODE_0;
                hblank = true;
                hdma.bytesThisBlock = 0;
                hdma.hblankBlockFinished = false;
                if (stat.enableM0Interrupt && !statTriggered) {
                    interrupts_.Set(InterruptType::LCDStat, true);
                    statTriggered = true;
                }
                break;
            }
        }
        break;
        default: break;
    }
    // The STAT IRQ line is the OR of every enabled condition; latch its level
    // here, after this dot's mode/coincidence updates but before the boundary
    // transitions below, so next dot's raise sites fire only on a rising edge.
    // A condition that stays true across a boundary (hblank into a new line,
    // LYC held while modes cycle) never re-fires
    statTriggered = StatLineHigh();

    scanlineCounter++;

    const uint16_t scanlineDuration = 456 - (shortenScanline ? 4 : 0);
    // DMG: the mode 2 (OAM) STAT interrupt for lines 1-143 asserts ~4 dots before
    // the line starts (line 0's asserts at line start instead, handled in the mode 2
    // case above). Blocked only if the STAT line is currently high from another
    // enabled condition — the current line's mode 2 condition deasserted back at
    // mode 3 entry
    if (hardware != Hardware::CGB && scanlineCounter == scanlineDuration - 3 &&
        currentLine < 143 && stat.mode == GPUMode::MODE_0 && stat.enableM2Interrupt &&
        !statTriggered) {
        interrupts_.Set(InterruptType::LCDStat, false);
        m2IrqRaisedEarly = true;
    }
    if (lcdEnableLine0_ && scanlineCounter == 82 && stat.mode == GPUMode::MODE_0) {
        stat.mode = GPUMode::MODE_3;
        pixelsDrawn = 0;
        ResetScanlineState(false);
    } else if (scanlineCounter == 80 && stat.mode == GPUMode::MODE_2) {
        stat.mode = GPUMode::MODE_3;
        pixelsDrawn = 0;
        if (hardware != Hardware::CGB || objectPriority) {
            std::ranges::sort(spriteBuffer, std::less{});
        } else if (hardware == Hardware::CGB && !objectPriority) {
            std::ranges::sort(spriteBuffer, [](const Sprite &a, const Sprite &b) {
                return a.spriteNum < b.spriteNum;
            });
        }
        ResetScanlineState(false);
    } else if (scanlineCounter == scanlineDuration) {
        shortenScanline = false;
        lcdEnableLine0_ = false;
        scanlineCounter = 0;
        currentLine++;
        m2IrqRaisedEarly = false;

        if (isFetchingWindow_ || windowWasActiveThisLine_) {
            windowLineCounter_++;
        }

        if (currentLine == 154) {
            vblank = false;
            hblank = false;
            currentLine = 0;
            stat.mode = GPUMode::MODE_2;
            windowLineCounter_ = 0;
            ResetScanlineState(true);
            windowTriggeredThisFrame = false;
            if (currentLine >= windowY) {
                windowTriggeredThisFrame = true;
            }
            initialSCXSet = false;
        } else if (currentLine == 144) {
            stat.mode = GPUMode::MODE_1;
            vblank = true;
            hblank = false;
            interrupts_.Set(InterruptType::VBlank, true);
            // Hardware quirk: entering vblank also asserts the mode 2 (OAM) STAT
            // condition — on DMG together with the vblank IF, on CGB one M-cycle
            // ahead of it (mooneye vblank_stat_intr-GS / -C)
            if (stat.enableM2Interrupt && !statTriggered) {
                interrupts_.Set(InterruptType::LCDStat, hardware != Hardware::CGB);
                statTriggered = true;
            }
            // The mode 1 STAT condition asserts on the line boundary itself, one
            // M-cycle ahead of the vblank IF (mooneye intr_1_2_timing)
            if (stat.enableM1Interrupt && !statTriggered) {
                interrupts_.Set(InterruptType::LCDStat, false);
                statTriggered = true;
            }
        } else if (currentLine < 144) {
            hblank = false;
            stat.mode = GPUMode::MODE_2;
            if (currentLine >= windowY) {
                windowTriggeredThisFrame = true;
            }
            ResetScanlineState(true);
            initialSCXSet = false;
        }
    }
}

void GPU::TickOAMScan() {
    if (!Bit<LCDC_OBJ_ENABLE>(lcdc)) return;
    if (!(scanlineCounter % 2)) return;
    const uint8_t index = scanlineCounter / 2;

    const uint16_t base = index * 4;
    const auto spriteY = static_cast<int16_t>(static_cast<int16_t>(oam[base]) - 16);
    const auto spriteX = static_cast<int16_t>(static_cast<int16_t>(oam[base + 1]) - 8);
    const uint8_t spriteTileIndex = oam[base + 2];
    const Attributes attr = GetAttrsFrom(oam[base + 3]);
    const uint8_t spriteSize = Bit<LCDC_OBJ_SIZE>(lcdc) ? 16 : 8;

    // OAM scan checks only Y — off-screen X (0 or ≥168) still occupies a buffer
    // slot, and an X=0 sprite stalls mode 3 without drawing anything
    const bool cond2 = currentLine >= spriteY;
    const bool cond3 = currentLine < spriteY + spriteSize;
    const bool cond4 = spriteBuffer.size() < 10;
    if (cond2 && cond3 && cond4) {
        spriteBuffer.push_back(Sprite{
            .spriteNum = static_cast<uint8_t>(scanlineCounter), .x = spriteX, .y = spriteY, .tileIndex = spriteTileIndex, .attributes = attr, .processed = false
        });
    }
}

void GPU::OutputPixel(const bool lcdcAhead) {
    if (backgroundQueue.empty()) return;

    if (initialScrollXDiscard_ > 0) {
        backgroundQueue.pop_front();
        initialScrollXDiscard_--;
        return;
    }

    const auto bgPixel = backgroundQueue.front();
    backgroundQueue.pop_front();

    const auto spritePixel = spriteArray[0];
    for (int i = 0; i < spriteArray.size() - 1; i++) {
        spriteArray[i] = spriteArray[i + 1];
    }
    spriteArray[spriteArray.size() - 1] = {.isSprite = true, .isPlaceholder = true};

    // OBJ enable gates sprite pixels at mix time: pixels already in the FIFO
    // keep shifting while disabled, but display as background. A pixel shipped
    // on a sprite-merge dot samples LCDC one dot ahead of the applied value
    const bool bgWinEnable = lcdcAhead && lcdcWriteStage == 1
                                 ? Bit<LCDC_BG_WINDOW_ENABLE>(lcdcPending)
                                 : Bit<LCDC_BG_WINDOW_ENABLE>(lcdc);
    bool backgroundWins = spritePixel.color == 0 || !Bit<LCDC_OBJ_ENABLE>(lcdc);
    if (!backgroundWins) {
        if (hardware == Hardware::CGB && !bgWinEnable) {
            backgroundWins = false;
        } else if (bgPixel.priority) {
            backgroundWins = bgPixel.color != 0;
        } else {
            backgroundWins = spritePixel.priority && bgPixel.color != 0;
        }
    }

    Pixel finalPixel = spritePixel;
    if (backgroundWins) {
        if (bgWinEnable || hardware == Hardware::CGB) {
            finalPixel = bgPixel;
        } else {
            finalPixel = Pixel{.color = 0};
        }
    }

    const uint8_t palette = hardware == Hardware::CGB ? finalPixel.cgbPalette : finalPixel.dmgPalette;
    const uint32_t finalColor = finalPixel.isSprite
                                    ? GetSpriteColor(finalPixel.color, palette)
                                    : GetBackgroundColor(finalPixel.color, palette);

    screenData[currentLine * SCREEN_WIDTH + pixelsDrawn] = finalColor;
    pixelsDrawn++;
}

void GPU::TickMode3() {
    // The WX <= 7 warmup trigger fires only on its exact dot, latched here so
    // it survives a sprite fetch occupying the fetcher at that moment
    if (pixelsDrawn == 0 && windowX <= 7 &&
        scanlineCounter == 85u + windowX + (windowX == 0 && (scrollX & 7) != 0 ? 1 : 0)) {
        windowPixel0Triggered_ = true;
    }
    // The window comparator is evaluated before the sprite one: an activation
    // resets the fetcher, deferring a pixel-0 sprite to the window's own push.
    // Sprites near the left edge (OAM X <= 2) claim the fetcher first instead —
    // their fetch is already in flight when the window match lands — so the
    // window activation waits until the sprite fetch completes
    if (!spriteFetchActive_ && spriteFetchQueue.empty()) {
        bool spriteClaimsFetcher = false;
        if (pixelsDrawn == 0 && !isFetchingWindow_) {
            for (const auto &sprite: spriteBuffer) {
                if (!sprite.processed && sprite.x <= -6) spriteClaimsFetcher = true;
            }
        }
        if (!spriteClaimsFetcher) CheckForWindowTrigger();
    }
    CheckForSpriteTrigger();
    const bool spritePending = !spriteFetchActive_ && !spriteFetchQueue.empty();
    if (spritePending) {
        if (spriteFetchWait_ > 0) {
            // Pixel output pauses until the sprite fetch may take over. Sprites
            // clipped by the left edge halt the background fetch for the whole wait
            // (only a pending push completes); on-screen sprites let it keep stepping
            spriteFetchWait_--;
            if (spriteFetchQueue.front().x >= 0 ||
                (fetcherState_ == FetcherState::PushToFIFO && fetcherDelay_ == 0 && backgroundQueue.empty())) {
                Fetcher_StepBackgroundFetch();
            }
            return;
        }
        // Clearing OBJ enable cancels a sprite fetch that has not yet taken over
        // the fetcher: the sprite is dropped outright and background output
        // resumes this very dot. The check sees the raw written LCDC value one
        // dot before it reaches the fetcher and mixer
        if (!Bit<LCDC_OBJ_ENABLE>(lcdcWriteStage > 0 ? lcdcPending : lcdc)) {
            spriteFetchQueue.pop_front();
        } else {
            // A background push that is ready on the takeover tick runs first, so the
            // FIFO has pixels for the dot where the sprite fetch completes — unless a
            // window trigger is pending at pixel 0, whose restart discards that push
            // so no background pixel ships before the window claims the line
            const bool windowPendingAtStart = pixelsDrawn == 0 && !isFetchingWindow_ &&
                                              Bit<LCDC_WINDOW_ENABLE>(lcdc) && windowTriggeredThisFrame &&
                                              windowX <= 7;
            if (fetcherState_ == FetcherState::PushToFIFO && fetcherDelay_ == 0 && backgroundQueue.empty() &&
                !windowPendingAtStart) {
                Fetcher_StepBackgroundFetch();
            }
            // The takeover lands exactly when the background fetcher reaches
            // its high-byte read; that read still executes on this dot rather
            // than being deferred until after the sprite fetch
            if (fetcherState_ == FetcherState::GetTileDataHigh && fetcherDelay_ == 0) {
                Fetcher_StepBackgroundFetch();
            }
            savedBgFetcherState_ = fetcherState_;
            savedBgFetcherDelay_ = fetcherDelay_;
            savedBgTileNum_ = fetcherTileNum_;
            savedBgTileDataLow_ = fetcherTileDataLow_;
            savedBgTileDataHigh_ = fetcherTileDataHigh_;
            savedBgLastAddress_ = lastAddress_;
            spriteFetchActive_ = true;
            fetcherState_ = FetcherState::GetTile;
            fetcherDelay_ = 0;
        }
    }
    if (spriteFetchActive_) {
        Fetcher_StepSpriteFetch();
        if (!spriteFetchActive_) {
            // Sprite fetch finished — resume the interrupted background fetch in place
            fetcherState_ = savedBgFetcherState_;
            fetcherDelay_ = savedBgFetcherDelay_;
            fetcherTileNum_ = savedBgTileNum_;
            fetcherTileDataLow_ = savedBgTileDataLow_;
            fetcherTileDataHigh_ = savedBgTileDataHigh_;
            lastAddress_ = savedBgLastAddress_;
            // A tile-number read pending on the resumed fetch — including one
            // belonging to a window activation deferred by this sprite — can
            // execute on the merge dot itself when the fetcher's 2-dot
            // cadence allows it
            if ((scanlineCounter & 1) == 0) {
                if (pixelsDrawn == 0 && !isFetchingWindow_ &&
                    Bit<LCDC_WINDOW_ENABLE>(lcdc) && windowTriggeredThisFrame && windowX <= 7 &&
                    backgroundQueue.empty()) {
                    CheckForWindowTrigger();
                }
                if (fetcherState_ == FetcherState::GetTile && fetcherDelay_ == 0) {
                    Fetcher_StepBackgroundFetch();
                }
            }
            OutputPixel(true);
        }
    } else {
        Fetcher_StepBackgroundFetch();
        OutputPixel();
    }
}

void GPU::CheckForSpriteTrigger() {
    if (!Bit<LCDC_OBJ_ENABLE>(lcdc) || spriteFetchActive_ || !spriteFetchQueue.empty()) return;
    // Pixel-0 triggers for on-screen sprites hold until the first background
    // push is ready; earlier, the fetcher warmup would absorb the stall the
    // sprite fetch is supposed to cause. Left-clipped sprites instead take
    // over the moment pixel output would begin (dot 92), even if a window
    // restart has claimed the fetcher
    const bool pushReady = fetcherState_ == FetcherState::PushToFIFO && fetcherDelay_ == 0;
    for (auto &sprite: spriteBuffer) {
        if (sprite.processed) continue;
        if (sprite.x < 0) {
            if (pixelsDrawn != 0 || scanlineCounter < 92) continue;
        } else if (pixelsDrawn != sprite.x || (sprite.x == 0 && !pushReady)) {
            continue;
        }
        sprite.processed = true;
        spriteFetchQueue.push_back(sprite);
    }
    if (!spriteFetchQueue.empty()) {
        // The fetch begins once the background fetcher reaches its high-byte read:
        // max(0, 5 - phase) dots, where its phase within the tile is (x + SCX) mod 8
        const int phase = (spriteFetchQueue.front().x + scrollX) & 7;
        spriteFetchWait_ = static_cast<uint8_t>(phase < 5 ? 5 - phase : 0);
        // Left-edge-clipped sprites take over immediately — their VRAM reads run
        // ahead of the alignment wait — but the merge into the FIFO still waits
        // out the alignment dots, so the total stall and the abort window are
        // unchanged
        if (spriteFetchQueue.front().x < 0) {
            spriteMergeDelay_ = spriteFetchWait_;
            spriteFetchWait_ = 0;
        }
    }
}

void GPU::CheckForWindowTrigger() {
    const bool matched = windowMatchLatch_;
    windowMatchLatch_ = pixelsDrawn + 7 == windowX;
    // The comparator is equality-based: a match missed while WIN_EN was off
    // never re-fires. The left-edge case (WX <= 7) matches only on its exact
    // warmup dot — latched in TickMode3 so a sprite claim can defer the
    // restart — and a WX rewrite landing on that dot pre-empts it
    const bool windowMatch = (pixelsDrawn == 0 && windowPixel0Triggered_) ||
                             (pixelsDrawn > 0 && pixelsDrawn + 7 == windowX);
    // The comparator sees a WIN_EN change one dot after the write, in both
    // directions. An activation whose enable is just arriving via the pending
    // value takes effect one dot later still
    const bool winEnabled = lcdcWriteStage == 1 ? Bit<LCDC_WINDOW_ENABLE>(lcdcPending)
                                                : Bit<LCDC_WINDOW_ENABLE>(lcdc);
    if (windowActivatePending_ || (winEnabled && !isFetchingWindow_ && windowTriggeredThisFrame && windowMatch)) {
        if (!windowActivatePending_ && lcdcWriteStage == 1 && !Bit<LCDC_WINDOW_ENABLE>(lcdc)) {
            windowActivatePending_ = true;
            return;
        }
        windowActivatePending_ = false;
        // A window with WX <= 7 triggers mid-warmup (dot 85 + WX); its first
        // 7 - WX pixels pop into the offscreen dots, left-clipping the first
        // window tile. The SCX fine-scroll discard belongs to the background,
        // so an activation cancels any unconsumed discards rather than letting
        // them eat window pixels — except at WX = 0, whose delayed trigger
        // lets the SCX stall play out, stacking under the window's clip
        if (windowWasActiveThisLine_) windowLineCounter_++;
        windowWasActiveThisLine_ = true;
        isFetchingWindow_ = true;
        backgroundQueue.clear();
        fetcherState_ = FetcherState::GetTile;
        fetcherDelay_ = 0;
        fetcherTileX_ = 0;
        if (pixelsDrawn == 0) {
            if (windowX != 0) initialScrollXDiscard_ = 0;
            initialScrollXDiscard_ += 7 - windowX;
        }
    } else if (windowTriggeredThisFrame && pixelsDrawn != 0 &&
               windowMatchLatch_ && !matched && initialScrollXDiscard_ == 0 &&
               fetcherState_ == FetcherState::PushToFIFO && fetcherDelay_ == 0 && backgroundQueue.empty()) {
        // A WX re-match that does not activate the window — because it is
        // already active, or WIN_EN is currently off — emits a single color-0
        // glitch pixel when the match dot coincides with the fetcher's
        // tile-number read (the dot our model refills the FIFO), and the rest
        // of the line shifts right by one dot. Priority sprites show through
        // it since it mixes as background color 0
        backgroundQueue.push_front(Pixel{
            .color = 0,
            .dmgPalette = backgroundPalette ? obp1Palette : obp0Palette,
            .cgbPalette = 0,
            .priority = false,
            .isSprite = false,
            .isPlaceholder = false,
        });
    }
}

void GPU::Fetcher_StepBackgroundFetch() {
    if (fetcherDelay_ > 0) {
        fetcherDelay_--;
        return;
    }

    using enum FetcherState;
    switch (fetcherState_) {
        case GetTile: {
            if (!initialSCXSet) {
                initialScrollXDiscard_ = isFetchingWindow_ ? 0 : scrollX & 0x07;
                initialSCXSet = true;
            }
            const auto tileMapAddress = CalculateBGTileMapAddress();
            if (hardware == Hardware::CGB) backgroundTileAttributes_ = GetAttrsFrom(vram[tileMapAddress - 0x6000]);
            fetcherTileNum_ = vram[tileMapAddress - 0x8000];
            fetcherState_ = GetTileDataLow;
            fetcherDelay_ = 1;
            break;
        }
        case GetTileDataLow: {
            const auto tileDataAddress = CalculateTileDataAddress();
            const bool bank1 = (hardware == Hardware::CGB) && backgroundTileAttributes_.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileDataLow_ = vram[tileDataAddress - base];
            fetcherState_ = GetTileDataHigh;
            fetcherDelay_ = 1;
            break;
        }
        case GetTileDataHigh: {
            // TILE_SEL (and the scroll row) are sampled again for the high
            // bitplane read, so a change between the two data reads mixes
            // bitplanes from two different tile patterns
            const auto tileDataAddress = CalculateTileDataAddress();
            const bool bank1 = (hardware == Hardware::CGB) && backgroundTileAttributes_.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileDataHigh_ = vram[(tileDataAddress + 1) - base];
            fetcherState_ = PushToFIFO;
            fetcherDelay_ = 1;
            if (firstScanlineDataHigh) {
                fetcherState_ = GetTile;
                firstScanlineDataHigh = false;
            }
            break;
        }
        case Sleep:
            fetcherState_ = PushToFIFO;
            fetcherDelay_ = 1;
            break;
        case PushToFIFO: {
            if (!backgroundQueue.empty()) break;
            for (int i = 0; i < 8; i++) {
                const uint8_t pixelBit = backgroundTileAttributes_.xflip ? i : 7 - i;
                const uint8_t bitLow = (fetcherTileDataLow_ >> pixelBit) & 1;
                const uint8_t bitHigh = (fetcherTileDataHigh_ >> pixelBit) & 1;
                const uint8_t color = (bitHigh << 1) | bitLow;

                backgroundQueue.push_back(Pixel{
                    .color = color,
                    .dmgPalette = backgroundPalette ? obp1Palette : obp0Palette,
                    .cgbPalette = backgroundTileAttributes_.paletteNumberCGB,
                    .priority = backgroundTileAttributes_.priority,
                    .isSprite = false,
                    .isPlaceholder = false,
                });
            }
            fetcherTileX_++;
            fetcherState_ = GetTile;
            fetcherDelay_ = 0;
            if (isFetchingWindow_ && windowEndPending_) {
                // The tile in flight when WIN_EN was cleared has now been
                // pushed; background fetching resumes from here
                isFetchingWindow_ = false;
                windowEndPending_ = false;
            }
            break;
        }
    }
}

void GPU::Fetcher_StepSpriteFetch() {
    if (fetcherDelay_ > 0) {
        fetcherDelay_--;
        return;
    }

    const Sprite &sprite = spriteFetchQueue.front();

    using enum FetcherState;
    switch (fetcherState_) {
        case GetTile: {
            // The tile number comes from the OAM buffer, not VRAM. On the line's
            // first sprite fetch this step overlaps the tail of the background
            // fetch, so the VRAM reads land at takeover+1 (low) and takeover+3
            // (high); later fetches on the same line read one dot later, at
            // takeover+2 and takeover+4, with the merge dot unchanged. Each read
            // samples the OBJ size bit independently, so an LCDC write between
            // them mixes rows from two different tiles
            fetcherTileNum_ = sprite.tileIndex;
            fetcherState_ = GetTileDataLow;
            spriteFetchIsFirst_ = !spriteFetchedThisLine_;
            spriteFetchedThisLine_ = true;
            fetcherDelay_ = spriteFetchIsFirst_ ? 0 : 1;
            break;
        }
        case GetTileDataLow: {
            const auto tileAddress = CalculateSpriteDataAddress(sprite);
            const bool bank1 = (hardware == Hardware::CGB) && sprite.attributes.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileDataLow_ = vram[tileAddress - base];
            fetcherState_ = GetTileDataHigh;
            fetcherDelay_ = 1;
            break;
        }
        case GetTileDataHigh: {
            const auto tileAddress = CalculateSpriteDataAddress(sprite);
            const bool bank1 = (hardware == Hardware::CGB) && sprite.attributes.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileDataHigh_ = vram[(tileAddress + 1) - base];
            fetcherDelay_ = (spriteFetchIsFirst_ ? 2 : 1) + spriteMergeDelay_;
            spriteMergeDelay_ = 0;
            fetcherState_ = PushToFIFO;
            break;
        }
        case Sleep: {
            fetcherState_ = PushToFIFO;
            fetcherDelay_ = 1;
            break;
        }
        case PushToFIFO: {
            const Attributes attrs = sprite.attributes;
            // Only the palette SELECT is latched into the FIFO — the OBP
            // register itself is read when the pixel is output, so a mode-3
            // palette write lands on pixels still queued
            const uint8_t paletteSelect = attrs.paletteNumberDMG ? 1 : 0;

            const auto xPos = sprite.x;
            const int clip = xPos < 0 ? -xPos : 0; // leftmost tile pixels lost off the screen edge
            for (int i = 0; !spriteFetchAbort_ && i < 8 - clip; i++) {
                const bool hasHigherPriority = hardware == Hardware::CGB && sprite.spriteNum <= spriteArray[0].spriteNum;
                if (!hasHigherPriority && spriteArray[i].color != 0 && !spriteArray[i].isPlaceholder) continue;
                const auto pixelIndex = attrs.xflip ? i + clip : 7 - (i + clip);
                const uint8_t bitLow = (fetcherTileDataLow_ >> pixelIndex) & 1;
                const uint8_t bitHigh = (fetcherTileDataHigh_ >> pixelIndex) & 1;
                const uint8_t color = (bitHigh << 1) | bitLow;

                spriteArray[i] = Pixel{
                    .color = color,
                    .dmgPalette = paletteSelect,
                    .cgbPalette = attrs.paletteNumberCGB,
                    .priority = attrs.priority,
                    .isSprite = true,
                    .isPlaceholder = false,
                    .spriteNum = sprite.spriteNum,
                };
            }
            spriteFetchQueue.pop_front();
            if (spriteFetchQueue.empty()) spriteFetchActive_ = false;
            spriteFetchAbort_ = false;
            fetcherState_ = GetTile;
            fetcherDelay_ = 0;
            break;
        }
    }
}

uint16_t GPU::CalculateBGTileMapAddress() const {
    uint16_t tileMapBase = 0;
    if (isFetchingWindow_) {
        tileMapBase = Bit<LCDC_WINDOW_TILE_MAP_AREA>(lcdc) ? 0x9C00 : 0x9800;
        const uint8_t tileRow = windowLineCounter_ >> 3 & 0x1F;
        const uint8_t tileCol = fetcherTileX_;
        return tileMapBase + tileRow * 32 + tileCol;
    } else {
        tileMapBase = Bit<LCDC_BG_TILE_MAP_AREA>(lcdc) ? 0x9C00 : 0x9800;
        const uint8_t scxForFetch = scxWriteStage > 0 ? scxFetcherOld : scrollX;
        const bool scyOld = scyWriteStage > 0 || (scyJustApplied_ && scanlineCounter < 92);
        const uint8_t scyForFetch = scyOld ? scyFetcherOld : scrollY;
        const uint8_t tileRow = ((scyForFetch + currentLine & 0xFF) >> 3) & 0x1F;
        const uint8_t tileCol = (fetcherTileX_ + (scxForFetch / 8)) & 0x1F;
        return tileMapBase + tileRow * 32 + tileCol;
    }
}

uint16_t GPU::CalculateTileDataAddress() {
    const bool scyOld = scyWriteStage > 0 || (scyJustApplied_ && scanlineCounter < 92);
    const uint8_t scyForFetch = scyOld ? scyFetcherOld : scrollY;
    uint8_t lineInTile = isFetchingWindow_ ? windowLineCounter_ % 8 : ((currentLine + scyForFetch) % 8);
    lineInTile = backgroundTileAttributes_.yflip ? 7 - (lineInTile & 7) : (lineInTile & 7);
    if (Bit<LCDC_BG_AND_WINDOW_TILE_DATA>(lcdc)) {
        const uint16_t address = 0x8000 + fetcherTileNum_ * 16 + lineInTile * 2;
        lastAddress_ = address;
        return address;
    } else {
        const auto signedTileNum = std::bit_cast<int8_t>(fetcherTileNum_);
        const uint16_t address = 0x9000 + signedTileNum * 16 + lineInTile * 2;
        lastAddress_ = address;
        return address;
    }
}

uint16_t GPU::CalculateSpriteDataAddress(const Sprite &sprite) {
    // The fetcher sees LCDC writes raw (one dot before they reach the mixer),
    // and the size bit shapes the address at each read: 8x16 replaces tile bit 0
    // with row bit 3, 8x8 ignores row bit 3 entirely
    const uint8_t effLcdc = lcdcWriteStage > 0 ? lcdcPending : lcdc;
    const uint8_t spriteHeight = Bit<LCDC_OBJ_SIZE>(effLcdc) ? 16 : 8;
    uint8_t tile = sprite.tileIndex;
    if (spriteHeight == 16) tile &= 0xFE;
    const auto row = static_cast<uint8_t>((currentLine - sprite.y) & (spriteHeight - 1));
    const uint8_t tileY = sprite.attributes.yflip ? spriteHeight - 1 - row : row;
    const uint16_t address = 0x8000 + tile * 16 + tileY * 2;
    lastAddress_ = address;
    return address;
}

uint32_t GPU::GetSpriteColor(const uint8_t color, const uint8_t palette) const {
    if (hardware != Hardware::CGB) {
        const uint8_t reg = palette ? obp1Palette : obp0Palette;
        return DMG_SHADE[(reg >> (color * 2)) & 0x03];
    }

    const uint8_t red = obpd[palette][color][0];
    const uint8_t green = obpd[palette][color][1];
    const uint8_t blue = obpd[palette][color][2];

    const auto r5 = static_cast<uint8_t>(red & 0x1F);
    const auto g5 = static_cast<uint8_t>(green & 0x1F);
    const auto b5 = static_cast<uint8_t>(blue & 0x1F);

    const auto corrR5 = static_cast<uint8_t>((26 * r5 + 4 * g5 + 2 * b5) >> 5);
    const auto corrG5 = static_cast<uint8_t>((6 * r5 + 24 * g5 + 2 * b5) >> 5);
    const auto corrB5 = static_cast<uint8_t>((2 * r5 + 4 * g5 + 26 * b5) >> 5);

    const uint8_t r = expand5(corrR5);
    const uint8_t g = expand5(corrG5);
    const uint8_t b = expand5(corrB5);

    const uint32_t rgba = 0xFF000000u |
                          (static_cast<uint32_t>(b) << 16) |
                          (static_cast<uint32_t>(g) << 8) |
                          r;
    return rgba;
}

uint32_t GPU::GetBackgroundColor(const uint8_t color, const uint8_t palette) const {
    if (hardware != Hardware::CGB) return DMG_SHADE[(backgroundPalette >> (color * 2)) & 0x03];

    const uint8_t red = bgpd[palette][color][0];
    const uint8_t green = bgpd[palette][color][1];
    const uint8_t blue = bgpd[palette][color][2];

    const auto r5 = static_cast<uint8_t>(red & 0x1F);
    const auto g5 = static_cast<uint8_t>(green & 0x1F);
    const auto b5 = static_cast<uint8_t>(blue & 0x1F);

    const auto corrR5 = static_cast<uint8_t>((26 * r5 + 4 * g5 + 2 * b5) >> 5);
    const auto corrG5 = static_cast<uint8_t>((6 * r5 + 24 * g5 + 2 * b5) >> 5);
    const auto corrB5 = static_cast<uint8_t>((2 * r5 + 4 * g5 + 26 * b5) >> 5);

    const uint8_t r = expand5(corrR5);
    const uint8_t g = expand5(corrG5);
    const uint8_t b = expand5(corrB5);

    const uint32_t rgba = 0xFF000000u |
                          (static_cast<uint32_t>(b) << 16) |
                          (static_cast<uint32_t>(g) << 8) |
                          r;
    return rgba;
}

Attributes GPU::GetAttrsFrom(const uint8_t byte) {
    return {
        .priority = Bit<OAM_PRIORITY_BIT>(byte), .yflip = Bit<OAM_Y_FLIP_BIT>(byte),
        .xflip = Bit<OAM_X_FLIP_BIT>(byte), .paletteNumberDMG = Bit<OAM_PALETTE_NUMBER_DMG_BIT>(byte),
        .vramBank = Bit<OAM_VRAM_BANK_BIT>(byte), .paletteNumberCGB = static_cast<uint8_t>(byte & 0x07)
    };
}

uint8_t GPU::ReadGpi(const Gpi &gpi) {
    return (gpi.autoIncrement ? 0x80 : 0x00) | gpi.index | 0x40;
}

void GPU::WriteGpi(Gpi &gpi, const uint8_t value) {
    gpi.autoIncrement = (value & 0x80) != 0x00;
    gpi.index = value & 0x3F;
}

uint8_t GPU::ReadVRAM(const uint16_t address) const {
    return stat.mode == GPUMode::MODE_3 ? 0xFF : vram[vramBank * 0x2000 + address - 0x8000];
}

void GPU::WriteVRAM(const uint16_t address, const uint8_t value) {
    if (stat.mode == GPUMode::MODE_3) return;
    vram[vramBank * 0x2000 + address - 0x8000] = value;
}

uint8_t GPU::ReadRegisters(const uint16_t address) const {
    switch (address) {
        case 0xFF40: return lcdc;
        case 0xFF41: return stat.value();
        case 0xFF42: return scrollY;
        case 0xFF43: return scrollX;
        case 0xFF44: return currentLine;
        case 0xFF45: return lyc;
        case 0xFF47: return backgroundPalette;
        case 0xFF48: return obp0Palette;
        case 0xFF49: return obp1Palette;
        case 0xFF4A: return windowY;
        case 0xFF4B: return wxWriteStage > 0 ? wxPending : windowX;
        case 0xFF4F: return hardware == Hardware::CGB ? (0xFE | vramBank) : 0xFF;
        case 0xFF68: return ReadGpi(bgpi);
        case 0xFF69: {
            const uint8_t r = bgpi.index >> 3;
            const uint8_t c = (bgpi.index >> 1) & 3;
            if ((bgpi.index & 0x01) == 0) {
                return bgpd[r][c][0] | (bgpd[r][c][1] << 5);
            }
            return (bgpd[r][c][1] >> 3) | (bgpd[r][c][2] << 2);
        }
        case 0xFF6A: return ReadGpi(obpi);
        case 0xFF6B: {
            const uint8_t r = obpi.index >> 3;
            const uint8_t c = (obpi.index >> 1) & 3;
            if ((obpi.index & 0x01) == 0) {
                return obpd[r][c][0] | (obpd[r][c][1] << 5);
            }
            return (obpd[r][c][1] >> 3) | (obpd[r][c][2] << 2);
        }
        case 0xFF6C: return 0xFE | objectPriority;
        default:
            throw UnreachableCodeException("GPU::ReadRegisters unreachable code at address: " + std::to_string(address));
    }
}

void GPU::ApplyLCDC(const uint8_t value) {
    const bool oldEnable = Bit<LCDC_ENABLE_BIT>(lcdc);
    lcdc = value;
    const bool newEnable = Bit<LCDC_ENABLE_BIT>(lcdc);
    if (!newEnable && oldEnable) {
        scanlineCounter = currentLine = 0;
        stat.mode = GPUMode::MODE_0;
        screenData.fill(0);
        hblank = true;
        hdma.hblankBlockFinished = false;
        vblank = false;
    } else if (newEnable && !oldEnable) {
        hdma.singleBlockTransfer = false;
        hdma.hblankBlockFinished = false;
        shortenScanline = true;
        // Line 0 after enabling reads as mode 0 and never runs an OAM scan;
        // OAM and VRAM stay accessible until mode 3 begins
        stat.mode = GPUMode::MODE_0;
        lcdEnableLine0_ = true;
        ResetScanlineState(true);
        hblank = false;
        vblank = false;
    }
}

void GPU::WriteRegisters(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0xFF40: {
            if (hardware != Hardware::CGB && stat.mode == GPUMode::MODE_3 && !LCDDisabled()) {
                // Clearing OBJ enable while a left-clipped sprite's fetch is in flight aborts the fetch — its pixels never reach the FIFO
                // A fetch already on its load tick completes; an on-screen sprite's fetch is never aborted
                if (!Bit<LCDC_OBJ_ENABLE>(value) && Bit<LCDC_OBJ_ENABLE>(lcdc) &&
                    spriteFetchActive_ && spriteFetchQueue.front().x < 0 &&
                    !(fetcherState_ == FetcherState::PushToFIFO && fetcherDelay_ == 0)) {
                    spriteFetchAbort_ = true;
                }
                // Disabling the window reaches the window logic one dot after
                // the write: the fetch in flight completes and its pixels are
                // drawn, then the fetcher moves on to background tiles.
                // Re-enabling cancels a pending, unconsumed end
                if (!Bit<LCDC_WINDOW_ENABLE>(value)) {
                    windowEndStage_ = 2;
                } else {
                    windowEndStage_ = 0;
                    // A pending end on an active window is already committed
                    // to finish its tile; only an unconsumed stale end clears
                    if (!isFetchingWindow_) windowEndPending_ = false;
                }
                if (lcdcWriteStage > 0) ApplyLCDC(lcdcPending);
                lcdcPending = value;
                lcdcWriteStage = 3;
            } else {
                ApplyLCDC(value);
            }
            break;
        }
        case 0xFF41: {
            stat.enableLYInterrupt = value & 0x40;
            stat.enableM2Interrupt = value & 0x20;
            stat.enableM1Interrupt = value & 0x10;
            stat.enableM0Interrupt = value & 0x08;
            // A write that leaves every enabled condition false drops the line
            // and re-arms the edge detector, so the next condition fires an IRQ
            // even if one was already taken this scanline. A write that raises
            // the line fires through the per-dot condition sites instead
            if (!LCDDisabled() && !StatLineHigh()) statTriggered = false;
            break;
        }
        case 0xFF42:
            // Like SCX, a mode-3 SCY write reaches the fetcher two dots late
            if (hardware != Hardware::CGB && stat.mode == GPUMode::MODE_3 && !LCDDisabled()) {
                if (scyWriteStage == 0) scyFetcherOld = scrollY;
                scyWriteStage = 3;
            }
            scrollY = value;
            break;
        case 0xFF43:
            // The fine-scroll consumers see an SCX write immediately, but the
            // fetcher's tile-map read keeps seeing the old value for two dots
            if (hardware != Hardware::CGB && stat.mode == GPUMode::MODE_3 && !LCDDisabled()) {
                if (scxWriteStage == 0) scxFetcherOld = scrollX;
                scxWriteStage = 3;
            }
            scrollX = value;
            break;
        case 0xFF44: currentLine = 0;
            break;
        case 0xFF45: lyc = value;
            break;
        case 0xFF47:
            if (hardware != Hardware::CGB && stat.mode == GPUMode::MODE_3 && !LCDDisabled()) {
                if (bgpWriteStage > 0) backgroundPalette = bgpPending;
                bgpPending = value;
                bgpWriteStage = 3;
            } else {
                backgroundPalette = value;
            }
            break;
        case 0xFF48:
            if (hardware != Hardware::CGB && stat.mode == GPUMode::MODE_3 && !LCDDisabled()) {
                if (obp0WriteStage > 0) obp0Palette = obp0Pending;
                obp0Pending = value;
                obp0WriteStage = 3;
            } else {
                obp0Palette = value;
            }
            break;
        case 0xFF49:
            if (hardware != Hardware::CGB && stat.mode == GPUMode::MODE_3 && !LCDDisabled()) {
                if (obp1WriteStage > 0) obp1Palette = obp1Pending;
                obp1Pending = value;
                obp1WriteStage = 3;
            } else {
                obp1Palette = value;
            }
            break;
        case 0xFF4A: windowY = value;
            break;
        case 0xFF4B:
            if (hardware != Hardware::CGB && stat.mode == GPUMode::MODE_3 && !LCDDisabled()) {
                if (wxWriteStage > 0) windowX = wxPending;
                wxPending = value;
                wxWriteStage = 4;
            } else {
                windowX = value;
            }
            break;
        case 0xFF4F: vramBank = value & 0x01;
            break;
        case 0xFF68: WriteGpi(bgpi, value);
            break;
        case 0xFF69: {
            const uint8_t r = bgpi.index >> 3;
            const uint8_t c = (bgpi.index >> 1) & 0x03;
            if ((bgpi.index & 0x01) == 0) {
                bgpd[r][c][0] = value & 0x1F;
                bgpd[r][c][1] = (bgpd[r][c][1] & 0x18) | (value >> 5);
            } else {
                bgpd[r][c][1] = (bgpd[r][c][1] & 0x07) | ((value & 0x03) << 3);
                bgpd[r][c][2] = (value >> 2) & 0x1F;
            }
            if (bgpi.autoIncrement) bgpi.index = (bgpi.index + 1) & 0x3F;
            break;
        }
        case 0xFF6A: WriteGpi(obpi, value);
            break;
        case 0xFF6B: {
            const uint8_t r = obpi.index >> 3;
            const uint8_t c = (obpi.index >> 1) & 0x03;
            if ((obpi.index & 0x01) == 0) {
                obpd[r][c][0] = value & 0x1F;
                obpd[r][c][1] = (obpd[r][c][1] & 0x18) | (value >> 5);
            } else {
                obpd[r][c][1] = (obpd[r][c][1] & 0x07) | ((value & 0x03) << 3);
                obpd[r][c][2] = (value >> 2) & 0x1F;
            }
            if (obpi.autoIncrement) obpi.index = (obpi.index + 1) & 0x3F;
            break;
        }
        case 0xFF6C: objectPriority = value & 0x01;
            break;
        default:
            break;
    }
}

const uint32_t *GPU::GetScreenData() const {
    return screenData.data();
}

bool GPU::SaveState(std::ofstream &stateFile) const {
    try {
        stateFile.write(reinterpret_cast<const char *>(&lcdc), sizeof(lcdc));
        stateFile.write(reinterpret_cast<const char *>(&stat), sizeof(stat));

        stateFile.write(reinterpret_cast<const char *>(vram.data()), vram.size());
        stateFile.write(reinterpret_cast<const char *>(oam.data()), oam.size());
        stateFile.write(reinterpret_cast<const char *>(screenData.data()), screenData.size());
        stateFile.write(reinterpret_cast<const char *>(priority_), sizeof(priority_));
        stateFile.write(reinterpret_cast<const char *>(&lyc), sizeof(lyc));
        stateFile.write(reinterpret_cast<const char *>(&currentLine), sizeof(currentLine));
        stateFile.write(reinterpret_cast<const char *>(&windowX), sizeof(windowX));
        stateFile.write(reinterpret_cast<const char *>(&windowY), sizeof(windowY));
        stateFile.write(reinterpret_cast<const char *>(&backgroundPalette), sizeof(backgroundPalette));
        stateFile.write(reinterpret_cast<const char *>(&obp0Palette), sizeof(obp0Palette));
        stateFile.write(reinterpret_cast<const char *>(&obp1Palette), sizeof(obp1Palette));
        stateFile.write(reinterpret_cast<const char *>(&scrollX), sizeof(scrollX));
        stateFile.write(reinterpret_cast<const char *>(&scrollY), sizeof(scrollY));
        stateFile.write(reinterpret_cast<const char *>(&scanlineCounter), sizeof(scanlineCounter));
        stateFile.write(reinterpret_cast<const char *>(&vblank), sizeof(vblank));
        stateFile.write(reinterpret_cast<const char *>(&hblank), sizeof(hblank));
        stateFile.write(reinterpret_cast<const char *>(&bgpi), sizeof(bgpi));
        stateFile.write(reinterpret_cast<const char *>(&obpi), sizeof(obpi));
        stateFile.write(reinterpret_cast<const char *>(&vramBank), sizeof(vramBank));
        stateFile.write(reinterpret_cast<const char *>(&bgpd), sizeof(bgpd));
        stateFile.write(reinterpret_cast<const char *>(&obpd), sizeof(obpd));
        stateFile.write(reinterpret_cast<const char *>(&hdma), sizeof(hdma));
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
    return true;
}

bool GPU::LoadState(std::ifstream &stateFile) {
    try {
        stateFile.read(reinterpret_cast<char *>(&lcdc), sizeof(lcdc));
        stateFile.read(reinterpret_cast<char *>(&stat), sizeof(stat));
        stateFile.read(reinterpret_cast<char *>(vram.data()), vram.size());
        stateFile.read(reinterpret_cast<char *>(oam.data()), oam.size());
        stateFile.read(reinterpret_cast<char *>(screenData.data()), screenData.size());
        stateFile.read(reinterpret_cast<char *>(priority_), sizeof(priority_));
        stateFile.read(reinterpret_cast<char *>(&lyc), sizeof(lyc));
        stateFile.read(reinterpret_cast<char *>(&currentLine), sizeof(currentLine));
        stateFile.read(reinterpret_cast<char *>(&windowX), sizeof(windowX));
        stateFile.read(reinterpret_cast<char *>(&windowY), sizeof(windowY));
        stateFile.read(reinterpret_cast<char *>(&backgroundPalette), sizeof(backgroundPalette));
        stateFile.read(reinterpret_cast<char *>(&obp0Palette), sizeof(obp0Palette));
        stateFile.read(reinterpret_cast<char *>(&obp1Palette), sizeof(obp1Palette));
        stateFile.read(reinterpret_cast<char *>(&scrollX), sizeof(scrollX));
        stateFile.read(reinterpret_cast<char *>(&scrollY), sizeof(scrollY));
        stateFile.read(reinterpret_cast<char *>(&scanlineCounter), sizeof(scanlineCounter));
        stateFile.read(reinterpret_cast<char *>(&vblank), sizeof(vblank));
        stateFile.read(reinterpret_cast<char *>(&hblank), sizeof(hblank));
        stateFile.read(reinterpret_cast<char *>(&bgpi), sizeof(bgpi));
        stateFile.read(reinterpret_cast<char *>(&obpi), sizeof(obpi));
        stateFile.read(reinterpret_cast<char *>(&vramBank), sizeof(vramBank));
        stateFile.read(reinterpret_cast<char *>(&bgpd), sizeof(bgpd));
        stateFile.read(reinterpret_cast<char *>(&obpd), sizeof(obpd));
        stateFile.read(reinterpret_cast<char *>(&hdma), sizeof(hdma));
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
    return true;
}
