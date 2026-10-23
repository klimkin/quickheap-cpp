#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: compare.sh [original-rust|controlled-operations|both [OUTPUT_DIR]]

Build and verify local C++ packages, build Rust, run matching profiles,
and generate separate CSV, LaTeX, Markdown, SVG, and PDF reports.
Default profile: both. Default output: new build/compare-* directory.

Environment:
  RUST_REPO      Rust checkout, default sibling quickheap-rs
  HOST_PROFILE   Conan host profile, default default
  BUILD_PROFILE  Conan build profile, default HOST_PROFILE
  CPP_AXIS       gcc-native or clang-native, default gcc-native
  PAIR           quickheap (AVX2) or std, default quickheap
  MIN MAX STEP   Size exponents, defaults 10 16 2
  ELEM           i32 or i64, default i64
  REPEAT WARMUP  Controlled trials, defaults 3 1
  BENCH_CPU      One kernel-identified P core, default first available
  RUNS           Independent alternating process runs, default 7
  SEEDS          Controlled uint64 decimal seeds, comma-separated; default 1363692610

Requires Bash, Python with matplotlib, Conan profiles, and Rust toolchain.
Original retains three trials, one warmup, contiguous sizes, and cutoffs.
Rust original runs all enabled registrations; C++ selects PAIR counterpart.
Reports retain Rust source identity, including declared upstream repairs.
Failures retain logs and exit nonzero. With both, independent profiles continue.
EOF
}

fail() { printf 'error: %s\n' "$*" >&2; exit 2; }
if [[ ${1:-} == --help || ${1:-} == -h ]]; then usage; exit 0; fi
(( $# <= 2 )) || fail 'expected profile and optional output directory'
profile=${1:-both}
case "$profile" in original-rust|controlled-operations|both) ;; *) fail "unknown profile $profile" ;; esac
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cpp_root=$(cd -- "$script_dir/.." && pwd)
rust_root=${RUST_REPO:-"$cpp_root/../quickheap-rs"}
[[ -f "$rust_root/quickheap-dev/bench/profiles.py" ]] || fail "missing Rust launcher in $rust_root"
rust_root=$(cd -- "$rust_root" && pwd)
host_profile=${HOST_PROFILE:-default}
build_profile=${BUILD_PROFILE:-$host_profile}
cpp_axis=${CPP_AXIS:-gcc-native}
case "$cpp_axis" in gcc-native|clang-native) ;; *) fail 'CPP_AXIS requires gcc-native or clang-native' ;; esac
pair=${PAIR:-quickheap}
cpp_backend=()
rust_features=()
case "$pair" in
    quickheap) cpp_heap=quickheap; rust_heap=quickheap; cpp_backend=(--backend avx2); rust_features=(--features quickheap-avx2) ;;
    std) cpp_heap=std_priority_queue; rust_heap=binary_heap ;;
    *) fail 'PAIR requires quickheap or std' ;;
esac
elem=${ELEM:-i64}
case "$elem" in i32|i64) ;; *) fail 'ELEM requires i32 or i64' ;; esac
minimum=${MIN:-10}; maximum=${MAX:-16}; step=${STEP:-2}
repeat=${REPEAT:-3}; warmup=${WARMUP:-1}; runs=${RUNS:-7}
for value in "$minimum" "$maximum" "$step" "$repeat" "$warmup" "$runs"; do
    [[ "$value" =~ ^[0-9]{1,9}$ ]] || fail "invalid nonnegative integer $value"
