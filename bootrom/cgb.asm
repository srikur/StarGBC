; StarGBC custom CGB bootrom.
; 2304 bytes total: $0000-$00FF + $0100-$01FF (cart-header gap) + $0200-$08FF.

SECTION "Bootrom", ROM0[$0000]

EntryPoint:
    ld sp, $FFFE                ; init stack

    ; --- Clear VRAM bank 0 ($8000-$9FFF) ---
    xor a
    ld hl, $9FFF
.clearBank0:
    ld [hl-], a
    bit 7, h
    jr nz, .clearBank0

    ; --- Switch to VRAM bank 1, clear, switch back ---
    ld a, 1
    ldh [$FF4F], a
    xor a
    ld hl, $9FFF
.clearBank1:
    ld [hl-], a
    bit 7, h
    jr nz, .clearBank1
    xor a
    ldh [$FF4F], a              ; back to bank 0

    ; --- APU on, ch1 setup ---
    ld a, $80
    ldh [$FF26], a              ; NR52: APU on
    ldh [$FF11], a              ; NR11: duty 50%
    ld a, $F3
    ldh [$FF12], a              ; NR12: envelope
    ldh [$FF25], a              ; NR51: panning
    ld a, $77
    ldh [$FF24], a              ; NR50: master volume

    ld a, $FC
    ldh [$FF47], a

    ; --- Initialise CGB BG palettes 0 + 1 ---
    ld a, $80                   ; BCPS auto-increment from index 0
    ldh [$FF68], a
    ld hl, BgPaletteInit
    ld b, 16                    ; 2 palettes × 4 colors × 2 bytes
.bgpalLoop:
    ld a, [hl+]
    ldh [$FF69], a              ; BCPD
    dec b
    jr nz, .bgpalLoop

    jp Main                     ; into $0200 segment

; Pad first segment to $0100 with $FF
    ds $100 - @, $FF
    ds $100, $FF

SECTION "BootromMain", ROM0[$0200]

Main:
    ; --- Expand StarGBC text glyphs into VRAM tiles 1..28 ---
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

    ; --- Tilemap (VRAM bank 0): row 5-6 cols 9-10 = star (4 tiles),
    ;                            rows 8-9 cols 3-16 = text (14 tiles each row) ---
    xor a
    ldh [$FF4F], a              ; ensure bank 0 for the tilemap writes

    ; Star top row (row 5 cols 9-10): tiles 29, 30
    ld hl, $98A9                ; $9800 + 5*32 + 9
    ld a, 29
    ld [hl+], a
    inc a
    ld [hl], a                  ; HL = $98AA, holds tile 30

    ; Star bottom row (row 6 cols 9-10): tiles 31, 32
    ld hl, $98C9                ; $9800 + 6*32 + 9
    ld a, 31
    ld [hl+], a
    inc a
    ld [hl], a

    ; Text top row (row 8 cols 3-16): tiles 1..14
    ld hl, $9903
    ld a, 1
    ld c, 14
.tmTop:
    ld [hl+], a
    inc a
    dec c
    jr nz, .tmTop

    ; Text bottom row (row 9 cols 3-16): tiles 15..28
    ld hl, $9923
    ld c, 14
.tmBot:
    ld [hl+], a
    inc a
    dec c
    jr nz, .tmBot

    ld a, 1
    ldh [$FF4F], a              ; switch to bank 1

    ld hl, $98A9                ; star top-left
    ld a, $01                   ; palette 1
    ld [hl+], a
    ld [hl], a                  ; star top-right
    ld hl, $98C9                ; star bottom-left
    ld [hl+], a
    ld [hl], a                  ; star bottom-right

    xor a
    ldh [$FF4F], a              ; back to bank 0

    ; --- Initial scroll: SCY = 64 → text/star at top of screen ---
    ld a, 64
    ldh [$FF42], a

    ; --- LCD on (BG enable, tile data $8000, BG tilemap $9800) ---
    ld a, $91
    ldh [$FF40], a

    ; --- First ding (low note) ---
    ld a, $83
    ldh [$FF13], a
    ld a, $87
    ldh [$FF14], a

    ; --- Scroll loop: SCY 64 → 0 in 64 steps × 2 frames ---
    ld c, 64
