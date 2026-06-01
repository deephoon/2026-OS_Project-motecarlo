#!/usr/bin/env sh
set -eu

TRIALS=${TRIALS:-100000}
STEPS=${STEPS:-50}
REPEATS=${REPEATS:-5}
SEED=${SEED:-42}
PRE_WORK=${PRE_WORK:-50000}
POST_WORK=${POST_WORK:-10000}
OUT_DIR=${OUT_DIR:-results/csv}
RAW_OUT=${RAW_OUT:-$OUT_DIR/final_raw.csv}
ANALYZED_OUT=${ANALYZED_OUT:-$OUT_DIR/final_analyzed.csv}
SUMMARY_OUT=${SUMMARY_OUT:-$OUT_DIR/final_summary.md}

make clean
make

mkdir -p "$OUT_DIR" results/raw

HEADER='case_name,repeat,mode,schedule,merge,sync,processes,threads,trials,steps,batch_size,queue_size,ipc,workload,skew_factor,pre_work,post_work,time_total,time_pre,time_compute,time_sync,time_ipc,time_merge,time_post,speedup,efficiency,sequential_fraction_estimate,compute_ratio,sync_overhead_ratio,ipc_overhead_ratio,merge_overhead_ratio,throughput_batches_per_sec,total_trials,collision_count,hist_sum,checksum,valid,lock_wait_count,cond_wait_count,queue_push_count,queue_pop_count,ipc_write_count,ipc_read_count,ipc_bytes,notes'
printf '%s\n' "$HEADER" > "$RAW_OUT"

run_case() {
    case_name=$1
    repeat=$2
    shift 2
    row=$(./sim "$@" \
        --trials "$TRIALS" \
        --steps "$STEPS" \
        --seed "$SEED" \
        --pre-work "$PRE_WORK" \
        --post-work "$POST_WORK" \
        --metrics-detail 1 | sed -n '2p')
    printf '%s,%s,%s\n' "$case_name" "$repeat" "$row" >> "$RAW_OUT"
}

for repeat in $(seq 1 "$REPEATS"); do
    run_case seq "$repeat" --mode seq

    for threads in 1 2 4 8; do
        run_case "thread_${threads}_reduce" "$repeat" \
            --mode thread --schedule static --merge final --threads "$threads" --sync reduce
    done

    run_case thread_4_mutex "$repeat" \
        --mode thread --schedule static --merge final --threads 4 --sync mutex
    run_case thread_4_nosync "$repeat" \
        --mode thread --schedule static --merge final --threads 4 --sync nosync

    for processes in 1 2 4; do
        run_case "process_${processes}_pipe" "$repeat" \
            --mode process --processes "$processes" --ipc pipe
        run_case "process_${processes}_shm" "$repeat" \
            --mode process --processes "$processes" --ipc shm
    done

    run_case hybrid_2x2 "$repeat" \
        --mode hybrid --processes 2 --threads 2 --ipc pipe
    run_case hybrid_2x4 "$repeat" \
        --mode hybrid --processes 2 --threads 4 --ipc pipe
    run_case hybrid_4x2 "$repeat" \
        --mode hybrid --processes 4 --threads 2 --ipc pipe
    run_case hybrid_2x4_shm "$repeat" \
        --mode hybrid --processes 2 --threads 4 --ipc shm

    run_case pipeline_final_b1000 "$repeat" \
        --mode pipeline --schedule queue --merge final --threads 4 --batch-size 1000 --queue-size 1024
    run_case pipeline_interactive_b1000 "$repeat" \
        --mode pipeline --schedule queue --merge interactive --threads 4 --batch-size 1000 --queue-size 1024

    for batch in 100 10000; do
        run_case "pipeline_interactive_b${batch}" "$repeat" \
            --mode pipeline --schedule queue --merge interactive --threads 4 --batch-size "$batch" --queue-size 1024
    done
done

if command -v python3 >/dev/null 2>&1; then
    python3 scripts/analyze_results.py "$RAW_OUT" "$ANALYZED_OUT" "$SUMMARY_OUT"
fi

printf 'wrote %s\n' "$RAW_OUT"
printf 'wrote %s\n' "$ANALYZED_OUT"
printf 'wrote %s\n' "$SUMMARY_OUT"
