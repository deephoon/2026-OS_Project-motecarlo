# 🚗 Monte Carlo 기반 차량 추종 위험 시뮬레이션

> 운영체제 프로젝트  
> **Child Process + Multithread + Synchronization + IPC + Pipeline 성능 분석**

이 프로젝트는 실제 차량 시뮬레이터를 만드는 것이 아닙니다.  
차량 추종 상황을 **CPU-bound Monte Carlo 반복 계산 모델**로 만들고, 같은 작업을 여러 운영체제 실행 구조로 처리하면서 **정확성, 병렬화 효과, synchronization overhead, IPC 비용, pipeline 처리량**을 분석하는 프로젝트입니다.

핵심 키워드:

`pthread` · `fork()` · `pipe IPC` · `mutex` · `condition variable` · `task queue` · `pipeline` · `local reduce` · `interactive merge` · `Amdahl's Law` · `checksum validation`

---

## 🚗 프로젝트 한 줄 정의

**자동차 위험 시뮬레이션을 소재로 삼아, child process와 multithread를 이용한 병렬처리 및 synchronization 문제 해결 과정을 정량적으로 보여주는 운영체제 실험 시스템입니다.**

중간발표 당시에는 `seq/thread` 중심의 synchronization 비교가 핵심이었고, 현재는 최종 프로젝트 가이드에 맞춰 `process`, `hybrid`, `pipeline`, `IPC`, `interactive merge`, stage-level metrics까지 확장했습니다.

| 구분 | 중간발표 MVP | 현재 최종 구조 |
| --- | --- | --- |
| 실행 모드 | `seq`, `thread` | `seq`, `thread`, `pipeline`, `process`, `hybrid` |
| 병렬 단위 | pthread worker | thread + child process + process 내부 thread |
| 작업 분배 | static partition | static partition + bounded task queue |
| 동기화 | `nosync`, `mutex`, `reduce` | queue/merge synchronization까지 확장 |
| 병합 방식 | final reduce 중심 | final reduce + interactive merge |
| IPC | 없음 | pipe 기반 child result 전달 |
| 성능 지표 | 실행시간 중심 | stage time, throughput, validation CSV |

---

## 📌 프로젝트 가이드 요구사항 대응

`ref/os26_project.pdf`의 핵심 요구사항을 현재 구현과 연결하면 다음과 같습니다.

| 가이드 요구사항 | 현재 구현/문서 대응 |
| --- | --- |
| 4개 이상 core 활용 가능 시스템 | thread 수 `1/2/4/8`, process 수 `2/4`, hybrid 조합 실험 가능 |
| child process 사용 | `--mode process`, `fork()`, `waitpid()`, pipe IPC 구현 |
| multiple threads 사용 | `--mode thread`, `--mode pipeline`, `--mode hybrid`에서 pthread 사용 |
| synchronization 문제 정의 및 해결 | `nosync` race condition, `mutex`, `reduce`, queue mutex/condvar 비교 |
| parent sequential vs child process 비교 | `--mode seq`와 `--mode process` 결과 비교 |
| single/multi process와 single/multi thread 비교 | `thread 1/2/4/8`, `process 2/4`, `hybrid 2x2/2x4` 실험 |
| process와 thread 역할 구분 | process는 큰 simulation group, thread는 내부 batch 계산 담당 |
| synchronization 사용/미사용 비교 | `nosync` vs `mutex` vs `reduce` |
| 다양한 test vector | trials, steps, threads, processes, batch size, merge mode 변경 가능 |
| 정량적 성능 분석 | `time_total`, `T_pre`, `T_compute`, `T_sync`, `T_merge`, `T_post`, throughput, speedup, efficiency 후처리 |
| 문제 인식과 해결 과정 | 단순 병렬화 한계 → pre/post stage → queue/pipeline/interactive merge 도입 |
| CPU/memory 측정 | Docker Linux, `pidstat`, `/usr/bin/time -v` 사용 계획 정리 |
| AI agent 사용 기록 | OpenAI Codex를 코드/문서 정리에 사용. 최종 보고서에 사용 범위와 prompt 별도 기재 필요 |

---

## 🧩 OS 개념이 프로젝트에 들어간 위치

이 프로젝트에서 차량 추종 위험 계산은 **workload**입니다.  
진짜 핵심은 이 workload를 어떤 운영체제 실행 단위와 동기화 방식으로 처리하느냐입니다.

즉, 프로젝트의 중심 질문은 다음입니다.

```text
같은 계산을
1. 단일 실행 흐름으로 처리할 때
2. 여러 thread로 나눌 때
3. 여러 child process로 나눌 때
4. process 내부에서 다시 thread를 사용할 때
5. queue와 pipeline으로 stage를 나눌 때

정확성, 실행 시간, synchronization overhead, IPC 비용이 어떻게 달라지는가?
```

### 1. Process

관련 코드:

```text
src/process_mode.c
src/hybrid_mode.c
src/ipc_pipe.c
```

사용 OS 개념:

| OS 개념 | 프로젝트 적용 |
| --- | --- |
| `fork()` | parent process가 child process를 생성 |
| process address space | child는 parent와 독립된 메모리 공간에서 계산 |
| `waitpid()` | parent가 child 종료를 기다림 |
| process isolation | child가 계산한 결과는 parent 메모리에 바로 반영되지 않음 |
| IPC | child result를 parent에게 전달하기 위해 pipe 사용 |

개념적으로 process mode는 다음처럼 동작합니다.

```text
Parent process
  ├─ fork child 0  -> 자기 trial range 계산 -> pipe write
  ├─ fork child 1  -> 자기 trial range 계산 -> pipe write
  └─ fork child N  -> 자기 trial range 계산 -> pipe write

Parent process
  -> pipe read
  -> waitpid
  -> result merge
  -> checksum validation
```

이 구조를 통해 확인하려는 것은 단순히 “process가 빠른가?”가 아닙니다.  
process는 thread보다 메모리 격리성이 좋지만, `fork()`와 IPC 비용이 존재합니다. 따라서 작은 workload에서는 손해를 볼 수 있고, 큰 workload에서는 병렬 계산 이득이 overhead를 이길 수 있습니다.

### 2. Thread

관련 코드:

```text
src/thread_mode.c
src/pipeline_mode.c
src/hybrid_mode.c
```

사용 OS 개념:

| OS 개념 | 프로젝트 적용 |
| --- | --- |
| `pthread_create()` | worker thread 생성 |
| `pthread_join()` | worker thread 종료 대기 |
| shared address space | 여러 thread가 같은 `Result` 구조체에 접근 가능 |
| context switching | thread 수 증가에 따른 scheduling overhead 관찰 |
| race condition | `nosync`에서 shared result 갱신 충돌 발생 |

