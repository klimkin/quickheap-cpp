#!/usr/bin/env bash
set -euo pipefail

if (( $# != 2 )); then
    echo "usage: algorithms.sh QUICKHEAP_BENCH OUTPUT_DIR" >&2
    exit 2
fi
bench=$1
output=$2
cpu=${BENCH_CPU:-3}
mkdir -- "$output"

plot_heaps=std_priority_queue,dary8,weak_heap,radix_heap,sequence_heap,s3q,original_quickheap,scalar_quickheap,quickheap
table_heaps=$plot_heaps,custom_binary_heap,dary4,custom_dary4,custom_dary8,dary16,external_radix_heap,blocked_quickheap,boost_dary4
slow_heaps=boost_fibonacci,boost_pairing,boost_binomial,boost_skew

run() {
    local name=$1
    shift
    echo "$(date -u +%FT%TZ) $name" >&2
    taskset -c "$cpu" "$bench" --workload MonotoneConstantSize --perf "$@" > "$output/$name.csv"
}

{
    uname -a
    lscpu
    taskset -c "$cpu" "$bench" --list
} > "$output/machine.txt"
# Each pass times every size once, so a load burst on the shared host
# corrupts one sample per cell instead of all three.
for pass in 1 2 3; do
    run "plot-$pass" --i32 --i64 --min 10 --max 25 --heap "$plot_heaps" --repeat 1
done
run table --u64 --min 10 --max 20 --step 5 --heap "$table_heaps,$slow_heaps" --repeat 3
# One 2^25 trial of a slow heap runs 10-20 minutes, so these sizes skip warmup.
run table-25 --u64 --min 25 --max 25 --heap "$table_heaps" --repeat 3 --warmup 0
run slow-25 --u64 --min 25 --max 25 --heap "$slow_heaps" --repeat 1 --warmup 0
taskset -c "$cpu" "$bench" --u32 --u64 --workload MonotoneConstantSize --perf \
    --min 20 --max 20 --heap quickheap,kv_quickheap --repeat 5 > "$output/types.csv"
echo "$(date -u +%FT%TZ) done" >&2
