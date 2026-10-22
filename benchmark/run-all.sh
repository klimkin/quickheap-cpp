#!/usr/bin/env bash
set -euo pipefail

if (( $# < 1 || $# > 3 )); then
    printf 'Usage: %s BINARY_DIR [OUTPUT_DIR [GRAPH_INPUT]]\n' "$0" >&2
    exit 2
fi

binary_dir=$1
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
output_dir=${2:-"$script_dir/evals/data"}
graph_input=${3:-}
bench="$binary_dir/quickheap_bench"
experiments="$binary_dir/quickheap_experiments"
graph_bench="$binary_dir/quickheap_bench_graph"
if [[ ! -x "$bench" ]]; then
    printf 'Missing executable: %s\n' "$bench" >&2
    exit 2
fi
mkdir -p "$output_dir"
common=(--min "${MIN:-10}" --max "${MAX:-20}" --step "${STEP:-2}" --repeat "${REPEAT:-3}")
memory_args=("${common[@]}")
if [[ -n ${HEAP:-} ]]; then common+=(--heap "$HEAP"); fi
if [[ -n ${MEMORY_HEAP:-} ]]; then memory_args+=(--heap "$MEMORY_HEAP"); fi
if [[ -n ${WORKLOAD:-} ]]; then
    common+=(--workload "$WORKLOAD")
    memory_args+=(--workload "$WORKLOAD")
fi
if [[ -n ${ELEM:-} ]]; then
    common+=("--$ELEM")
    memory_args+=("--$ELEM")
fi

run_csv() {
    local name=$1
    shift
    printf 'Writing %s\n' "$output_dir/$name.csv" >&2
    "$@" > "$output_dir/$name.csv.tmp"
    mv "$output_dir/$name.csv.tmp" "$output_dir/$name.csv"
}

run_csv plot "$bench" "${common[@]}"
run_csv table "$bench" "${common[@]}" --table
run_csv memory "$bench" "${memory_args[@]}" --memory
comparison_args=(--min "${MIN:-10}" --max "${MAX:-20}" --repeat "${REPEAT:-3}")
if [[ -n ${ELEM:-} ]]; then comparison_args+=("--$ELEM"); fi
if [[ -n ${WORKLOAD:-} ]]; then comparison_args+=(--workload "$WORKLOAD"); fi
run_csv comparisons "$bench" "${comparison_args[@]}" --comparisons
ablation_args=(--min "${MIN:-10}" --max "${MAX:-20}" --step "${STEP:-2}" --repeat "${REPEAT:-3}")
if [[ -n ${ELEM:-} ]]; then ablation_args+=("--$ELEM"); fi
if [[ -n ${WORKLOAD:-} ]]; then ablation_args+=(--workload "$WORKLOAD"); fi
run_csv ablation "$bench" "${ablation_args[@]}" --ablation

if [[ -x "$experiments" ]]; then
    experiment_args=(--min "${MIN:-10}" --max "${MAX:-20}" --step "${STEP:-2}" --repeat "${REPEAT:-3}")
    if [[ -n ${ELEM:-} ]]; then experiment_args+=("--$ELEM"); fi
    if [[ ${PERF:-0} == 1 ]]; then experiment_args+=(--perf); fi
    for mode in variants ablation comparisons partition rebalancing; do
        mode_args=("${experiment_args[@]}")
        if [[ "$mode" == variants && -n ${VARIANT:-} ]]; then mode_args+=(--variant "$VARIANT"); fi
        name=$mode
        if [[ "$mode" == ablation || "$mode" == comparisons ]]; then name="experiment-$mode"; fi
        run_csv "$name" "$experiments" "${mode_args[@]}" --mode "$mode"
    done
else
    printf 'Skipping experiments; missing %s\n' "$experiments" >&2
fi

if [[ -n "$graph_input" ]]; then
    if [[ ! -x "$graph_bench" ]]; then
        printf 'Missing executable: %s\n' "$graph_bench" >&2
        exit 2
    fi
    graph_args=(--repeat "${REPEAT:-3}")
    if [[ -n ${GRAPH_HEAP:-} ]]; then graph_args+=(--heap "$GRAPH_HEAP"); fi
    if [[ -n ${GRAPH_WORKLOAD:-} ]]; then graph_args+=(--workload "$GRAPH_WORKLOAD"); fi
    run_csv graphs "$graph_bench" "$graph_input" "${graph_args[@]}"
else
    printf 'Skipping graph benchmarks; no GRAPH_INPUT argument\n' >&2
fi
