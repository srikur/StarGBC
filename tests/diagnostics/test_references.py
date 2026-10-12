import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('references', ROOT / 'scripts/compare-references.py')
references = importlib.util.module_from_spec(spec)
spec.loader.exec_module(references)


def capture(origin=6, overshoot=0):
    capabilities = dict(name='fixture', resolutionTicks=4, instructionValues=True, busValues=True, banks=True,
                        instructionPhase='opcode fetch', limitations=[])
    header = dict(romHash='rom', bootromHash='boot', masterTicksPerSecond=8388608, initialCartridgeRam='ZeroFilled',
                  rtc='VirtualZero', requestedTicks=1000, settings=dict(model='CGBC', noBootrom=False), capabilities=capabilities)
    events = [dict(kind='InstructionBegin', component='CPU', position=dict(tick=origin + tick), address=0x100 + index,
                   bank=0, pcBank=0, value=0) for index, tick in enumerate((0, 8, 16, 24))]
    samples = [dict(requestedTick=100, actualTick=100 + overshoot, overshootTicks=overshoot, registers=dict(pc=0x104), framebufferHash='screen')]
    return dict(header=header, samples=samples, events=events, complete=dict(actualTick=1000 + overshoot))


class References(unittest.TestCase):
    @unittest.skipUnless(os.environ.get('STARGBC_REFERENCE_BINARIES'), 'Optional pinned reference binaries')
    def test_cpu_executed_boot_alignment_and_callback_equivalence(self):
        binaries = Path(os.environ.get('STARGBC_REFERENCE_BINARIES')).resolve()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rom = bytearray(32768)
            rom[0x100:0x103] = bytes.fromhex('C35001')
            program = bytes.fromhex('3E55E0803EAAE081F08006080520FDC35001')
            rom[0x150:0x150 + len(program)] = program
            rom[0x14d] = (-sum(rom[0x134:0x14d]) - 25) & 255
            (root / 'fixture.gb').write_bytes(rom)
            boot = bytearray(256)
            boot[:9] = bytes.fromhex('31FEFFAFE040C3FC00')
            boot[252:] = bytes.fromhex('3E01E050')
            (root / 'boot.bin').write_bytes(boot)
            captures = []
            for name in ('StarGBC_Reference', 'StarGBC_GambatteReference', 'StarGBC_SameBoyReference'):
                base = root / name
                command = [str(binaries / name), str(root / 'fixture.gb'), '--model', 'DMGB', '--bios', str(root / 'boot.bin'),
                           '--ticks', '2003', '--sample-ticks', '91', '--output', str(base)]
                subprocess.run(command, check=True, capture_output=True)
                captures.append(references.read_capture(base))
                subprocess.run(command[:-1] + [str(base) + '-disabled', '--no-events'], check=True, capture_output=True)
                disabled = references.read_capture(str(base) + '-disabled')
                self.assertEqual(captures[-1]['samples'], disabled['samples'])
                self.assertGreaterEqual(captures[-1]['complete']['actualTick'], 2003)
            for capture in captures[1:]:
                with self.subTest(core=capture['header']['capabilities']['name']):
                    result = references.compare_captures(captures[0], capture)
                    self.assertEqual(result['status'], 'Equal', result)

    def test_opcode_alignment_preserves_relative_timing(self):
        left, right = capture(6), capture(0, 8)
        result = references.compare_captures(left, right)
        self.assertEqual(result['status'], 'Equal')
        self.assertEqual(result['alignment']['leftTick'], 6)
        self.assertEqual(result['alignment']['rightTick'], 0)
        right['events'][2]['position']['tick'] += 4
        result = references.compare_captures(left, right)
        self.assertEqual(result['status'], 'Divergence')
        self.assertEqual(result['firstRelativeTick'], 16)
        self.assertEqual(result['difference']['kind'], 'Mistimed')

    def test_unavailable_values_are_not_invented(self):
        left, right = capture(), capture()
        right['header']['capabilities']['instructionValues'] = False
        right['events'][1]['value'] = 255
        result = references.compare_captures(left, right)
        self.assertEqual(result['status'], 'Equal')
        self.assertNotIn('value', result['comparedFields'])

    def test_state_comparison_never_substitutes_requested_time_for_actual_time(self):
        result = references.compare_captures(capture(), capture(6, 8), projection='state')
        self.assertEqual(result['status'], 'Incompatible')
        result = references.compare_captures(capture(), capture(), projection='state')
        self.assertEqual(result['status'], 'Equal')
        self.assertEqual(result['comparedSamples'], 1)

    def test_missing_alignment_and_model_mismatches_are_explicit(self):
        left, right = capture(), capture()
        right['header']['settings']['model'] = 'CGBE'
        self.assertEqual(references.compare_captures(left, right)['status'], 'Incompatible')
        self.assertEqual(references.compare_captures(left, capture(), anchor_pc=0x200)['status'], 'Incompatible')


if __name__ == '__main__':
    unittest.main()