.scroll:
    call WaitFrame
    call WaitFrame
    ldh a, [$FF42]
    dec a
    ldh [$FF42], a
    dec c
    jr nz, .scroll

    ; --- Second ding (high note) ---
    ld a, $C1
    ldh [$FF13], a
    ld a, $87
    ldh [$FF14], a

    ; --- Rainbow palette flash on palette 0 colour 3
    ;     6 colours × 10 frames each (red → yellow → green → blue → purple → cyan-blue)
    ld de, FlashColors
    ld b, 6
.flashColor:
    push bc
    ld a, $86                   ; BCPS = palette 0 colour 3, auto-increment
    ldh [$FF68], a
    ld a, [de]
    inc de
    ldh [$FF69], a              ; low byte
    ld a, [de]
    inc de
    ldh [$FF69], a              ; high byte

    ld c, 10                    ; 10 frames per colour
.flashWait:
    call WaitFrame
    dec c
    jr nz, .flashWait

    pop bc
    dec b
    jr nz, .flashColor

    ; --- Hold final blue-cyan for ~30 frames ---
    ld c, 30
.hold:
    call WaitFrame
    dec c
    jr nz, .hold

    ; --- Set CGB handoff register A=$11 and jump to cart ---
    ld a, $11
    jp $0100

; -----------------------------------------------------------------------------
; WaitFrame: spin until LY enters VBlank.
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

; Preserves HL/DE/BC.
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
    ; Palette 0
    dw $0000        ; color 0
    dw $0000        ; color 1
    dw $0000        ; color 2
    dw $7FFF        ; color 3 (white, will be overwritten by the flash)
    ; Palette 1 — gold ramp
    dw $0000        ; color 0
    dw $0843        ; color 1 (dark amber)
    dw $0CC7        ; color 2 (deep gold)
    dw $131F        ; color 3 (bright gold: R=31 G=24 B=4)

; Rainbow flash sequence applied to palette 0 colour 3.
FlashColors:
    dw $001F        ; Red       (R=31 G=0  B=0)
    dw $039F        ; Yellow    (R=31 G=28 B=0)
    dw $23E0        ; Green     (R=0  G=31 B=8)
    dw $7D40        ; Blue      (R=0  G=10 B=31)
    dw $5014        ; Purple    (R=20 G=0  B=20)
    dw $7F08        ; Cyan-blue (R=8  G=24 B=31) — settles here

ExpandTable:
    db $00, $03, $0C, $0F
    db $30, $33, $3C, $3F
    db $C0, $C3, $CC, $CF
    db $F0, $F3, $FC, $FF

; 1bpp 8×8 glyphs for "StarGBC", same layout as the DMG bootrom:
; 28 bytes of top halves (rows 0-3), then 28 bytes of bottom halves (rows 4-7).
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
; Gold star: 16×16 pre-rendered as 4 2bpp tiles (16 bytes each).
; Both bit-planes are identical, so each pixel is color 3 of palette 1.
; Tile order: top-left, top-right, bottom-left, bottom-right.
; -----------------------------------------------------------------------------
StarData:
    ; Tile 29 — top-left
    db $01, $01, $01, $01, $03, $03, $03, $03
    db $07, $07, $07, $07, $FF, $FF, $FF, $FF
    ; Tile 30 — top-right
    db $80, $80, $80, $80, $C0, $C0, $C0, $C0
    db $E0, $E0, $E0, $E0, $FF, $FF, $FF, $FF
    ; Tile 31 — bottom-left
    db $7F, $7F, $3F, $3F, $1F, $1F, $1E, $1E
    db $3C, $3C, $3C, $3C, $70, $70, $E0, $E0
    ; Tile 32 — bottom-right
    db $FE, $FE, $FC, $FC, $F8, $F8, $78, $78
    db $3C, $3C, $3C, $3C, $0E, $0E, $07, $07

; Pad to exactly 2304 bytes
    ds $900 - @, $FF
