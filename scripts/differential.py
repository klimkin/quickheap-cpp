#!/usr/bin/env python3
"""Compare C++ traces with original Rust C ABI and a min-heap oracle."""

from __future__ import annotations

import argparse
import ctypes
import heapq
import itertools
import os
from pathlib import Path
import random
import subprocess
import sys


INTEGER_TYPES = {
    "i32": (ctypes.c_int32, -(1 << 31), (1 << 31) - 1),
    "u32": (ctypes.c_uint32, 0, (1 << 32) - 1),
    "i64": (ctypes.c_int64, -(1 << 63), (1 << 63) - 1),
    "u64": (ctypes.c_uint64, 0, (1 << 64) - 1),
}


def build_rust(workspace: Path) -> Path:
    manifest = workspace / "quickheap" / "Cargo.toml"
    if not manifest.is_file():
        raise FileNotFoundError(f"Rust manifest missing: {manifest}")
    env = os.environ.copy()
    env["RUSTFLAGS"] = (env.get("RUSTFLAGS", "") + " -C target-cpu=native").strip()
    subprocess.run(
        ["cargo", "build", "--locked", "--release", "--lib", "-p", "quickheap",
         "--manifest-path", str(manifest)],
        cwd=workspace,
        env=env,
        check=True,
    )
    target = Path(env.get("CARGO_TARGET_DIR", workspace / "target"))
    if not target.is_absolute():
        target = workspace / target
    suffix = ".dylib" if sys.platform == "darwin" else ".dll" if os.name == "nt" else ".so"
    library = target / "release" / ("quickheap" if os.name == "nt" else "libquickheap")
    library = library.with_suffix(suffix)
    if not library.is_file():
        raise FileNotFoundError(f"Cargo built no cdylib at {library}")
    return library


def bind_api(library: ctypes.CDLL, kind: str, element: type[ctypes._SimpleCData]):
    prefix = f"quickheap_{kind}_"

    def bind(name: str, result, *arguments):
        function = getattr(library, prefix + name)
        function.restype = result
        function.argtypes = list(arguments)
        return function

    return {
        "new": bind("new", ctypes.c_void_p),
        "free": bind("free", None, ctypes.c_void_p),
        "push": bind("push", None, ctypes.c_void_p, element),
        "pop": bind("pop", ctypes.c_bool, ctypes.c_void_p, ctypes.POINTER(element)),
        "len": bind("len", ctypes.c_size_t, ctypes.c_void_p),
        "empty": bind("is_empty", ctypes.c_bool, ctypes.c_void_p),
        "capacity": bind("capacity", ctypes.c_size_t, ctypes.c_void_p),
    }


def cases(low: int, high: int, seed: int):
    rng = random.Random(seed)

    def load_and_drain(values):
        commands = [("empty", None), ("len", None), ("pop", None)]
        for index, value in enumerate(values, 1):
            commands.append(("push", value))
            if index in (1, 15, 16, 17, 31, 32, 33, 255, 256, 257, 4096):
                commands.extend((("len", None), ("empty", None)))
        commands.extend((("len", None), ("empty", None)))
        commands.extend(("pop", None) for _ in values)
        commands.extend((("pop", None), ("len", None), ("empty", None)))
        return commands

    yield "ascending", load_and_drain(range(low, low + 4096))
    yield "descending", load_and_drain(range(high, high - 4096, -1))

    duplicate_values = [low, low, low + 1, 0, 1, high - 1, high]
    duplicates = [duplicate_values[rng.randrange(len(duplicate_values))]
                  for _ in range(12000)]
    yield "duplicates_and_extremes", load_and_drain(duplicates)

    growth = [rng.randint(low, high) for _ in range(30000)]
    yield "growth_and_deep_pivots", load_and_drain(growth)

    mixed = [("empty", None), ("pop", None)]
    live = 0
    for index in range(30000):
        if live == 0 or rng.random() < (0.7 if index < 15000 else 0.45):
            mixed.append(("push", rng.choice(duplicate_values)
                          if index % 4 == 0 else rng.randint(low, high)))
            live += 1
        else:
            mixed.append(("pop", None))
            live -= 1
        if index % 97 == 0:
            mixed.extend((("len", None), ("empty", None)))
    mixed.extend(("pop", None) for _ in range(live + 1))
    mixed.extend((("len", None), ("empty", None)))
    yield "mixed_push_pop", mixed


