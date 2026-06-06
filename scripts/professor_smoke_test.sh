#!/usr/bin/env sh
set -eu

echo "============================================================"
echo "OS Monte Carlo Risk - Professor Smoke Test"
echo "This script builds the project and runs representative modes."
echo "Expected: build succeeds and simulation rows show valid=1."
echo "============================================================"

echo "[0/9] Build: make clean && make"
make clean
make

echo "[1/9] Basic regression test: make test"
make test

echo "[2/9] Actual Monte Carlo: sequential baseline"
./sim --mode seq --trials 10000 --steps 30

echo "[3/9] Actual Monte Carlo: pthread local reduce"
./sim --mode thread --threads 4 --sync reduce \
    --trials 10000 --steps 30

echo "[4/9] Actual Monte Carlo: child process + shared memory IPC"
./sim --mode process --processes 4 --ipc shm \
    --trials 10000 --steps 30

echo "[5/9] Actual Monte Carlo: child process + pipe IPC"
./sim --mode process --processes 4 --ipc pipe \
    --trials 10000 --steps 30

echo "[6/9] Actual Monte Carlo: child process + pthread + shared memory IPC"
./sim --mode hybrid --processes 2 --threads 4 --ipc shm \
    --trials 10000 --steps 30

echo "[7/9] Actual Monte Carlo: task queue + final merge"
./sim --mode pipeline --threads 4 --schedule queue --merge final \
    --trials 10000 --steps 30

echo "[8/9] Actual Monte Carlo: task queue + interactive merge"
./sim --mode pipeline --threads 4 --schedule queue --merge interactive \
    --trials 10000 --steps 30

echo "[9/9] Actual Monte Carlo: pipeline CPU affinity option"
./sim --mode pipeline --threads 4 --schedule queue --merge final \
    --trials 10000 --steps 30 --affinity on --core-count 4

echo "============================================================"
echo "Professor smoke test completed."
echo "If the script reached this line, the submission is executable."
echo "For performance interpretation, see README.md and SUBMISSION.md."
echo "============================================================"
