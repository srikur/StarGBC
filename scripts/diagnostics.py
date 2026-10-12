#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET


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
    args = parser.parse_args()
    if args.command == "import-legacy":
        records = legacy_baseline(args.xml, args.executable)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text("".join(json.dumps(r, separators=(",", ":")) + "\n" for r in records))
        print(json.dumps(records[-1]))


if __name__ == "__main__":
    main()
