#!/usr/bin/env sh
set -eu

THREADS=${THREADS:-4}
PROCESSES=${PROCESSES:-4}
WORK_ITERS=${WORK_ITERS:-2000000000}
MODE=${MODE:-ideal}
PROFILE=${PROFILE:-default}
TRIALS=${TRIALS:-1000000}
STEPS=${STEPS:-100}
IPC=${IPC:-shm}
OUT_DIR=${OUT_DIR:-results/csv/core_util}
TIME_BIN=${TIME_BIN:-/usr/bin/time}
TIME_FLAG="-v"

mkdir -p "$OUT_DIR"

make >/dev/null

if ! "$TIME_BIN" -v true >/dev/null 2>/dev/null; then
    printf 'warning: %s -v is not available; CPU percent will be unavailable unless GNU time is used\n' "$TIME_BIN" >&2
    TIME_FLAG=""
fi

if [ "$MODE" = "ideal" ] || [ "$MODE" = "thread" ]; then
    N=$THREADS
else
    N=$PROCESSES
fi

SIM_OUT="$OUT_DIR/sim_${MODE}_${N}.csv"
TIME_OUT="$OUT_DIR/time_${MODE}_${N}.txt"
MPSTAT_OUT="$OUT_DIR/mpstat_${MODE}_${N}.txt"

MPSTAT_PID=""
if command -v mpstat >/dev/null 2>&1; then
    mpstat -P ALL 1 > "$MPSTAT_OUT" &
    MPSTAT_PID=$!
else
    printf 'warning: mpstat not found; install sysstat for per-core utilization\n' >&2
    printf 'mpstat not available\n' > "$MPSTAT_OUT"
fi

if [ "$MODE" = "ideal" ]; then
    "$TIME_BIN" $TIME_FLAG ./sim \
        --mode ideal \
        --scaling utilization \
        --threads "$THREADS" \
        --work-iters "$WORK_ITERS" \
        --affinity on \
        > "$SIM_OUT" 2> "$TIME_OUT"
elif [ "$MODE" = "process" ]; then
    "$TIME_BIN" $TIME_FLAG ./sim \
        --mode process \
        --processes "$PROCESSES" \
        --ipc "$IPC" \
        --profile "$PROFILE" \
        --trials "$TRIALS" \
        --steps "$STEPS" \
        --affinity on \
        --metrics-detail 1 \
        > "$SIM_OUT" 2> "$TIME_OUT"
elif [ "$MODE" = "thread" ]; then
    "$TIME_BIN" $TIME_FLAG ./sim \
        --mode thread \
        --threads "$THREADS" \
        --sync reduce \
        --profile "$PROFILE" \
        --trials "$TRIALS" \
        --steps "$STEPS" \
        --affinity on \
        --metrics-detail 1 \
        > "$SIM_OUT" 2> "$TIME_OUT"
elif [ "$MODE" = "hybrid" ]; then
    "$TIME_BIN" $TIME_FLAG ./sim \
        --mode hybrid \
        --processes "$PROCESSES" \
        --threads "$THREADS" \
        --ipc "$IPC" \
        --profile "$PROFILE" \
        --trials "$TRIALS" \
        --steps "$STEPS" \
        --affinity on \
        --metrics-detail 1 \
        > "$SIM_OUT" 2> "$TIME_OUT"
else
    printf 'unsupported MODE=%s; use ideal, process, thread, or hybrid\n' "$MODE" >&2
    exit 2
fi

if [ -n "$MPSTAT_PID" ]; then
    kill "$MPSTAT_PID" >/dev/null 2>&1 || true
    wait "$MPSTAT_PID" 2>/dev/null || true
fi

printf 'wrote %s\n' "$SIM_OUT"
printf 'wrote %s\n' "$TIME_OUT"
printf 'wrote %s\n' "$MPSTAT_OUT"
