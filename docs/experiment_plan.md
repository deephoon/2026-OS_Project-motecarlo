# Final Experiment Plan

The primary final experiment uses the actual Monte Carlo simulation with a fixed total workload. Artificial CPU work and pre/post stress are secondary diagnostic experiments, not the main scaling evidence.

## Goals

1. Compare sequential, thread, process, and hybrid execution.
2. Compare static partition and queue-based scheduling.
3. Compare final reduce and interactive merge.
4. Demonstrate race condition with `nosync`.
5. Collect stage timing for Amdahl-style analysis.
6. Measure throughput in batch-based pipeline mode.

## Test Vector

| Parameter | Values |
| --- | --- |
| `trials` | `10000` for smoke tests, `120000000` for final scaling |
| `steps` | `30`, `50`, `100` |
| `threads` | `1`, `2`, `4`, `8` |
| `processes` | `2`, `4` |
| `batch_size` | `100`, `1000`, `10000` |
| `sync` | `nosync`, `mutex`, `reduce` |
| `merge` | `final`, `interactive` |
| `seed` | `42` fixed |

## Cases

| Case | Command Pattern | Purpose |
| --- | --- | --- |
| A | `--mode seq` | Sequential baseline |
| B | `--mode thread --schedule static --threads 1/2/4/8` | Thread scaling |
| C | `--mode thread --sync nosync/mutex/reduce` | Synchronization correctness |
| D | `--mode pipeline --merge final/interactive` | Merge strategy comparison |
| E | `--mode pipeline --batch-size 100/1000/10000` | Queue granularity analysis |
| F | `--mode process --processes 2/4` | Child process comparison |
| G | `--mode hybrid --processes 2 --threads 2/4` | Process + thread structure |

## Metrics

| Metric | Meaning |
| --- | --- |
| `time_total` | End-to-end wall-clock time |
| `time_pre` | Batch generation time |
| `time_compute` | Worker execution window |
| `time_sync` | Queue and lock overhead measured in critical paths |
| `time_merge` | Result merge time |
| `time_post` | Validation and summary time |
| `throughput_batches_per_sec` | Processed batches per second |
| `hist_sum`, `valid` | Correctness validation |
| `checksum` | Reproducibility check |

## Amdahl Analysis Template

| Mode | Workers | T_total | T_pre | T_compute | T_sync | T_merge | T_post | Sequential Fraction Estimate | Speedup | Efficiency |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| seq | 1 | | | | | | | | 1.00 | 1.00 |
| thread | 4 | | | | | | | | | |
| pipeline | 4 | | | | | | | | | |
| process | 4 | | | | | | | | | |
| hybrid | 4 | | | | | | | | | |

Use:

```text
speedup = T_seq / T_parallel
efficiency = speedup / worker_count
sequential_fraction_estimate = (T_pre + T_sync + T_ipc + T_merge + T_post) / T_total
```

## Throughput Analysis Template

| Mode | Batch Size | Batches | T_total | Throughput | Notes |
| --- | --- | --- | --- | --- | --- |
| pipeline interactive | 100 | | | | fine-grained scheduling overhead |
| pipeline interactive | 1000 | | | | balanced baseline |
| pipeline interactive | 10000 | | | | coarse-grained scheduling |

## Primary Commands

```sh
TRIALS=120000000 STEPS=50 REPEATS=5 scripts/run_real_scaling.sh
python3 scripts/analyze_real_scaling.py
TRIALS=120000000 STEPS=50 scripts/run_real_utilization.sh
```

## Capture Targets

1. `make test` success.
2. `scripts/professor_smoke_test.sh`.
3. `cat results/csv/real_scaling/real_scaling_summary.csv`.
4. `nosync` row with `valid=0` when race appears.
5. Docker build and Docker-internal script execution.
6. CPU utilization using `pidstat -u -r -C sim 1` during a long pipeline or hybrid run.
