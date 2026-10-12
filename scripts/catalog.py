#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def catalogs(root=ROOT):
    result = []
    for path in sorted((root / 'tests/catalog').glob('*.json')):
        catalog = json.loads(path.read_text())
        if 'cases' not in catalog:
            continue
        for case in catalog['cases']:
            result.append(dict(catalog['defaults'], **case, catalog=str(path.relative_to(root))))
    return result


def audit(root=ROOT):
    cases = catalogs(root)
    problems = dict(duplicateCases=[], missingRoms=[], missingReferences=[], staleHashes=[], unknownApplicability=[],
                    unknownSources=[])
    seen = set()
    registered = set()
    for case in cases:
        identity = case['id']
        if identity in seen:
            problems['duplicateCases'].append(identity)
        seen.add(identity)
        registered.add(case['rom'])
        if not case.get('model') or case['model'] == 'Auto':
            problems['unknownApplicability'].append(identity)
        if not case.get('sourceRevision') or not case.get('sourceFile'):
            problems['unknownSources'].append(identity)
        expected = case.get('expected', '')
        for name, fingerprint, missing in ((case['rom'], case.get('romHash'), 'missingRoms'),
                                           (expected if expected.endswith(('.screen', '.vram')) else '',
                                            case.get('referenceHash'), 'missingReferences'),
                                           (case.get('bios', ''), case.get('bootromHash'), 'missingReferences')):
            if not name:
                continue
            path = root / name
            if not path.is_file():
                problems[missing].append(dict(id=identity, path=name))
            elif fingerprint and digest(path) != fingerprint:
                problems['staleHashes'].append(dict(id=identity, path=name))
    bundled = {str(path.relative_to(root)) for path in (root / 'roms').rglob('*') if
               path.suffix.lower() in ('.gb', '.gbc')}
    inventory_path = root / 'tests/catalog/unregistered.json'
    accounted = {row['rom']: row for row in
                 json.loads(inventory_path.read_text())['roms']} if inventory_path.exists() else {}
    unregistered = sorted(bundled - registered)
    unexplained = [path for path in unregistered if path not in accounted or not accounted[path].get('reason')]
    stale_inventory = [path for path, entry in accounted.items() if
                       path not in bundled or path in registered or digest(root / path) != entry['romHash']]
    return dict(schemaVersion=1, cases=len(cases), registeredRoms=len(registered), bundledRoms=len(bundled),
                unregisteredRoms=unregistered, unregisteredUnknownApplicability=[path for path in unregistered if
                                                                                 not accounted.get(path, {}).get(
                                                                                     'applicability')],
                unexplainedRoms=unexplained, staleInventory=stale_inventory, **problems)


def read_symbols(text, format):
    result = []
    bank = None
    for line in text.splitlines():
        line = line.split(';', 1)[0].strip()
        if not line:
            continue
        if format == 'RGBDS.sym':
            match = re.fullmatch(r'([0-9a-fA-F]+):([0-9a-fA-F]{4})\s+(\S+)', line)
            if not match:
                raise ValueError('Not an RGBDS symbol record: ' + line)
            result.append(dict(bank=int(match[1], 16), address=int(match[2], 16), name=match[3]))
        elif format == 'RGBDS.map':
            heading = re.fullmatch(r'(ROM0|ROMX|VRAM|SRAM|WRAM0|WRAMX|OAM|HRAM) bank #(\d+):', line)
            if heading:
                bank = int(heading[2])
            symbol = re.fullmatch(r'\$([0-9a-fA-F]{4}) = (.+)', line)
            if symbol:
                if bank is None:
                    raise ValueError('Map symbol without a bank')
                result.append(dict(bank=bank, address=int(symbol[1], 16), name=symbol[2]))
        else:
            raise ValueError('Unsupported symbol format')
    if not result:
        raise ValueError('No symbols found')
    return sorted(result, key=lambda item: (item['bank'], item['address'], item['name']))


