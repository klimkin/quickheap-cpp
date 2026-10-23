#!/usr/bin/env python3
"""Build and verify attributed C++ benchmark packages across compiler axes."""

import argparse
import contextlib
import csv
from dataclasses import asdict, dataclass, replace
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

if os.name == "nt":
    import msvcrt
else:
    import fcntl


@dataclass(frozen=True)
class Axis:
    name: str
    profile: str
    compiler: str
    build_type: str = "Release"
    scalar: bool = False
    boost: bool = False
    external: bool = False
    sanitizer: str | None = None


def axes():
    result = []
    for compiler in ("gcc", "clang"):
        profile = f"{compiler}-native"
        result.extend((
            Axis(f"{compiler}-native", profile, compiler),
            Axis(f"{compiler}-scalar", profile, compiler, scalar=True),
            Axis(f"{compiler}-asan", f"{profile}-asan", compiler, "Debug", True, sanitizer="address"),
            Axis(f"{compiler}-ubsan", f"{profile}-ubsan", compiler, "Debug", sanitizer="undefined"),
            Axis(f"{compiler}-tsan", f"{profile}-tsan", compiler, "Debug", sanitizer="thread"),
            Axis(f"{compiler}-boost", profile, compiler, boost=True),
            Axis(f"{compiler}-external", profile, compiler, external=True),
        ))
    return {axis.name: axis for axis in result}


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class CheckFailure(RuntimeError):
    pass


class Unavailable(RuntimeError):
    pass


class Checks:
    def __init__(self, directory):
        self.directory = directory
        self.records = []

    def run(self, name, command, *, cwd=None, env=None):
        log = self.directory / f"{name}.log"
        stderr_log = self.directory / f"{name}.stderr.log"
        started = utc_now()
        start = time.monotonic()
        try:
            with log.open("w", encoding="utf-8") as stream, stderr_log.open("w", encoding="utf-8") as stderr:
                result = subprocess.run(command, cwd=cwd, env=env, stdout=stream,
                                        stderr=stderr, check=False)
            returncode = result.returncode
        except OSError as error:
            log.write_text(str(error) + "\n", encoding="utf-8")
            returncode = 127
        self.records.append({"name": name, "command": command,
                             "cwd": str(cwd) if cwd else None,
                             "started": started, "seconds": time.monotonic() - start,
                             "returncode": returncode, "log": str(log), "stderr_log": str(stderr_log)})
        if returncode:
            details = stderr_log.read_text(encoding="utf-8").strip() if stderr_log.exists() else ""
            reason = details.splitlines()[-1] if details else str(log)
            raise CheckFailure(f"{name} exited {returncode}: {reason}; see {log} and {stderr_log}")
        return log


