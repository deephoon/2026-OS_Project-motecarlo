# Final Plan

## Implemented Scope

- Sequential baseline
- pthread static thread mode
- nosync / mutex / local reduce synchronization comparison
- Pre-processing stage and `TaskBatch`
- mutex + condition variable task queue
- queue-based pipeline worker mode
- final reduce and interactive merge
- fork-based process mode
- pipe-based IPC result transfer
- shared-memory IPC result transfer
- hybrid process + thread mode
- post-processing validation
- stage-level CSV output with `time_ipc` and queue wait counters
- skewed workload option for load-imbalance experiments
- Docker Linux execution environment

## Remaining Work

| Item | Status | Plan |
| --- | --- | --- |
| Semaphore queue comparison | TODO | Optional comparison only; current core uses mutex + condvar |
| CPU utilization automation | Partial | Use manual `pidstat`; script automation can be added |
| Memory analysis | Partial | Use `/usr/bin/time -v`, `pidstat -r`, or `/proc` |
| Graph generation | Done | `scripts/make_final_graphs.py` generates SVG graphs from analyzed CSV |
| Dynamic difficulty load balance | Implemented | `--workload skewed --skew-factor N` adds deterministic dummy CPU work without changing checksum |

## Presentation Message

The project should be presented as a CPU-bound OS parallel processing experiment, not as a vehicle simulator. The vehicle-following model supplies independent deterministic trials; the main contribution is the execution system around it: process, thread, synchronization, queue, pipeline, IPC, validation, and performance analysis.