thread mode는 다음처럼 동작합니다.

```text
Main thread
  -> 전체 trials를 thread 수만큼 정적 분할
  -> pthread_create로 worker 생성

Worker thread 0 -> trial range 계산
Worker thread 1 -> trial range 계산
Worker thread 2 -> trial range 계산
Worker thread 3 -> trial range 계산

Main thread
  -> pthread_join
  -> local result merge 또는 shared result 확인
  -> checksum validation
```

thread는 process보다 생성 비용과 통신 비용이 낮습니다.  
하지만 같은 메모리를 공유하기 때문에 shared result를 동시에 갱신하면 race condition이 발생합니다. 그래서 synchronization 전략이 필요합니다.

### 3. Synchronization

관련 코드:

```text
src/thread_mode.c
src/task_queue.c
src/merge_queue.c
src/pipeline_mode.c
```

이 프로젝트는 synchronization을 세 단계로 비교합니다.

| 방식 | OS 개념 | 프로젝트에서의 의미 |
| --- | --- | --- |
| `nosync` | 동기화 없음 | shared counter/histogram을 동시에 갱신해 race condition 관찰 |
| `mutex` | mutual exclusion | shared result 갱신 구간을 lock으로 보호 |
| `reduce` | thread-local aggregation | hot loop에서는 공유 쓰기를 없애고 마지막에만 병합 |

`nosync`는 일부러 틀릴 수 있게 만든 모드입니다.

```text
Thread A: total_trials 읽음
Thread B: total_trials 읽음
Thread A: total_trials + 1 저장
Thread B: total_trials + 1 저장

결과: 실제로는 2번 증가해야 하는데 1번만 증가할 수 있음
```

이것이 lost update입니다.  
최종 실험에서도 `thread_4_nosync`는 빠르게 보일 수 있지만 `valid=0`, checksum mismatch가 발생하므로 “동기화가 왜 필요한지”를 보여주는 실패 사례로 사용합니다.

### 4. Mutex + Condition Variable

관련 코드:

```text
src/task_queue.c
src/merge_queue.c
```

pipeline mode에서는 단순히 result만 보호하는 것이 아니라, queue 자체가 공유 자료구조가 됩니다.

queue가 가진 공유 상태:

```text
head
tail
count
closed
buffer
```

이 상태는 여러 worker가 동시에 접근하므로 mutex로 보호해야 합니다.  
하지만 mutex만으로는 “queue가 비었을 때 기다리기”, “queue가 찼을 때 기다리기”를 효율적으로 표현하기 어렵습니다. 그래서 condition variable을 함께 사용합니다.

```text
producer:
  queue가 full이면 not_full 조건을 기다림
  batch를 push
  not_empty signal

worker:
  queue가 empty이면 not_empty 조건을 기다림
  batch를 pop
  not_full signal
```

이 구조는 busy waiting을 피하고, 필요한 thread만 sleep/wakeup 할 수 있게 합니다.  
교수님 피드백의 “semaphore 대신 더 가벼운 mutex를 쓰되, 부족한 기능을 어떻게 보완할 것인가”에 대한 답이 바로 `mutex + condition variable`입니다.

### 5. IPC

관련 코드:

```text
src/ipc_pipe.c
src/process_mode.c
src/hybrid_mode.c
```

process는 서로 메모리를 공유하지 않습니다.  
따라서 child process가 계산한 `Result`는 parent가 직접 볼 수 없습니다. 이 프로젝트는 pipe를 사용해 child result를 parent에게 전달합니다.

```text
Child process
  -> local Result 계산
  -> pipe write
  -> exit

Parent process
  -> pipe read
  -> result merge
  -> waitpid
```

이때 pipe read/write와 waitpid 대기 시간이 `T_sync` 또는 merge 관련 overhead로 관찰됩니다.  
따라서 process mode는 계산 성능뿐 아니라 IPC 비용까지 함께 분석할 수 있습니다.

### 6. Scheduling / Work Distribution

관련 코드:

```text
src/thread_mode.c
src/preprocess.c
src/pipeline_mode.c
src/task_queue.c
```

이 프로젝트에는 두 가지 작업 분배 방식이 있습니다.

| 방식 | 설명 | 장점 | 단점 |
| --- | --- | --- | --- |
| static partition | 처음부터 trial range를 worker 수만큼 나눔 | 단순하고 overhead 작음 | workload imbalance에 약함 |
| queue scheduling | batch를 queue에 넣고 worker가 가져감 | 동적 분배 가능 | queue lock/condvar overhead 발생 |

`thread` mode는 주로 static partition입니다.

```text
trials = 1000000
threads = 4

thread 0: 0 ~ 249999
thread 1: 250000 ~ 499999
thread 2: 500000 ~ 749999
thread 3: 750000 ~ 999999
```

`pipeline` mode는 batch queue를 사용합니다.

```text
preprocessor -> batch 0 push
preprocessor -> batch 1 push
preprocessor -> batch 2 push

worker 0 -> pop batch
worker 1 -> pop batch
worker 2 -> pop batch
```

이 차이를 통해 단순 partition과 producer-consumer queue 구조의 trade-off를 비교할 수 있습니다.

### 7. Pipeline

관련 코드:

```text
src/pipeline_mode.c
src/task_queue.c
src/merge_queue.c
```

pipeline mode는 실제 시스템처럼 작업을 stage로 나눕니다.

```text
Preprocessor thread
  -> TaskQueue
  -> Worker pool
  -> MergeQueue
  -> Aggregator thread
```

개념적으로는 다음과 같습니다.

```text
시간 흐름:

t0: preprocessor가 batch 0 생성
t1: worker 0이 batch 0 계산, preprocessor는 batch 1 생성
t2: worker 1이 batch 1 계산, aggregator는 batch 0 결과 merge
t3: worker 2가 batch 2 계산, aggregator는 batch 1 결과 merge
```

초기 latency는 존재하지만, batch가 계속 들어오면 stage overlap으로 throughput을 높일 수 있습니다.  
다만 이 프로젝트의 실험 결과에서는 interactive merge가 final reduce보다 항상 빠르지는 않았습니다. 이는 queue, condition variable, aggregator thread의 overhead가 있기 때문입니다.

### 8. Amdahl's Law

관련 코드:

```text
src/metrics.c
src/main.c
scripts/analyze_results.py
```

이 프로젝트는 전체 시간을 stage별로 나눠 측정합니다.