def rust_oracle_outputs(api, element, commands, label):
    handle = api["new"]()
    if not handle:
        raise AssertionError(f"{label}: Rust allocation returned null")
    model = []
    lines = []
    try:
        for step, (operation, value) in enumerate(commands):
            if operation == "push":
                api["push"](handle, element(value))
                heapq.heappush(model, value)
            elif operation == "pop":
                output = element(71)
                found = api["pop"](handle, ctypes.byref(output))
                expected = heapq.heappop(model) if model else None
                if found != (expected is not None):
                    raise AssertionError(f"{label} step {step}: Rust pop presence differs from oracle")
                if expected is None:
                    if output.value != 71:
                        raise AssertionError(f"{label} step {step}: empty Rust pop changed output")
                    lines.append("pop none")
                else:
                    if output.value != expected:
                        raise AssertionError(
                            f"{label} step {step}: Rust returned {output.value}, expected {expected}"
                        )
                    lines.append(f"pop {expected}")
            elif operation == "len":
                length = api["len"](handle)
                if length != len(model):
                    raise AssertionError(f"{label} step {step}: Rust length {length}, expected {len(model)}")
                if api["capacity"](handle) < length:
                    raise AssertionError(f"{label} step {step}: Rust capacity below length")
                lines.append(f"len {length}")
            elif operation == "empty":
                empty = api["empty"](handle)
                if empty != (not model):
                    raise AssertionError(f"{label} step {step}: Rust empty flag differs from oracle")
                lines.append(f"empty {int(empty)}")
            else:
                raise ValueError(operation)
    finally:
        api["free"](handle)
    return lines


def compare_lines(expected, actual, label):
    for line, (want, got) in enumerate(itertools.zip_longest(expected, actual), 1):
        if want != got:
            raise AssertionError(f"{label} output line {line}: expected {want!r}, got {got!r}")


def check_fault_control(expected, actual):
    if not expected:
        raise AssertionError("fault control needs at least one output")
    corrupted = actual.copy()
    corrupted[0] = "pop deliberately_wrong"
    try:
        compare_lines(expected, corrupted, "injected fault")
    except AssertionError:
        return
    raise AssertionError("comparison accepted a deliberately wrong output")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="path to standalone C++ trace executable")
    parser.add_argument(
        "--rust-workspace", type=Path,
        default=Path(__file__).resolve().parents[2] / "quickheap-rs",
        help="original quickheap-rs workspace",
    )
    arguments = parser.parse_args()
    trace = arguments.trace.resolve()
    if not trace.is_file():
        parser.error(f"trace executable missing: {trace}")

    library = ctypes.CDLL(str(build_rust(arguments.rust_workspace.resolve())))
    checked = 0
    fault_checked = False
    for type_index, (kind, (element, low, high)) in enumerate(INTEGER_TYPES.items()):
        api = bind_api(library, kind, element)
        for name, commands in cases(low, high, 0x515549434B000000 + type_index):
            label = f"{kind}/{name}"
            expected = rust_oracle_outputs(api, element, commands, label)
            input_text = "".join(
                f"push {value}\n" if operation == "push" else f"{operation}\n"
                for operation, value in commands
            )
            completed = subprocess.run(
                [str(trace), kind], input=input_text, text=True,
                capture_output=True, check=True,
            )
            actual = completed.stdout.splitlines()
            compare_lines(expected, actual, label)
            if not fault_checked:
                check_fault_control(expected, actual)
                fault_checked = True
            print(f"PASS {label}: {len(commands)} commands, {len(expected)} outputs")
            checked += 1
    print(f"PASS {checked} Rust/C++ traces; injected wrong output rejected")


if __name__ == "__main__":
    main()
