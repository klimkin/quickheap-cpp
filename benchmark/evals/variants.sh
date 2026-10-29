#!/usr/bin/env bash
set -euo pipefail

if (( $# < 2 || $# > 3 )); then
    echo "usage: variants.sh QUICKHEAP_BENCH OUTPUT_DIR [PASSES=7]" >&2
    exit 2
fi
bench=$1
output=$2
passes=${3:-7}
cpu=${BENCH_CPU:-3}
min=${BENCH_MIN:-10}
max=${BENCH_MAX:-22}
mkdir -- "$output"

heaps=quickheap,quickheap_a1,quickheap_a2,quickheap_a3,quickheap_a4,quickheap_a5
heaps=$heaps,quickheap_a12,quickheap_a123,quickheap_a1234,quickheap_a12345,quickheap_aa
workloads=MonotoneConstantSize,RandomConstantSize,WorstCaseDescending,MostlyPushDecreasing,HeapSort

{
    uname -a
    lscpu
    taskset -c "$cpu" "$bench" --list
} > "$output/machine.txt"
for (( pass = 1; pass <= passes; pass++ )); do
    echo "$(date -u +%FT%TZ) pass-$pass $(cut -d" " -f1-3 /proc/loadavg)" | tee -a "$output/load.txt" >&2
    taskset -c "$cpu" "$bench" --workload "$workloads" --perf --i32 --i64 \
        --min "$min" --max "$max" --step 2 --heap "$heaps" --repeat 3 > "$output/pass-$pass.csv"
done
echo "$(date -u +%FT%TZ) done" >&2
