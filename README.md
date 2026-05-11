# 🚗 Monte Carlo 기반 차량 추종 위험 시뮬레이션

> 운영체제 프로젝트: **CPU-bound 병렬처리, Synchronization, Process/Thread/IPC 성능 분석**

이 프로젝트는 실제 차량 시뮬레이터를 만드는 것이 아닙니다.  
차량 추종 상황을 **독립적인 Monte Carlo trial**로 모델링하고, 같은 계산을 여러 OS 실행 구조로 수행하면서 정확성, 동기화 비용, 병렬화 효율, IPC overhead를 비교하는 실험 프로젝트입니다.

핵심 키워드:

`pthread` · `fork()` · `pipe IPC` · `mutex` · `condition variable` · `task queue` · `pipeline` · `local reduce` · `interactive merge` · `checksum validation`

---

## ✅ 현재 한 줄 요약

중간발표 당시에는 `seq/thread` 중심의 synchronization 비교가 핵심이었고, 현재는 최종 발표 요구사항에 맞춰 **Process / Hybrid / Pipeline / IPC / Interactive Merge 구조까지 확장**되었습니다.

| 구분 | 중간발표 | 현재 상태 |
| --- | --- | --- |
| 실행 모드 | `seq`, `thread` | `seq`, `thread`, `pipeline`, `process`, `hybrid` |
| 동기화 | `nosync`, `mutex`, `reduce` | 유지 + queue/merge synchronization 추가 |
| 작업 분배 | static partition | static partition + bounded task queue |
| 결과 병합 | final reduce 중심 | final reduce + interactive merge |
| Process | 계획 단계 | `fork()` 기반 구현 |
| IPC | 계획 단계 | pipe 기반 child result 전달 |
| Hybrid | 계획 단계 | process 내부 pthread worker |
| 성능 지표 | 실행시간 중심 | stage time, throughput, validation CSV |

---

## 🎯 프로젝트 목표

같은 CPU-bound Monte Carlo 계산을 다양한 OS 구조로 실행하고, 다음 질문에 답하는 것이 목표입니다.

| 질문 | 실험으로 보여주는 내용 |
| --- | --- |
| Thread를 늘리면 항상 빨라지는가? | thread count별 실행시간과 overhead 비교 |
| Lock 없이 공유 결과를 갱신하면 어떻게 되는가? | `nosync` race condition과 `valid=0` 가능성 |
| Mutex는 정확하지만 왜 느릴 수 있는가? | per-trial lock contention |
| Local reduce는 왜 유리한가? | hot loop에서 shared write 제거 |
| Process는 thread보다 좋은가? | 격리성 vs `fork`/IPC overhead |
| Hybrid 구조는 언제 의미가 있는가? | process와 thread 역할 분리 |
| Interactive merge는 항상 빠른가? | merge queue와 aggregator overhead 분석 |

---

## 🧩 구현된 기능

| 기능 | 상태 | 설명 |
| --- | --- | --- |
| Sequential baseline | ✅ 완료 | 단일 thread 기준 실행 |
| Pthread thread mode | ✅ 완료 | static partition 기반 thread 병렬화 |
| `nosync` | ✅ 완료 | race condition 관찰용 |
| `mutex` | ✅ 완료 | shared result 보호 |
| `reduce` | ✅ 완료 | thread-local result 후 병합 |
| Task queue | ✅ 완료 | mutex + condition variable 기반 bounded queue |
| Pipeline mode | ✅ 완료 | batch 생성, queue, worker, merge stage |
| Final merge | ✅ 완료 | worker 종료 후 한 번에 병합 |
| Interactive merge | ✅ 완료 | aggregator thread가 실행 중간 병합 |
| Process mode | ✅ 완료 | `fork()` 기반 child process 실행 |
| Pipe IPC | ✅ 완료 | child result를 parent로 전달 |
| Hybrid mode | ✅ 완료 | child process 내부 pthread worker |
| CSV metrics | ✅ 완료 | stage별 시간, throughput, validation 출력 |
| Docker Linux 환경 | ✅ 완료 | Ubuntu 기반 재현 환경 |
| Shared memory IPC | 🚧 TODO | `--ipc shm`은 확장 예정 |
| 반복 평균 summary 자동화 | 🚧 TODO | 현재는 raw CSV 수집 중심 |

