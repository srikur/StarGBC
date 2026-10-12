#!/usr/bin/env python3
import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path
import xml.etree.ElementTree as ET


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


def compare_reports(before, after):
    groups = {key: [] for key in ("newlyFailing", "newlyPassing", "unchangedFailing", "unchangedPassing",
                                  "missing", "added", "materiallyChanged")}
    for identity in sorted(before.keys() | after.keys()):
        old, new = before.get(identity), after.get(identity)
        if old is None:
            group = "added"
        elif new is None:
            group = "missing"
        elif (old["inputs"] != new["inputs"] or not all(old["inputs"].get(k) for k in ("rom", "reference", "configuration"))
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


def reproduce(bundle, executable, output):
    bundle = bundle.resolve()
    spec = json.loads((bundle / "reproduction.json").read_text())
    original = json.loads((bundle / "result.json").read_text())
    if spec["schemaVersion"] != 1 or spec["test"]["suite"] != "gambatte":
        raise ValueError("Unsupported reproduction contract")
    if (spec["intervals"] != 15 or not spec["noBootrom"] or spec["realRTC"]
            or spec["noAudio"] or spec["colorCorrection"] or spec["completion"] != "FixedInterval"):
        raise ValueError("Unsupported Gambatte reproduction configuration")
    for key, fingerprint in (("rom", "rom"), ("reference", "reference"), ("cartridgeSave", "cartridgeSave")):
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
    command = [str(executable.resolve()), "--reproduce-gambatte", str(bundle / spec["rom"]), spec["test"]["model"],
               expectation, spec["test"]["id"], str(output), spec["inputs"].get("cartridgeSave", "absent")]
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
    replay = sub.add_parser("reproduce")
    replay.add_argument("bundle", type=Path)
    replay.add_argument("--binary", type=Path, default=Path("build/release/Release/StarGBC_Tests"))
    replay.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.command == "import-legacy":
        records = legacy_baseline(args.xml, args.executable)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text("".join(json.dumps(r, separators=(",", ":")) + "\n" for r in records))
        print(json.dumps(records[-1]))
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
        report = reproduce(args.bundle, args.binary, args.output)
        print(json.dumps(report))
        return int(not report["reproduced"])
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(f"Diagnostics infrastructure error: {error}") from error
