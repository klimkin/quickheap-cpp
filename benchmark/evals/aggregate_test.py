#!/usr/bin/env python3
"""Check attributed process aggregation and incomplete comparison rejection."""

import argparse
import csv
import json
from pathlib import Path
import tempfile
import unittest

from aggregate import aggregate
from report_test import CPP_BUILD, RUST_BUILD, ReportContract
import report


class AggregateContract(unittest.TestCase):
    rows = ReportContract.rows
    source = ReportContract.source
    def setUp(self):
        ReportContract.setUp(self)
        self.cpu = {"schema": "quickheap-cpu-v1", "cpu": 2, "core_class": "P", "performance_cpus": "0-3",
                    "allowed_cpus": [0, 1, 2, 3], "online_cpus": [0, 1, 2, 3], "observed_affinity": [2]}

    def process(self, index, multiplier, rust=True):
        rows = self.rows(build=CPP_BUILD)
        if rust:
            rows += self.rows(build=RUST_BUILD)
        for row in rows:
            row["nanos"] = str(int(row["nanos"]) * multiplier)
        source = self.source(rows, f"process-{index}.csv")
        metadata = self.root / f"cpu-{index}.json"
        metadata.write_text(json.dumps(self.cpu))
        directory = self.root / f"report-{index}"
        report.generate(argparse.Namespace(csv=[source], fixtures=self.fixtures, output=directory,
                                          archive=None, plot=False, run_metadata=metadata))
        return directory

    def test_aggregate_process_medians_and_emit_reports(self):
        reports = [self.process(i, value) for i, value in enumerate((2, 9, 3))]
        aggregate(reports, self.root / "aggregate")
        with (self.root / "aggregate/summary.csv").open() as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 2)
        self.assertEqual({r["process_median"] for r in rows}, {"21.0"})
        self.assertEqual({r["processes"] for r in rows}, {"3"})
        self.assertEqual({r["complete"] for r in rows}, {"True"})
        for name in ("paper.md", "report.tex", "nanos.svg", "nanos.pdf", "manifest.json",
                     "provenance/historical.json", "provenance/catalog.csv"):
            self.assertTrue((self.root / "aggregate" / name).is_file())

    def test_missing_process_or_language_rejected(self):
        reports = [self.process(0, 3), self.process(1, 4, rust=False)]
        with self.assertRaisesRegex(ValueError, "missing process"):
            aggregate(reports, self.root / "aggregate")
        with self.assertRaisesRegex(ValueError, "both languages"):
            aggregate(reports[1:], self.root / "aggregate")

    def test_input_and_summary_tampering_rejected(self):
        directory = self.process(0, 3)
        summary = directory / "summary.csv"
        summary.write_text(summary.read_text().replace(",21.0,", ",22.0,"))
        with self.assertRaisesRegex(ValueError, "summary differs"):
            aggregate([directory], self.root / "aggregate")
        directory = self.process(1, 3)
        source = next((directory / "inputs").glob("*.csv"))
        source.write_bytes(source.read_bytes() + b"\n")
        with self.assertRaisesRegex(ValueError, "input digest"):
            aggregate([directory], self.root / "aggregate")

    def test_missing_expected_fixture_rejected(self):
        directory = self.process(0, 3)
        path = directory / "manifest.json"
        manifest = json.loads(path.read_text())
        manifest["fixture_inventory"].append("fnv1a64:0000000000000000")
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "missing expected"):
            aggregate([directory], self.root / "aggregate")

    def test_mixed_cpu_rejected(self):
        a = self.process(0, 3)
        self.cpu["cpu"] = 3
        self.cpu["observed_affinity"] = [3]
        b = self.process(1, 5)
        with self.assertRaisesRegex(ValueError, "mix CPU"):
            aggregate([a, b], self.root / "aggregate")


if __name__ == "__main__":
    unittest.main()
