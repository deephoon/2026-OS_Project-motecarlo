# Ideal Benchmark Report

## Purpose

The ideal benchmark is not a vehicle simulation. It is a pure CPU-bound upper-bound experiment used to check whether N workers can keep N cores busy and whether strong scaling approaches T1/N under minimal OS overhead.

## Design

- No shared result updates inside the hot loop.
- No pipe, mmap IPC, task queue, merge queue, or repeated printf in workers.
- Each pthread computes an independent deterministic local checksum.
- The main thread joins workers and merges checksums once.
- CPU affinity is attempted on Linux and falls back with a warning if unsupported.

## Strong Scaling Summary

| Threads | Avg time | Ideal time | Speedup | Efficiency | Avg CPU% | Util/core |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 5.013117982 | 5.013117982 | 1.000 | 1.000 | 99.0 | 99.0 |
| 2 | 2.502995793 | 2.506558991 | 2.003 | 1.001 | 199.0 | 99.5 |
| 4 | 1.427956938 | 1.253279495 | 3.511 | 0.878 | 399.0 | 99.8 |
| 8 | 0.912267687 | 0.626639748 | 5.495 | 0.687 | 778.5 | 97.3 |

## CPU Utilization Summary

| Threads | Avg CPU% | Avg util/core | Judgement |
| ---: | ---: | ---: | --- |
| 4 | 399.0 | 99.8 | near ideal |

## Interpretation For Final Report

Use ideal mode as the upper bound: it removes synchronization, IPC, queueing, and merge overhead from the hot path. Then compare it with real simulation modes, where pthread mutexes, condition variables, fork/waitpid, pipe or shared memory result transfer, and merge strategy reduce efficiency.

## Environment Caveat

Docker Desktop and WSL2 run through a VM layer, and native Linux can differ by CPU model, core count, scheduler state, CPU governor, and background load. Do not claim universal 100% utilization; claim that the project creates an ideal condition and compares it with real OS-structured simulation modes.
