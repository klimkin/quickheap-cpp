import csv
import io
import os
import shlex
import subprocess
import sys

from conan import ConanFile
from conan.tools.cmake import cmake_layout
from conan.tools.env import VirtualRunEnv


class BenchmarkTestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires(self.tested_reference_str, headers=False, libs=False, run=True)

    def generate(self):
        VirtualRunEnv(self).generate()

    def test(self):
        package = self.dependencies["quickheap-bench"].package_folder
        assets = os.path.join(package, "share", "quickheap-bench")
        suffix = ".exe" if self.settings.os == "Windows" else ""
        commands = (
            ("quickheap_bench", ["--min", "5", "--max", "5", "--repeat", "1"]),
            ("quickheap_bench_graph", [os.path.join(assets, "data", "smoke.gr"), "--repeat", "1"]),
            ("quickheap_experiments", ["--mode", "variants", "--min", "8", "--max", "8", "--repeat", "1"]),
            ("quickheap_fixture", ["--inspect", os.path.join(assets, "fixtures", "HeapSort-i32-n8.qhf")]),
            ("quickheap_bench", ["--fixture", os.path.join(assets, "fixtures", "MonotoneWiggle-i64-n8.qhf"),
                                 "--heap", "std_priority_queue", "--repeat", "1", "--warmup", "0"]),
        )
        for name, arguments in commands:
            output = io.StringIO()
            command = [os.path.join(package, "bin", name + suffix), *arguments]
            quoted = subprocess.list2cmdline(command) if suffix else shlex.join(command)
            self.run(quoted, env="conanrun", stdout=output)
            rows = list(csv.DictReader(io.StringIO(output.getvalue())))
            if not rows or any(None in row or None in row.values() for row in rows):
                raise RuntimeError(name + " produced invalid packaged CSV")
        for path in ("run-all.sh", "README.md", "PROTOCOL.md", "COVERAGE.md", "protocol.json", "verify.py",
                     "evals/plot.py", "evals/graph_weights.py"):
            if not os.path.isfile(os.path.join(assets, path)):
                raise RuntimeError("missing benchmark asset " + path)
        table = io.StringIO()
        command = [sys.executable, os.path.join(assets, "evals", "graph_weights.py"),
                   os.path.join(assets, "data", "smoke.gr")]
        quoted = subprocess.list2cmdline(command) if suffix else shlex.join(command)
        self.run(quoted, env="conanrun", stdout=table)
        if not list(csv.DictReader(io.StringIO(table.getvalue()))):
            raise RuntimeError("packaged graph analysis produced no output")
