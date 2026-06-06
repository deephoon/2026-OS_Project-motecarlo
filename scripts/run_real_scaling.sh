#!/usr/bin/env sh
set -eu

TRIALS=${TRIALS:-120000000}
STEPS=${STEPS:-50}
REPEATS=${REPEATS:-5}
BATCH_SIZE=${BATCH_SIZE:-100000}
QUEUE_SIZE=${QUEUE_SIZE:-1024}
OUT_DIR=${OUT_DIR:-results/csv/real_scaling}
RAW_OUT=${RAW_OUT:-$OUT_DIR/real_scaling_raw.csv}
TIME_DIR=${TIME_DIR:-$OUT_DIR/time}
TIME_BIN=${TIME_BIN:-/usr/bin/time}

mkdir -p "$OUT_DIR" "$TIME_DIR"
make >/dev/null

if ! "$TIME_BIN" -v true >/dev/null 2>&1; then
    printf '%s does not support GNU -v output; run this script in Ubuntu/Docker/WSL or set TIME_BIN to GNU time\n' "$TIME_BIN" >&2
    exit 1
fi

printf 'case_name,mode,workers,processes,threads,repeat,trials,steps,time_total,cpu_percent,valid,checksum\n' > "$RAW_OUT"

parse_cpu_percent() {
    awk -F: '/Percent of CPU this job got/ {
        gsub(/%/, "", $2);
        gsub(/^[ \t]+/, "", $2);
        print $2;
    }' "$1"
}

run_case() {
    case_name=$1
    mode=$2
    workers=$3
    processes=$4
    threads=$5
    repeat=$6
    shift 6

    sim_out="$TIME_DIR/sim_${case_name}_repeat${repeat}.csv"
    time_out="$TIME_DIR/time_${case_name}_repeat${repeat}.txt"

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

    time_total=$(awk -F, 'NR==2 {print $16}' "$sim_out")
    checksum=$(awk -F, 'NR==2 {print $34}' "$sim_out")
    valid=$(awk -F, 'NR==2 {print $35}' "$sim_out")
    cpu_percent=$(parse_cpu_percent "$time_out")
    [ -n "$cpu_percent" ] || cpu_percent=0

    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
        "$case_name" "$mode" "$workers" "$processes" "$threads" "$repeat" \
        "$TRIALS" "$STEPS" "$time_total" "$cpu_percent" "$valid" "$checksum" \
        >> "$RAW_OUT"
}

repeat=1
while [ "$repeat" -le "$REPEATS" ]; do
    run_case seq seq 1 1 1 "$repeat" --mode seq

    for threads in 1 2 4 8; do
        run_case "thread_${threads}_reduce" thread "$threads" 1 "$threads" "$repeat" \
            --mode thread --threads "$threads" --sync reduce
    done

    for processes in 1 2 4; do
        run_case "process_${processes}_shm" process "$processes" "$processes" 1 "$repeat" \
            --mode process --processes "$processes" --ipc shm
    done

    run_case hybrid_2x2_shm hybrid 4 2 2 "$repeat" \
        --mode hybrid --processes 2 --threads 2 --ipc shm
    run_case hybrid_2x4_shm hybrid 8 2 4 "$repeat" \
        --mode hybrid --processes 2 --threads 4 --ipc shm

    for threads in 1 2 4 8; do
        run_case "pipeline_${threads}_final" pipeline "$threads" 1 "$threads" "$repeat" \
            --mode pipeline --threads "$threads" --schedule queue --merge final
    done

    repeat=$((repeat + 1))
done

printf 'wrote %s\n' "$RAW_OUT"
