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
- hybrid process + thread mode
- post-processing validation
- stage-level CSV output
- Docker Linux execution environment

## Remaining Work

| Item | Status | Plan |
| --- | --- | --- |
| Shared memory IPC | TODO | Add `shm_open` or `mmap` based result region |
| Semaphore queue comparison | TODO | Optional comparison only; current core uses mutex + condvar |
| CPU utilization automation | Partial | Use manual `pidstat`; script automation can be added |
| Memory analysis | Partial | Use `/usr/bin/time -v`, `pidstat -r`, or `/proc` |
| Graph generation | TODO | Can be added with Python or spreadsheet from CSV |
| Dynamic difficulty load balance | Skeleton-level | `TaskBatch.difficulty_level` exists as metadata |

## Presentation Message

The project should be presented as a CPU-bound OS parallel processing experiment, not as a vehicle simulator. The vehicle-following model supplies independent deterministic trials; the main contribution is the execution system around it: process, thread, synchronization, queue, pipeline, IPC, validation, and performance analysis.
