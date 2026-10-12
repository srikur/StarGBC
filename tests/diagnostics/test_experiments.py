import copy
import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('experiments', ROOT / 'scripts/experiments.py')
experiments = importlib.util.module_from_spec(spec)
spec.loader.exec_module(experiments)


def failure():
    return dict(outcome='AssertionFailure', failureSignature='TimerReload:probe-value:37!=17',
                completionContract='ComponentBudgetAndRequiredObservations', checks=[],
                scenario=dict(mode='Component', family='TimerReload', model='DMGB', doubleSpeed=False,
                              expectedValue=17, basis='Hypothesis', basisEvidence='Suspected reload behavior',
                              budgetTicks=32))


class Experiments(unittest.TestCase):
    def test_reduction_rejects_timeout_changed_signature_or_completion(self):
        original = failure()
        self.assertTrue(experiments.preserves_failure(original, copy.deepcopy(original)))
        for key, value in (('outcome', 'Timeout'), ('failureSignature', 'TimerReload:probe-value:193!=17'),
                           ('completionContract', 'SomeOtherCompletion')):
            candidate = copy.deepcopy(original)
            candidate[key] = value
            self.assertFalse(experiments.preserves_failure(original, candidate))
        candidate = copy.deepcopy(original)
        candidate['scenario']['budgetTicks'] = 1
        self.assertFalse(experiments.preserves_failure(original, candidate))
        candidate = copy.deepcopy(original)
        candidate['checks'] = [dict(invariant='cpu-action-timing', passed=False)]
        self.assertFalse(experiments.preserves_failure(original, candidate))

    def test_promotion_requires_evidence_and_completed_expectations(self):
        result = failure()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'regression.json'
            with self.assertRaises(ValueError):
                experiments.promote(result['scenario'], result, 'hypothesis', path)
            result['outcome'] = 'Pass'
            with self.assertRaises(ValueError):
                experiments.promote(result['scenario'], result, 'hypothesis', path)
            result['scenario']['basis'] = 'InternalInvariant'
            with self.assertRaises(ValueError):
                experiments.promote(result['scenario'], result, '', path)

    @unittest.skipUnless(os.environ.get('STARGBC_EXPERIMENT_BINARY'), 'Native experiment integration')
    def test_phase_sweep_crosses_reload_and_reduction_preserves_the_same_failure(self):
        binary = Path(os.environ['STARGBC_EXPERIMENT_BINARY'])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scenario = dict(schemaVersion=1, mode='Component', family='TimerReload', model='DMGB', seed=1, phase=1,
                            noise=[7, 8, 9], expectedValue=17, basis='Hypothesis',
                            basisEvidence='Intentional competing timing hypothesis')
            index = 0

            def execute(candidate):
                nonlocal index
                path = root / str(index)
                index += 1
                return experiments.execute(candidate, binary, path)

            reduced = experiments.reduce_scenario(scenario, execute)
            self.assertEqual(reduced['reducedScenario']['noise'], [])
            self.assertEqual(reduced['reducedScenario']['phase'], 1)
            self.assertTrue(experiments.preserves_failure(reduced['original'], reduced['reducedFailure']))
            self.assertTrue(
                any(not trial['accepted'] and trial['scenario']['phase'] == 0 for trial in reduced['trials']))
            for double_speed in (False, True):
                records = []
                for phase in (0, 1, 2):
                    records.append(execute(
                        dict(mode='Component', family='TimerReload', model='CGBC', doubleSpeed=double_speed,
                             phase=phase)))
                self.assertEqual(records[0]['probeValue'], records[2]['probeValue'])
                self.assertEqual(records[1]['probeValue'], 37)
                self.assertNotEqual(records[0]['probeValue'], records[1]['probeValue'])


if __name__ == '__main__':
    unittest.main()
