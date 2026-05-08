# Capture Checklist

This checklist is for the final presentation. Capture in Docker Ubuntu Linux when possible, because the final evaluation target is Linux.

## 1. Project Definition

Capture target:

- README title and first paragraph.
- Emphasize that this is a CPU-bound OS parallel processing experiment, not a vehicle simulator.

Suggested command:

```sh
sed -n '1,80p' README.md
```

Use in slides:

- Slide 1: project definition
- Slide 2: project goal

## 2. Build Evidence

Capture target:

- Successful clean build.
- No gcc warning output.

Commands:

```sh
make clean
make
make test
```

Use in slides:

- Implementation and execution environment
- Verification evidence

## 3. CLI and Program Structure

Capture target:

- `--mode`, `--schedule`, `--merge`, `--processes`, `--ipc`, `--batch-size`, `--queue-size`.

Command:

```sh
./sim --help
```

Use in slides:

- System overview
- Experiment setup

## 4. Sequential Baseline

Capture target:

- `seq` mode row.
- `valid=1`, `hist_sum=trials`.

Command:

```sh
./sim --mode seq --trials 10000 --steps 30 --seed 42
```

Use in slides:

- Baseline result
- Speedup calculation reference

## 5. Thread Static Reduce

Capture target:

- pthread-based thread mode.
- same checksum as sequential.

Command:

```sh
./sim --mode thread --schedule static --threads 4 --trials 10000 --steps 30 --sync reduce --seed 42
```

Use in slides:

- Thread scalability
- correctness validation

## 6. Race Condition Evidence

Capture target:

- `nosync` output with `valid=0`.
- `hist_sum` smaller than `trials`.

Command:

```sh
./sim --mode thread --threads 4 --trials 10000 --steps 30 --sync nosync --seed 42
```

If `valid=1` appears due scheduling timing, increase trials:

```sh
./sim --mode thread --threads 8 --trials 1000000 --steps 30 --sync nosync --seed 42
```

Use in slides:

- Synchronization correctness
- race condition proof

## 7. Mutex vs Local Reduce

Capture target:

- `mutex` valid result.
- `reduce` valid result.
- Compare `time_total`.

Commands:

```sh
./sim --mode thread --threads 4 --trials 100000 --steps 50 --sync mutex --seed 42
./sim --mode thread --threads 4 --trials 100000 --steps 50 --sync reduce --seed 42
```

Use in slides:

- lock contention
- local reduce optimization

## 8. Pipeline and Interactive Merge

Capture target:

- pipeline mode with `schedule=queue`.
- `merge=interactive`.
- `time_pre`, `time_sync`, `time_merge`, `throughput_batches_per_sec`.

Command:

```sh
./sim --mode pipeline --schedule queue --merge interactive --threads 4 \
  --trials 100000 --steps 50 --batch-size 1000 --queue-size 1024 --seed 42
```

Use in slides:

- pipeline structure
- task queue
- interactive merge
- throughput

## 9. Final Reduce vs Interactive Merge

Capture target:

- two rows with the same workload.
- compare `time_sync`, `time_merge`, and total time.

Commands:

```sh
./sim --mode pipeline --schedule queue --merge final --threads 4 \
  --trials 100000 --steps 50 --batch-size 1000 --queue-size 1024 --seed 42

./sim --mode pipeline --schedule queue --merge interactive --threads 4 \
  --trials 100000 --steps 50 --batch-size 1000 --queue-size 1024 --seed 42
```

Use in slides:

- final reduce vs interactive merge comparison

## 10. Process Mode

Capture target:

- child process mode with `processes=2` or `4`.
- `ipc=pipe`.
- `valid=1`.

Command:

```sh
./sim --mode process --processes 2 --trials 10000 --steps 30 --ipc pipe --seed 42
```

Use in slides:

- child process implementation
- pipe IPC merge

## 11. Hybrid Mode

Capture target:

- process + thread execution.
- `processes=2`, `threads=2` or `4`.
- `valid=1`.

Command:

```sh
./sim --mode hybrid --processes 2 --threads 2 --trials 10000 --steps 30 --ipc pipe --seed 42
```

Use in slides:

- process/thread role separation
- final extension result

## 12. Final Experiment CSV

Capture target:

- script execution.
- generated CSV first rows.

Commands:

```sh
TRIALS=10000 STEPS=30 scripts/run_final.sh
sed -n '1,18p' results/csv/final_results.csv
```

Use in slides:

- experiment setup
- performance table
- graph source

## 13. Docker Linux Evidence

Capture target:

- Docker image build.
- Docker-internal script execution.

Commands:

```sh
docker build -t os-montecarlo-risk .
docker run --rm os-montecarlo-risk sh -c 'TRIALS=10000 STEPS=30 scripts/run_final.sh'
```

Use in slides:

- reproducible Linux environment

## 14. CPU Utilization

Terminal 1:

```sh
./sim --mode pipeline --schedule queue --merge interactive --threads 4 \
  --trials 10000000 --steps 100 --batch-size 1000 --queue-size 1024 --seed 42
```

Terminal 2:

```sh
pidstat -u -r -C sim 1
```

Alternative:

```sh
top
/usr/bin/time -v ./sim --mode hybrid --processes 2 --threads 4 --trials 1000000 --steps 100 --ipc pipe --seed 42
```

Use in slides:

- CPU utilization
- memory usage
- resource analysis

## 15. Code Capture Points

Capture these files and line regions in an editor:

| File | Capture Point |
| --- | --- |
| `src/task_queue.c` | mutex + condition variable push/pop |
| `src/merge_queue.c` | interactive merge queue |
| `src/pipeline_mode.c` | preprocessor, workers, aggregator |
| `src/process_mode.c` | fork, pipe, waitpid |
| `src/hybrid_mode.c` | child process with pthread workers |
| `src/simulation.c` | CPU-bound trial loop |
| `src/main.c` | mode dispatch and final CSV output |

Do not spend slide space on every line of code. Use code screenshots only as implementation evidence, then explain OS concepts: producer-consumer queue, race condition, lock contention, IPC, and Amdahl-style stage timing.