---

## 🏗️ 전체 구조

```text
CLI Config
   |
   v
Pre-processing
   - trials를 TaskBatch로 분할
   - batch metadata 생성
   |
   v
Execution Mode
   +--> seq
   +--> thread
   +--> pipeline
   +--> process
   +--> hybrid
   |
   v
Merge Stage
   +--> final reduce
   +--> interactive merge
   |
   v
Post-processing
   - hist_sum 검증
   - checksum 계산
   - valid flag 출력
   |
   v
CSV Output
```

---

## 🔬 Simulation Model

각 trial은 하나의 차량 추종 상황입니다.

1. deterministic seed 기반으로 차량 초기 상태 생성
2. ego/front 차량 속도, 거리, 반응 시간, 감속도 설정
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

trial은 서로 독립적이므로 병렬화에 적합합니다.  
실행 순서가 달라도 같은 trial index는 같은 seed를 사용하도록 설계했습니다.

```text
trial_seed = base_seed ^ (trial_index * 2654435761u)
```

---

## ⚙️ 실행 모드 설명

### 1. `seq`

단일 thread로 모든 trial을 순차 실행합니다.  
모든 성능 비교의 기준 baseline입니다.

```sh
./sim --mode seq --trials 10000 --steps 30 --seed 42
```

### 2. `thread`

`pthread_create()`로 worker thread를 만들고 static partition으로 trial range를 나눕니다.

```sh
./sim --mode thread --threads 4 --trials 10000 --steps 30 --sync reduce --seed 42
```

| Sync mode | 의미 | 해석 |
| --- | --- | --- |
| `nosync` | lock 없이 shared result 갱신 | 빠를 수 있지만 race condition 가능 |
| `mutex` | 매 trial마다 lock/unlock | 정확하지만 lock contention 발생 |
| `reduce` | thread-local result 후 merge | 정확성과 성능의 균형 |

### 3. `pipeline`

trial을 batch로 나누고, bounded task queue를 통해 worker thread가 batch를 가져갑니다.

```sh
./sim --mode pipeline --schedule queue --merge interactive \
  --threads 4 --trials 10000 --steps 30 \
  --batch-size 1000 --queue-size 1024 --seed 42
```

| Merge mode | 의미 | 장점 | 단점 |
| --- | --- | --- | --- |
| `final` | worker 종료 후 마지막에 병합 | 단순하고 overhead 작음 | 결과 병합이 끝에 몰림 |
| `interactive` | batch 결과를 aggregator가 중간 병합 | pipeline 구조 설명에 좋음 | queue/condvar overhead 증가 |

### 4. `process`

parent가 child process를 `fork()`하고, child가 계산한 `Result`를 pipe로 전달합니다.

```sh
./sim --mode process --processes 2 --trials 10000 --steps 30 --ipc pipe --seed 42
```

```text
Parent
  -> fork child
  -> pipe read
  -> waitpid
  -> final merge
```

### 5. `hybrid`

process와 thread를 함께 사용합니다.  
child process 내부에서 pthread worker가 trial range를 병렬 계산합니다.

```sh
./sim --mode hybrid --processes 2 --threads 2 \
  --trials 10000 --steps 30 --ipc pipe --seed 42
```

---

## 🧪 빌드 및 빠른 검증

```sh
make clean
make
make test
```

`make test`는 다음을 확인합니다.

| 확인 항목 | 기대 결과 |
| --- | --- |
| build 성공 | `gcc -std=c11 -O2 -Wall -Wextra -pthread` |
| sequential 실행 | `valid=1` |
| thread reduce 실행 | `valid=1` |
| pipeline interactive 실행 | `valid=1` |
| checksum | 같은 seed/조건에서 동일 |

정확성 기준:

```text
total_trials == trials
hist_sum == trials
valid == 1
checksum 동일
```

---

## 📊 실험 결과 요약

