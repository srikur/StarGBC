#!/usr/bin/env python3
import argparse
import base64
import hashlib
import json
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET
from pathlib import Path


def read_report(path):
    rows = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    if not rows or rows[0].get("record") != "run" or rows[0].get("schemaVersion") != 1:
        raise ValueError(f"Unsupported report: {path}")
    if rows[-1].get("record") != "complete":
        raise ValueError(f"Incomplete report: {path}")
    cases = {}
    for row in rows:
        if row.get("record") != "result":
            continue
        identity = row["test"]["id"]
        if row.get("schemaVersion") != 1 or identity in cases:
            raise ValueError(f"Unsupported or duplicate result: {identity}")
        cases[identity] = row
    return rows, cases


def export_inspector(bundle, output):
    bundle = bundle.resolve()
    assets = Path(__file__).resolve().parents[1] / "diagnostics/inspector"
    texts, images = {}, {}
    for name in ("result.json", "reproduction.json", "terminal.json", "pixels.json", "provenance.json", "events.jsonl"):
        path = bundle / name
        if path.is_file():
            if path.resolve().parent != bundle:
                raise ValueError(f"Report inputs must be bundled files: {name}")
            texts[name] = path.read_text()
    if "result.json" not in texts:
        raise ValueError("A failure bundle must contain result.json")
    for name in ("actual.bmp", "expected.bmp", "difference.bmp"):
        path = bundle / name
        if path.is_file():
            if path.resolve().parent != bundle:
                raise ValueError(f"Report images must be bundled files: {name}")
            images[name] = "data:image/bmp;base64," + base64.b64encode(path.read_bytes()).decode("ascii")
    payload = json.dumps(dict(texts=texts, images=images), separators=(",", ":")).replace("<", "\\u003c")
    html = (assets / "index.html").read_text()
    html = html.replace('<link rel="stylesheet" href="style.css">',
                        '<style>' + (assets / "style.css").read_text() + '</style>')
    html = html.replace('<script id="embedded-bundle" type="application/json"></script>',
                        '<script id="embedded-bundle" type="application/json">' + payload + '</script>')
    html = html.replace('<script src="app.js"></script>', '<script>' + (assets / "app.js").read_text() + '</script>')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(html)
    return dict(report=str(output.resolve()), files=sorted(texts.keys() | images.keys()))


def compare_reports(before, after):
    groups = {key: [] for key in ("newlyFailing", "newlyPassing", "unchangedFailing", "unchangedPassing",
                                  "missing", "added", "materiallyChanged")}
    for identity in sorted(before.keys() | after.keys()):
        old, new = before.get(identity), after.get(identity)
        if old is None:
            group = "added"
        elif new is None:
            group = "missing"
        elif (old["inputs"] != new["inputs"] or not all(
                old["inputs"].get(k) for k in ("rom", "reference", "configuration"))
              or old["budgetTicks"] != new["budgetTicks"] or old["test"]["model"] != new["test"]["model"]
              or old["test"]["startup"] != new["test"]["startup"]
              or old["test"]["expectationVariant"] != new["test"]["expectationVariant"]):
            group = "materiallyChanged"
        elif old["outcome"] == "Pass" and new["outcome"] != "Pass":
            group = "newlyFailing"
        elif old["outcome"] != "Pass" and new["outcome"] == "Pass":
            group = "newlyPassing"
        else:
            group = "unchangedPassing" if new["outcome"] == "Pass" else "unchangedFailing"
        groups[group].append(identity)
    return {"schemaVersion": 1, "counts": {k: len(v) for k, v in groups.items()}, **groups}


