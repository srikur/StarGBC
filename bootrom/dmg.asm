; StarGBC custom DMG bootrom.
; 256 bytes; assembled with RGBDS.
;
; Layout:
;   - Init SP, clear VRAM, configure APU, set BG palette
;   - Expand 56-byte 1bpp "StarGBC" glyph data into 28 stretched 2x2 tiles
;   - Build tilemap centered horizontally on row 8-9
;   - Scroll text in from below to vertical centre over ~2 s, dual ding
;   - Hold for 1 s, JP $0100 (also auto-unmaps bootrom in this emulator)

SECTION "Bootrom", ROM0[$0000]

EntryPoint:
    ld sp, $FFFE                ; init stack

    ; --- Clear VRAM $8000-$9FFF ---
    xor a
    ld hl, $9FFF
.clearVRAM:
    ld [hl-], a
    bit 7, h
    jr nz, .clearVRAM

    ; --- APU on, ch1 setup ---
    ld a, $80
    ldh [$FF26], a                ; NR52: APU on
    ldh [$FF11], a                ; NR11: duty 50%
    ld a, $F3
    ldh [$FF12], a                ; NR12: envelope
    ldh [$FF25], a                ; NR51: panning
    ld a, $77
    ldh [$FF24], a                ; NR50: master volume

    ; --- BG palette ---
    ld a, $FC
    ldh [$FF47], a                ; BGP

    ; --- Expand glyphs into VRAM tiles 1..28 ---
    call ExpandGlyphs

    ; --- Tilemap: row 8 cols 3..16 = tiles 1..14; row 9 cols 3..16 = tiles 15..28 ---
    ld hl, $9903
    ld a, 1
    ld c, 14
.tmTop:
    ld [hl+], a
    inc a
    dec c
    jr nz, .tmTop

    ld hl, $9923
    ld c, 14
.tmBot:
    ld [hl+], a
    inc a
    dec c
    jr nz, .tmBot

    ; --- Initial scroll: text just below visible area ---
    ld a, 192                   ; SCY = -64 → text at screen Y=128
    ldh [$FF42], a

    ; --- Turn on LCD ---
    ld a, $91                   ; LCD on, tile data $8000, BG on
    ldh [$FF40], a

    ; --- First ding (low note, ~1048 Hz) ---
    ld a, $83
    ldh [$FF13], a                ; NR13
    ld a, $87
    ldh [$FF14], a                ; NR14 trigger

    ; --- Scroll loop: SCY 192 → 0 over 64 steps (×2 frames each) ---
    ld c, 64
.scroll:
    call WaitFrame
    call WaitFrame
    ldh a, [$FF42]
    inc a
    ldh [$FF42], a
    dec c
    jr nz, .scroll

    ; --- Second ding (high note, ~2080 Hz) ---
    ld a, $C1
    ldh [$FF13], a
    ld a, $87
    ldh [$FF14], a

    ; --- Hold ~60 frames ---
    ld c, 60
.hold:
    call WaitFrame
    dec c
    jr nz, .hold

    ; Hand off to cart. PC == $0100 auto-unmaps the bootrom in this emulator.
    jp $0100

; -----------------------------------------------------------------------------
; WaitFrame: spin until the next VBlank start (LY transitions to 144).
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
; ExpandGlyphs
; In : DE = GlyphData (56 bytes), HL = $8010
; Out: writes 28 tiles (448 bytes) to VRAM.
;
; Source layout: 7 top-halves then 7 bottom-halves, 4 bytes per half.
; Per output tile (16 bytes): 4 source bytes, each expanded into 4 identical
; output bytes (= one 8-pixel row at 2bpp, vertically stretched). Even tiles
; (left) use the source byte's high nibble; odd tiles (right) use the low
; nibble. After writing a left tile we rewind DE so the right tile re-reads
; the same 4 source bytes.
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
    bit 0, c                    ; bit 0 = 0 → left, = 1 → right
    jr nz, .lo
    swap a                      ; left: high nibble into low nibble
.lo:
    call ExpandLookup           ; A = doubled-bit byte
    ld [hl+], a
    ld [hl+], a
    ld [hl+], a
    ld [hl+], a
    dec b
    jr nz, .row

    bit 0, c
    jr nz, .next                ; right tile: keep DE
    dec de                      ; left tile: rewind 4 bytes
    dec de
    dec de
    dec de
.next:
    dec c
    jr nz, .tile
    ret

; -----------------------------------------------------------------------------
; ExpandLookup: low nibble of A → byte where every input bit B is doubled (BB).
; Preserves HL/DE/BC; the caller's B (row counter) and C (tile counter) survive.
; -----------------------------------------------------------------------------
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
ExpandTable:
    db $00, $03, $0C, $0F
    db $30, $33, $3C, $3F
    db $C0, $C3, $CC, $CF
    db $F0, $F3, $FC, $FF

; 1bpp 8x8 glyphs for "StarGBC", arranged for sequential VRAM tile output.
; 28 bytes of top halves (rows 0-3), then 28 bytes of bottom halves (rows 4-7).
GlyphData:
    ; Top halves (rows 0-3 of each letter)
    db $7E, $C0, $C0, $7C       ; S
    db $30, $30, $FC, $30       ; t
    db $00, $00, $7C, $06       ; a
    db $00, $00, $DC, $E0       ; r
    db $7E, $C0, $C0, $DE       ; G
    db $FC, $C6, $C6, $FC       ; B
    db $7E, $C0, $C0, $C0       ; C

    ; Bottom halves (rows 4-7 of each letter)
    db $06, $06, $FC, $00       ; S
    db $30, $30, $3C, $00       ; t
    db $7E, $C6, $7E, $00       ; a
    db $C0, $C0, $C0, $00       ; r
    db $C6, $C6, $7E, $00       ; G
    db $C6, $C6, $FC, $00       ; B
    db $C0, $C0, $7E, $00       ; C

    ; Pad to exactly 256 bytes
    ds $100 - @, $FF
