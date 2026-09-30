#!/usr/bin/env python3
"""Convert the bundled AGE PNG oracles to StarGBC's RGBA framebuffer palette."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "tests/expected/age-test-roms"


def read_png(path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"Not a PNG: {path}")
    offset = 8
    compressed = bytearray()
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        tag = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        if tag == b"IHDR":
            width, height, depth, kind, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            if (width, height, depth, compression, filtering, interlace) != (160, 144, 8, 0, 0, 0) or kind not in (2, 6):
                raise ValueError(f"Expected a noninterlaced 160x144 RGB/RGBA PNG: {path}")
        elif tag == b"IDAT":
            compressed.extend(payload)
        offset += length + 12

    raw = zlib.decompress(compressed)
    channels = 3 if kind == 2 else 4
    stride = width * channels
    if len(raw) != height * (stride + 1):
        raise ValueError(f"Unexpected PNG data size: {path}")
    previous = bytearray(stride)
    pixels = bytearray()
    for offset in range(0, len(raw), stride + 1):
        filter_type = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            above = previous[x]
            upper_left = previous[x - channels] if x >= channels else 0
            estimate = left + above - upper_left
            distances = [abs(estimate - value) for value in (left, above, upper_left)]
            paeth = (left, above, upper_left)[distances.index(min(distances))]
            predictor = (0, left, above, (left + above) // 2, paeth)[filter_type]
            row[x] = (row[x] + predictor) & 255
        for x in range(0, stride, channels):
            pixels.extend((*row[x:x + 3], 255))
        previous = row
    return pixels


def convert(path):
    pixels = read_png(path)
    for offset in range(0, len(pixels), 4):
        red, green, blue = pixels[offset:offset + 3]
        if "-dmgC" in path.stem:
            if red != green or red != blue:
                raise ValueError(f"Non-grayscale DMG reference: {path}")
            shade = {0: 0, 85: 96, 170: 192, 255: 255}[red]
            rgb = (shade, shade, shade)
        else:
            red, green, blue = red >> 3, green >> 3, blue >> 3
            rgb5 = ((26 * red + 4 * green + 2 * blue) >> 5,
                    (6 * red + 24 * green + 2 * blue) >> 5,
                    (2 * red + 4 * green + 26 * blue) >> 5)
            rgb = tuple((value << 3) | (value >> 2) for value in rgb5)
        pixels[offset:offset + 3] = bytes(rgb)
    return pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify fixtures and hashes without writing")
    args = parser.parse_args()
    manifest = {}
    outputs = {}
    for source in sorted((ROOT / "roms/age-test-roms").rglob("*.png")):
        pixels = convert(source)
        name = source.stem + ".screen"
        if name in outputs:
            raise ValueError(f"Duplicate reference name: {name}")
        outputs[name] = pixels
        manifest[name] = {
            "source": str(source.relative_to(ROOT)),
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "screen_sha256": hashlib.sha256(pixels).hexdigest(),
        }
    if not outputs:
        raise ValueError("No AGE reference PNGs found")
    outputs["manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, content in outputs.items():
        target = OUTPUT / name
        if args.check:
            if not target.exists() or target.read_bytes() != content:
                raise SystemExit(f"Reference needs regeneration: {target}")
        else:
            target.write_bytes(content)
    print(f"{'Verified' if args.check else 'Generated'} {len(manifest)} AGE references")


if __name__ == "__main__":
    main()
