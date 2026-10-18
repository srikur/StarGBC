; StarGBC custom CGB bootrom.
; 2304 bytes total: $0000-$00FF + $0100-$01FF (cart-header gap) + $0200-$08FF.

SECTION "Bootrom", ROM0[$0000]

EntryPoint:
    ld sp, $FFFE

    ; --- Clear VRAM bank 0 ($8000-$9FFF) ---
    xor a
    ld hl, $9FFF
.clearBank0:
    ld [hl-], a
    bit 7, h
    jr nz, .clearBank0

    ; --- Switch to bank 1, clear, switch back ---
    ld a, 1
    ldh [$FF4F], a
    xor a
    ld hl, $9FFF
.clearBank1:
    ld [hl-], a
    bit 7, h
    jr nz, .clearBank1
    xor a
    ldh [$FF4F], a              ; bank 0

    ; --- Audio init ---
    ld a, $80
    ldh [$FF26], a              ; NR52
    ldh [$FF11], a              ; NR11
    ld a, $F3
    ldh [$FF12], a              ; NR12
    ldh [$FF25], a              ; NR51
    ld a, $77
    ldh [$FF24], a              ; NR50

    ld a, $FC
    ldh [$FF47], a              ; BGP (DMG compat; ignored on CGB)

    ; --- Init CGB BG palettes ---
    ld a, $80                   ; BCPS auto-increment from index 0
    ldh [$FF68], a
    ld hl, BgPaletteInit
    ld b, 64                    ; 8 palettes * 4 colours * 2 bytes
.bgpalLoop:
    ld a, [hl+]
    ldh [$FF69], a
    dec b
    jr nz, .bgpalLoop

    jp Main

; Pad first segment to $0100 with $FF
    ds $100 - @, $FF

; Gap $0100-$01FF -- cart header on a real cart.
    ds $100, $FF

SECTION "BootromMain", ROM0[$0200]

Main:
    ; --- Expand 1bpp StarGBC glyphs into tiles 1..28 ---
    call ExpandGlyphs

    ; --- Copy the 64-byte gold star bitmap into tiles 29..32 ($81D0-$820F) ---
    ld de, StarData
    ld hl, $81D0
    ld b, 64
.starCopy:
    ld a, [de]
    inc de
    ld [hl+], a
    dec b
    jr nz, .starCopy

    ; -------------------------------------------------------------------------
    ; Tilemap (VRAM bank 0)
    ;   The text area (rows 8-9 cols 3-16) is left blank — the unroll routine
    ;   below fills it tile-by-tile. The star is written immediately so it sits
    ;   on the white background from the moment the LCD turns on.
    ; -------------------------------------------------------------------------
    xor a
    ldh [$FF4F], a

    ; Star top-row cells (row 5 cols 9-10): tiles 29, 30
    ld hl, $98A9
    ld a, 29
    ld [hl+], a
    inc a
    ld [hl], a

    ; Star bottom-row cells (row 6 cols 9-10): tiles 31, 32
    ld hl, $98C9
    ld a, 31
    ld [hl+], a
    inc a
    ld [hl], a

    ; -------------------------------------------------------------------------
    ; Attributes (VRAM bank 1).
    ;   Per-letter palette assignment is written up-front, even though the
    ;   corresponding tilemap cells are still blank — when the unroll routine
    ;   later drops a glyph tile in, the colour is already correct.
    ; -------------------------------------------------------------------------
    ld a, 1
    ldh [$FF4F], a

    ; Star: 4 cells = palette 7
    ld a, 7
    ld hl, $98A9
    ld [hl+], a
    ld [hl], a
    ld hl, $98C9
    ld [hl+], a
    ld [hl], a

    ; Text top row attributes (row 8 cols 3-16)
    ld hl, $9903
    ld de, AttrPattern
    ld b, 14
.attrTop:
    ld a, [de]
    inc de
    ld [hl+], a
    dec b
    jr nz, .attrTop

    ; Text bottom row attributes (row 9 cols 3-16)
    ld hl, $9923
    ld de, AttrPattern
    ld b, 14
.attrBot:
    ld a, [de]
    inc de
    ld [hl+], a
    dec b
    jr nz, .attrBot

    xor a
    ldh [$FF4F], a              ; back to bank 0

    ; --- No SCY scroll: text and star render at their final positions. ---
    xor a
    ldh [$FF42], a              ; SCY
    ldh [$FF43], a              ; SCX

    ; --- LCD on (BG enable, tile data $8000, BG tilemap $9800) ---
    ld a, $91
    ldh [$FF40], a

    ; --- Hold the white screen + star for ~30 frames before the unroll. ---
    ld c, 30
