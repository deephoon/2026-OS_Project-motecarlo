#!/usr/bin/env sh
set -eu

TRIALS=${TRIALS:-100000}
STEPS=${STEPS:-50}
SEED=${SEED:-42}
OUT=${OUT:-results/csv/final_results.csv}

make clean
make

mkdir -p results/csv results/raw

HEADER='mode,schedule,merge,sync,processes,threads,trials,steps,batch_size,queue_size,time_total,time_pre,time_compute,time_sync,time_merge,time_post,speedup,efficiency,sequential_fraction_estimate,compute_ratio,sync_overhead_ratio,merge_overhead_ratio,throughput_batches_per_sec,total_trials,collision_count,hist_sum,checksum,valid,notes'
printf '%s\n' "$HEADER" > "$OUT"

run_final_case() {
    ./sim "$@" --metrics-detail 1 | sed -n '2p' >> "$OUT"
}

run_final_case --mode seq --trials "$TRIALS" --steps "$STEPS" --seed "$SEED"

for threads in 1 2 4 8; do
    run_final_case --mode thread --schedule static --merge final --threads "$threads" --trials "$TRIALS" --steps "$STEPS" --sync reduce --seed "$SEED"
done

run_final_case --mode pipeline --schedule queue --merge interactive --threads 4 --trials "$TRIALS" --steps "$STEPS" --batch-size 1000 --queue-size 1024 --seed "$SEED"

run_final_case --mode pipeline --schedule queue --merge final --threads 4 --trials "$TRIALS" --steps "$STEPS" --batch-size 1000 --queue-size 1024 --seed "$SEED"
run_final_case --mode pipeline --schedule queue --merge interactive --threads 4 --trials "$TRIALS" --steps "$STEPS" --batch-size 1000 --queue-size 1024 --seed "$SEED"

for batch in 100 1000 10000; do
    run_final_case --mode pipeline --schedule queue --merge interactive --threads 4 --trials "$TRIALS" --steps "$STEPS" --batch-size "$batch" --queue-size 1024 --seed "$SEED"
done

run_final_case --mode process --processes 2 --trials "$TRIALS" --steps "$STEPS" --ipc pipe --seed "$SEED"
run_final_case --mode process --processes 4 --trials "$TRIALS" --steps "$STEPS" --ipc pipe --seed "$SEED"

run_final_case --mode hybrid --processes 2 --threads 2 --trials "$TRIALS" --steps "$STEPS" --ipc pipe --seed "$SEED"
run_final_case --mode hybrid --processes 2 --threads 4 --trials "$TRIALS" --steps "$STEPS" --ipc pipe --seed "$SEED"

printf 'wrote %s\n' "$OUT"
