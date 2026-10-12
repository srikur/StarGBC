#!/usr/bin/env python3
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description='Verify catalog ROMs by rebuilding with the pinned upstream assembler')
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    catalog = json.loads((ROOT / 'tests/catalog/gambatte.json').read_text())
    assembler_path = args.source / 'test/qdgbas.py'
    if hashlib.sha256(assembler_path.read_bytes()).hexdigest() != catalog['defaults']['assemblerHash']:
        raise ValueError('Assembler differs from the pinned catalog provenance')
    spec = importlib.util.spec_from_file_location('gambatte_assembler', assembler_path)
    assembler = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(assembler)
    records = {}
    for case in catalog['cases']:
        if case['rom'] in records:
            continue
        source = args.source / case['sourceFile']
        if hashlib.sha256(source.read_bytes()).hexdigest() != case['sourceHash']:
            raise ValueError('Source changed: ' + case['sourceFile'])
        rebuilt = assembler.assembleFile(assembler.readDataFromFile(source))
        rebuilt_hash = hashlib.sha256(rebuilt).hexdigest()
        records[case['rom']] = dict(sourceFile=case['sourceFile'], sourceHash=case['sourceHash'],
                                    rebuiltHash=rebuilt_hash,
                                    romHash=case['romHash'], verified=rebuilt_hash == case['romHash'])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(dict(schemaVersion=1, revision=catalog['defaults']['sourceRevision'], roms=records),
                   indent=2) + '\n')
    print(f'{sum(row["verified"] for row in records.values())}/{len(records)} ROMs rebuilt identically')
    return int(not all(row['verified'] for row in records.values()))


if __name__ == '__main__':
    raise SystemExit(main())