def reproduce(bundle, executable, output, trace_capacity=0, trace_begin=0, trace_end=2 ** 64 - 1, provenance_line=None):
    bundle = bundle.resolve()
    spec = json.loads((bundle / "reproduction.json").read_text())
    original = json.loads((bundle / "result.json").read_text())
    if spec["schemaVersion"] != 1 or spec["test"]["suite"] not in ("gambatte", "age", "gbmicrotest", "rom"):
        raise ValueError("Unsupported reproduction contract")
    if spec["test"]["suite"] == "gambatte" and (spec["intervals"] != 15 or not spec["noBootrom"] or spec["realRTC"]
                                                or spec["noAudio"] or spec["colorCorrection"] or spec[
                                                    "completion"] != "FixedInterval"):
        raise ValueError("Unsupported Gambatte reproduction configuration")
    for key, fingerprint in (("rom", "rom"), ("bootrom", "bootrom"), ("reference", "reference"),
                             ("cartridgeSave", "cartridgeSave")):
        name = spec[key]
        if not name:
            continue
        path = bundle / name
        if path.resolve().parent != bundle:
            raise ValueError("Reproduction inputs must be bundled files")
        expected = spec["inputs"].get(fingerprint, "")
        if not expected:
            if path.exists():
                raise ValueError(f"Expected missing input now exists: {name}")
        elif hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Reproduction input hash mismatch: {name}")
    if output is None:
        output = bundle / "reruns" / str(time.time_ns()) / "results.jsonl"
    output = output.resolve()
    expectation = str(bundle / spec["reference"]) if spec["reference"] else spec["expectation"]
    if provenance_line is not None:
        if not 0 <= provenance_line <= 143:
            raise ValueError("Invalid provenance line")
        trace_capacity = trace_capacity or 65536
    if trace_capacity and (not 1 <= trace_capacity <= 1_000_000 or not 0 <= trace_begin <= trace_end < 2 ** 64):
        raise ValueError("Invalid trace capacity or interval")
    if spec["test"]["suite"] == "gambatte":
        command = [str(executable.resolve()), "--reproduce-gambatte", str(bundle / spec["rom"]), spec["test"]["model"],
                   expectation, spec["test"]["id"], str(output), spec["inputs"].get("cartridgeSave", "absent")]
        if trace_capacity:
            command.extend(map(str, (trace_capacity, trace_begin, trace_end)))
            if provenance_line is not None:
                command.append(str(provenance_line))
        sampling = spec.get("dmg_palette_sampling", "BitwiseOr")
        if sampling not in ("BitwiseOr", "NewValue"):
            raise ValueError("Unsupported DMG palette sampling setting")
        command.append(f"--dmg-palette-sampling={sampling}")
    else:
        if (spec["noBootrom"] or spec["realRTC"] or spec["noAudio"] or not spec["colorCorrection"]
                or not 1 <= spec["intervals"] <= 1000000):
            raise ValueError("Unsupported catalog reproduction configuration")
        command = [str(executable.resolve()), "--reproduce-catalog", str(bundle / spec["rom"]),
                   str(bundle / spec["bootrom"]) if spec["bootrom"] else "",
                   str(bundle / spec["reference"]) if spec["reference"] else "", spec["test"]["id"], str(output),
                   spec["inputs"].get("cartridgeSave", "absent"), str(spec["intervals"]),
                   str(int(spec["fixedIntervals"])),
                   str(trace_capacity), str(trace_begin), str(trace_end),
                   str(provenance_line) if provenance_line is not None else "none"]
    process = subprocess.run(command, check=False, capture_output=True, text=True)
    if process.returncode not in (0, 1):
        raise ValueError(f"Reproduction process failed: {process.stderr}")
    _, cases = read_report(output)
    actual = cases[spec["test"]["id"]]
    keys = ("outcome", "completion", "actual")
    matched = all(actual[k] == original[k] for k in keys)
    result = dict(schemaVersion=1, reproduced=matched, original={k: original[k] for k in keys},
                  rerun={k: actual[k] for k in keys}, results=str(output))
    output.with_name("reproduction-check.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def run_selected(args):
    identities = set(args.case_id)
    if args.failed_from:
        _, previous = read_report(args.failed_from)
        identities.update(identity for identity, result in previous.items() if result["outcome"] != "Pass")
        if not identities:
            print("No previous failures to rerun")
            return 0
    command = [str(args.binary.resolve()), f"--max-threads={args.workers}", f"--results={args.output.resolve()}"]
    if args.provenance_line is not None:
        command.append(f"--provenance-line={args.provenance_line}")
    if args.model:
        command.append(f"--model={args.model}")
    if args.subsystem:
        command.append(f"--subsystem={args.subsystem}")
    with tempfile.TemporaryDirectory(prefix="stargbc-selection-") as directory:
        if identities:
            path = Path(directory) / "cases.txt"
            path.write_text("".join(identity + "\n" for identity in sorted(identities)))
            command.append(f"--case-file={path}")
        process = subprocess.run(command, check=False)
    if args.diagnostic_rerun:
        _, results = read_report(args.output)
        reruns = []
        for result in results.values():
            if result["outcome"] != "Pass":
                reruns.append(
                    reproduce(Path(result["artifacts"][0]), args.binary, None, args.trace_capacity, args.trace_begin,
                              args.trace_end, args.provenance_line))
        args.output.with_name("diagnostic-reruns.json").write_text(json.dumps(reruns, indent=2) + "\n")
    return process.returncode


def legacy_baseline(xml_path, executable):
    root = ET.parse(xml_path).getroot()
    summary = root.find("OverallResultsTestCases")
    if summary is None:
        raise ValueError("Incomplete doctest run")
    records = [{
        "record": "run", "schemaVersion": 1,
        "executableHash": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "format": "legacy-doctest-assertions", "masterTicksPerSecond": 8388608,
        "runFrameTicks": 140448,
    }]
    for suite in root.findall("TestSuite"):
        for case in suite.findall("TestCase"):
            if case.get("skipped") == "true":
                continue
            result = case.find("OverallResultsAsserts")
            if result is None:
                raise ValueError(f"Missing outcome: {case.get('name')}")
            records.append({
                "record": "assertion", "name": case.get("name"),
                "suite": suite.get("name", ""),
                "outcome": "Pass" if result.get("test_case_success") == "true" else "AssertionFailure",
                "assertions": int(result.get("successes")) + int(result.get("failures")),
                "failures": int(result.get("failures")),
            })
    records.append({"record": "complete", "cases": len(records) - 1,
                    "failed": sum(r.get("outcome") == "AssertionFailure" for r in records)})
    return records


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    legacy = sub.add_parser("import-legacy")
    legacy.add_argument("xml", type=Path)
    legacy.add_argument("executable", type=Path)
    legacy.add_argument("output", type=Path)
    comparison = sub.add_parser("compare")
    comparison.add_argument("before", type=Path)
    comparison.add_argument("after", type=Path)
    comparison.add_argument("--output", type=Path)
    inspector = sub.add_parser("inspect")
    inspector.add_argument("bundle", type=Path)
    inspector.add_argument("--output", type=Path, required=True)
    replay = sub.add_parser("reproduce")
    replay.add_argument("bundle", type=Path)
    replay.add_argument("--binary", type=Path, default=Path("build/release/Release/StarGBC_Tests"))
    replay.add_argument("--output", type=Path)
    run = sub.add_parser("run")
    run.add_argument("--binary", type=Path, default=Path("build/release/Release/StarGBC_Tests"))
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--workers", type=int, default=4)
    run.add_argument("--failed-from", type=Path)
    run.add_argument("--case-id", action="append", default=[])
    run.add_argument("--model")
    run.add_argument("--subsystem")
    run.add_argument("--diagnostic-rerun", action="store_true")
    for traced in (replay, run):
        traced.add_argument("--trace-capacity", type=int, default=65536 if traced is run else 0)
        traced.add_argument("--trace-begin", type=int, default=0)
        traced.add_argument("--trace-end", type=int, default=2 ** 64 - 1)
        traced.add_argument("--provenance-line", type=int, choices=range(144))
    args = parser.parse_args()
    if args.command == "import-legacy":
        records = legacy_baseline(args.xml, args.executable)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text("".join(json.dumps(r, separators=(",", ":")) + "\n" for r in records))
        print(json.dumps(records[-1]))
    elif args.command == "inspect":
        print(json.dumps(export_inspector(args.bundle, args.output)))
    elif args.command == "compare":
        _, before = read_report(args.before)
        _, after = read_report(args.after)
        if not before or not after:
            raise ValueError("Comparison requires structured result records")
        report = compare_reports(before, after)
        if args.output:
            args.output.write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report["counts"]))
        return int(bool(report["newlyFailing"] or report["missing"] or report["materiallyChanged"]))
    elif args.command == "reproduce":
        report = reproduce(args.bundle, args.binary, args.output, args.trace_capacity, args.trace_begin, args.trace_end,
                           args.provenance_line)
        print(json.dumps(report))
        return int(not report["reproduced"])
    elif args.command == "run":
        return run_selected(args)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(f"Diagnostics infrastructure error: {error}") from error
