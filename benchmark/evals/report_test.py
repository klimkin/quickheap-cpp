#!/usr/bin/env python3
"""Check report arithmetic, attributed grouping, and rejected protocols."""

import argparse
import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


spec = importlib.util.spec_from_file_location("benchmark_report", Path(__file__).with_name("report.py"))
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)

CPP_BUILD = "GNU-15.2.0/Release/-O3/source=sha256:" + "a" * 64 + "/dependencies=sha256:" + "b" * 64
RUST_BUILD = "rustc 1.99.0;target=x86_64;opt=3;features=std;flags=-O;source=fnv1a64:" + "c" * 16


class ReportContract(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.fixtures = self.root / "fixtures"
        self.fixtures.mkdir()
        self.protocol = {"protocol": "quickheap-cpp-v1"}

    def rows(self, workload="HeapSort", n=8, heap="heap", build=CPP_BUILD, backend="scalar", comparison=False):
        setup = 2 * (n + 50) if workload in ("MonotoneConstantSize", "RandomConstantSize") else 0
        timed = n * report.FACTORS[workload]
        raw = f"QHB1 i64 {workload} {n}\nsetup {setup}\n".encode() + b"1 " * setup + f"\ntimed {timed}\n".encode() + b"1 " * timed + b"\n"
        (self.fixtures / f"{workload}.qhf").write_bytes(raw)
        rows = []
        for repeat, multiplier in enumerate((5, 7, 9)):
            row = {"elem": "i64", "heap": heap, "n": str(n), "workload": workload,
                   "ops": str(timed), "repeat": str(repeat), "nanos": str(timed * 3 * multiplier),
                   "protocol": self.protocol["protocol"], "build": build, "backend": backend,
                   "fixture_digest": report.fixture_digest(raw), "reservation": "backend-default",
                   "timer": "workload-only-raii", "repeats": "3", "warmup": "1",
                   "push_comparisons": "", "pop_comparisons": ""}
            if comparison:
                row.update(nanos="0", push_comparisons=str(timed * 3 * 2), pop_comparisons=str(timed * 3 * 5))
            rows.append(row)
        return rows

    def source(self, rows, name="input.csv"):
        path = self.root / name
        with path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=rows[0])
            writer.writeheader()
            writer.writerows(rows)
        return path

    def read(self, rows):
        return report.read_results([self.source(rows)], report.read_fixtures(self.fixtures), self.protocol)[0]

    def test_run_metadata_requires_observed_performance_core(self):
        row = self.rows()
        source = self.source(row)
        metadata = self.root / "cpu.json"
        record = {"schema": "quickheap-cpu-v1", "cpu": 2, "core_class": "P", "performance_cpus": "0-3",
                  "allowed_cpus": [0, 1, 2, 3], "online_cpus": [0, 1, 2, 3], "observed_affinity": [2]}
        metadata.write_text(json.dumps(record))
        args = argparse.Namespace(csv=[source], fixtures=self.fixtures, output=self.root / "report",
                                  archive=None, plot=False, run_metadata=metadata)
        report.generate(args)
        manifest = json.loads((args.output / "manifest.json").read_text())
        self.assertEqual(manifest["run_metadata"]["value"], record)
        self.assertEqual((args.output / "provenance/cpu.json").read_bytes(), metadata.read_bytes())
        record["cpu"] = 4
        record["observed_affinity"] = [4]
        metadata.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, "P-core metadata"):
            report.generate(args)

    def test_workload_factor_applies_once(self):
        for workload in report.FACTORS:
            with self.subTest(workload=workload):
                summary = self.read(self.rows(workload))[0]
                self.assertEqual((summary.median, summary.minimum, summary.maximum), (7, 5, 9))
                counts = self.read(self.rows(workload, comparison=True))
                self.assertEqual({item.metric: item.median for item in counts},
                                 {"comparisons": 7, "push_comparisons": 2, "pop_comparisons": 5})

    def test_compilers_languages_and_backends_remain_distinct(self):
        rows = self.rows() + self.rows(build=CPP_BUILD.replace("GNU", "Clang")) + self.rows(build=RUST_BUILD)
        rows += self.rows(backend="avx2")
        summaries = self.read(rows)
        self.assertEqual(len(summaries), 4)
        self.assertEqual({s.identity.language for s in summaries}, {"C++", "Rust"})
        self.assertEqual(len({s.identity.build for s in summaries}), 3)

    def test_identity_violations_reject_rows(self):
        cases = {"protocol": "historical-rust", "fixture_digest": "fnv1a64:" + "0" * 16,
                 "timer": "includes-destruction", "build": "release", "ops": "79", "n": "9",
                 "repeats": "2", "warmup": "", "reservation": "precomputed"}
        for column, value in cases.items():
            with self.subTest(column=column):
                rows = self.rows()
                rows[0][column] = value
                with self.assertRaises(ValueError):
                    self.read(rows)

    def test_storage_reports_raw_units_and_omits_placeholder_timing(self):
        rows = self.rows()
        for row in rows:
            row.update(nanos="0", capacity="17", allocated_key_bytes="136", spare_slots="9",
                       peak_size="8", elements_partitioned="4", retries="2")
        summaries = self.read(rows)
        values = {summary.metric: summary.median for summary in summaries}
        self.assertEqual(values, {"capacity": 17, "allocated_key_bytes": 136, "spare_slots": 9,
                                  "peak_size": 8, "elements_partitioned": 4, "retries": 2})

    def test_incomplete_duplicate_and_mixed_schedule_reject(self):
        rows = self.rows()
        for invalid in (rows[:2], rows + [rows[0]], rows + self.rows(build=RUST_BUILD)):
            if len(invalid) == 6:
                invalid[-1]["warmup"] = "2"
            with self.assertRaises(ValueError):
                self.read(invalid)

    def test_corrupted_fixture_bytes_reject(self):
        rows = self.rows()
        path = self.fixtures / "HeapSort.qhf"
        path.write_bytes(path.read_bytes().replace(b"\n1 ", b"\n2 ", 1))
        with self.assertRaisesRegex(ValueError, "fixture digest"):
            self.read(rows)

    def test_original_fresh_trials_group_without_shared_fixture_claim(self):
        rows = self.rows(heap="std_priority_queue")
        for repeat, row in enumerate(rows):
            row.update(protocol=report.ORIGINAL_PROTOCOL, profile="original-rust",
                       input_policy="fresh-random-original", cutoff="32",
                       timer="workload-input-queue-destruction",
                       fixture_digest="fnv1a64:" + str(repeat) * 16,
                       reservation="reserve:1048576", push_comparisons="0.0", pop_comparisons="0.0")
        summaries, _ = report.read_results([self.source(rows)], {}, self.protocol, "original-rust")
        self.assertEqual(len(summaries), 1)
        self.assertEqual(summaries[0].identity.fixture_digest, "")
        self.assertEqual(summaries[0].median, 7)
        rust = [dict(row, build=RUST_BUILD, heap="alloc::collections::binary_heap::BinaryHeap", fixture_digest="") for row in rows]
        summaries, _ = report.read_results([self.source(rows + rust)], {}, self.protocol, "original-rust")
        self.assertEqual(len(summaries), 2)
        with self.assertRaisesRegex(ValueError, "mixed profile"):
            report.read_results([self.source(rows + self.rows())], {}, self.protocol, "original-rust")
        for column, value in (("warmup", "0"), ("input_policy", "shared-fixture"),
                              ("workload", "RandomConstantSize"), ("cutoff", "disabled"), ("fixture_digest", "")):
            invalid = [dict(row) for row in rows]
            invalid[0][column] = value
            with self.subTest(column=column), self.assertRaises(ValueError):
                report.read_results([self.source(invalid)], {}, self.protocol, "original-rust")

    def test_original_report_regenerates_without_exported_rust_inputs(self):
        rows = self.rows(build=RUST_BUILD)
        for row in rows:
            row.update(protocol=report.ORIGINAL_PROTOCOL, profile="original-rust",
                       input_policy="fresh-random-original", cutoff="32", fixture_digest="",
                       timer="workload-input-queue-destruction")
        args = argparse.Namespace(csv=[self.source(rows)], fixtures=self.root / "absent",
                                  output=self.root / "original", profile="original-rust", plot=False, archive=None)
        report.generate(args)
        manifest = json.loads((args.output / "manifest.json").read_text())
        self.assertEqual(manifest["fixtures"], [])
        self.assertEqual(manifest["profile"], "original-rust")
        first = (args.output / "summary.csv").read_bytes()
        report.generate(args)
        self.assertEqual(first, (args.output / "summary.csv").read_bytes())

    def test_document_escape_preserves_literal_names(self):
        self.assertEqual(report.latex("a_b&c%{d}\\"), r"a\_b\&c\%\{d\}\textbackslash{}")
        self.assertEqual(report.markdown("a|<b>"), "a&#124;&lt;b&gt;")

    def test_report_copies_hashed_inputs_and_regenerates(self):
        source = self.source(self.rows(heap="heap_%&"))
        args = argparse.Namespace(csv=[source], fixtures=self.fixtures, output=self.root / "output", plot=False, archive=None)
        report.generate(args)
        first = {path.relative_to(args.output): path.read_bytes() for path in args.output.rglob("*") if path.is_file()}
        report.generate(args)
        self.assertEqual(first, {path.relative_to(args.output): path.read_bytes() for path in args.output.rglob("*") if path.is_file()})
        manifest = json.loads((args.output / "manifest.json").read_text())
        for section in ("inputs", "fixtures", "provenance"):
            for item in manifest[section]:
                self.assertEqual(report.sha256((args.output / item["path"]).read_bytes()), item["sha256"])
        self.assertIn(r"heap\_\%\&", (args.output / "report.tex").read_text())


if __name__ == "__main__":
    unittest.main()
