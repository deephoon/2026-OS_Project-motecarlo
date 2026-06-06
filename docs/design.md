# System Design

## Initial Structure

```text
Input Parameters
   |
   v
Sequential or Thread Execution
   |
   v
Final Reduce
   |
   v
Result Validation
   |
   v
CSV Output
```

This structure is useful as a baseline, but it is too close to a pure embarrassingly parallel loop. The final version keeps this baseline and adds stages that make synchronization, pipeline, IPC, and Amdahl-style analysis visible.

## Improved Final Structure

```text
Pre-processing Stage
   |
   v
Task Queue
   |
   v
Parallel Simulation Workers
   |
   v
Interactive Merge or Final Reduce
   |
   v
Post-processing Stage
   |
   v
Stage-level Performance Analysis
```

## Modules

| Module | Role |
| --- | --- |
| `config` / `cli` | Parse execution mode, scheduling, merge, process/thread counts |
| `simulation` | CPU-bound Monte Carlo trial and batch execution |
| `preprocess` | Split trial count into `TaskBatch` units |
| `task_queue` | Bounded producer-consumer queue using mutex + condition variable |
| `merge_queue` | Bounded queue for worker-produced `PartialResult` values |
| `pipeline_mode` | Preprocessor, worker pool, and aggregator pipeline |
| `process_mode` | fork + pipe based child process execution |
| `hybrid_mode` | child process partition with process-local pthread workers |
| `postprocess` | validation, checksum, collision probability, risk ratios |
| `metrics` | wall-clock stage timing and ratio helpers |

## Pipeline Structure

```text
[Preprocessor Thread]
  create TaskBatch values
  push to TaskQueue
       |
       v
[Worker Threads]
  pop TaskBatch
  run_batch()
  produce PartialResult
       |
       v
[Aggregator Thread]
  pop PartialResult
  merge into global Result
```

The task queue is protected by `pthread_mutex_t`. `pthread_cond_t` is used for `not_empty` and `not_full` waiting. This is intentionally chosen instead of semaphore for the core implementation because queue state requires a mutex-protected critical section, while condition variables directly express sleep/wakeup for empty/full transitions.

## Synchronization Points

| Point | Primitive | Purpose |
| --- | --- | --- |
| Task queue push/pop | mutex + condition variable | Protect queue state and handle full/empty wait |
| Merge queue push/pop | mutex + condition variable | Transfer partial results from workers to aggregator |
| Static mutex mode | pthread mutex | Protect global histogram update per trial |
| Local reduce | thread-local result | Remove shared writes from hot loop |
| Process merge | pipe + parent merge | Transfer child result to parent process |

## Process Mode

```text
[Parent Process]
   fork child 0
   fork child 1
   ...
   waitpid()
   read Result from pipe or shared memory slot
   merge child results

[Child Process]
   compute assigned trial range
   write Result to pipe or shared memory slot
   _exit(0)
```

The IPC implementation supports two result-transfer paths. `--ipc pipe` writes the `Result` struct through a pipe. `--ipc shm` uses an anonymous shared-memory result table where each child writes only to its own slot and the parent reads slots after `waitpid()`. This keeps synchronization simple while still allowing pipe-vs-shared-memory IPC comparison.

## Hybrid Mode

```text
[Parent Process]
   fork process groups
       |
       v
[Child Process]
   create pthread workers
   split child trial range
   local reduce
   write process-local Result through pipe or shared memory
       |
       v
[Parent Process]
   merge process results
```

Process and thread roles are separated: processes own large simulation groups, while threads perform fine-grained local computation inside each process.

## Stage Timer Design

| Field | Meaning |
| --- | --- |
| `T_pre` | Batch generation and metadata construction |
| `T_compute` | Main worker execution window |
| `T_sync` | Queue wait/lock/push/pop overhead that can be measured locally |
| `T_ipc` | Parent-side result read from pipe or shared memory |
| `T_merge` | Result merge time |
| `T_post` | Validation and summary calculation |
| `T_total` | End-to-end run time |

Synchronization time is not perfectly separable in a pthread program because waiting and computation can overlap. The current implementation measures observable queue synchronization sections and condition-variable waits. In process/hybrid mode, the fork-to-reap interval is treated as the parallel compute window instead of incorrectly counting the full `waitpid()` interval as sequential synchronization overhead.

## TODO Extensions

| Extension | Purpose |
| --- | --- |
| Semaphore comparison | Compare semaphore-based queue control with mutex + condition variable |
| Double buffering | Reduce producer-consumer idle time |
| perf stat automation | Add hardware counter evidence |
| Work stealing | Improve load balance beyond the current skewed workload + queue scheduling test |