.preHold:
    call WaitFrame
    dec c
    jr nz, .preHold

    ; --- First ding (low note) at the start of the unroll. ---
    ld a, $83
    ldh [$FF13], a
    ld a, $87
    ldh [$FF14], a

    ; -------------------------------------------------------------------------
    ; Unroll: 4 steps spreading outward from the centre letter.
    ;   step 0: letter 3 (r)
    ;   step 1: letters 2 and 4 (a, G)
    ;   step 2: letters 1 and 5 (t, B)
    ;   step 3: letters 0 and 6 (S, C)
    ; Each step does its tilemap writes during VBlank (safe VRAM window) and
    ; then waits 10 frames before the next step.
    ; -------------------------------------------------------------------------
    ld b, 0
.unrollStep:
    call WaitFrame
    push bc
    call RevealStep
    pop bc

    ld c, 10
.unrollWait:
    call WaitFrame
    dec c
    jr nz, .unrollWait

    inc b
    ld a, b
    cp 4
    jr c, .unrollStep

    ; --- Let the fully-unrolled wordmark sit briefly before the flash. ---
    ld c, 20
.afterUnroll:
    call WaitFrame
    dec c
    jr nz, .afterUnroll

    ; --- Second ding (high note) at the start of the flash. ---
    ld a, $C1
    ldh [$FF13], a
    ld a, $87
    ldh [$FF14], a

    ; -------------------------------------------------------------------------
    ; Rainbow flash. 50 frames; rotation offset D advances each frame so the
    ; colour wheel sweeps across the seven letter palettes. 50 mod 7 == 1, so
    ; the last write lands with D == 0 and palette p settles on RainbowColors[p].
    ; -------------------------------------------------------------------------
    ld b, 50                    ; total flash frames
    ld d, 0                     ; rotation offset (0..6)
.flashFrame:
    call WaitFrame
    ld e, 0                     ; palette index
.palLoop:
    ; Colour index = (E + D) mod 7
    ld a, e
    add d
    cp 7
    jr c, .noWrap
    sub 7
.noWrap:
    ; HL = RainbowColors + (colour_idx * 2)
    add a
    ld c, a
    ld hl, RainbowColors
    ld a, l
    add c
    ld l, a
    ld a, h
    adc 0
    ld h, a

    ; BCPS = palette E colour 3 byte 0 (= E*8 + 6), auto-increment
    ld a, e
    sla a
    sla a
    sla a
    add 6
    or $80
    ldh [$FF68], a

    ; Write the two colour bytes
    ld a, [hl+]
    ldh [$FF69], a
    ld a, [hl]
    ldh [$FF69], a

    inc e
    ld a, e
    cp 7
    jr c, .palLoop

    ; Rotate (D = (D+1) mod 7)
    inc d
    ld a, d
    cp 7
    jr c, .noDWrap
    xor a
    ld d, a
.noDWrap:

    dec b
    jr nz, .flashFrame

    ; --- Hold the settled rainbow for ~45 frames so the user can see it. ---
    ld c, 45
.hold:
    call WaitFrame
    dec c
    jr nz, .hold

    ; --- Hand off: A = $11 (CGB indicator), jump to cart. ---
    ld a, $11
    jp $0100

; -----------------------------------------------------------------------------
; RevealStep
;   Input:  B = step (0..3)
;   Writes the tilemap entries for one symmetric unroll step.
;   B == 0  => letter 3 alone (the centre).
;   B  > 0  => letters (3 - B) and (3 + B).
;   Clobbers A, BC, HL.
; -----------------------------------------------------------------------------
RevealStep:
    ld a, 3
    sub b
    push bc
    call RevealLetterByIndex
    pop bc
    ld a, b
    or a
    ret z
    ld a, 3
    add b
    jp RevealLetterByIndex

; -----------------------------------------------------------------------------
; RevealLetterByIndex
;   Input:  A = letter index (0..6)
;   Writes the four tilemap entries for that letter (top-left, top-right,
;   bot-left, bot-right) into the row 8 / row 9 text area.
;     top tiles : 1 + i*2, 2 + i*2 at $9903 + i*2
;     bot tiles : 15 + i*2, 16 + i*2 at $9923 + i*2
;   Clobbers A, BC, HL.
; -----------------------------------------------------------------------------
RevealLetterByIndex:
    add a                       ; col offset = i * 2
    ld c, a
    ld b, 0

    ld hl, $9903
    add hl, bc
    ld a, 1
    add c
    ld [hl+], a
    inc a
    ld [hl], a

    ld hl, $9923
    add hl, bc
    ld a, 15
    add c
    ld [hl+], a
    inc a
    ld [hl], a
    ret

; -----------------------------------------------------------------------------
; WaitFrame: spin until LY transitions into VBlank.
; Clobbers A only.
; -----------------------------------------------------------------------------
WaitFrame:
.notVbl:
    ldh a, [$FF44]
    cp 144
    jr z, .notVbl
