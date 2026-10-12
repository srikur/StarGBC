import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('catalog', ROOT / 'scripts/catalog.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)


class Catalog(unittest.TestCase):
    def test_complete_inventory_preserves_legacy_names_and_explicit_exclusions(self):
        cases = catalog.catalogs()
        self.assertEqual(len(cases), 6257)
        legacy = [json.loads(line) for line in
                  (ROOT / 'tests/baselines/pre-diagnostics.jsonl').read_text().splitlines()]
        for suite in ('age', 'gbmicrotest', 'rom'):
            self.assertEqual(
                {row['name'] for row in legacy if row.get('record') == 'assertion' and row['suite'] == suite},
                {row['name'] for row in cases if row['suite'] == suite})
        result = catalog.audit()
        for key in ('duplicateCases', 'missingRoms', 'missingReferences', 'staleHashes', 'unknownApplicability',
                    'unexplainedRoms', 'staleInventory'):
            self.assertEqual(result[key], [], key)
        self.assertEqual(result['bundledRoms'], result['registeredRoms'] + len(result['unregisteredRoms']))

    def test_bank_aware_symbols_and_maps_do_not_invent_source_lines(self):
        symbols = catalog.read_symbols('; generated\n00:0150 start\n01:4000 sample\n02:4000 different\n', 'RGBDS.sym')
        mapping = dict(symbols=symbols, sourceLines=[])
        self.assertEqual(catalog.lookup(mapping, 1, 0x4000)['symbols'], ['sample'])
        self.assertEqual(catalog.lookup(mapping, 2, 0x4000)['symbols'], ['different'])
        self.assertEqual(catalog.lookup(mapping, 1, 0x4001)['symbols'], [])
        self.assertEqual(catalog.lookup(mapping, 1, 0x4000)['sourceLines'], [])
        self.assertEqual(
            catalog.read_symbols('ROMX bank #2:\n  SECTION: $4000-$4002 ($0003 bytes) ["code"]\n $4000 = different\n',
                                 'RGBDS.map'),
            [dict(bank=2, address=0x4000, name='different')])
        with self.assertRaises(ValueError):
            catalog.read_symbols('[labels]\n00:0150 start', 'RGBDS.sym')

    def test_provenance_rejects_changed_rom_symbol_and_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rom, symbols, source, manifest = (root / name for name in ('rom.gb', 'rom.sym', 'main.asm', 'mapping.json'))
            rom.write_bytes(bytes(32768))
            symbols.write_text('00:0150 start\n')
            source.write_text('start: nop\n')
            record = dict(schemaVersion=1, romHash=catalog.digest(rom), buildProvenance='test build fixture',
                          symbols=dict(path='rom.sym', sha256=catalog.digest(symbols), format='RGBDS.sym'))
            manifest.write_text(json.dumps(record))
            self.assertFalse(catalog.verified_mapping(manifest, rom)['capabilities']['exactSourceLines'])
            record['sourceLines'] = [
                dict(bank=0, begin=0x150, end=0x151, file='main.asm', line=1, sha256=catalog.digest(source))]
            manifest.write_text(json.dumps(record))
            self.assertTrue(catalog.verified_mapping(manifest, rom)['capabilities']['exactSourceLines'])
            for path in (rom, symbols, source):
                original = path.read_bytes()
                path.write_bytes(original + b'changed')
                with self.assertRaises(ValueError):
                    catalog.verified_mapping(manifest, rom)
                path.write_bytes(original)

    @unittest.skipUnless(os.environ.get('STARGBC_TEST_BINARY'), 'Native ROM runner integration')
    def test_subsystem_selection_accepts_every_declared_tag(self):
        binary = Path(os.environ['STARGBC_TEST_BINARY']).resolve()
        case = next(row for row in catalog.catalogs() if row['suite'] == 'rom' and 'apu' in row['tags'])
        with tempfile.TemporaryDirectory() as directory:
            for index, tag in enumerate(case['tags']):
                report = Path(directory) / str(index) / 'results.jsonl'
                process = subprocess.run([str(binary), '--case-id=' + case['id'], '--subsystem=' + tag, '--frames=1',
                                          '--results=' + str(report)], capture_output=True, text=True)
                self.assertEqual(process.returncode, 1, process.stderr)
                rows = [json.loads(line) for line in report.read_text().splitlines()]
                results = [row for row in rows if row.get('record') == 'result']
                self.assertEqual([row['test']['id'] for row in results], [case['id']])
                self.assertEqual(rows[-1]['record'], 'complete')

    @unittest.skipUnless(os.environ.get('STARGBC_TEST_BINARY'), 'Native ROM runner integration')
    def test_timeout_bundles_reproduce_each_remaining_suite(self):
        binary = Path(os.environ['STARGBC_TEST_BINARY']).resolve()
        cases = catalog.catalogs()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for suite in ('age', 'gbmicrotest', 'rom'):
                case = next(row for row in cases if row['suite'] == suite)
                report = root / suite / 'results.jsonl'
                process = subprocess.run(
                    [str(binary), '--case-id=' + case['id'], '--frames=1', '--results=' + str(report)],
                    capture_output=True)
                self.assertEqual(process.returncode, 1, process.stderr)
                rows = [json.loads(line) for line in report.read_text().splitlines()]
                result = next(row for row in rows if row.get('record') == 'result')
                self.assertEqual(result['outcome'], 'Timeout')
                process = subprocess.run(
                    ['python3', str(ROOT / 'scripts/diagnostics.py'), 'reproduce', result['artifacts'][0], '--binary',
                     str(binary)], capture_output=True, text=True)
                self.assertEqual(process.returncode, 0, process.stderr)
                self.assertTrue(json.loads(process.stdout)['reproduced'])

    @unittest.skipUnless(os.environ.get('STARGBC_TEST_BINARY'), 'Native ROM runner integration')
    def test_pixel_value_and_missing_input_failures_reproduce(self):
        binary = Path(os.environ['STARGBC_TEST_BINARY']).resolve()
        case = next(row for row in catalog.catalogs() if row.get('runner') == 'MicroProtocol')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rom = bytearray(32768)
            rom[0x100:0x103] = bytes.fromhex('C35001')
            program = bytes.fromhex('3E02E0803E01E0813EFFE082C35C01')
            rom[0x150:0x150 + len(program)] = program
            rom[0x14d] = (-sum(rom[0x134:0x14d]) - 25) & 255
            (root / 'rom.gb').write_bytes(rom)
            boot = bytearray(256)
            boot[:9] = bytes.fromhex('31FEFFAFE040C3FC00')
            boot[252:] = bytes.fromhex('3E01E050')
            (root / 'boot.bin').write_bytes(boot)
            (root / 'reference.screen').write_bytes(bytes([255]) * (160 * 144 * 4))
            pixel_case = next(row for row in catalog.catalogs() if row.get('runner') == 'RomScreen')
            for filename, outcome, selected, reference in (('rom.gb', 'AssertionFailure', case, ''),
                                                           ('missing.gb', 'InfrastructureError', case, ''),
                                                           ('rom.gb', 'AssertionFailure', pixel_case,
                                                            str(root / 'reference.screen'))):
                report = root / (outcome + selected['suite']) / 'results.jsonl'
                command = [str(binary), '--reproduce-catalog', str(root / filename), str(root / 'boot.bin'), reference,
                           selected['id'],
                           str(report), 'absent', '1', '1', '0', '0', str(2 ** 64 - 1), 'none']
                process = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(process.returncode, 1, process.stderr)
                result = next(json.loads(line) for line in report.read_text().splitlines() if
                              json.loads(line).get('record') == 'result')
                self.assertEqual(result['outcome'], outcome)
                if reference:
                    self.assertTrue((Path(result['artifacts'][0]) / 'difference.bmp').is_file())
                process = subprocess.run(
                    ['python3', str(ROOT / 'scripts/diagnostics.py'), 'reproduce', result['artifacts'][0], '--binary',
                     str(binary)], capture_output=True, text=True)
                self.assertEqual(process.returncode, 0, process.stderr)
                self.assertTrue(json.loads(process.stdout)['reproduced'])


if __name__ == '__main__':
    unittest.main()