아래 표는 업로드된 `results/res/*/final_results.csv` 기준입니다.  
조건은 주로 `trials=10000`, `steps=30`, `seed=42`이며, 실행 시간이 매우 짧기 때문에 **기능 검증용 결과**로 보는 것이 현실적입니다.

### ✅ 정확성 검증

| 항목 | 결과 |
| --- | --- |
| CSV row 수 | 30개 |
| `valid=1` 비율 | 100% |
| `hist_sum` | 모두 `10000` |
| checksum | 모두 `9158329899332878926` |
| 해석 | 모든 구현 모드가 같은 simulation 결과를 재현 |

### ⏱️ 평균 실행시간 요약

| Mode | 조건 | 평균 `time_total` | 해석 |
| --- | --- | ---: | --- |
| `seq` | baseline | `0.001524s` | 기준 실행 |
| `thread` | 1 thread | `0.001475s` | seq와 거의 유사 |
| `thread` | 2 threads | `0.001092s` | 병렬화 효과 관찰 |
| `thread` | 4 threads | `0.000749s` | 해당 결과에서 가장 빠른 thread 조건 |
| `thread` | 8 threads | `0.000882s` | 4 threads보다 느려짐, scheduling overhead 가능 |
| `pipeline final` | batch 1000 | `0.000954s` | queue 구조 중 안정적 |
| `pipeline interactive` | batch 1000 | `0.001112s` | aggregator/merge queue overhead 존재 |
| `pipeline interactive` | batch 100 | `0.002188s` | batch가 너무 작아 queue 접근 비용 증가 |
| `pipeline interactive` | batch 10000 | `0.001852s` | batch가 너무 커 load balancing 약화 가능 |
| `process` | 2 processes | `0.001211s` | fork/IPC overhead 존재 |
| `process` | 4 processes | `0.001003s` | 2 processes보다 개선되지만 thread 4보다 느림 |
| `hybrid` | 2 processes x 2 threads | `0.001128s` | 구조는 정상, overhead 존재 |
| `hybrid` | 2 processes x 4 threads | `0.001264s` | 작은 workload에서는 과설계 가능 |

### 🧠 결과 해석

현재 결과에서 바로 말할 수 있는 점:

- ✅ 모든 모드가 `valid=1`과 동일 checksum을 보여 정확성은 확인되었습니다.
- ✅ `thread + reduce + 4 threads`가 10,000 trials 조건에서는 가장 실용적으로 보입니다.
- ⚠️ `8 threads`가 `4 threads`보다 느려지는 현상이 있어 thread 수 증가가 항상 성능 향상으로 이어지지는 않습니다.
- ⚠️ `interactive merge`는 구조적으로 의미 있지만, 작은 workload에서는 queue와 aggregator overhead 때문에 `final merge`보다 느릴 수 있습니다.
- ⚠️ `process`와 `hybrid`는 OS 개념 설명에는 좋지만, 작은 workload에서는 `fork`, pipe, `waitpid`, thread 생성 비용이 커질 수 있습니다.

중요한 제한:

> 10,000 trials 결과는 시간이 1ms 안팎이라 OS scheduler noise의 영향을 크게 받습니다.  
> 최종 보고서의 성능 결론은 `100000` 또는 `1000000` trials 이상에서 반복 측정한 결과로 작성해야 합니다.

---

## 📈 성능 지표와 계산식

CSV 주요 필드:

| 필드 | 의미 |
| --- | --- |
| `time_total` | 전체 실행 시간 |
| `time_pre` | batch 생성 등 전처리 |
| `time_compute` | worker 계산 구간 |
| `time_sync` | queue wait, lock 등 synchronization 비용 |
| `time_merge` | partial result 병합 시간 |
| `time_post` | validation/checksum 등 후처리 |
| `throughput_batches_per_sec` | 초당 처리 batch 수 |
| `hist_sum` | histogram 합계 |
| `checksum` | 결과 재현성 확인 |
| `valid` | 결과 정합성 flag |

현재 단일 실행 CSV의 `speedup`, `efficiency`는 placeholder입니다.  
보고서에서는 다음 식으로 후처리 계산합니다.

```text
speedup = T_seq / T_parallel
efficiency = speedup / worker_count
sequential_fraction_estimate = (T_pre + T_sync + T_merge + T_post) / T_total
```

