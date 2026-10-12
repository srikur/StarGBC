import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('comparison', ROOT / 'scripts/compare-executions.py')
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)


def event(tick, value=1, address=0xFF41):
    return dict(position=dict(tick=tick, frame=0, dot=tick // 2, ly=0), sequence=tick, component='Bus', kind='BusRead', address=address, value=value)


def stream():
    header = dict(schemaVersion=1, record='observations', build='build-a', hardwareFormat='state-1', initial=dict(rom='rom', configuration='CGBC'),
                  replayHash='inputs', masterTicksPerSecond=8388608, plan=dict(beginTick=0, endTick=30, sampleTicks=10, events=True))
    samples = [dict(record='sample', state=dict(position=dict(tick=tick), hardwareHash='hardware', framebufferHash='pixels',
                    machine=dict(audio=dict(mixerLevelChanges=5))), eventHash='events') for tick in range(0, 31, 10)]
    return header, samples


class Comparison(unittest.TestCase):
    def test_identical_streams_and_earliest_transient_divergence(self):
        left, right = stream(), stream()
        self.assertEqual(comparison.compare_observations(left, right)['status'], 'Equal')
        right[1][1]['state']['framebufferHash'] = 'transient difference'
        result = comparison.compare_observations(left, right)
        self.assertEqual(result['status'], 'Divergence')
        self.assertEqual((result['lowerTick'], result['upperTick']), (0, 10))
        self.assertEqual(result['comparedSamples'], 2)
        self.assertEqual(left[1][-1], right[1][-1])

    def test_changed_events_are_detected_even_when_all_sampled_states_match(self):
        left, right = stream(), stream()
        right[1][2]['eventHash'] = 'transient bus write'
        result = comparison.compare_observations(left, right)
        self.assertEqual(result['upperTick'], 20)
        self.assertEqual(result['difference']['field'], 'eventHash')

    def test_missing_extra_mistimed_and_changed_values(self):
        baseline = [event(3), event(7), event(11)]
        self.assertEqual(comparison.compare_events(baseline, baseline), None)
        self.assertEqual(comparison.compare_events(baseline, [event(3), event(11)])['kind'], 'ExtraOnLeft')
        self.assertEqual(comparison.compare_events([event(3), event(11)], baseline)['kind'], 'MissingFromLeft')
        self.assertEqual(comparison.compare_events(baseline, [event(3), event(8), event(11)])['kind'], 'Mistimed')
        changed = comparison.compare_events(baseline, [event(3), event(7, 99), event(11)])
        self.assertEqual((changed['kind'], changed['tick']), ('ChangedValue', 7))
        self.assertEqual(comparison.compare_events(baseline, baseline + [event(20)])['kind'], 'MissingFromLeft')

    def test_different_models_inputs_and_resolution_are_incompatible(self):
        for key in ('rom', 'configuration'):
            left, right = stream(), stream()
            right[0]['initial'][key] = 'changed'
            self.assertEqual(comparison.compare_observations(left, right)['status'], 'Incompatible')
        left, right = stream(), stream()
        right[0]['plan']['sampleTicks'] = 11
        self.assertIn('plan.sampleTicks', comparison.compare_observations(left, right)['mismatches'])

    def test_private_state_and_diagnostic_bookkeeping_do_not_cross_builds(self):
        left, right = stream(), stream()
        right[0]['build'] = 'build-b'
        right[1][1]['state']['hardwareHash'] = 'different serialized layout'
        right[1][1]['state']['machine']['audio']['mixerLevelChanges'] = 99
        result = comparison.compare_observations(left, right)
        self.assertEqual(result['status'], 'Equal')
        self.assertFalse(result['hardwareCompared'])
        right[1][1]['state']['framebufferHash'] = 'different output'
        self.assertEqual(comparison.compare_observations(left, right)['status'], 'Divergence')

    def test_incomplete_or_reordered_capture_is_rejected(self):
        header, samples = stream()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'capture.jsonl'
            path.write_text('\n'.join(map(json.dumps, [header, *samples])))
            with self.assertRaisesRegex(ValueError, 'Incomplete'):
                comparison.read_observations(path)
            complete = dict(record='complete', tick=30, samples=4)
            path.write_text('\n'.join(map(json.dumps, [header, *samples, complete])))
            self.assertEqual(comparison.read_observations(path), (header, samples))
            samples[1], samples[2] = samples[2], samples[1]
            path.write_text('\n'.join(map(json.dumps, [header, *samples, complete])))
            with self.assertRaisesRegex(ValueError, 'ordered'):
                comparison.read_observations(path)


if __name__ == '__main__':
    unittest.main()
