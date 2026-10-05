#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "roms/gambatte"
REFERENCES = ROOT / "tests/expected/gambatte"
OUTPUT = ROOT / "tests/GambatteTests.txt"

DMG = "dmgb"
CGB = "cgbc"


def out_expectation(name, marker):
    rest = name[name.find(marker) + len(marker) :]
    if rest.startswith("audio0") or rest.startswith("audio1"):
        return f"audio:{rest[5]}"
    digits = re.match(r"[0-9A-Fa-f]*", rest).group(0)
    if not digits:
        raise ValueError(f"No result after '{marker}' in {name}")
    return f"out:{digits.upper()}"


def cases_for(rom):
    stem = rom.stem
    cases = []

    if "dmg08_cgb04c_out" in stem:
        expected = out_expectation(stem, "dmg08_cgb04c_out")
        cases += [(expected, CGB), (expected, DMG)]
    elif "dmg08_out" in stem:
        if "cgb04c_out" in stem:
            cases.append((out_expectation(stem, "cgb04c_out"), CGB))
        cases.append((out_expectation(stem, "dmg08_out"), DMG))
    elif "_out" in stem:
        cases.append((out_expectation(stem, "_out"), CGB))

    for suffix, model in (("_cgb04c", CGB), ("_dmg08", DMG)):
        png = rom.with_name(stem + suffix + ".png")
        if png.exists():
            screen = REFERENCES / png.relative_to(SOURCE).with_suffix(".screen")
            if not screen.exists():
                raise FileNotFoundError(
                    f"{screen.relative_to(ROOT)} is missing; run generate-gambatte-references.py"
                )
            cases.append((screen.relative_to(ROOT).as_posix(), model))

    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()

    roms = sorted(p for p in SOURCE.rglob("*") if p.suffix in (".gb", ".gbc"))
    lines = []
    for rom in roms:
        rom_path = rom.relative_to(ROOT).as_posix()
        lines += [
            f"{rom_path},{expected},{model}" for expected, model in cases_for(rom)
        ]

    args.output.write_text("\n".join(lines) + "\n")
    print(f"Wrote {len(lines)} cases from {len(roms)} ROMs to {args.output}")


if __name__ == "__main__":
    main()
