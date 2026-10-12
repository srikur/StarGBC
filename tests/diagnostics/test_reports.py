import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("diagnostics", ROOT / "scripts/diagnostics.py")
diagnostics = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diagnostics)


def case(identity, outcome="Pass", reference="ref"):
    return dict(schemaVersion=1, record="result", test=dict(id=identity, model="DMGB", startup="skip", expectationVariant="screen"),
                inputs=dict(rom="rom", reference=reference, configuration="config"), budgetTicks=100, outcome=outcome)


class Reports(unittest.TestCase):
    def test_regression_detected_despite_improved_pass_count(self):
        before = {k: case(k, outcome) for k, outcome in (("regression", "Pass"), ("fixed1", "AssertionFailure"),
                                                        ("fixed2", "AssertionFailure"), ("missing", "Pass"),
                                                        ("changed", "AssertionFailure"), ("known", "Timeout"))}
        after = {k: case(k, outcome) for k, outcome in (("regression", "AssertionFailure"), ("fixed1", "Pass"),
                                                       ("fixed2", "Pass"), ("added", "Pass"), ("known", "Timeout"))}
        after["changed"] = case("changed", "Pass", "new-expectation")
        result = diagnostics.compare_reports(before, after)
        self.assertEqual(result["newlyFailing"], ["regression"])
        self.assertEqual(result["newlyPassing"], ["fixed1", "fixed2"])
        self.assertEqual(result["missing"], ["missing"])
        self.assertEqual(result["added"], ["added"])
        self.assertEqual(result["materiallyChanged"], ["changed"])
        self.assertEqual(result["unchangedFailing"], ["known"])

    def test_incomplete_and_duplicate_results_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.jsonl"
            rows = [dict(record="run", schemaVersion=1), case("a")]
            path.write_text("\n".join(map(json.dumps, rows)))
            with self.assertRaisesRegex(ValueError, "Incomplete"):
                diagnostics.read_report(path)
            rows.extend([case("a"), dict(record="complete")])
            path.write_text("\n".join(map(json.dumps, rows)))
            with self.assertRaisesRegex(ValueError, "duplicate"):
                diagnostics.read_report(path)


if __name__ == "__main__":
    unittest.main()
