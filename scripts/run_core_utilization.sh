#!/usr/bin/env sh
set -eu

THREADS=${THREADS:-4}
WORK_ITERS=${WORK_ITERS:-2000000000}
OUT_DIR=${OUT_DIR:-results/csv/core_util}
TIME_BIN=${TIME_BIN:-/usr/bin/time}
TIME_FLAG="-v"

mkdir -p "$OUT_DIR"

make >/dev/null

if ! "$TIME_BIN" -v true >/dev/null 2>/dev/null; then
    printf 'warning: %s -v is not available; CPU percent will be unavailable unless GNU time is used\n' "$TIME_BIN" >&2
    TIME_FLAG=""
fi

SIM_OUT="$OUT_DIR/sim_threads${THREADS}.csv"
TIME_OUT="$OUT_DIR/time_threads${THREADS}.txt"
MPSTAT_OUT="$OUT_DIR/mpstat_threads${THREADS}.txt"

MPSTAT_PID=""
if command -v mpstat >/dev/null 2>&1; then
    mpstat -P ALL 1 > "$MPSTAT_OUT" &
    MPSTAT_PID=$!
else
    printf 'warning: mpstat not found; install sysstat for per-core utilization\n' >&2
    printf 'mpstat not available\n' > "$MPSTAT_OUT"
fi

"$TIME_BIN" $TIME_FLAG ./sim \
    --mode ideal \
    --scaling utilization \
    --threads "$THREADS" \
    --work-iters "$WORK_ITERS" \
    --affinity on \
    > "$SIM_OUT" 2> "$TIME_OUT"

if [ -n "$MPSTAT_PID" ]; then
    kill "$MPSTAT_PID" >/dev/null 2>&1 || true
    wait "$MPSTAT_PID" 2>/dev/null || true
fi

printf 'wrote %s\n' "$SIM_OUT"
printf 'wrote %s\n' "$TIME_OUT"
printf 'wrote %s\n' "$MPSTAT_OUT"
