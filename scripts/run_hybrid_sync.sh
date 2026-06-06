#!/usr/bin/env sh
set -eu

TRIALS=${TRIALS:-1000000}
STEPS=${STEPS:-50}
REPEATS=${REPEATS:-5}
SEED=${SEED:-42}
PROCESSES=${PROCESSES:-2}
THREADS=${THREADS:-2}
IPC=${IPC:-shm}
OUT_DIR=${OUT_DIR:-results/csv/hybrid_sync}
RAW_OUT=${RAW_OUT:-$OUT_DIR/hybrid_sync_raw.csv}
ANALYZED_OUT=${ANALYZED_OUT:-$OUT_DIR/hybrid_sync_summary.csv}

mkdir -p "$OUT_DIR"
make >/dev/null

HEADER='case_name,repeat,mode,schedule,merge,sync,processes,threads,trials,steps,batch_size,queue_size,ipc,workload,skew_factor,pre_work,post_work,time_total,time_pre,time_compute,time_sync,time_ipc,time_merge,time_post,speedup,efficiency,sequential_fraction_estimate,compute_ratio,sync_overhead_ratio,ipc_overhead_ratio,merge_overhead_ratio,throughput_batches_per_sec,total_trials,collision_count,hist_sum,checksum,valid,lock_wait_count,cond_wait_count,queue_push_count,queue_pop_count,ipc_write_count,ipc_read_count,ipc_bytes,notes,profile,inner_work'
printf '%s\n' "$HEADER" > "$RAW_OUT"

run_case() {
    case_name=$1
    repeat=$2
    shift 2
    row=$(./sim "$@" \
        --trials "$TRIALS" \
        --steps "$STEPS" \
        --seed "$SEED" \
        --metrics-detail 1 | sed -n '2p')
    printf '%s,%s,%s\n' "$case_name" "$repeat" "$row" >> "$RAW_OUT"
}

repeat=1
while [ "$repeat" -le "$REPEATS" ]; do
    run_case seq "$repeat" --mode seq

    for sync in nosync mutex reduce; do
        run_case "hybrid_${PROCESSES}x${THREADS}_${sync}_${IPC}" "$repeat" \
            --mode hybrid --processes "$PROCESSES" --threads "$THREADS" \
            --sync "$sync" --ipc "$IPC"
    done

    for sync in nosync mutex reduce; do
        run_case "thread_${THREADS}_${sync}" "$repeat" \
            --mode thread --threads "$THREADS" --sync "$sync"
    done

    repeat=$((repeat + 1))
done

python3 scripts/analyze_results.py "$RAW_OUT" "$ANALYZED_OUT"
printf 'wrote %s and %s\n' "$RAW_OUT" "$ANALYZED_OUT"