@contextlib.contextmanager
def package_cache_lock(path):
    with path.open('a+b') as stream:
        if os.name == 'nt':
            stream.write(b'0')
            stream.flush()
            stream.seek(0)
            while True:
                try:
                    msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
                    break
                except OSError as error:
                    if error.errno not in (13, 11, 36):
                        raise
                    time.sleep(0.25)
        else:
            fcntl.flock(stream.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            if os.name == 'nt':
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def profile_arguments(axis, build_profile):
    return ["-pr:h", axis.profile, "-pr:b", build_profile,
            "-s:h", f"build_type={axis.build_type}",
            "-s:h", "compiler.cppstd=20",
            "-o:h", f"quickheap/*:scalar={axis.scalar}",
            "-o:h", "quickheap/*:shared=False"]


def compiler_commands(context):
    configured = context.get("conf", {}).get("tools.build:compiler_executables", {})
    environment = dict(line.split("=", 1) for line in context.get("build_env", "").splitlines()
                       if "=" in line)
    family = context.get("settings", {}).get("compiler")
    defaults = {"gcc": {"c": "gcc", "cpp": "g++"},
                "clang": {"c": "clang", "cpp": "clang++"}}
    if family not in defaults:
        raise CheckFailure(f"unsupported profile compiler {family!r}")
    return {language: shlex.split(configured.get(language) or environment.get(variable)
                                 or defaults[family][language])
            for language, variable in (("c", "CC"), ("cpp", "CXX"))}


def check_profiles(checks, axis, build_profile):
    path = checks.directory / "profiles.json"
    try:
        checks.run("profiles", ["conan", "profile", "show", *profile_arguments(axis, build_profile),
                                "--format=json", "--out-file", str(path)])
    except CheckFailure as error:
        if "Profile not found" in (checks.directory / "profiles.stderr.log").read_text(encoding="utf-8"):
            raise Unavailable(str(error)) from error
        raise
    profiles = json.loads(path.read_text(encoding="utf-8"))
    if profiles["host"]["settings"].get("compiler") != axis.compiler:
        raise CheckFailure(f"{axis.name} requires host compiler {axis.compiler}")
    if axis.sanitizer:
        flags = []
        for line in profiles["host"].get("build_env", "").splitlines():
            if "=" in line:
                flags.extend(shlex.split(line.split("=", 1)[1]))
        for value in profiles["host"].get("conf", {}).values():
            if isinstance(value, list):
                flags.extend(value)
        sanitizers = {sanitizer for flag in flags if isinstance(flag, str) and flag.startswith("-fsanitize=")
                      for sanitizer in flag.split("=", 1)[1].split(",")}
        if axis.sanitizer not in sanitizers:
            raise CheckFailure(f"{axis.name} profile must enable -fsanitize={axis.sanitizer}")
    compiler_info = {}
    for context in ("host", "build"):
        compiler_info[context] = {}
        for language, command in compiler_commands(profiles[context]).items():
            if not command or not shutil.which(command[0]):
                raise Unavailable(f"{context} {language} compiler unavailable: {shlex.join(command)}")
            compiler_info[context][language] = {
                "command": command, "executable": shutil.which(command[0]),
                "version_log": str(checks.run(f"{context}-{language}-version", [*command, "--version"]))}
    return profiles, compiler_info


def package_node(graph_file, package_name):
    graph = json.loads(graph_file.read_text(encoding="utf-8"))
    nodes = graph.get("graph", {}).get("nodes", {})
    values = nodes.values() if isinstance(nodes, dict) else nodes
    matches = [node for node in values if node.get("name") == package_name
               or str(node.get("ref", "")).startswith(package_name + "/")]
    if len(matches) != 1 or not matches[0].get("package_folder"):
        raise CheckFailure(f"{graph_file} must attribute one {package_name} package folder")
    return matches[0]


def package_folder(graph_file, package_name):
    node = package_node(graph_file, package_name)
    if node.get("binary") != "Build":
        raise CheckFailure(f"{package_name} did not build from source in this axis")
    return Path(node["package_folder"]), node


def read_csv(path):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or len(set(reader.fieldnames)) != len(reader.fieldnames):
            raise CheckFailure(f"{path} has missing or duplicate CSV columns")
        rows = list(reader)
    if not rows or any(None in row or None in row.values() for row in rows):
        raise CheckFailure(f"{path} has missing or malformed CSV rows")
    return rows


def packaged_checks(checks, package):
    binary_dir = package / "bin"
    assets = package / "share" / "quickheap-bench"
    suffix = ".exe" if os.name == "nt" else ""
    binary = lambda name: str(binary_dir / (name + suffix))
    required = ("quickheap_bench", "quickheap_bench_graph", "quickheap_experiments", "quickheap_fixture")
    for name in required:
        if not (binary_dir / (name + suffix)).is_file():
            raise CheckFailure(f"missing packaged executable {name}")
    for name in ("protocol.json", "README.md", "run-all.sh", "evals/plot.py", "evals/graph_weights.py", "evals/report.py", "evals/historical.json"):
        if not (assets / name).is_file():
            raise CheckFailure(f"missing packaged asset {name}")
    fixtures = sorted((assets / "fixtures").glob("*.qhf"))
    if not fixtures:
        raise CheckFailure("packaged benchmark has no shared .qhf input fixtures")
    catalog = checks.run("packaged-catalog", [binary("quickheap_bench"), "--list"])
    catalog_text = catalog.read_text(encoding="utf-8")
    if "heap,std_priority_queue" not in catalog_text:
        raise CheckFailure("packaged catalog must register std_priority_queue")
    generated = checks.directory / "generated.qhf"
    checks.run("fixture-generate", [binary("quickheap_fixture"), "--generate", "HeapSort", "--elem", "i32",
                                    "--n", "32", "--output", str(generated)])
    if not generated.is_file():
        raise CheckFailure("packaged fixture generator did not produce generated.qhf")
    checks.run("fixture-generated-inspect", [binary("quickheap_fixture"), "--inspect", str(generated), "--dump"])
    checks.run("fixture-generated-verify", [binary("quickheap_bench"), "--fixture", str(generated),
                                            "--heap", "std_priority_queue", "--verify-only"])
    fixture_records = []
    for index, fixture in enumerate(fixtures):
        checks.run(f"fixture-{index}-inspect", [binary("quickheap_fixture"), "--inspect", str(fixture)])
        checks.run(f"fixture-{index}-verify", [binary("quickheap_bench"), "--fixture", str(fixture),
                                              "--heap", "std_priority_queue", "--verify-only"])
        fixture_records.append({"path": str(fixture), "sha256": digest(fixture)})
    fixture_csv = checks.run("packaged-fixture", [binary("quickheap_bench"), "--fixture", str(fixtures[0]),
                                                 "--heap", "std_priority_queue", "--repeat", "1", "--warmup", "0"])
    paired_rows = read_csv(fixture_csv)
    for row in paired_rows:
        if row.get("protocol") != "quickheap-cpp-v1":
            raise CheckFailure("paired fixture result lacks protocol identity")
        for column in ("build", "backend", "fixture_digest", "reservation", "timer", "repeats", "warmup"):
            if not row.get(column):
                raise CheckFailure(f"paired fixture result lacks {column}")
    common = ["--min", "5", "--max", "5", "--repeat", "1", "--i32", "--i64", "--u32", "--u64"]
    synthetic_csv = checks.run("packaged-synthetic", [binary("quickheap_bench"), *common])
    read_csv(synthetic_csv)
    for mode in ("variants", "ablation", "comparisons", "partition", "rebalancing"):
        path = checks.run(f"packaged-experiment-{mode}", [binary("quickheap_experiments"), "--mode", mode, *common])
        read_csv(path)
    graphs = [assets / "data" / name for name in ("smoke.gr", "zero-weight.gr")]
    for graph in graphs:
        path = checks.run(f"packaged-graph-{graph.stem}", [binary("quickheap_bench_graph"), str(graph), "--repeat", "1"])
        read_csv(path)
    table = checks.run("report-summary", [sys.executable, str(assets / "evals" / "plot.py"), str(synthetic_csv), "--table"])
    read_csv(table)
    weights = checks.run("report-graph-weights", [sys.executable, str(assets / "evals" / "graph_weights.py"), *map(str, graphs)])
    read_csv(weights)
    checks.run("report-plot", [sys.executable, str(assets / "evals" / "plot.py"), str(synthetic_csv),
                               "--output", str(checks.directory / "timings.svg")])
    if not (checks.directory / "timings.svg").is_file():
        raise CheckFailure("report plot did not produce timings.svg")
    paired_report = checks.directory / 'paired-report'
    checks.run('report-attributed', [sys.executable, str(assets / 'evals' / 'report.py'),
               str(fixture_csv), '--fixtures', str(assets / 'fixtures'), '--output', str(paired_report), '--plot'])
    for name in ('summary.csv', 'report.tex', 'paper.md', 'manifest.json', 'nanos.svg', 'nanos.pdf'):
        if not (paired_report / name).is_file():
            raise CheckFailure(f'attributed report lacks {name}')
    return {"protocol_sha256": digest(assets / "protocol.json"), "fixtures": fixture_records,
            "executables": {name: digest(binary_dir / (name + suffix)) for name in required}}


def run_axis(root, axis, build_profile, output):
    directory = output / axis.name
    directory.mkdir()
    checks = Checks(directory)
    record = {"axis": asdict(axis), "build_profile": build_profile,
              "started": utc_now(), "status": "failed", "checks": checks.records}
    print(f"Running {axis.name}", flush=True)
    try:
        record["profiles"], record["compilers"] = check_profiles(checks, axis, build_profile)
        args = [*profile_arguments(axis, build_profile), "--build=missing", "--lockfile="]
        for name, source in (("quickheap", root), ("quickheap-bench", root / "benchmark")):
            test_folder = directory / f"{name}-test-package"
            shutil.copytree(source / "test_package", test_folder,
                            ignore=shutil.ignore_patterns("build", "__pycache__", "CMakeUserPresets.json"))
            graph = directory / f"{name}-create.json"
            command = ["conan", "create", str(source), *args, f"--build={name}/*",
                       "--test-folder", str(test_folder), "--format=json", "--out-file", str(graph)]
            if name == "quickheap-bench":
                command.extend(("-o:h", f"quickheap-bench/*:boost={axis.boost}",
                                "-o:h", f"quickheap-bench/*:external={axis.external}"))
            checks.run(f"{name}-create", command, cwd=root)
            package, node = package_folder(graph, name)
            record.setdefault("packages", {})[name] = node
            if name == "quickheap-bench":
                dependency = package_node(graph, "quickheap")
                local = record["packages"]["quickheap"]
                for key in ("ref", "package_id", "prev"):
                    if dependency.get(key) != local.get(key):
                        raise CheckFailure(f"benchmark quickheap dependency differs from local build at {key}")
        record["artifacts"] = packaged_checks(checks, package)
        if axis.boost:
            text = (checks.directory / "packaged-catalog.log").read_text(encoding="utf-8")
            if "heap,boost_dary4" not in text:
                raise CheckFailure("Boost axis did not register boost_dary4")
        if axis.external:
            text = (checks.directory / "packaged-catalog.log").read_text(encoding="utf-8")
            for name in ("sequence_heap", "s3q", "original_quickheap", "external_radix_heap"):
                if f"heap,{name}" not in text:
                    raise CheckFailure(f"external axis did not register {name}")
        record["status"] = "passed"
    except Unavailable as error:
        record["status"] = "skipped"
        record["reason"] = str(error)
    except (CheckFailure, OSError, ValueError, KeyError) as error:
        record["reason"] = str(error)
    record["finished"] = utc_now()
    write_json(directory / "provenance.json", record)
    print(f"{axis.name}: {record['status']}" + (f" ({record['reason']})" if "reason" in record else ""), flush=True)
    return record


def source_state(root):
    def git(*arguments):
        result = subprocess.run(["git", "-C", str(root), *arguments], capture_output=True, check=True)
        return result.stdout
    files = {}
    for directory in (root / "benchmark", root / "src", root / "include", root / "profiles"):
        for path in sorted(directory.rglob("*")):
            relative = path.relative_to(root)
            if (path.is_file() and not any(part in {"build", "__pycache__", ".audit"} for part in relative.parts)
                    and (directory.name == "profiles" or path.name == "CMakeLists.txt"
                         or path.suffix in {".cpp", ".cc", ".c", ".hpp", ".h", ".in", ".py", ".json", ".qhf", ".gr", ".cmake"})):
                files[str(relative)] = digest(path)
    for name in ("conanfile.py", "CMakeLists.txt", "benchmark/CMakeLists.txt"):
        files[name] = digest(root / name)
    return {"revision": git("rev-parse", "HEAD").decode().strip(),
            "status": git("status", "--porcelain").decode(),
            "diff_sha256": hashlib.sha256(git("diff", "--binary", "HEAD")).hexdigest(),
            "files": files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    catalog = axes()
    parser.add_argument("--list", action="store_true", help="print axes without building or probing tools")
    parser.add_argument("--axis", action="append", choices=tuple(catalog), help="select an axis; repeat for multiple axes")
    parser.add_argument("--build-profile", default=os.environ.get("QUICKHEAP_BUILD_PROFILE", "gcc-native-build"))
    parser.add_argument("--output", type=Path, default=Path("build/benchmark-verify"), help="parent directory for attributed runs")
    parser.add_argument("--host-profile", help="override host profile for selected axes of one compiler family")
    args = parser.parse_args()
    if args.host_profile:
        families = {catalog[name].compiler for name in args.axis or catalog}
        if len(families) != 1:
            parser.error('--host-profile requires axes from one compiler family')
        for name in args.axis or catalog:
            catalog[name] = replace(catalog[name], profile=args.host_profile)
    if args.axis and len(args.axis) != len(set(args.axis)):
        parser.error("duplicate --axis")
    if args.list:
        print("axis\thost profile\tbuild type\tscalar\tboost\texternal")
        for axis in catalog.values():
            print(f"{axis.name}\t{axis.profile}\t{axis.build_type}\t{axis.scalar}\t{axis.boost}\t{axis.external}")
        return 0
    root = Path(__file__).resolve().parent.parent
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    summary = {"schema": "quickheap-benchmark-verification-v1", "started": utc_now(),
               "platform": platform.platform(), "python": sys.version,
               "build_profile": args.build_profile, "selected_axes": args.axis or list(catalog), "axes": []}
    try:
        summary["source"] = source_state(root)
        if not shutil.which("conan"):
            raise Unavailable("Conan unavailable")
        preflight = Checks(output)
        preflight.run("conan-version", ["conan", "--version"])
        editable_path = output / "editables.json"
        preflight.run("editables", ["conan", "editable", "list", "--format=json", "--out-file", str(editable_path)])
        editables = json.loads(editable_path.read_text(encoding="utf-8"))
        if any(reference.startswith(("quickheap/", "quickheap-bench/")) for reference in editables):
            raise CheckFailure("Quickheap editable registration exists; package verification requires isolated noneditable dependencies")
        summary["preflight"] = preflight.records
        conan_cache = Path(os.environ.get('CONAN_HOME', Path.home() / '.conan2'))
        with package_cache_lock(conan_cache / 'quickheap-benchmark-verification.lock'):
            for name in args.axis or catalog:
                summary["axes"].append(run_axis(root, catalog[name], args.build_profile, output))
        summary["status"] = "passed" if all(axis["status"] == "passed" for axis in summary["axes"]) else "incomplete"
    except (Unavailable, CheckFailure, OSError, ValueError, subprocess.CalledProcessError) as error:
        summary["status"] = "failed"
        summary["reason"] = str(error)
    summary["finished"] = utc_now()
    write_json(output / "summary.json", summary)
    print(f"Verification {summary['status']}. Provenance {output / 'summary.json'}", flush=True)
    return 0 if summary["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())
