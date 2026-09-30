# AGE screenshot references

These 11 `.screen` files are converted from the original PNGs bundled in
`roms/age-test-roms`. They are independent test oracles, not emulator captures.
Each file contains 160 × 144 pixels in RGBA byte order, with alpha 255.

The originals are from [Christoph Sprenger's AGE test ROMs](https://github.com/c-sp/age-test-roms).
The source used to investigate the tests was
[`1f5bc10e2cb60b86c19f8d0287ff78f410c4f639`](https://github.com/c-sp/age-test-roms/tree/1f5bc10e2cb60b86c19f8d0287ff78f410c4f639/src).
The bundled ROMs and PNGs are retained unchanged. The upstream MIT license is
included in [LICENSE](LICENSE).

The conversion changes only colors to match StarGBC's framebuffer palette:

- DMG grayscale levels 255, 170, 85, 0 map to 255, 192, 96, 0.
- CGB channels are reduced to five bits, passed through the core's color matrix
  `((26r+4g+2b), (6r+24g+2b), (2r+4g+26b)) / 32`, and expanded to eight bits.
  Compatibility-mode references use the same CGB conversion.

Reproduce the files from the repository root, using only Python's standard library:

```sh
python3 scripts/generate-age-references.py
python3 scripts/generate-age-references.py --check
```

[manifest.json](manifest.json) records the original PNG path and SHA-256 hashes
of both the PNG and converted framebuffer. No pixels are shifted or masked.
The automated graphics cases require two consecutive frames to match exactly.

The test registration in `tests/AgeTestCases.inc` runs every hardware revision
named by each ROM or reference image: DMG-C and CGB-B/C/E. Numeric cases require
AGE's terminal freeze loop and BC=0305, DE=080D, HL=1522. The speed-switch ROMs
in upstream's `caution` directory are included with the same strict checks.