.vbl:
    ldh a, [$FF44]
    cp 144
    jr nz, .vbl
    ret

; -----------------------------------------------------------------------------
; ExpandGlyphs / ExpandLookup -- same as the DMG bootrom.
; -----------------------------------------------------------------------------
ExpandGlyphs:
    ld de, GlyphData
    ld hl, $8010
    ld c, 28
.tile:
    ld b, 4
.row:
    ld a, [de]
    inc de
    bit 0, c
    jr nz, .lo
    swap a
.lo:
    call ExpandLookup
    ld [hl+], a
    ld [hl+], a
    ld [hl+], a
    ld [hl+], a
    dec b
    jr nz, .row

    bit 0, c
    jr nz, .next
    dec de
    dec de
    dec de
    dec de
.next:
    dec c
    jr nz, .tile
    ret

ExpandLookup:
    push bc
    push hl
    and $0F
    ld c, a
    ld b, 0
    ld hl, ExpandTable
    add hl, bc
    ld a, [hl]
    pop hl
    pop bc
    ret

; -----------------------------------------------------------------------------
; Data
; -----------------------------------------------------------------------------
BgPaletteInit:
    ; Palettes 0..6 -- letters: white background, black ink to start.
    REPT 7
    dw $7FFF, $0000, $0000, $0000
    ENDR
    ; Palette 7 -- gold star on white.
    dw $7FFF        ; colour 0 (white background)
    dw $0843        ; colour 1 (dark amber)
    dw $0CC7        ; colour 2 (deep gold)
    dw $131F        ; colour 3 (bright gold: R=31 G=24 B=4)

AttrPattern:
    db $00, $00     ; S (palette 0)
    db $01, $01     ; t (palette 1)
    db $02, $02     ; a (palette 2)
    db $03, $03     ; r (palette 3)
    db $04, $04     ; G (palette 4)
    db $05, $05     ; B (palette 5)
    db $06, $06     ; C (palette 6)

RainbowColors:
    dw $001F        ; 0: Red       (R=31 G=0  B=0)
    dw $025F        ; 1: Orange    (R=31 G=18 B=0)
    dw $03DF        ; 2: Yellow    (R=31 G=30 B=0)
    dw $23E0        ; 3: Green     (R=0  G=31 B=8)
    dw $7FE0        ; 4: Cyan      (R=0  G=31 B=31)
    dw $7C00        ; 5: Blue      (R=0  G=0  B=31)
    dw $600C        ; 6: Violet    (R=12 G=0  B=24)

ExpandTable:
    db $00, $03, $0C, $0F
    db $30, $33, $3C, $3F
    db $C0, $C3, $CC, $CF
    db $F0, $F3, $FC, $FF

; 1bpp 8x8 glyphs for "StarGBC" -- top halves then bottom halves, 4 bytes each.
GlyphData:
    db $7E, $C0, $C0, $7C       ; S top
    db $30, $30, $FC, $30       ; t top
    db $00, $00, $7C, $06       ; a top
    db $00, $00, $DC, $E0       ; r top
    db $7E, $C0, $C0, $DE       ; G top
    db $FC, $C6, $C6, $FC       ; B top
    db $7E, $C0, $C0, $C0       ; C top

    db $06, $06, $FC, $00       ; S bot
    db $30, $30, $3C, $00       ; t bot
    db $7E, $C6, $7E, $00       ; a bot
    db $C0, $C0, $C0, $00       ; r bot
    db $C6, $C6, $7E, $00       ; G bot
    db $C6, $C6, $FC, $00       ; B bot
    db $C0, $C0, $7E, $00       ; C bot

; -----------------------------------------------------------------------------
; Gold star: 16x16, pre-rendered 2bpp
; -----------------------------------------------------------------------------
StarData:
    ; Tile 29 -- top-left
    db $01, $01, $01, $01, $03, $03, $03, $03
    db $07, $07, $07, $07, $FF, $FF, $FF, $FF
    ; Tile 30 -- top-right
    db $80, $80, $80, $80, $C0, $C0, $C0, $C0
    db $E0, $E0, $E0, $E0, $FF, $FF, $FF, $FF
    ; Tile 31 -- bottom-left
    db $7F, $7F, $3F, $3F, $1F, $1F, $1E, $1E
    db $3C, $3C, $3C, $3C, $70, $70, $E0, $E0
    ; Tile 32 -- bottom-right
    db $FE, $FE, $FC, $FC, $F8, $F8, $78, $78
    db $3C, $3C, $3C, $3C, $0E, $0E, $07, $07

; Pad to exactly 2304 bytes
    ds $900 - @, $FF