```text
T_total = T_pre + T_compute + T_sync + T_merge + T_post
```

각 항목의 의미:

| 항목 | OS 관점 |
| --- | --- |
| `T_pre` | 병렬 계산 전 준비 단계. 순차 구간으로 작용 가능 |
| `T_compute` | 병렬화 가능한 핵심 계산 |
| `T_sync` | lock, condition wait, IPC wait, join/wait overhead |
| `T_merge` | partial result를 하나로 합치는 구간 |
| `T_post` | validation/checksum 등 후처리 |

Amdahl's Law에 따르면 병렬화할 수 없는 구간이 전체 speedup의 상한을 만듭니다.  
그래서 이 프로젝트는 `--pre-work`, `--post-work` 옵션을 통해 순차 stage 비용을 조절할 수 있게 했습니다.

```text
sequential_fraction_estimate
  = (T_pre + T_sync + T_merge + T_post) / T_total
```

결론적으로 이 프로젝트는 단순 계산 프로그램이 아니라, **OS 실행 단위와 synchronization 비용이 전체 성능에 미치는 영향을 관찰하는 실험 장치**입니다.

---

## 🧠 문제 인식: 왜 단순 병렬화만으로 부족했나

처음 구조는 매우 단순했습니다.

```text
trials를 thread 수만큼 나눔
  -> 각 thread가 자기 trial range 계산
  -> 마지막에 결과 merge
```

이 방식은 Monte Carlo trial이 서로 독립적이라 병렬화가 너무 쉽습니다.  
운영체제 프로젝트에서 요구하는 문제 해결 과정, synchronization 병목, process/thread 역할 분리, Amdahl's Law 분석을 보여주기에는 부족했습니다.

그래서 현재 구조는 일부러 실제 시스템에 가까운 stage를 추가했습니다.

```text
Pre-processing
  -> TaskQueue
  -> Parallel Workers
  -> Merge Stage
  -> Post-processing
  -> CSV Output
```

### 왜 이렇게 바꿨나?

| 문제 인식 | 개선 방향 |
| --- | --- |
| 시작하자마자 바로 병렬화되어 너무 쉬움 | pre-processing과 post-processing stage를 추가 |
| 모든 trial이 끝난 뒤 한 번만 merge하면 너무 단순함 | interactive merge와 aggregator thread 추가 |
| mutex만으로 queue empty/full 처리가 부족함 | condition variable로 wait/signal 보완 |
| process와 thread 역할 차이가 약함 | process는 큰 작업 단위, thread는 내부 계산 병렬화로 분리 |
| 성능 저하 원인을 설명하기 어려움 | stage time과 throughput 지표 추가 |
| Amdahl's Law가 잘 드러나지 않음 | `--pre-work`, `--post-work`로 순차 stage 부하를 조절 가능하게 추가 |

이 변경 덕분에 단순히 “빠르다/느리다”가 아니라, **어느 stage에서 overhead가 생기는지** 설명할 수 있게 됐습니다.

---

## 🧭 프로젝트의 개념적 실행 흐름

사용자가 다음과 같은 명령을 실행한다고 가정합니다.

```sh
./sim --mode pipeline --threads 4 --trials 1000000 --steps 50 \
  --schedule queue --merge interactive \
  --batch-size 1000 --queue-size 1024 \
  --pre-work 50000 --post-work 10000 \
  --metrics-detail 1 --seed 42
```

프로그램은 내부적으로 다음 순서로 실행됩니다.

### 1단계: CLI 옵션 파싱

관련 코드:

```text
src/main.c
src/cli.c
src/config.c
include/config.h
```

먼저 `cli_parse_args()`가 사용자의 명령행 옵션을 읽어 `Config` 구조체를 채웁니다.

```text
mode = pipeline
threads = 4
trials = 1000000
steps = 50
schedule = queue
merge = interactive
batch_size = 1000
queue_size = 1024
pre_work = 50000
post_work = 10000
seed = 42
```

그 다음 `main.c`는 `cfg.mode`를 보고 어떤 실행 엔진을 호출할지 결정합니다.

```text
MODE_SEQ      -> run_sequential_metrics
MODE_THREAD   -> run_thread_mode_metrics
MODE_PIPELINE -> run_pipeline_mode
MODE_PROCESS  -> run_process_mode
MODE_HYBRID   -> run_hybrid_mode
```

이 단계는 OS 관점에서 아직 병렬 처리가 시작되기 전입니다.  
다만 이후 실행 구조를 결정하는 설정이 여기서 확정됩니다.

### 2단계: 전체 시간 측정 시작

관련 코드:

```text
src/metrics.c
include/metrics.h
```

각 mode는 실행 시작 시 `metrics_init()`으로 stage timer를 초기화하고, `now_sec()`을 이용해 전체 시간을 측정합니다.

```text
metrics->t_total_start = now_sec()
```

최종적으로 출력되는 `time_total`은 여기서 시작해서 post-processing까지 끝난 wall-clock time입니다.

### 3단계: Pre-processing

관련 코드:

```text
src/preprocess.c
include/task_batch.h
```

pre-processing은 전체 trial을 batch 단위로 나누고, 각 batch에 metadata를 부여합니다.

```text
TaskBatch {
  batch_id
  start_idx
  end_idx
  base_seed
  time_steps
  difficulty_level
}
```

예를 들어:

```text
trials = 1000000
batch_size = 1000

batch_count = 1000

batch 0: start=0, end=1000
batch 1: start=1000, end=2000
...
batch 999: start=999000, end=1000000
```

이 단계는 실제 시스템에서 입력 데이터 분할, metadata 생성, 작업 단위 준비에 해당합니다.  
`--pre-work`는 이 stage에 고정된 CPU work를 추가해, 순차 구간이 speedup에 미치는 영향을 관찰하기 위한 실험 장치입니다.

### 4단계: 실행 mode에 따른 병렬 처리

여기서부터 OS 개념이 본격적으로 갈라집니다.

#### `seq`

```text
for trial in all_trials:
  run_trial()
  result_add_trial()
```

하나의 실행 흐름에서 모든 trial을 처리합니다.  
이 결과가 speedup 계산의 baseline입니다.

#### `thread`

```text
main thread
  -> thread 0 생성
  -> thread 1 생성
  -> thread 2 생성
  -> thread 3 생성

각 thread
  -> 자기 trial range 계산
  -> sync mode에 따라 결과 저장

main thread
  -> join
  -> merge
```

`thread` mode에서는 같은 process 안에서 여러 thread가 실행됩니다.  
메모리를 공유하기 때문에 빠르게 통신할 수 있지만, shared result를 동시에 수정하면 race condition이 발생할 수 있습니다.

