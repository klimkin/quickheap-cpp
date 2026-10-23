#!/usr/bin/env python3
import argparse
import csv
import io
from pathlib import Path
import tempfile
import unittest

from measure_parity import Case, integers, load_cases, parse_trials, summarize, validate_trace


class MeasurementContract(unittest.TestCase):
    def setUp(self):
        self.case = Case("HeapSort", 8, 1, "i64", Path("heap.qhf"), "fnv1a64:0123456789abcdef", "hash")

    def rows(self, side="cpp"):
        return [{"elem": "i64", "heap": "quickheap_avx2" if side == "rust" else "quickheap",
                 "n": "8", "workload": "HeapSort", "ops": "8", "repeat": str(index), "nanos": str(100 + index),
                 "protocol": "quickheap-cpp-v1", "build": ("rustc 1.0 source=hash" if side == "rust" else "GNU-15/Release/source=hash"),
                 "backend": "rust-production-avx2" if side == "rust" else "AVX2",
                 "fixture_digest": self.case.fixture_digest, "reservation": "backend-default", "timer": "workload-only-raii",
                 "repeats": "3", "warmup": "1", "push_comparisons": "", "pop_comparisons": "",
                 "profile": "controlled-operations", "input_policy": "shared-fixture", "cutoff": "disabled"}
                for index in range(3)]

    def csv(self, rows):
        stream = io.StringIO()
        writer = csv.DictWriter(stream, fieldnames=list(self.rows()[0]))
        writer.writeheader()
        writer.writerows(rows)
        return stream.getvalue()

    def test_accepts_complete_attributed_trials_for_both_sides(self):
        for side in ("cpp", "rust"):
            values, build = parse_trials(self.csv(self.rows(side)), self.case, side, 3, 1)
            self.assertEqual(values, [100, 101, 102])
            self.assertIn("source=", build)

    def test_rejects_missing_duplicate_and_invalid_trials(self):
        with self.assertRaisesRegex(ValueError, "expected 3 trials"):
            parse_trials(self.csv(self.rows()[:2]), self.case, "cpp", 3, 1)
        for key, value in (("repeat", "0"), ("repeat", "3"), ("nanos", "0"), ("nanos", "nan")):
            with self.subTest(key=key, value=value):
                rows = self.rows()
                rows[1][key] = value
                with self.assertRaises(ValueError):
                    parse_trials(self.csv(rows), self.case, "cpp", 3, 1)

    def test_rejects_different_fixture_or_workload_contract(self):
        for key, value in (("fixture_digest", "fnv1a64:wrong"), ("n", "16"), ("ops", "16"),
                           ("heap", "other"), ("backend", "scalar"), ("elem", "i32"),
                           ("profile", "original-rust"), ("protocol", "other"),
                           ("timer", "different"), ("repeats", "4"), ("warmup", "0")):
            with self.subTest(key=key):
                rows = self.rows()
                rows[1][key] = value
                with self.assertRaisesRegex(ValueError, "contract differs"):
                    parse_trials(self.csv(rows), self.case, "cpp", 3, 1)

    def test_rejects_missing_or_changing_build_identity(self):
        for value in ("", "rustc 1.0 source=hash", "GNU-15/Release/source=other"):
            with self.subTest(value=value):
                rows = self.rows()
                rows[1]["build"] = value
                with self.assertRaises(ValueError):
                    parse_trials(self.csv(rows), self.case, "cpp", 3, 1)

    def processes(self, cpp):
        return [{"case": self.case.name, "side": side, "process": process, "median_ns": value}
                for process in range(7) for side, value in (("candidate", cpp), ("rust", 100))]

    def test_strict_three_percent_gate_detects_both_sides(self):
        for cpp, passed in ((80, True), (102, True), (103, False), (104, False)):
            with self.subTest(cpp=cpp):
                result = summarize(self.processes(cpp), [self.case], ["candidate"], 7)[0]
                self.assertEqual(result["passed"], passed)
                self.assertAlmostEqual(result["slowdown_percent"], cpp - 100)

    def test_rejects_incomplete_duplicate_or_unexpected_process_groups(self):
        for rows in (self.processes(100)[:-1], self.processes(100) + self.processes(100)[:1],
                     [dict(row, side="other") if index == 0 else row for index, row in enumerate(self.processes(100))]):
            with self.assertRaises(ValueError):
                summarize(rows, [self.case], ["candidate"], 7)
        for value in (0, -1, float("nan"), float("inf")):
            rows = self.processes(100)
            rows[0]["median_ns"] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                summarize(rows, [self.case], ["candidate"], 7)

    def test_trace_requires_complete_operation_counts_and_final_state(self):
        raw = b"push,1\n" * 8 + b"pop,1\n" * 8 + b"final,0\n"
        validate_trace(raw, self.case)
        for invalid in (b"", raw.replace(b"final,0", b"final,1"), raw[:-1], raw.replace(b"push,1\n", b"", 1)):
            with self.assertRaises(ValueError):
                validate_trace(invalid, self.case)
        constant = Case("MonotoneConstantSize", 1, 1, "i64", Path("constant.qhf"), "digest", "hash")
        raw = b"push,1\n" * 112 + b"pop,1\n" * 61 + b"final,51\n" + b"drain,1\n" * 51
        validate_trace(raw, constant)
        with self.assertRaises(ValueError):
            validate_trace(raw.replace(b"drain,1\n", b"", 1), constant)

    def test_fixture_inventory_is_explicit_and_required(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(FileNotFoundError):
                load_cases(root, "i64", [8], [1])
            (root / "HeapSort-i64-n8-seed1.qhf").write_bytes(b"QHB1 i32 HeapSort 8\nsetup 0\ntimed 8\n")
            with self.assertRaisesRegex(ValueError, "header differs"):
                load_cases(root, "i64", [8], [1])

    def test_integer_lists_are_strict(self):
        self.assertEqual(integers("4096,65536"), [4096, 65536])
        for raw in ("", "1,1", "-1", "1,", "nan", "1, 2"):
            with self.subTest(raw=raw), self.assertRaises(argparse.ArgumentTypeError):
                integers(raw)


if __name__ == "__main__":
    unittest.main()
