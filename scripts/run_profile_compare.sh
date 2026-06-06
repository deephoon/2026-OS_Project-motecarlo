#!/usr/bin/env sh
set -eu

TRIALS=${TRIALS:-1000000}
STEPS=${STEPS:-100}
REPEATS=${REPEATS:-5}
SEED=${SEED:-42}
OUT_DIR=${OUT_DIR:-results/csv/profile_compare}
RAW_OUT=${RAW_OUT:-$OUT_DIR/profile_compare_raw.csv}

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

for repeat in $(seq 1 "$REPEATS"); do
    run_case seq_process_friendly "$repeat" \
        --mode seq --profile process_friendly

    for processes in 1 2 4; do
        run_case "process_${processes}_shm_process_friendly" "$repeat" \
            --mode process --processes "$processes" --ipc shm --profile process_friendly
        run_case "process_${processes}_pipe_process_friendly" "$repeat" \
            --mode process --processes "$processes" --ipc pipe --profile process_friendly
    done

    run_case thread_4_reduce_process_friendly "$repeat" \
        --mode thread --threads 4 --sync reduce --profile process_friendly

    run_case seq_thread_friendly "$repeat" \
        --mode seq --profile thread_friendly

    for threads in 1 2 4 8; do
        run_case "thread_${threads}_reduce_thread_friendly" "$repeat" \
            --mode thread --threads "$threads" --sync reduce --profile thread_friendly
    done

    run_case process_4_shm_thread_friendly "$repeat" \
        --mode process --processes 4 --ipc shm --profile thread_friendly

    run_case hybrid_2x2_process_friendly "$repeat" \
        --mode hybrid --processes 2 --threads 2 --ipc shm --profile process_friendly
    run_case hybrid_2x4_process_friendly "$repeat" \
        --mode hybrid --processes 2 --threads 4 --ipc shm --profile process_friendly
    run_case hybrid_4x2_process_friendly "$repeat" \
        --mode hybrid --processes 4 --threads 2 --ipc shm --profile process_friendly
done

printf 'wrote %s\n' "$RAW_OUT"