---

## 🚀 최종 실험 자동화

```sh
chmod +x scripts/run_final.sh
TRIALS=100000 STEPS=50 scripts/run_final.sh
```

결과 파일:

```text
results/csv/final_results.csv
```

`scripts/run_final.sh`가 수집하는 항목:

| 실험 | 목적 |
| --- | --- |
| Sequential | baseline |
| Thread 1/2/4/8 | thread scaling |
| Pipeline final vs interactive | merge 전략 비교 |
| Batch size 100/1000/10000 | queue granularity 분석 |
| Process 2/4 | child process overhead |
| Hybrid 2x2 / 2x4 | process + thread 조합 |

권장 최종 측정:

```sh
TRIALS=1000000 STEPS=50 scripts/run_final.sh
```

반복 측정은 아직 자동 summary가 없으므로 다음 방식으로 보강합니다.

```text
1. 같은 조건을 5회 이상 실행
2. time_total 평균 / 최소 / 표준편차 계산
3. sequential baseline 기준 speedup 계산
4. worker_count 기준 efficiency 계산
```

---

## 🐳 Docker Linux 실행

macOS에서도 컴파일은 가능하지만, 발표/보고서용 성능 수치는 Linux 기준이 더 적합합니다.

```sh
docker build -t os-montecarlo-risk .
docker run --rm -it os-montecarlo-risk
```

컨테이너 내부:

```sh
make clean
make
TRIALS=100000 STEPS=50 scripts/run_final.sh
```

CPU/memory 캡처:

```sh
pidstat -u -r -C sim 1
/usr/bin/time -v ./sim --mode hybrid --processes 2 --threads 4 \
  --trials 1000000 --steps 100 --ipc pipe --seed 42
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
│   └── run_final.sh
├── docs/
│   ├── final_presentation_changes.md
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

## 🧭 최종 보고서에서 가져갈 결론

| 구조 | 결론 |
| --- | --- |
| `thread + reduce` | hot loop에서 shared write를 제거하는 가장 실용적인 baseline |
| `mutex` | 정확하지만 lock contention을 보여주는 비교군 |
| `nosync` | race condition 설명에 적합 |
| `pipeline final` | queue 구조에서 비교적 단순하고 안정적인 병합 |
| `pipeline interactive` | 실시간 병합 가능, 하지만 queue/aggregator overhead 존재 |
| `process` | 격리성은 좋지만 `fork`와 IPC 비용이 있음 |
| `hybrid` | process/thread 역할 분리를 설명하기 좋지만 작은 workload에서는 과설계 가능 |

프로젝트의 최종 가치는 단순히 가장 빠른 모드를 찾는 것이 아닙니다.  
같은 CPU-bound 작업을 여러 OS 실행 구조로 바꾸어 보면서 **정확성, overhead, scalability의 trade-off를 설명할 수 있다는 점**이 핵심입니다.

---

## 🛠️ TODO

| 우선순위 | 작업 | 이유 |
| --- | --- | --- |
| 1 | Docker Linux에서 큰 workload 반복 측정 | 발표용 수치 신뢰도 확보 |
| 2 | speedup/efficiency 후처리 | sequential baseline 기준 비교 |
| 3 | 5회 이상 반복 측정 summary | 평균/최소/표준편차 필요 |
| 4 | CPU/memory 캡처 | `pidstat`, `/usr/bin/time -v` 근거 확보 |
| 5 | 그래프 생성 | 보고서 가독성 향상 |
| 6 | shared memory IPC | pipe IPC와 비교 가능 |

---

## 🧯 Troubleshooting

| 문제 | 해결 |
| --- | --- |
| `Permission denied` | `chmod +x scripts/run_final.sh` |
| `make: command not found` | `apt-get install -y build-essential make` |
| `nosync`가 invalid | 정상이다. race condition 관찰용 모드 |
| `--ipc shm` 실패 | shared memory IPC는 TODO |
| 8 threads가 더 느림 | core 수 한계, scheduling/context switching overhead 확인 |
| macOS와 Docker 결과 차이 | 최종 수치는 Docker Linux 기준으로 통일 |

