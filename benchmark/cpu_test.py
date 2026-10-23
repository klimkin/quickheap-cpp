#!/usr/bin/env python3
"""Verify performance-core selection and explicit subprocess affinity."""

from pathlib import Path
import tempfile
import unittest

from cpu import cpu_list, pinned_command, select_cpu, validate_record


class CpuContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name, value in {"bus/event_source/devices/cpu_core/cpus": "0-3,6",
                            "devices/system/cpu/online": "0-7",
                            "devices/system/cpu/cpu2/topology/core_id": "2",
                            "devices/system/cpu/cpu2/topology/physical_package_id": "0"}.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(value)

    def test_selects_allowed_online_performance_core(self):
        record = select_cpu("auto", self.root, {2, 4, 5})
        self.assertEqual(record["cpu"], 2)
        self.assertEqual(record["core_class"], "P")
        self.assertEqual(pinned_command(["heap", "--repeat", "3"], record["cpu"]),
                         ["taskset", "-c", "2", "heap", "--repeat", "3"])

    def test_rejects_efficiency_unavailable_and_invalid_cpus(self):
        for requested in ("4", "1", "2,3", "-2", "2-3"):
            with self.subTest(requested=requested), self.assertRaises(ValueError):
                select_cpu(requested, self.root, {2, 4, 5})
        (self.root / "devices/system/cpu/online").write_text("4-7")
        with self.assertRaises(ValueError):
            select_cpu("2", self.root, {2, 4, 5})

    def test_missing_kernel_proof_fails(self):
        (self.root / "bus/event_source/devices/cpu_core/cpus").unlink()
        with self.assertRaisesRegex(ValueError, "cannot prove P-core"):
            select_cpu("2", self.root, {2})

    def test_archived_record_requires_observed_exact_affinity(self):
        record = select_cpu("2", self.root, {2, 4, 5})
        record["observed_affinity"] = [2]
        self.assertIs(validate_record(record), record)
        for wrong in ([2, 4], [4], []):
            record["observed_affinity"] = wrong
            with self.assertRaises(ValueError):
                validate_record(record)

    def test_kernel_lists_are_strict(self):
        self.assertEqual(cpu_list("0-2,5\n"), {0, 1, 2, 5})
        for value in ("", "2-1", "a", "1,,2", "1-2-3", "-1"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                cpu_list(value)


if __name__ == "__main__":
    unittest.main()
