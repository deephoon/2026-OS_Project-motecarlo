#!/usr/bin/env sh
set -eu

WORK_ITERS=${WORK_ITERS:-4000000000}
REPEATS=${REPEATS:-3}
THREAD_LIST=${THREAD_LIST:-"1 2 4 8"}
OUT_DIR=${OUT_DIR:-results/csv/ideal}
RAW_OUT=${RAW_OUT:-$OUT_DIR/ideal_strong_raw.csv}
TIME_DIR=${TIME_DIR:-$OUT_DIR/time}
TIME_BIN=${TIME_BIN:-/usr/bin/time}
TIME_FLAG="-v"

mkdir -p "$OUT_DIR" "$TIME_DIR"

make >/dev/null

if ! "$TIME_BIN" -v true >/dev/null 2>/dev/null; then
    printf 'warning: %s -v is not available; CPU percent will be 0 unless GNU time is used\n' "$TIME_BIN" >&2
    TIME_FLAG=""
fi

printf 'threads,repeat,work_iters,time_total,cpu_percent\n' > "$RAW_OUT"

parse_cpu_percent() {
    awk -F: '/Percent of CPU this job got/ {
        gsub(/%/, "", $2);
        gsub(/^[ \t]+/, "", $2);
        print $2;
    }' "$1"
}

for threads in $THREAD_LIST; do
    repeat=1
    while [ "$repeat" -le "$REPEATS" ]; do
        sim_out="$TIME_DIR/sim_threads${threads}_repeat${repeat}.csv"
        time_out="$TIME_DIR/time_threads${threads}_repeat${repeat}.txt"

        "$TIME_BIN" $TIME_FLAG ./sim \
            --mode ideal \
            --scaling strong \
            --threads "$threads" \
            --work-iters "$WORK_ITERS" \
            --affinity on \
            > "$sim_out" 2> "$time_out"

        time_total=$(awk -F, 'NR==2 {print $6}' "$sim_out")
        cpu_percent=$(parse_cpu_percent "$time_out")
        if [ -z "$cpu_percent" ]; then
            cpu_percent=0
        fi
        printf '%s,%s,%s,%s,%s\n' \
            "$threads" "$repeat" "$WORK_ITERS" "$time_total" "$cpu_percent" \
            >> "$RAW_OUT"

        repeat=$((repeat + 1))
    done
done

printf 'wrote %s\n' "$RAW_OUT"