#### `process`

```text
parent process
  -> fork child 0
  -> fork child 1
  -> fork child 2
  -> fork child 3

child process
  -> 자기 trial range 계산
  -> pipe로 Result 전송
  -> exit

parent process
  -> pipe read
  -> waitpid
  -> merge
```

`process` mode에서는 child process가 독립된 address space에서 계산합니다.  
따라서 결과 공유를 위해 pipe IPC가 필요합니다.

#### `hybrid`

```text
parent process
  -> 여러 child process 생성

각 child process
  -> 내부에서 pthread worker 생성
  -> child-local reduce 수행
  -> pipe로 parent에게 결과 전송

parent process
  -> child result merge
```

hybrid는 process와 thread를 모두 사용합니다.  
process는 큰 작업 그룹을 나누고, thread는 각 process 내부에서 계산을 병렬화합니다.

#### `pipeline`

```text
preprocessor thread
  -> TaskQueue에 batch push

worker threads
  -> TaskQueue에서 batch pop
  -> run_batch()
  -> final partial 저장 또는 MergeQueue push

aggregator thread
  -> MergeQueue에서 partial result pop
  -> global result에 merge
```

pipeline mode는 producer-consumer 구조입니다.  
`TaskQueue`와 `MergeQueue`는 mutex와 condition variable을 사용해 동기화됩니다.

### 5단계: Trial 계산

관련 코드:

```text
src/simulation.c
```

각 trial은 독립적인 차량 추종 위험 시나리오입니다.

```text
seed 생성
차량 초기 조건 생성
time step 반복
거리/TTC 계산
충돌 여부 판단
risk bucket 분류
```

중요한 점은 trial들이 deterministic seed 기반이라는 것입니다.  
따라서 mode가 달라도 같은 trial index는 같은 결과를 만들어야 합니다. 이 성질 덕분에 checksum으로 correctness를 검증할 수 있습니다.

### 6단계: Result 병합

관련 코드:

```text
src/result.c
src/thread_mode.c
src/pipeline_mode.c
src/process_mode.c
src/hybrid_mode.c
```

각 worker가 만든 결과는 결국 하나의 `Result`로 합쳐집니다.

```text
Result {
  total_trials
  collision_count
  histogram[RISK_BUCKETS]
  checksum
}
```

병합 방식은 mode마다 다릅니다.

| mode | 병합 방식 |
| --- | --- |
| `seq` | 바로 global result에 누적 |
| `thread mutex` | 매 trial마다 mutex로 global result 보호 |
| `thread reduce` | thread-local result를 마지막에 merge |
| `process` | child result를 pipe로 받은 뒤 parent가 merge |
| `hybrid` | child 내부 thread 결과 merge 후 parent가 다시 merge |
| `pipeline final` | 모든 worker 종료 후 final partials merge |
| `pipeline interactive` | aggregator가 실행 중간에 partial result를 계속 merge |

이 단계에서 `T_merge`가 측정됩니다.

### 7단계: Post-processing

관련 코드:

```text
src/postprocess.c
```

post-processing은 결과가 맞는지 검증합니다.

```text
hist_sum == trials ?
total_trials == trials ?
collision_count 범위가 정상인가?
checksum 계산
```

최종 CSV의 `valid=1`은 이 검증을 통과했다는 뜻입니다.  
`--post-work`는 이 stage에 고정된 CPU work를 추가해, 후처리 순차 구간이 전체 speedup에 어떤 영향을 주는지 관찰하기 위한 옵션입니다.

### 8단계: CSV Metrics 출력

관련 코드:

```text
src/main.c
scripts/run_final.sh
scripts/analyze_results.py
```

`--metrics-detail 1`이면 다음과 같은 CSV가 출력됩니다.

```text
mode,schedule,merge,sync,processes,threads,trials,steps,
time_total,time_pre,time_compute,time_sync,time_merge,time_post,
sequential_fraction_estimate,compute_ratio,
total_trials,collision_count,hist_sum,checksum,valid
```

단일 실행만으로는 speedup baseline을 알 수 없으므로, 최종 speedup/efficiency는 `scripts/analyze_results.py`가 계산합니다.

```text
speedup_vs_seq_avg = seq 평균 시간 / 해당 mode 평균 시간
efficiency_vs_seq_avg = speedup / worker_count
```

최종 보고서에서는 raw 실행 결과가 아니라 `final_analyzed.csv`를 기준으로 해석합니다.

### 전체 흐름 요약

```text
사용자 명령
  -> CLI parsing
  -> Config 생성
  -> mode 선택
  -> metrics 시작
  -> pre-processing
  -> seq/thread/process/hybrid/pipeline 실행
  -> synchronization 또는 IPC
  -> merge
  -> post-processing
  -> checksum validation
  -> CSV 출력
  -> analyze_results.py로 반복 실험 요약
```

이 흐름 때문에 프로젝트는 단순 simulation program이 아니라, **OS 실행 구조별 성능과 정확성 trade-off를 관찰하는 실험 시스템**이 됩니다.

---

## 🏗️ 개선된 시스템 구조

```text
CLI Config
   |
   v
Pre-processing Stage
   - 전체 trials를 TaskBatch 단위로 분할
   - batch id, trial range, seed metadata 생성
   |
   v
Task Queue
   - bounded queue
   - mutex로 queue state 보호
   - condition variable로 empty/full 대기 처리
   |
   v
Parallel Execution
   +--> seq
   +--> thread
   +--> pipeline worker pool
   +--> child process
   +--> hybrid process + thread
   |
   v
Merge Stage
   +--> final reduce
   +--> interactive merge with aggregator thread
   |
   v
Post-processing Stage
   - hist_sum 검증
   - checksum 계산
   - valid flag 출력
   |
   v
CSV Metrics
```

### Stage별 의미

| Stage | 의미 | 병렬화 관점 |
| --- | --- | --- |
| `T_pre` | batch 생성, metadata 구성 | 일부 순차 구간 |
| `T_compute` | Monte Carlo trial 계산 | 핵심 병렬 구간 |
| `T_sync` | queue wait, lock, condition wait | synchronization overhead |
| `T_merge` | partial result 병합 | 병합 전략에 따라 병목 가능 |
| `T_post` | validation, checksum | 일부 순차 구간 |
| `T_total` | 전체 실행 시간 | 최종 비교 기준 |

`--pre-work`, `--post-work`는 결과값을 바꾸지 않는 deterministic CPU work입니다.  
이 옵션은 “일부러 더 어려운 환경”을 만들기 위한 실험 장치입니다. 기본 Monte Carlo trial은 너무 쉽게 병렬화되므로, pre/post 순차 구간을 조절해 Amdahl's Law의 한계를 관찰할 수 있게 했습니다.