def verified_mapping(manifest_path, rom_path):
    manifest = json.loads(manifest_path.read_text())
    if manifest.get('schemaVersion') != 1 or not manifest.get('buildProvenance') or manifest['romHash'] != digest(
            rom_path):
        raise ValueError('Mapping build provenance does not match the ROM')
    path = manifest_path.parent / manifest['symbols']['path']
    if digest(path) != manifest['symbols']['sha256']:
        raise ValueError('Symbol file hash mismatch')
    symbols = read_symbols(path.read_text(), manifest['symbols']['format'])
    lines = manifest.get('sourceLines', [])
    for line in lines:
        path = manifest_path.parent / line['file']
        if digest(path) != line['sha256'] or not 0 <= line['begin'] < line['end'] <= 65536 or not 0 < line[
            'line'] <= len(path.read_text().splitlines()):
            raise ValueError('Invalid or stale source-line mapping')
    return dict(romHash=manifest['romHash'], buildProvenance=manifest['buildProvenance'], symbols=symbols,
                sourceLines=lines,
                capabilities=dict(symbols=True, exactSourceLines=bool(lines)))


def lookup(mapping, bank, address):
    candidates = [symbol for symbol in mapping['symbols'] if symbol['bank'] == bank and symbol['address'] == address]
    lines = [line for line in mapping['sourceLines'] if line['bank'] == bank and line['begin'] <= address < line['end']]
    return dict(bank=bank, address=address, symbols=[symbol['name'] for symbol in candidates], sourceLines=lines)


def main():
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest='command', required=True)
    audit_command = commands.add_parser('audit')
    audit_command.add_argument('--root', type=Path, default=ROOT)
    explain = commands.add_parser('explain')
    explain.add_argument('case_id')
    binding = commands.add_parser('bind-build')
    binding.add_argument('rom', type=Path)
    binding.add_argument('rebuilt_rom', type=Path)
    binding.add_argument('symbols', type=Path)
    binding.add_argument('--format', choices=('RGBDS.sym', 'RGBDS.map'), default='RGBDS.sym')
    binding.add_argument('--build-provenance', required=True)
    symbols = commands.add_parser('symbols')
    symbols.add_argument('manifest', type=Path)
    symbols.add_argument('rom', type=Path)
    symbols.add_argument('--bank', type=lambda value: int(value, 0), required=True)
    symbols.add_argument('--address', type=lambda value: int(value, 0), required=True)
    symbols.add_argument('--trace', type=Path)
    for command in (audit_command, explain, symbols, binding):
        command.add_argument('--output', type=Path)
    args = parser.parse_args()
    failed = False
    if args.command == 'audit':
        result = audit(args.root)
        failed = any(result[key] for key in
                     ('duplicateCases', 'missingRoms', 'missingReferences', 'staleHashes', 'unknownApplicability',
                      'unexplainedRoms', 'staleInventory'))
    elif args.command == 'explain':
        result = next((case for case in catalogs() if case['id'] == args.case_id), None)
        if result is None:
            raise ValueError('Unknown case ID')
    elif args.command == 'bind-build':
        if not args.output or digest(args.rom) != digest(args.rebuilt_rom):
            raise ValueError('Binding requires an output manifest and a byte-identical rebuilt ROM')
        read_symbols(args.symbols.read_text(), args.format)
        import os
        result = dict(schemaVersion=1, romHash=digest(args.rom), buildProvenance=args.build_provenance,
                      symbols=dict(path=os.path.relpath(args.symbols.resolve(), args.output.resolve().parent),
                                   sha256=digest(args.symbols), format=args.format))
    else:
        mapping = verified_mapping(args.manifest, args.rom)
        result = dict(schemaVersion=1, capabilities=mapping['capabilities'],
                      location=lookup(mapping, args.bank, args.address))
        if args.trace:
            rows = [json.loads(line) for line in args.trace.read_text().splitlines()]
            if rows[0].get('record') != 'trace' or rows[0].get('romHash') != mapping['romHash']:
                raise ValueError('Trace ROM differs from the verified mapping')
            result['events'] = [dict(event=event, location=lookup(mapping, event['pcBank'], event['pc'])) for event in
                                rows[1:]]
    encoded = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded)
        print(json.dumps(dict(output=str(args.output), valid=not failed)))
    else:
        print(encoded, end='')
    return int(failed)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(f'Catalog infrastructure error: {error}') from error
