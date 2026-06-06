#!/usr/bin/env sh
set -eu

TRIALS=${TRIALS:-120000000}
STEPS=${STEPS:-50}
BATCH_SIZE=${BATCH_SIZE:-100000}
QUEUE_SIZE=${QUEUE_SIZE:-1024}
OUT_DIR=${OUT_DIR:-results/csv/real_utilization}
TIME_BIN=${TIME_BIN:-/usr/bin/time}
mpstat_pid=""

cleanup_mpstat() {
    if [ -n "$mpstat_pid" ]; then
        kill "$mpstat_pid" >/dev/null 2>&1 || true
        wait "$mpstat_pid" 2>/dev/null || true
        mpstat_pid=""
    fi
}

trap cleanup_mpstat EXIT HUP INT TERM

mkdir -p "$OUT_DIR"
make >/dev/null

if ! "$TIME_BIN" -v true >/dev/null 2>&1; then
    printf '%s does not support GNU -v output; run this script in Ubuntu/Docker/WSL or set TIME_BIN to GNU time\n' "$TIME_BIN" >&2
    exit 1
fi

run_case() {
    case_name=$1
    workers=$2
    shift 2

    sim_out="$OUT_DIR/sim_${case_name}.csv"
    time_out="$OUT_DIR/time_${case_name}.txt"
    mpstat_out="$OUT_DIR/mpstat_${case_name}.txt"
    mpstat_pid=""

    if command -v mpstat >/dev/null 2>&1; then
        mpstat -P ALL 1 > "$mpstat_out" &
        mpstat_pid=$!
    else
        printf 'mpstat not available\n' > "$mpstat_out"
    fi

    "$TIME_BIN" -v ./sim "$@" \
        --trials "$TRIALS" \
        --steps "$STEPS" \
        --batch-size "$BATCH_SIZE" \
        --queue-size "$QUEUE_SIZE" \
        --pre-work 0 \
        --post-work 0 \
        --affinity on \
        --core-count "$workers" \
        --metrics-detail 1 \
        > "$sim_out" 2> "$time_out"

    cleanup_mpstat
}

run_case thread_4_reduce 4 --mode thread --threads 4 --sync reduce
run_case process_4_shm 4 --mode process --processes 4 --ipc shm
run_case hybrid_2x2_shm 4 --mode hybrid --processes 2 --threads 2 --ipc shm
run_case pipeline_4_final 4 --mode pipeline --threads 4 --schedule queue --merge final

printf 'wrote utilization files under %s\n' "$OUT_DIR"