---

## 🔬 Simulation Model

각 trial은 하나의 차량 추종 상황입니다.

1. deterministic seed 기반으로 ego/front 차량 초기 상태 생성
2. 속도, 거리, 반응 시간, 감속도 설정
3. `dt=0.1s` 단위로 위치와 속도 업데이트
4. 상대거리와 TTC(Time-To-Collision) 계산
5. 충돌 여부 또는 risk level 판단
6. histogram, collision count, checksum에 반영

Risk bucket:

| Risk Level | 기준 |
| --- | --- |
| Collision | `relative_distance <= 0` |
| High | `TTC < 1.5` |
| Medium | `TTC < 3.0` |
| Low | `TTC < 5.0` |
| Safe | 그 외 |

trial은 서로 독립적입니다. 따라서 compute stage는 병렬화하기 좋습니다.  
하지만 전체 시스템에는 pre/post, queue, merge, IPC가 붙으므로 **전체 실행시간이 compute만으로 결정되지 않습니다.**

```text
trial_seed = base_seed ^ (trial_index * 2654435761u)
```

---

## ⚙️ 실행 모드별 의미

### 1. `seq`: Sequential baseline

```sh
./sim --mode seq --trials 10000 --steps 30 --seed 42
```

단일 thread로 모든 trial을 순차 실행합니다.  
모든 speedup 계산의 기준입니다.

### 2. `thread`: pthread static partition

```sh
./sim --mode thread --threads 4 --trials 10000 --steps 30 --sync reduce --seed 42
```

| Sync mode | 의미 | 해석 |
| --- | --- | --- |
| `nosync` | lock 없이 shared result 갱신 | race condition 관찰용 |
| `mutex` | 매 trial마다 lock/unlock | 정확하지만 lock contention 발생 |
| `reduce` | thread-local result 후 merge | 정확성과 성능의 균형 |

### 3. `pipeline`: Task queue + worker pool

```sh
./sim --mode pipeline --schedule queue --merge interactive \
  --threads 4 --trials 10000 --steps 30 \
  --batch-size 1000 --queue-size 1024 \
  --pre-work 50000 --post-work 10000 --seed 42
```

작업을 batch로 나누고 queue에 넣습니다. worker thread가 queue에서 batch를 꺼내 계산합니다.  
이 모드는 producer-consumer 구조, mutex/condition variable, merge queue overhead를 보여주는 핵심 확장입니다.

### 4. `process`: fork + pipe IPC

```sh
./sim --mode process --processes 2 --trials 10000 --steps 30 --ipc pipe --seed 42
```

parent가 child process를 만들고, child가 계산한 `Result`를 pipe로 전달합니다.

```text
Parent
  -> fork child
  -> pipe read
  -> waitpid
  -> result merge
```

### 5. `hybrid`: process 내부 pthread

```sh
./sim --mode hybrid --processes 2 --threads 2 \
  --trials 10000 --steps 30 --ipc pipe --seed 42
```

process와 thread의 역할을 분리합니다.

| 구성 | 역할 |
| --- | --- |
| Parent process | child 생성, pipe 수신, 최종 merge |
| Child process | 큰 simulation group 담당 |
| Thread worker | child 내부 trial range 병렬 계산 |
| Pipe IPC | child result를 parent로 전달 |

---

## 🔒 Synchronization 설계

### `nosync`, `mutex`, `reduce`

| 방식 | 정확성 | 성능 | 목적 |
| --- | --- | --- | --- |
| `nosync` | 깨질 수 있음 | 빠를 수 있음 | race condition 증명 |
| `mutex` | 정확함 | lock overhead 큼 | shared update 보호 |
| `reduce` | 정확함 | 보통 가장 실용적 | hot loop의 shared write 제거 |

`nosync`는 의도적으로 안전하지 않은 모드입니다.  
여러 thread가 동시에 `total_trials`, `histogram`, `collision_count`를 갱신하면 lost update가 발생할 수 있습니다.

### 왜 semaphore 대신 mutex + condition variable인가?

피드백에서 semaphore는 Linux 라이브러리 내부 구조가 복잡해 binary 방식으로 사용해도 overhead가 있을 수 있다는 점을 고려했습니다. 그래서 core queue 구현은 다음 방식으로 구성했습니다.

```text
pthread_mutex_t mutex
pthread_cond_t not_empty
pthread_cond_t not_full
```

| 문제 | mutex만 사용하면? | 보완 방법 |
| --- | --- | --- |
| queue state 보호 | 가능 | `head`, `tail`, `count`, `closed`를 mutex로 보호 |
| queue가 비어 있을 때 worker 대기 | 부족 | `not_empty` condition variable 사용 |
| queue가 가득 찼을 때 producer 대기 | 부족 | `not_full` condition variable 사용 |
| 불필요한 busy waiting 방지 | 부족 | `pthread_cond_wait/signal`로 sleep/wakeup 처리 |

즉, 현재 설계는 **가벼운 mutex로 상태를 보호하고, mutex만으로 부족한 대기/깨우기 기능은 condition variable로 보완**하는 방식입니다.

---

## 🔁 Final Reduce vs Interactive Merge

### 왜 final reduce만으로는 부족한가?

`reduce` 방식은 정확하고 빠릅니다.

```text
각 worker가 local_result에 저장
  -> 모든 worker 종료
  -> 마지막에 한 번 merge
```

하지만 현실 시스템에서는 모든 작업이 끝날 때까지 기다렸다가 한 번에 결과를 합치는 경우만 있는 것은 아닙니다. 중간 결과를 계속 집계하거나 모니터링해야 하는 경우가 많습니다.

그래서 `interactive merge`를 추가했습니다.

```text
worker가 batch 계산 완료
  -> PartialResult를 MergeQueue에 push
  -> aggregator thread가 pop
  -> 실행 중간에 계속 merge
```

| 항목 | Final reduce | Interactive merge |
| --- | --- | --- |
| 병합 시점 | 모든 worker 종료 후 | batch 결과가 나올 때마다 |
| 추가 구조 | 거의 없음 | merge queue + aggregator thread |
| 장점 | 단순하고 빠름 | 중간 결과 병합 가능 |
| 단점 | 결과가 마지막에 몰림 | queue/condvar/context switch overhead |
| 해석 | 작은 workload에 유리 | 현실적 구조 설명에 유리 |

