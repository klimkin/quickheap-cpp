#!/usr/bin/env python3
"""Verify pilot-driven repeat and warmup planning."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from repeat_plan import plan


def pilot(median, spread, trials=15):
    return [median * (1 + spread * ((index * 7) % trials - trials // 2) / trials) for index in range(trials)]


class RepeatPlanContract(unittest.TestCase):
    def test_noisier_pilot_needs_more_repeats(self):
        tight, moderate, noisy = (plan(pilot(20000, spread), pilot(20000, 0.001)).repeat
                                  for spread in (0.001, 0.04, 0.08))
        self.assertEqual(tight, 5)
        self.assertLess(5, moderate)
        self.assertLess(moderate, noisy)
        self.assertLess(noisy, 1000)

    def test_noisier_side_decides_shared_plan(self):
        quiet, loud = pilot(20000, 0.001), pilot(30000, 0.06)
        self.assertEqual(plan(quiet, loud)[:2], plan(loud, quiet)[:2])
        self.assertGreater(plan(quiet, loud).repeat, plan(quiet, quiet).repeat)

    def test_slow_trials_cap_timed_budget(self):
        noisy = pilot(1e8, 0.5)
        self.assertEqual(plan(noisy, noisy).repeat, 20)
        self.assertEqual(plan(noisy, noisy, max_timed_ns=1e8).repeat, 5)
        self.assertEqual(plan(pilot(100, 0.5), pilot(100, 0.5)).repeat, 1000)

    def test_warmup_covers_requested_time(self):
        result = plan(pilot(16000, 0.01), pilot(21000, 0.01))
        slowest = max(result.cpp_median_ns, result.rust_median_ns)
        self.assertGreaterEqual(result.warmup * slowest, 10e6)
        self.assertLess((result.warmup - 1) * slowest, 10e6)
        self.assertEqual(plan(pilot(5e7, 0.01), pilot(5e7, 0.01)).warmup, 1)
        self.assertEqual(plan(pilot(100, 0.01), pilot(100, 0.01)).warmup, 1000)

    def test_same_pilot_same_plan(self):
        cpp, rust = pilot(20000, 0.05), pilot(25000, 0.03)
        self.assertEqual(plan(cpp, rust), plan(list(cpp), list(rust)))

    def test_rejects_invalid_pilots(self):
        good = pilot(20000, 0.01)
        for bad in ([], good[:2], good[:-1] + [0], good[:-1] + [-5], good[:-1] + [float("nan")], good[:-1]):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                plan(good, bad)

    def test_cli_reads_runner_csv(self):
        with tempfile.TemporaryDirectory() as temp:
            trials = {side: [round(value) for value in pilot(median, 0.02)] for side, median in (("cpp", 16000), ("rust", 18000))}
            paths = []
            for side, values in trials.items():
                path = Path(temp) / f"{side}.csv"
                path.write_text("repeat,nanos,repeats\n" + "".join(f"{i},{value},15\n" for i, value in enumerate(values)))
                paths.append(path)
            script = Path(__file__).with_name("repeat_plan.py")
            command = [sys.executable, str(script), "--cpp", str(paths[0]), "--rust", str(paths[1])]
            fields = subprocess.run(command, check=True, text=True, capture_output=True).stdout.rstrip("\n").split("\t")
            self.assertEqual(len(fields), 7)
            self.assertEqual(fields[2], "15")
            self.assertEqual((int(fields[0]), int(fields[1])), plan(trials["cpp"], trials["rust"])[:2])
            paths[1].write_text("repeat,nanos\n0,0\n")
            failed = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(failed.returncode, 2)
            self.assertIn("error:", failed.stderr)


if __name__ == "__main__":
    unittest.main()
