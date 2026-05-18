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
│   ├── final_plan.md
│   └── experiment_plan.md
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