냉정하게 보면, 작은 workload에서는 interactive merge가 final reduce보다 느릴 수 있습니다.  
하지만 이 구조는 **현실적인 producer-consumer merge 구조와 synchronization overhead를 설명하기 위해 필요**합니다.

---

## 📐 Amdahl's Law와 Pipeline 분석

기본 Amdahl's Law 관점에서는 병렬화할 수 없는 순차 구간이 전체 speedup의 한계를 만듭니다.

```text
sequential_fraction_estimate
  = (T_pre + T_sync + T_merge + T_post) / T_total

speedup
  = T_seq / T_parallel

efficiency
  = speedup / worker_count
```

raw `./sim` 출력의 `speedup`, `efficiency`는 단일 실행만으로 baseline을 알 수 없기 때문에 0으로 남을 수 있습니다.  
최종 보고서에는 `scripts/analyze_results.py`가 생성하는 `results/csv/final_analyzed.csv`의 `speedup_vs_seq_avg`, `efficiency_vs_seq_avg`를 사용합니다.

### 이 프로젝트에서 순차 구간은 무엇인가?

| 구간 | 병렬화 어려운 이유 |
| --- | --- |
| `T_pre` | batch 생성과 queue 준비 |
| `T_sync` | mutex, condition variable, queue wait |
| `T_merge` | partial result 병합 |
| `T_post` | validation, checksum |

### Pipeline 관점

일반적인 Amdahl's Law는 순차 작업을 병렬화할 수 없다고 봅니다.  
하지만 pipeline을 적용하면 pre-processing, compute, merge를 다른 실행 주체가 맡아 **stage overlap**을 만들 수 있습니다.

```text
초기 latency는 존재
하지만 이후 batch들이 연속적으로 들어오면 throughput 향상 가능
```

현재 구현은 완전한 상용 pipeline 시스템은 아니지만, 다음 구조를 통해 stage 기반 분석이 가능합니다.

| Pipeline 요소 | 구현 |
| --- | --- |
| Preprocessor | TaskBatch 생성 및 queue push |
| Worker pool | batch pop 후 Monte Carlo 계산 |
| Aggregator | interactive merge에서 partial result 병합 |
| Metrics | stage time과 throughput CSV 출력 |

---

## 📊 실험 결과와 냉정한 해석

실험 결과는 두 단계로 나누어 해석합니다.

| 구분 | 목적 | 조건 | 해석 기준 |
| --- | --- | --- | --- |
| 기능 검증용 | 모든 mode가 같은 결과를 내는지 확인 | `trials=10000`, `steps=30` | `valid`, `hist_sum`, checksum |
| 성능 분석용 | 최종 보고서에 사용할 speedup/efficiency 분석 | `trials=1000000`, `steps=50`, `repeats=5` | 평균/최소/표준편차/speedup/efficiency |

### 1. 기능 검증 결과

| 항목 | 결과 |
| --- | --- |
| CSV row 수 | 30개 |
| `valid=1` 비율 | 100% |
| `hist_sum` | 모두 `10000` |
| checksum | 모두 `9158329899332878926` |
| 의미 | 모든 주요 모드가 같은 simulation 결과를 재현 |

10,000 trials 결과는 기능 검증에는 충분하지만, 실행 시간이 1ms 안팎이라 최종 성능 결론에는 약합니다. 그래서 최종 성능 분석은 아래 Docker Ubuntu Linux 반복 측정 결과를 기준으로 합니다.

### 2. 최종 성능 분석 조건

```text
Docker image = ubuntu:22.04
trials = 1000000
steps = 50
repeats = 5
pre_work = 50000
post_work = 10000
sequential baseline avg = 0.099473s
```

결과 파일:

```text
results/csv/docker_1m/final_raw.csv
results/csv/docker_1m/final_analyzed.csv
results/csv/docker_1m/final_summary.md
```

단, Docker Desktop은 macOS/Windows 위 Linux VM에서 실행됩니다. 따라서 이 결과는 “Docker Ubuntu Linux container 기준 반복 측정 결과”이지, 순수 물리 Linux 결과라고 주장하면 안 됩니다.

### 3. 1,000,000 trials 상위 성능 결과

| Case | Avg time | Min time | Stdev | Speedup | Efficiency | Valid | Checksum |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `thread_8_reduce` | `0.020732s` | `0.020030s` | `0.000601s` | `4.798x` | `0.600` | 1 | match |
| `hybrid_2x4` | `0.023281s` | `0.020878s` | `0.002841s` | `4.273x` | `0.534` | 1 | match |
| `hybrid_4x2` | `0.023545s` | `0.020877s` | `0.004660s` | `4.225x` | `0.528` | 1 | match |
| `thread_4_reduce` | `0.029020s` | `0.028023s` | `0.001494s` | `3.428x` | `0.857` | 1 | match |
| `pipeline_final_b1000` | `0.029900s` | `0.027392s` | `0.004725s` | `3.327x` | `0.832` | 1 | match |

### 4. Thread synchronization 비교

| Case | Avg time | Speedup | Valid | Checksum | 해석 |
| --- | ---: | ---: | ---: | ---: | --- |
| `thread_4_reduce` | `0.029020s` | `3.428x` | 1 | match | thread-local result 후 병합. 정확성과 성능 균형이 좋음 |
| `thread_4_mutex` | `0.132535s` | `0.751x` | 1 | match | 매 trial마다 lock을 잡아 contention이 큼 |
| `thread_4_nosync` | `0.040384s` | `2.463x` | 0 | mismatch | 빠르게 보일 수 있지만 결과가 틀림 |

이 비교는 synchronization의 필요성을 가장 직접적으로 보여줍니다.  
`nosync`는 성능 수치만 보면 괜찮아 보일 수 있지만, `valid=0`이고 checksum이 sequential과 다르므로 사용할 수 없습니다.

### 5. Process / Hybrid 비교

| Case | Avg time | Speedup | Efficiency | 해석 |
| --- | ---: | ---: | ---: | --- |
| `process_1_pipe` | `0.103178s` | `0.964x` | `0.964` | 단일 child는 fork/IPC overhead 때문에 sequential보다 약함 |
| `process_2_pipe` | `0.057633s` | `1.726x` | `0.863` | workload가 커지면서 process 병렬화 효과 발생 |
| `process_4_pipe` | `0.030225s` | `3.291x` | `0.823` | multi-process 병렬화 효과가 명확함 |
| `hybrid_2x4` | `0.023281s` | `4.273x` | `0.534` | process 격리 + 내부 thread 병렬화 효과 |
| `hybrid_4x2` | `0.023545s` | `4.225x` | `0.528` | 비슷한 worker 수에서 hybrid trade-off 확인 가능 |

