#!/usr/bin/env sh
set -eu

TRIALS=${TRIALS:-10000}
STEPS=${STEPS:-30}
SEED=${SEED:-42}
baseline_checksum=""

run_and_check() {
    label=$1
    shift
    row=$(./sim "$@" \
        --trials "$TRIALS" \
        --steps "$STEPS" \
        --seed "$SEED" \
        --metrics-detail 0)
    checksum=$(printf '%s\n' "$row" | awk -F, '{print $11}')
    valid=$(printf '%s\n' "$row" | awk -F, '{print $12}')

    if [ "$valid" != "1" ]; then
        printf 'FAIL %-22s valid=%s\n' "$label" "$valid" >&2
        exit 1
    fi
    if [ -z "$baseline_checksum" ]; then
        baseline_checksum=$checksum
    elif [ "$checksum" != "$baseline_checksum" ]; then
        printf 'FAIL %-22s checksum=%s expected=%s\n' \
            "$label" "$checksum" "$baseline_checksum" >&2
        exit 1
    fi
    printf 'PASS %-22s checksum=%s\n' "$label" "$checksum"
}

run_and_check seq --mode seq
run_and_check thread_reduce --mode thread --threads 4 --sync reduce
run_and_check process_pipe --mode process --processes 2 --ipc pipe
run_and_check process_shm --mode process --processes 2 --ipc shm
run_and_check hybrid_reduce --mode hybrid --processes 2 --threads 2 --ipc shm --sync reduce
run_and_check hybrid_mutex --mode hybrid --processes 2 --threads 2 --ipc shm --sync mutex
run_and_check pipeline_final --mode pipeline --threads 4 --schedule queue --merge final
run_and_check pipeline_interactive --mode pipeline --threads 4 --schedule queue --merge interactive

printf 'all normal modes are valid and checksum-consistent\n'
