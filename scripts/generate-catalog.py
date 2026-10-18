#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def gambatte_case(rom, expected, model):
    model = model.upper()
    variant = "audio" if expected.startswith("audio:") else "digits" if expected.startswith("out:") else "screen"
    return dict(id=stable_id("gambatte", rom, model, "skip-bootrom", variant), rom=rom, model=model,
                variant=variant, expected=expected, romHash=hashlib.sha256((ROOT / rom).read_bytes()).hexdigest(),
                referenceHash=hashlib.sha256(
                    (ROOT / expected).read_bytes()).hexdigest() if variant == "screen" else None,
                tags=[Path(rom).parts[2]], sourceFile=None)


def write_catalog(path, catalog):
    metadata = {k: v for k, v in catalog.items() if k != "cases"}
    text = json.dumps(metadata, indent=2)[:-2] + ',\n  "cases": [\n'
    text += ",\n".join("    " + json.dumps(c, separators=(",", ":")) for c in catalog["cases"])
    path.write_text(text + "\n  ]\n}\n")


def stable_id(suite, rom, model, startup, variant):
    def escape(part):
        return "".join(f"%{ord(c):02x}" if c in "%|" or ord(c) < 32 else c for c in part)

    return "|".join(map(escape, (suite, rom, model, startup, variant)))


def read_catalog(path):
    catalog = json.loads(path.read_text())
    if catalog["schemaVersion"] != 1:
        raise ValueError("Unsupported catalog schema")
    defaults = catalog["defaults"]
    if (defaults["budget"] != {"value": 15, "unit": "RunFrameIntervals", "masterTicks": 2106720}
            or defaults["startup"] != "skip-bootrom" or defaults["colorCorrection"] or defaults["realRTC"]
            or defaults["dmg_palette_sampling"] != "NewValue" or not defaults["force_cgb_mode_for_color_models"]):
        raise ValueError("Unsupported Gambatte execution contract")
    models = set(re.findall(r"^\s+(\w+)\s*(?:,|\[\[)", (ROOT / "includes/Model.h").read_text(), re.M)) - {"Auto"}
    seen = set()
    for case in catalog["cases"]:
        if case["model"] not in models:
            raise ValueError(f"Unknown model: {case['model']}")
        expected = case["expected"]
        if not (re.fullmatch(r"out:[0-9A-F]{1,20}|audio:[01]", expected) or expected.endswith(".screen")):
            raise ValueError(f"Invalid expectation: {expected}")
        identity = stable_id(defaults["suite"], case["rom"], case["model"], defaults["startup"], case["variant"])
        if identity != case["id"] or identity in seen:
            raise ValueError(f"Invalid or duplicate case ID: {case['id']}")
        seen.add(identity)
    return catalog


def generate(catalog, template):
    rows = []
    for index, case in enumerate(catalog["cases"], 1):
        name = case["rom"].removeprefix("roms/gambatte/") + f" [{case['model'].lower()}]"
        fields = [json.dumps(case[k]) for k in ("id", "rom", "expected")]
        fields.extend((case["model"], json.dumps(name), str(index), json.dumps(",".join(case["tags"]))))
        rows.append("GAMBATTE_CASE(" + ", ".join(fields) + ")")
    marker = "#define GAMBATTE_EXPANSION_CASES"
    if template.count(marker) != 1:
        raise ValueError("Missing or duplicate catalog expansion marker")
    return template.replace(marker, "\n".join(rows))


def main():
    parser = argparse.ArgumentParser(description="Generate C++ cases from the canonical diagnostics catalog")
    parser.add_argument("catalog", type=Path)
    parser.add_argument("template", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(read_catalog(args.catalog), args.template.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)


if __name__ == "__main__":
    main()
