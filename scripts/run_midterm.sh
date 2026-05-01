#!/usr/bin/env sh
set -eu

TRIALS=${TRIALS:-1000000}
STEPS=${STEPS:-50}
SEED=${SEED:-42}
OUT=${OUT:-results/csv/midterm_results.csv}
RAW=${RAW:-results/raw/midterm_raw.csv}

make clean
make

mkdir -p results/csv results/raw

printf 'mode,sync,threads,trials,steps,time_sec,speedup,total_trials,collision_count,hist_sum,checksum,valid\n' > "$RAW"

run_case() {
    ./sim "$@" >> "$RAW"
}

run_case --mode seq --trials "$TRIALS" --steps "$STEPS" --seed "$SEED"

for threads in 1 2 4 8; do
    run_case --mode thread --threads "$threads" --trials "$TRIALS" --steps "$STEPS" --sync reduce --seed "$SEED"
done

run_case --mode thread --threads 4 --trials "$TRIALS" --steps "$STEPS" --sync nosync --seed "$SEED" || true
run_case --mode thread --threads 4 --trials "$TRIALS" --steps "$STEPS" --sync mutex --seed "$SEED"
run_case --mode thread --threads 4 --trials "$TRIALS" --steps "$STEPS" --sync reduce --seed "$SEED"

for steps in 10 50 100; do
    run_case --mode thread --threads 4 --trials "$TRIALS" --steps "$steps" --sync reduce --seed "$SEED"
done

awk -F, '
BEGIN { OFS = "," }
NR == 1 { print; next }
NR == 2 { base = $6 }
{
    $7 = ($6 > 0.0) ? sprintf("%.6f", base / $6) : "0.000000";
    print
}
' "$RAW" > "$OUT"

printf 'wrote %s\n' "$OUT"