process/hybrid는 작은 workload에서는 overhead가 크지만, `1,000,000 trials`에서는 병렬화 효과가 뚜렷해집니다. 다만 hybrid는 구조가 복잡하므로 “항상 최적”이 아니라 “process와 thread의 역할 분리를 보여주는 확장 구조”로 해석하는 것이 안전합니다.

### 6. Pipeline / Merge 비교

| Case | Avg time | Speedup | 해석 |
| --- | ---: | ---: | --- |
| `pipeline_final_b1000` | `0.029900s` | `3.327x` | final reduce가 가장 단순하고 안정적 |
| `pipeline_interactive_b1000` | `0.033507s` | `2.969x` | 중간 병합 가능하지만 queue/aggregator overhead 존재 |
| `pipeline_interactive_b100` | `0.063280s` | `1.572x` | batch가 너무 작으면 queue 접근과 wake-up 비용이 커짐 |
| `pipeline_interactive_b10000` | `0.030529s` | `3.258x` | queue overhead는 작지만 load balancing은 약해질 수 있음 |

interactive merge는 final reduce보다 항상 빠르지는 않습니다.  
하지만 현실 시스템처럼 partial result를 실행 중간에 계속 집계할 수 있다는 점에서 OS synchronization 분석 가치가 있습니다.

### 7. 이 결과로 주장 가능한 것 / 위험한 것

✅ 주장 가능한 것:

- 정상 mode는 모두 `valid=1`이고 sequential checksum과 일치합니다.
- 이번 Docker Ubuntu Linux 조건에서는 `thread_8_reduce`가 가장 빠른 평균 실행시간을 보였습니다.
- `thread_4_reduce`는 `thread_8_reduce`보다 느리지만 efficiency가 높아 worker 사용 효율이 좋습니다.
- `mutex`는 정확하지만 lock contention 때문에 성능이 크게 나쁩니다.
- `nosync`는 결과가 깨지므로 synchronization 필요성을 보여주는 실패 사례입니다.
- process/hybrid는 workload가 커질수록 병렬화 효과를 보여줍니다.
- interactive merge는 중간 병합이 가능하지만 queue/aggregator overhead가 존재합니다.

❌ 위험한 주장:

- “thread 8개가 항상 최적이다”
- “process는 항상 thread보다 느리다”
- “hybrid가 모든 조건에서 가장 좋다”
- “pipeline interactive가 final reduce보다 성능이 좋다”
- “Docker Desktop 결과가 순수 물리 Linux 결과와 완전히 같다”

더 자세한 재현 절차와 Linux/Windows WSL 실행 방법은 [`docs/reproducible_linux_experiment_guide.md`](docs/reproducible_linux_experiment_guide.md)에 정리되어 있습니다.
최종 제출 전 검증 결과, Amdahl stress 실험, CPU/memory 측정, 그래프 목록은 [`docs/final_validation_report.md`](docs/final_validation_report.md)에 정리했습니다.

---

## 🚀 최종 실험 계획

```sh
chmod +x scripts/run_final.sh
TRIALS=100000 STEPS=50 REPEATS=5 scripts/run_final.sh
```

결과 파일:

```text
results/csv/final_raw.csv
results/csv/final_analyzed.csv
results/csv/final_summary.md
```

`scripts/run_final.sh`가 수집하는 항목:

| 실험 | 목적 |
| --- | --- |
| Sequential | baseline |
| Thread 1/2/4/8 | thread scaling |
| Thread nosync/mutex/reduce | synchronization correctness/overhead 비교 |
| Process 1/2/4 | single/multi child process 비교 |
| Hybrid 2x2 / 2x4 / 4x2 | process + thread 조합 |
| Pipeline final vs interactive | merge 전략 비교 |
| Batch size 100/1000/10000 | queue granularity 분석 |

권장 최종 측정:

```sh
TRIALS=1000000 STEPS=100 REPEATS=5 \
PRE_WORK=50000 POST_WORK=10000 scripts/run_final.sh
```

자동 후처리로 계산되는 값:

```text
speedup = T_seq / T_parallel
efficiency = speedup / worker_count
time_total 평균 / 최소 / 표준편차
checksum 일치 여부
valid_all
```

CPU/memory usage는 `pidstat`, `/usr/bin/time -v`로 별도 캡처합니다.

Linux/Windows WSL/Docker 재현 절차와 100,000·1,000,000 trials 반복 측정 결과는 [`docs/reproducible_linux_experiment_guide.md`](docs/reproducible_linux_experiment_guide.md)에 정리했습니다.

Amdahl's Law를 더 명확히 보여주기 위해 순차 pre/post 구간을 키운 stress 실험도 추가로 수행했습니다.

```sh
TRIALS=1000000 STEPS=50 REPEATS=5 \
PRE_WORK=50000000 POST_WORK=10000000 \
OUT_DIR=results/csv/amdahl_stress scripts/run_final.sh
```

이 조건에서는 `thread_8_reduce` speedup이 기본 실험의 `4.798x`에서 `1.443x`로 제한되어, 순차 구간이 커질수록 병렬화 효율이 제한되는 현상을 확인했습니다.

최종 발표용 그래프는 `results/graphs/`에 생성되어 있습니다.

| 그래프 | 목적 |
| --- | --- |
| `thread_speedup_efficiency.svg` | thread scaling과 efficiency 감소 |
| `sync_compare.svg` | nosync/mutex/reduce 비교 |
| `process_hybrid_compare.svg` | process/hybrid 비교 |
| `pipeline_merge_compare.svg` | final reduce vs interactive merge |
| `stage_time_stacked.svg` | stage별 실행 시간 |
| `amdahl_stress_speedup.svg` | 기본 실험 vs Amdahl stress speedup |
| `amdahl_stress_stage_time.svg` | stress 조건 stage time |

---

## 🐳 Docker Linux 실행

macOS에서도 컴파일은 가능하지만, 발표/보고서용 성능 수치는 Linux 기준이 더 적합합니다.
다만 Docker Desktop은 macOS/Windows 위의 Linux VM에서 실행되므로 **순수 물리 Linux 결과와 완전히 같다고 주장하면 안 됩니다.**  
보고서에서는 “Docker Ubuntu Linux container에서 반복 측정했다”고 쓰고, 가능하면 팀원 중 Windows 사용자는 WSL2 Ubuntu에서 한 번 더 돌려 교차 확인하는 것이 좋습니다.

```sh
docker build -t os-montecarlo-risk .
docker run --rm -it os-montecarlo-risk
```

컨테이너 내부:

