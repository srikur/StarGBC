#!/usr/bin/env python3
"""Convert bootrom binaries to a C++ header.

Usage: embed.py OUTPUT_HEADER ROM1 [ROM2 ...]
ROM file stems become symbol names (dmg.bin -> kDmgBootrom).
"""

import pathlib
import sys


def emit(rom: pathlib.Path) -> list[str]:
    data = rom.read_bytes()
    name = f"k{rom.stem.capitalize()}Bootrom"
    lines = [
        f"inline constexpr std::array<std::uint8_t, {len(data)}> {name} {{{{",
    ]
    for i in range(0, len(data), 16):
        row = data[i : i + 16]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in row) + ",")
    lines.append("}};")
    lines.append("")
    return lines


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 1
    out = pathlib.Path(sys.argv[1])
    roms = [pathlib.Path(p) for p in sys.argv[2:]]

    header = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "",
        "namespace stargbc::bootroms {",
        "",
    ]
    for rom in roms:
        header.extend(emit(rom))
    header.append("} // namespace stargbc::bootroms")
    header.append("")

    out.parent.mkdir(parents=True, exist_ok=True)
    _ = out.write_text("\n".join(header))
    return 0


if __name__ == "__main__":
    sys.exit(main())