done
minimum=$((10#$minimum)); maximum=$((10#$maximum)); step=$((10#$step))
repeat=$((10#$repeat)); warmup=$((10#$warmup)); runs=$((10#$runs))
(( minimum >= 1 && minimum <= maximum && maximum <= 25 && step > 0 && repeat > 0 && runs > 0 )) || fail 'require 1 <= MIN <= MAX <= 25, STEP > 0, REPEAT > 0, RUNS > 0'
for tool in python3 conan cargo git cmp taskset; do command -v "$tool" >/dev/null || fail "missing $tool"; done
python3 -c 'import matplotlib' || fail 'Python requires matplotlib for reports'
bench_cpu=$(python3 "$script_dir/cpu.py" --select "${BENCH_CPU:-auto}") || exit 2
export BENCH_CPU=$bench_cpu
seed_values=$(python3 - "${SEEDS:-1363692610}" <<'PYSEEDS'
import re, sys
values = sys.argv[1].split(',')
if not values or any(not re.fullmatch(r'[0-9]+', value) or int(value) > (1 << 64) - 1 for value in values):
    raise SystemExit('SEEDS requires comma-separated uint64 decimal values')
values = [str(int(value)) for value in values]
if len(set(values)) != len(values):
    raise SystemExit('SEEDS requires distinct values')
print(' '.join(values))
PYSEEDS
) || exit 2
read -r -a seeds <<< "$seed_values"
if (( $# == 2 )); then
    mkdir -p -- "$(dirname -- "$2")"
    mkdir -- "$2" || fail "output directory must be new: $2"
    output=$(cd -- "$2" && pwd)
else
    mkdir -p -- "$cpp_root/build"
    output=$(mktemp -d "$cpp_root/build/compare-XXXXXXXX")
fi
mkdir -- "$output/logs"
cp -- "${BASH_SOURCE[0]}" "$output/compare.sh"
python3 "$script_dir/cpu.py" --select "$bench_cpu" --output "$output/cpu.json" >/dev/null
printf 'phase\tstatus\n' > "$output/status.tsv"
printf 'command\texit\n' > "$output/commands.tsv"
export CARGO_TARGET_DIR="$output/rust-target"
export MPLBACKEND=Agg

run() {
    local name=$1 destination=$2 code=0
    shift 2
    printf 'Running %s\n' "$name" >&2
    { printf 'cd %q && ' "$PWD"; printf '%q ' "$@"; printf '\n'; } > "$output/logs/$name.command"
    "$@" > "$destination" 2> "$output/logs/$name.stderr.log" || code=$?
    printf '%s\t%s\n' "$name" "$code" >> "$output/commands.tsv"
    if (( code )); then
        printf 'Failed %s, exit %s. See %s and %s\n' "$name" "$code" "$destination" "$output/logs/$name.stderr.log" >&2
    fi
    return "$code"
}

cd -- "$cpp_root"
for language in cpp rust; do
    root=$cpp_root
    if [[ "$language" == rust ]]; then root=$rust_root; fi
    git -C "$root" rev-parse HEAD > "$output/$language.revision"
    git -C "$root" status --short > "$output/$language.status"
    git -C "$root" diff --binary HEAD > "$output/$language.diff"
done
for name in original-sources.json original-repairs.json; do
    if [[ -f "$rust_root/quickheap-dev/bench/$name" ]]; then
        cp -- "$rust_root/quickheap-dev/bench/$name" "$output/$name"
    fi
done
if command -v lscpu >/dev/null; then lscpu > "$output/cpu.txt"; fi
python3 - "$output/run.json" "$profile" "$pair" "$elem" "$minimum" "$maximum" "$step" "$repeat" "$warmup" "$runs" "$seed_values" <<'PY'
import json, os, platform, sys
from datetime import datetime, timezone
from pathlib import Path
keys = ("profile", "pair", "elem", "min", "max", "step", "repeats", "warmup", "process_runs", "seeds")
record = dict(zip(keys, sys.argv[2:]))
record.update(started=datetime.now(timezone.utc).isoformat(), platform=platform.platform(),
              cpu_count=os.cpu_count(), load_average=os.getloadavg() if hasattr(os, "getloadavg") else None)
names = {"RUSTC", "RUSTFLAGS", "CARGO_ENCODED_RUSTFLAGS", "CARGO_BUILD_TARGET", "CARGO_BUILD_RUSTFLAGS"}
record["cpu"] = json.loads((Path(sys.argv[1]).parent / "cpu.json").read_text())
record["rust_environment"] = {key: value for key, value in os.environ.items()
                              if key in names or key.startswith("CARGO_PROFILE_")
                              or key.startswith("CARGO_TARGET_") and key.endswith("_RUSTFLAGS")}
Path(sys.argv[1]).write_text(json.dumps(record, indent=2) + "\n")
PY
if ! run cpp-build "$output/logs/cpp-build.stdout.log" python3 "$script_dir/verify.py" \
    --axis "$cpp_axis" --host-profile "$host_profile" --build-profile "$build_profile" --output "$output/cpp-build"; then
    printf 'cpp-build\tfailed\n' >> "$output/status.tsv"
    exit 1
fi
package=$(python3 - "$output/cpp-build" <<'PY'
import json, sys
from pathlib import Path
files = list(Path(sys.argv[1]).glob("run-*/summary.json"))
if len(files) != 1:
    raise SystemExit("expected one C++ verification summary")
summary = json.loads(files[0].read_text())
if summary["status"] != "passed" or len(summary["axes"]) != 1:
    raise SystemExit("C++ verification incomplete")
print(summary["axes"][0]["packages"]["quickheap-bench"]["package_folder"])
PY
)
printf 'cpp-build\tpassed\n' >> "$output/status.tsv"
cpp_bench="$package/bin/quickheap_bench"
cpp_fixture="$package/bin/quickheap_fixture"
report="$package/share/quickheap-bench/evals/report.py"
aggregate="$package/share/quickheap-bench/evals/aggregate.py"

original() {
    local directory="$output/original-rust"
    mkdir -- "$directory" || return
    run original-source "$directory/source.json" python3 "$rust_root/quickheap-dev/bench/profiles.py" \
        --profile original-rust --no-default-features --features avx2 --min "$minimum" --max "$maximum" "--$elem" --dry-run || return
    run original-build "$output/logs/original-build.stdout.log" python3 - "$directory/source.json" <<'PY'
import json, subprocess, sys
from pathlib import Path
plan = json.loads(Path(sys.argv[1]).read_text())
command = plan["command"][:plan["command"].index("--")]
command[1] = "build"
command.append("--verbose")
print(json.dumps(command), flush=True)
sys.exit(subprocess.run(command, cwd=plan["cwd"]).returncode)
PY
    if (( $? )); then return 1; fi
    local process reports=()
    for (( process=0; process<runs; ++process )); do
        local trial="$directory/process-$process"
        mkdir -- "$trial" || return
        local cpp_command=(taskset -c "$bench_cpu" "$cpp_bench" --profile original-rust
            --min "$minimum" --max "$maximum" "--$elem" --heap "$cpp_heap" "${cpp_backend[@]}")
        local rust_command=(taskset -c "$bench_cpu" python3 "$rust_root/quickheap-dev/bench/profiles.py"
            --profile original-rust --no-default-features --features avx2 --min "$minimum" --max "$maximum" "--$elem"
            --output "$trial/rust.csv" --raw-output "$trial/rust-raw.csv")
        if (( process % 2 == 0 )); then
            run "original-cpp-$process" "$trial/cpp.csv" "${cpp_command[@]}" || return
            run "original-rust-$process" "$output/logs/original-rust-$process.stdout.log" "${rust_command[@]}" || return
        else
            run "original-rust-$process" "$output/logs/original-rust-$process.stdout.log" "${rust_command[@]}" || return
            run "original-cpp-$process" "$trial/cpp.csv" "${cpp_command[@]}" || return
        fi
        run "original-report-$process" "$output/logs/original-report-$process.stdout.log" python3 "$report" \
            "$trial/cpp.csv" "$trial/rust.csv" --profile original-rust --run-metadata "$output/cpu.json" --output "$trial/report" --plot || return
        reports+=("$trial/report")
    done
    run original-aggregate "$output/logs/original-aggregate.stdout.log" python3 "$aggregate" \
        "${reports[@]}" --output "$directory/report"

}

controlled() {
    local directory="$output/controlled-operations" manifest="$rust_root/quickheap-dev/bench/paired/Cargo.toml"
    local exponent workload stem fixture index=0
    mkdir -p -- "$directory/fixtures" || return
    cd -- "$rust_root" || return
    run rust-tests "$output/logs/rust-tests.stdout.log" cargo test --manifest-path "$manifest" --locked "${rust_features[@]}" || return
    run rust-build "$output/logs/rust-build.stdout.log" cargo build --release --locked --verbose --message-format=json-render-diagnostics \
        --manifest-path "$manifest" --bin quickheap-paired "${rust_features[@]}" || return
    cd -- "$cpp_root" || return
    local rust_bench
    rust_bench=$(python3 - "$output/logs/rust-build.stdout.log" <<'PY'
import json, sys
from pathlib import Path
rows = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
paths = {row["executable"] for row in rows if row.get("reason") == "compiler-artifact"
         and row["target"]["name"] == "quickheap-paired" and row.get("executable")}
if len(paths) != 1:
    raise SystemExit("expected one Rust paired executable")
print(paths.pop())
PY
    ) || return
    local exponents=() stems=() fixtures=() reports=()
    for (( exponent=minimum; exponent<=maximum; exponent+=step )); do exponents+=("$exponent"); done
    if [[ ${exponents[${#exponents[@]}-1]} != "$maximum" ]]; then exponents+=("$maximum"); fi
    for workload in HeapSort MonotoneConstantSize MonotoneWiggle RandomWiggle; do
        for exponent in "${exponents[@]}"; do
            for seed in "${seeds[@]}"; do
                stem="$workload-$elem-$exponent-seed-$seed"
                fixture="$directory/fixtures/$stem.qhf"
                run "fixture-$stem" "$output/logs/fixture-$stem.stdout.log" "$cpp_fixture" \
                    --generate "$workload" --elem "$elem" --n "$((1 << exponent))" --seed "$seed" --output "$fixture" || return
                run "verify-cpp-$stem" "$directory/$stem.cpp.trace" taskset -c "$bench_cpu" "$cpp_bench" --profile controlled-operations \
                    --fixture "$fixture" --heap "$cpp_heap" "${cpp_backend[@]}" --verify-only || return
                run "verify-rust-$stem" "$directory/$stem.rust.trace" taskset -c "$bench_cpu" "$rust_bench" --profile controlled-operations \
                    --fixture "$fixture" --heap "$rust_heap" --verify-only || return
                run "verify-match-$stem" "$output/logs/verify-match-$stem.stdout.log" \
                    cmp "$directory/$stem.cpp.trace" "$directory/$stem.rust.trace" || return
                stems+=("$stem")
                fixtures+=("$fixture")
            done
        done
    done
    local process
    for (( process=0; process<runs; ++process )); do
        local trial="$directory/process-$process" results=()
        mkdir -- "$trial" || return
        for (( index=0; index<${#stems[@]}; ++index )); do
            stem=${stems[index]}; fixture=${fixtures[index]}
            local cpp_command=(taskset -c "$bench_cpu" "$cpp_bench" --profile controlled-operations --fixture "$fixture"
                --heap "$cpp_heap" "${cpp_backend[@]}" --repeat "$repeat" --warmup "$warmup")
            local rust_command=(taskset -c "$bench_cpu" "$rust_bench" --profile controlled-operations --fixture "$fixture"
                --heap "$rust_heap" --repeat "$repeat" --warmup "$warmup")
            if (( (process + index) % 2 == 0 )); then
                run "controlled-cpp-$stem-$process" "$trial/$stem.cpp.csv" "${cpp_command[@]}" || return
                run "controlled-rust-$stem-$process" "$trial/$stem.rust.csv" "${rust_command[@]}" || return
            else
                run "controlled-rust-$stem-$process" "$trial/$stem.rust.csv" "${rust_command[@]}" || return
                run "controlled-cpp-$stem-$process" "$trial/$stem.cpp.csv" "${cpp_command[@]}" || return
            fi
            results+=("$trial/$stem.cpp.csv" "$trial/$stem.rust.csv")
        done
        run "controlled-report-$process" "$output/logs/controlled-report-$process.stdout.log" python3 "$report" \
            "${results[@]}" --profile controlled-operations --fixtures "$directory/fixtures" --run-metadata "$output/cpu.json" --output "$trial/report" --plot || return
        reports+=("$trial/report")
    done
    run controlled-aggregate "$output/logs/controlled-aggregate.stdout.log" python3 "$aggregate" \
        "${reports[@]}" --output "$directory/report"

}

status=0
if [[ "$profile" == both || "$profile" == original-rust ]]; then
    if original; then printf 'original-rust\tpassed\n' >> "$output/status.tsv"
    else printf 'original-rust\tfailed\n' >> "$output/status.tsv"; status=1; fi
fi
if [[ "$profile" == both || "$profile" == controlled-operations ]]; then
    if controlled; then printf 'controlled-operations\tpassed\n' >> "$output/status.tsv"
    else printf 'controlled-operations\tfailed\n' >> "$output/status.tsv"; status=1; fi
fi
printf 'Results and logs: %s\n' "$output"
cat "$output/status.tsv"
exit "$status"
