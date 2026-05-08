# Monte Carlo Car-Following Risk Simulation

CPU-bound parallel processing and synchronization analysis for an operating systems course project.

This project is not a realistic vehicle simulator. The car-following scenario is a compact CPU-bound workload used to expose operating system concepts: child process, pthread-based multithreading, synchronization, race condition, task queue, pipeline, IPC, local reduce, stage timing, throughput, and process/thread/hybrid performance comparison.

## Project Goal

The goal is to compare the same Monte Carlo workload under different execution structures:

| Target | Implementation |
| --- | --- |
| Sequential baseline | Single-thread execution |
| Thread parallelism | pthread static partition and queue scheduling |
| Synchronization comparison | nosync, mutex, local reduce |
| Pipeline | pre-processing, task queue, worker pool, merge queue, post-processing |
| Process mode | fork-based child process execution |
| IPC merge | pipe-based child result transfer |
| Hybrid mode | child processes with process-local pthread workers |
| Analysis | stage timing, throughput, validation, Amdahl-style ratios |

## Feedback Reflected

After the midterm version, the design was extended to avoid looking like a purely embarrassingly parallel loop.

| Feedback | Reflection in This Version |
| --- | --- |
| Main computation should have pre/post stages | Added pre-processing and post-processing stages |
| Amdahl's Law should be analyzable | CSV includes stage timing and sequential fraction estimate |
| mutex alone is insufficient for queue empty/full | Added mutex + condition variable bounded queues |
| final reduce is too simple | Added interactive merge through a merge queue and aggregator thread |
| child process must be meaningful | Added fork + pipe based process mode |
| process/thread roles should differ | Added hybrid mode: process-level partition + thread-level local work |

## Architecture

```text
CLI Config
   |
   v
Pre-processing Stage
   - split trials into TaskBatch units
   - assign deterministic trial ranges
   |
   v
Task Queue
   - bounded queue
   - pthread_mutex_t protects queue state
   - pthread_cond_t handles empty/full wait and wakeup
   |
   v
Parallel Simulation Workers
   - pthread workers pop TaskBatch
   - run Monte Carlo trials
   - produce PartialResult
   |
   v
Merge Stage
   - final reduce or interactive aggregator
   |
   v
Post-processing Stage
   - histogram sum validation
   - checksum
   - collision probability and risk ratios
   |
   v
CSV Output
```

## Simulation Model

Each trial is independent:

1. Generate ego/front vehicle state from deterministic seed.
2. Iterate `steps` time steps.
3. Update speed and position.
4. Compute relative distance, relative speed, and TTC.
5. Classify result into safe, low, medium, high, or collision.
6. Aggregate histogram, collision count, and checksum.

The seed policy keeps results reproducible even when scheduling order changes:

```text
trial_seed = base_seed ^ (trial_index * 2654435761u)
```

## Execution Modes

| Mode | Command Value | Description |
| --- | --- | --- |
| Sequential | `--mode seq` | Baseline single-thread execution |
| Thread static | `--mode thread --schedule static` | pthread static trial range partition |
| Pipeline queue | `--mode pipeline --schedule queue` | task queue + worker pool + merge queue |
| Process | `--mode process` | parent forks child processes, children send results through pipe |
| Hybrid | `--mode hybrid` | each child process runs process-local pthread workers |

## Synchronization Modes

| Mode | Behavior | Purpose |
| --- | --- | --- |
| `nosync` | Shared result is updated without a lock | Intentionally demonstrates race condition |
| `mutex` | Each trial update is protected by mutex | Correct but lock-heavy |
| `reduce` | Each worker accumulates local result and merges later | Correct with less hot-loop synchronization |

`nosync` can produce `hist_sum != trials` and `valid=0`. That is intentional evidence that shared counters and histograms require synchronization.

## Merge Modes

| Mode | Description |
| --- | --- |
| `--merge final` | Workers finish all batches first, then main thread merges partial results |
| `--merge interactive` | Workers push each `PartialResult` to a merge queue; an aggregator thread merges continuously |

Interactive merge models a more realistic producer-consumer system where results are consumed while computation is still running. It adds queue synchronization overhead, so it should be analyzed against final reduce rather than assumed to be always faster.

