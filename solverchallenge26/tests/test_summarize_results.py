#!/usr/bin/env python3
"""Regression tests for the user-facing timing summary."""

import importlib.util
from pathlib import Path
import statistics
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "summarize_results", ROOT / "scripts/summarize_results.py"
)
SUMMARY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUMMARY)


def make_log(path, diagnostic_line):
    raw_seconds = [0.90, 0.92, 0.94, 0.96, 0.98, 1.00, 1.02, 1.04, 1.06, 1.08]
    lines = []
    for trial, raw in enumerate(raw_seconds):
        lines.extend(
            [
                f"TRIAL trial={trial} raw_seconds={raw:.12f}",
                (
                    "XianzhiKrylov status=1 converged=1 iterations=7 "
                    f"prepare_solve_seconds={raw - 0.001:.12f}"
                ),
                diagnostic_line,
            ]
        )
    lines.append("VERIFICATION relative_l2=1e-9 threshold=1e-8 PASS")
    path.write_text("\n".join(lines) + "\n")
    return statistics.mean(raw_seconds[5:])


class SummaryTimingTests(unittest.TestCase):
    def test_reused_t04_transfer_statistic_is_not_subtracted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "T04_q1.log"
            expected = make_log(
                path, "excluded_input_transfer_seconds=2.5551936085"
            )
            result = SUMMARY.summarize_log(path)
            self.assertAlmostEqual(result["time_ms"], expected * 1000.0, places=6)

    def test_reused_t07_conversion_statistic_is_not_subtracted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "T07_q1.log"
            expected = make_log(
                path, "excluded_format_conversion_seconds=0.018459"
            )
            result = SUMMARY.summarize_log(path)
            self.assertAlmostEqual(result["time_ms"], expected * 1000.0, places=6)

    def test_legacy_aggregate_timing_still_falls_back(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "T01_q1.log"
            path.write_text(
                "COMPETITION_TIME_SECONDS=0.123456\n"
                "VERIFICATION relative_l2=1e-9 threshold=1e-8 PASS\n"
            )
            result = SUMMARY.summarize_log(path)
            self.assertEqual(result["time_ms"], 123.456)


if __name__ == "__main__":
    unittest.main()
