#!/usr/bin/env bash
set -euo pipefail

if (( $# < 3 || $# > 4 )); then
    echo "usage: factorial.sh QUICKHEAP_BENCH OUTPUT_DIR OPTS[,OPTS...] [PASSES=5]" >&2
    exit 2
fi
bench=$1
output=$2
opts=$3
passes=${4:-5}
cpu=${BENCH_CPU:-3}
evals=$(dirname "$0")
mkdir -- "$output"

heaps=$(python3 "$evals/factorial.py" names "$opts")
{
    uname -a
    lscpu
    taskset -c "$cpu" "$bench" --list
} > "$output/machine.txt"
for (( pass = 1; pass <= passes; pass++ )); do
    order=$(python3 "$evals/factorial.py" shuffle "$pass" "$heaps")
    echo "$(date -u +%FT%TZ) pass-$pass $(cut -d" " -f1-3 /proc/loadavg) $order" | tee -a "$output/load.txt" >&2
    taskset -c "$cpu" "$bench" --workload MonotoneConstantSize,RandomConstantSize,WorstCaseDescending,MostlyPushDecreasing,HeapSort \
        --perf --i32 --i64 --min 10 --max 22 --step 2 --heap "$order" --repeat 3 > "$output/pass-$pass.csv"
done
echo "$(date -u +%FT%TZ) done" >&2