## CLI

```text
--mode <seq|thread|pipeline|process|hybrid>
--schedule <static|queue>
--merge <final|interactive>
--trials <int>
--steps <int>
--threads <int>
--processes <int>
--batch-size <int>
--queue-size <int>
--sync <nosync|mutex|reduce>
--ipc <pipe|shm>
--enable-pipeline <0|1>
--metrics-detail <0|1>
--seed <int>
--verbose
--help
```

Defaults:

```text
mode=thread, schedule=static, merge=final, trials=100000, steps=50,
threads=4, processes=2, batch_size=1000, queue_size=1024,
sync=reduce, ipc=pipe, seed=42, metrics_detail=1
```

## Build

```sh
make clean
make
make test
```

## Quick Run

```sh
./sim --mode seq --trials 10000 --steps 30 --seed 42
./sim --mode thread --threads 4 --trials 10000 --steps 30 --sync reduce --seed 42
./sim --mode pipeline --schedule queue --merge interactive --threads 4 --trials 10000 --steps 30 --batch-size 1000 --queue-size 1024 --seed 42
./sim --mode process --processes 2 --trials 10000 --steps 30 --ipc pipe --seed 42
./sim --mode hybrid --processes 2 --threads 2 --trials 10000 --steps 30 --ipc pipe --seed 42
```

## Final Experiment Script

```sh
chmod +x scripts/run_final.sh
TRIALS=10000 STEPS=30 scripts/run_final.sh
```

The script writes:

```text
results/csv/final_results.csv
```

For final measurement, use larger values inside Docker Linux:

```sh
TRIALS=1000000 STEPS=50 scripts/run_final.sh
```

## CSV Fields

`final_results.csv` contains:

```text
mode,schedule,merge,sync,processes,threads,trials,steps,batch_size,queue_size,
time_total,time_pre,time_compute,time_sync,time_merge,time_post,
speedup,efficiency,sequential_fraction_estimate,compute_ratio,
sync_overhead_ratio,merge_overhead_ratio,throughput_batches_per_sec,
total_trials,collision_count,hist_sum,checksum,valid,notes
```

Current program output keeps `speedup` and `efficiency` as raw placeholders. Use the sequential row as baseline:

```text
speedup = T_seq / T_parallel
efficiency = speedup / worker_count
```

Amdahl-style indicators:

```text
sequential_fraction_estimate = (T_pre + T_sync + T_merge + T_post) / T_total
compute_ratio = T_compute / T_total
sync_overhead_ratio = T_sync / T_total
merge_overhead_ratio = T_merge / T_total
throughput = processed_batches / T_total
```

## Docker Linux

Build and enter the project image:

```sh
docker build -t os-montecarlo-risk .
docker run --rm -it os-montecarlo-risk
```

Inside the container:

```sh
make clean
make
TRIALS=10000 STEPS=30 scripts/run_final.sh
```

With docker compose:

```sh
docker compose build
docker compose run --rm os-sim
make
TRIALS=10000 STEPS=30 scripts/run_final.sh
```

For CPU utilization captures, run a long workload and monitor it from another terminal:

```sh
./sim --mode pipeline --schedule queue --merge interactive --threads 4 --trials 10000000 --steps 100 --batch-size 1000 --queue-size 1024 --seed 42
pidstat -u -r -C sim 1
```

`procps`, `sysstat`, and `/usr/bin/time` are installed in the Docker image so `top`, `pidstat`, and `/usr/bin/time -v` can be used.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| `Permission denied` on script | `chmod +x scripts/run_final.sh` |
| `make: command not found` | `apt-get install -y build-essential make` |
| Docker result file permission issue | Run with the project image or adjust host directory ownership |
| 8 threads are not faster | Physical core count, scheduling overhead, and context switching can dominate |
| `nosync` is invalid | Expected behavior; it demonstrates a race condition |
| `--ipc shm` fails or prints TODO note | Shared memory comparison is intentionally left as a future extension |

## Current Limitations

Shared memory IPC, semaphore comparison, double buffering, perf-stat automation, and work stealing are left as final-report discussion or future work. The implemented path focuses on a clear, compilable C/Linux system with process, thread, queue, condition variable, pipe IPC, and stage timing.
