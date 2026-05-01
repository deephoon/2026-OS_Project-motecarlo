# System Design

## Flow Diagram

```text
CLI args
   |
   v
Config
   |
   +--> sequential mode
   |       |
   |       v
   |   run_trial loop
   |       |
   |       v
   |   Result aggregate
   |
   +--> thread mode
           |
           v
      static partition
           |
           v
      pthread_create N workers
           |
           +--> nosync: shared Result without lock
           +--> mutex: shared Result with pthread_mutex_t
           +--> reduce: per-thread local Result
           |
           v
      pthread_join
           |
           v
      merge local results when reduce
           |
           v
CSV row
```

## Modules

- `main.c`: mode dispatch, result validation, CSV output
- `cli.c`: command-line option parsing and usage text
- `config.c`: default configuration and config validation
- `simulation.c`: one-trial CPU-bound Monte Carlo kernel
- `result.c`: aggregate counters, histogram, checksum, CSV formatting
- `metrics.c`: monotonic wall-clock timer and CPU usage skeleton
- `sequential.c`: sequential baseline
- `thread_mode.c`: pthread workers, static partitioning, sync mode implementation
- `sync.c`: enum/string conversion for mode and sync options

## Trial 계산 흐름

각 trial은 deterministic seed에서 다음 값을 생성합니다.

- ego speed: 60-120 km/h
- front speed: 40-110 km/h
- initial distance: 5-100 m
- reaction time: 0.5-2.5 sec
- ego deceleration: 3-9 m/s^2
- front deceleration: 2-10 m/s^2
- road factor: 0.5-1.0

`dt=0.1`초마다 앞차와 ego 차량의 위치와 속도를 갱신합니다. 상대거리 `relative_distance <= 0`이면 collision입니다. 충돌이 없으면 최소 TTC 기준으로 risk bucket을 분류합니다.

이 모델은 실제 차량 동역학 정확도가 목적이 아닙니다. OS 병렬처리 실험에 적합한 CPU-bound 반복 계산을 만드는 것이 목적입니다.

## Thread Mode 구조

Thread mode는 static partition을 사용합니다. 전체 `trials`를 thread 수로 나누고 remainder는 앞쪽 thread에 1개씩 배분합니다.

각 worker는 `pthread_create`로 생성됩니다. Linux scheduler가 worker thread를 CPU core에 분배하며, 각 worker는 자기 trial index 범위만 계산합니다. `pthread_join`은 모든 worker가 끝날 때까지 main thread가 기다리는 synchronization point입니다.

Per-trial seed는 다음 식으로 만듭니다.

```c
trial_seed = base_seed ^ (trial_index * 2654435761u)
```

이 방식은 실행 순서와 thread scheduling이 달라도 같은 trial index가 같은 난수를 사용하게 합니다.

## Sync Mode 비교

`nosync`: worker들이 같은 global `Result`를 동시에 갱신합니다. `total_trials += 1` 같은 read-modify-write 연산이 atomic하지 않기 때문에 lost update가 발생할 수 있습니다.

`mutex`: `pthread_mutex_lock`과 `pthread_mutex_unlock`으로 shared `Result` 갱신을 보호합니다. 정확하지만 매 trial마다 lock contention이 발생할 수 있습니다.

`reduce`: worker마다 `local_result`를 사용합니다. hot loop에는 shared state 갱신이 없고, 모든 worker 종료 후 main thread가 merge합니다. 이 구조가 중간 발표 MVP의 권장 병렬화 방식입니다.

## 현재 MVP

- sequential baseline
- pthread thread mode
- nosync/mutex/reduce 비교
- wall-clock elapsed time
- histogram sum, collision count, checksum, valid flag
- shell 기반 실험 자동화
- Docker Linux 환경

## 최종 확장 TODO

- `process_mode.c`: `fork`, shared memory, pipe 또는 file aggregation 실험
- `hybrid_mode.c`: process 내부 thread pool 구조
- dynamic task queue: mutex/condition variable 또는 atomic counter 기반 work distribution
- CPU utilization: `getrusage`, `pidstat`, `/usr/bin/time` 결과 수집
- memory footprint: process count와 thread count 변화에 따른 비교