```sh
make clean
make
TRIALS=100000 STEPS=50 REPEATS=5 scripts/run_final.sh
```

CPU/memory 캡처:

```sh
pidstat -u -r -C sim 1
/usr/bin/time -v ./sim --mode hybrid --processes 2 --threads 4 \
  --trials 1000000 --steps 100 --ipc pipe --seed 42
```

---

## 🪟 Windows에서 Linux 환경으로 실행

이 프로젝트는 `pthread`, `fork`, `pipe`, `waitpid` 같은 POSIX/Linux API를 사용합니다.  
따라서 Windows native MinGW 환경보다는 **WSL2 Ubuntu** 또는 **Docker Desktop**을 권장합니다.

### WSL2 Ubuntu 권장

PowerShell 관리자 권한:

```powershell
wsl --install -d Ubuntu-22.04
```

Ubuntu 터미널:

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat

git clone https://github.com/deephoon/2026-OS_Project-motecarlo.git
cd 2026-OS_Project-motecarlo

make clean
make
make test

TRIALS=1000000 STEPS=50 REPEATS=5 \
PRE_WORK=50000 POST_WORK=10000 scripts/run_final.sh
```

결과 파일:

```text
results/csv/final_raw.csv
results/csv/final_analyzed.csv
results/csv/final_summary.md
```

### Windows Docker Desktop

PowerShell:

```powershell
git clone https://github.com/deephoon/2026-OS_Project-motecarlo.git
cd 2026-OS_Project-motecarlo

docker build -t os-montecarlo-risk .

docker run --rm `
  -v "${PWD}/results/csv/docker_1m:/workspace/results/csv" `
  os-montecarlo-risk `
  sh -lc "TRIALS=1000000 STEPS=50 REPEATS=5 PRE_WORK=50000 POST_WORK=10000 OUT_DIR=results/csv scripts/run_final.sh"
```

---

## 🐧 일반 Linux 실행

Ubuntu/Debian 기준:

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat

git clone https://github.com/deephoon/2026-OS_Project-motecarlo.git
cd 2026-OS_Project-motecarlo

make clean
make
make test

TRIALS=1000000 STEPS=50 REPEATS=5 \
PRE_WORK=50000 POST_WORK=10000 scripts/run_final.sh
```

CPU/memory 측정:

```sh
/usr/bin/time -v ./sim --mode thread --threads 8 \
  --trials 1000000 --steps 50 --sync reduce \
  --pre-work 50000 --post-work 10000 --metrics-detail 1
```

실행 중 CPU/RSS 관찰:

```sh
pidstat -u -r -C sim 1
```

---

## 📁 프로젝트 구조

```text
.
├── include/                 # public headers
├── src/                     # C source files
│   ├── main.c               # CLI dispatch, CSV output
│   ├── simulation.c         # Monte Carlo trial kernel
│   ├── sequential.c         # sequential baseline
│   ├── thread_mode.c        # pthread static mode
│   ├── pipeline_mode.c      # queue 기반 pipeline
│   ├── process_mode.c       # fork + pipe IPC
│   ├── hybrid_mode.c        # process 내부 pthread
│   ├── task_queue.c         # bounded task queue
│   ├── merge_queue.c        # partial result queue
│   ├── preprocess.c         # TaskBatch 생성
│   ├── postprocess.c        # validation/checksum
│   └── metrics.c            # stage timing
├── scripts/
│   ├── run_midterm.sh
│   ├── run_final.sh
│   └── analyze_results.py
├── docs/
│   ├── final_presentation_changes.md
│   ├── os26_final_experiment_guide.md
│   ├── final_validation_report.md
│   ├── final_plan.md
│   └── experiment_plan.md
├── results/
│   └── graphs/              # final presentation SVG graphs
├── results/
│   ├── csv/
│   └── res/
├── Makefile
├── Dockerfile
└── docker-compose.yml
```

---

## 🧭 최종 결론

이 프로젝트의 목표는 “가장 빠른 모드 하나를 찾는 것”이 아닙니다.

핵심은 다음입니다.

| 구조 | 최종 해석 |
| --- | --- |
| `thread + reduce` | hot loop에서 shared write를 제거하는 가장 실용적인 baseline |
| `mutex` | 정확하지만 lock contention을 보여주는 비교군 |
| `nosync` | synchronization이 없을 때 race condition이 생기는 증거 |
| `pipeline final` | queue 구조에서 단순하고 안정적인 병합 |
| `pipeline interactive` | 실시간 병합 가능, 하지만 queue/aggregator overhead 존재 |
| `process` | 격리성은 좋지만 `fork`와 IPC 비용이 있음 |
| `hybrid` | process/thread 역할 분리를 설명하기 좋지만 작은 workload에서는 과설계 가능 |

즉, 최종 메시지는 다음과 같습니다.

> 같은 CPU-bound 작업이라도 OS 실행 구조에 따라 정확성, overhead, scalability가 달라진다.  
> 이 프로젝트는 그 차이를 process, thread, synchronization, IPC, pipeline 관점에서 정량적으로 보여주는 실험 시스템이다.

---

## 🛠️ TODO

| 우선순위 | 작업 | 이유 |
| --- | --- | --- |
| 1 | Docker Linux에서 큰 workload 반복 측정 | 발표용 수치 신뢰도 확보 |
| 2 | Docker Linux에서 `final_analyzed.csv` 생성 | sequential baseline 기준 speedup/efficiency 확보 |
| 3 | 5회 이상 반복 측정 summary 검토 | 평균/최소/표준편차 해석 필요 |
| 4 | CPU/memory 캡처 | `pidstat`, `/usr/bin/time -v` 근거 확보 |
| 5 | 그래프 생성 | 보고서 가독성 향상 |
| 6 | shared memory IPC | pipe IPC와 비교 가능 |
| 7 | semaphore queue 비교 | mutex + condvar와 overhead 비교 가능 |

---

## 🧯 Troubleshooting

| 문제 | 해결 |
| --- | --- |
| `Permission denied` | `chmod +x scripts/run_final.sh scripts/analyze_results.py` |
| `make: command not found` | `apt-get install -y build-essential make` |
| `nosync`가 invalid | 정상입니다. race condition 관찰용 모드입니다. |
| `--ipc shm` 실패 | shared memory IPC는 TODO입니다. 현재 기본은 `--ipc pipe`입니다. |
| 8 threads가 더 느림 | core 수 한계, scheduling/context switching overhead를 확인해야 합니다. |
| macOS와 Docker 결과 차이 | 최종 수치는 Docker Linux 기준으로 통일하는 것이 좋습니다. |
