# 운영체제 프로젝트 최종 보고서 작성 초안

> 이 문서는 팀원들이 동일한 최신 구현과 실험 결과를 기준으로 최종 문서를 작성하기 위한 공동 초안이다.
> 표와 수치는 `results/csv`의 최신 결과를 기준으로 작성했으며, 개인 기여도는 실제 수행 내역을 팀 회의에서 확인한 뒤 확정한다.

---

## 0. 작성 원칙

### 프로젝트의 핵심 주장

이 프로젝트의 핵심은 단순히 가장 빠른 실행 모드를 찾는 것이 아니다.

```text
실제 Monte Carlo 차량 추종 위험 계산을
child process, pthread, hybrid, pipeline 구조로 구현하고,
동기화 문제와 실행 구조별 overhead를 실험으로 확인한 뒤,
정확성과 N-core 확장성을 함께 확보하는 구조로 개선하였다.
```

최종 문서 전체는 다음 흐름을 유지한다.

```text
초기 구조와 목표
  -> 문제 발생
  -> 실험 데이터로 원인 확인
  -> 구조 개선
  -> 동일 조건 재측정
  -> 정확성 및 성능 검증
  -> 남아 있는 한계 분석
```

### 반드시 지켜야 할 수치 사용 기준

- 최종 strong scaling 수치는 실제 Monte Carlo `120,000,000 trials`, `steps=50`, 각 5회 결과를 사용한다.
- CPU utilization 수치는 Docker Desktop 기반 Ubuntu 22.04 Linux VM에서 측정한 값이라고 명시한다.
- 정상 성능 비교에는 `valid=1`, 동일 checksum인 결과만 사용한다.
- `nosync` 결과는 빠른 성능 결과가 아니라 race condition 재현 근거로 사용한다.
- Docker Desktop 결과를 native Linux 절대 성능으로 표현하지 않는다.
- 근소한 실행시간 차이를 절대적인 우열로 과장하지 않는다.
- `T_sync`는 여러 worker의 대기 시간 누적값일 수 있으므로 `T_total`과 단순 합산하지 않는다.

---

## 1. 팀 작성 역할 제안

아래 역할은 현재 개인 산출물과 구현 영역을 기준으로 한 작성 업무 제안이다.

| 담당자 | 근거가 확인된 영역 | 담당할 본문 | 추가 확인 업무 |
| --- | --- | --- | --- |
| 김태환 | Process, pipe IPC, Hybrid 구조 | Process/IPC/Hybrid 설계, process-thread 비교, shared memory 개선 과정 | `process_mode.c`, `hybrid_mode.c`, `ipc_pipe.c`, `ipc_shm.c` 코드 근거 검수 |
| 성도연 | TaskQueue, MergeQueue, Aggregator, mutex + condition variable | Synchronization, producer-consumer, Final Reduce와 Interactive Merge | queue 코드 캡처와 pipeline 흐름도 검수 |
| 유지원 | Simulation model, metrics, 환경 구축, 실험 데이터 추출 | 시스템 개요, Monte Carlo 계산, 실험 환경, scaling/utilization 결과 분석 | 최신 CSV 기반 그래프 및 수치 검수 |
| 정종근 | 구조 비교표, 용어 정리, 실행 결과 표 작성 경험 | 비교표 통합, 실행 명령 검증, 용어 통일, 문서 형식 검수 | 표 수치와 `valid/checksum` 일치 여부 확인 |

### 역할 배분 시 주의사항

- 정종근 님에게는 범위가 모호한 전체 분석보다, 입력과 완료 조건이 명확한 검수 업무를 배정한다.
- 한 명이 작성한 기술 설명은 해당 구현 담당자가 반드시 교차 검수한다.
- 개인 기여도 비율은 위 표를 그대로 사용하지 않는다. 실제 코드, 실험, 문서 작성량을 회의에서 확인한 뒤 합계 100%로 결정한다.
- 각 담당자는 자신이 작성한 문단 아래에 사용한 코드 파일과 CSV 경로를 기록해 근거를 남긴다.

### 권장 통합 순서

1. 유지원: 시스템 개요와 실험 조건, 최신 결과표를 먼저 고정한다.
2. 김태환: process, IPC, hybrid 구조와 개선 과정을 작성한다.
3. 성도연: synchronization, queue, pipeline 문제 해결 과정을 작성한다.
4. 정종근: 전체 표, 용어, 실행 명령, 결과 일치 여부를 검수한다.
5. 전원: 결론과 개인 기여도, 수행 과정 기록을 함께 확정한다.

---

## 2. 프로젝트 제목

### Monte Carlo 기반 차량 추종 위험 시뮬레이션의 운영체제 병렬처리 구조 설계 및 성능 분석

부제 예시:

> Child Process, POSIX Thread, Synchronization, IPC, Hybrid 및 Pipeline 구조 비교

---

## 3. 요약

본 프로젝트는 Monte Carlo 기반 차량 추종 위험 시뮬레이션을 CPU-bound workload로 구성하고, 동일한 계산을 sequential, pthread, child process, hybrid, pipeline 구조로 실행하여 정확성, 실행시간, CPU utilization, synchronization 및 IPC overhead를 비교하였다.

초기 계산은 각 trial이 독립적이어서 쉽게 병렬화할 수 있었지만, shared result 갱신 과정에서 race condition이 발생했고, mutex를 trial마다 사용하는 구조에서는 lock contention으로 성능이 저하되었다. 이를 해결하기 위해 worker별 local result를 계산한 후 마지막에 병합하는 local reduce를 적용하였다.

Process mode에서는 각 child process가 독립적인 trial range 전체를 계산한 후 결과를 pipe 또는 anonymous shared memory를 통해 parent에게 한 번만 전달하도록 구성하였다. Hybrid mode에서는 child process 내부에 pthread worker를 생성하여 process 단위 격리와 thread 단위 병렬화를 결합하였다. Pipeline mode에서는 bounded task queue, merge queue, mutex, condition variable, aggregator thread를 사용해 producer-consumer와 interactive merge 구조를 구현하였다.

Docker Desktop 기반 Ubuntu 22.04 Linux VM에서 실제 Monte Carlo workload `120,000,000 trials`, `steps=50`을 각 조건별 5회 반복 측정한 결과, 4-worker 조건에서 thread, process, hybrid, pipeline 구조는 각각 `92.5~94.0%` efficiency를 기록하였다. 또한 대표 4-worker 구조의 worker당 CPU utilization은 `99.2~99.8%`, affinity 대상 core의 최소 utilization은 `99.7%`로 측정되었다. 모든 정상 실행 모드는 동일 checksum과 `valid=1`을 유지하였다.

반면 8-worker 조건에서는 CPU utilization이 여전히 높았지만 efficiency가 `62.3~69.6%`로 감소하였다. 이는 core를 바쁘게 유지하는 것과 실행시간이 이상적으로 `T1/N`으로 감소하는 것이 동일하지 않으며, scheduler, worker 관리, queue 및 IPC overhead가 확장성에 영향을 준다는 점을 보여준다.

---

## 4. 시스템 개요

### 4.1 주제 선정 배경

운영체제의 process와 thread는 모두 병렬처리에 사용할 수 있지만, 주소 공간 공유 여부, 생성 비용, synchronization 필요성, IPC 비용에서 차이가 있다. 동일한 작업을 여러 실행 구조로 구현하고 정량적으로 비교하려면 충분한 CPU 계산량, deterministic한 결과, worker 수 조절 가능성, 정확성 검증 수단이 필요하다.

본 프로젝트는 차량 추종 상황에서 반복적으로 위험도를 계산하는 Monte Carlo simulation을 사용하였다. 각 trial은 초기 속도, 차간 거리, 반응시간, 감속력 등을 deterministic seed로 생성하고, 여러 time step 동안 차량 상태를 계산한 뒤 위험 등급과 충돌 여부를 반환한다.

### 4.2 프로젝트 목표

1. Child process와 pthread를 이용해 실제 병렬 프로그램을 구현한다.
2. 여러 worker가 shared result를 수정할 때 발생하는 synchronization 문제를 재현한다.
3. Mutex와 local reduce를 비교하여 정확성과 성능을 함께 확보한다.
4. Pipe와 shared memory IPC의 구조적 차이를 비교한다.
5. Process와 thread를 결합한 hybrid 구조를 구현한다.
6. Task queue와 interactive merge를 이용해 producer-consumer pipeline을 구현한다.
7. Worker 수 증가에 따라 실행시간이 `T1/N`에 얼마나 가까워지는지 측정한다.
8. 각 core가 균일하게 활용되는지 CPU utilization으로 검증한다.

### 4.3 Monte Carlo 계산 흐름

```text
Trial index
  -> deterministic seed 생성
  -> 초기 차량 상태 생성
  -> time step별 위치와 속도 갱신
  -> TTC, 위험 등급, 충돌 여부 계산
  -> histogram과 collision count에 결과 1회 반영
```

Trial index 기반 seed를 사용하므로 worker 수, 실행 순서, process/thread 구조가 달라도 정상 모드의 최종 checksum은 동일하다.

### 4.4 전체 시스템 흐름

```text
CLI Config
   |
   v
Pre-processing
   |
   v
Execution Engine
   +-- Sequential
   +-- pthread workers
   +-- Child processes
   +-- Child processes + pthread workers
   +-- Task queue + pipeline workers
   |
   v
Result Merge
   |
   v
Post-processing
   +-- hist_sum validation
   +-- checksum validation
   +-- metrics collection
   +-- CSV output
```

**삽입할 그림**

- 그림 1: 전체 시스템 실행 흐름도
- 그림 2: Monte Carlo trial 내부 계산 흐름
- 그림 3: mode별 process/thread 구성 비교

---

## 5. 운영체제 구성요소 설계

### 5.1 Sequential Mode

Sequential mode는 parent process 하나가 전체 trial을 순서대로 실행한다. 이 결과는 병렬 실행의 speedup과 efficiency를 계산하기 위한 baseline으로 사용한다.

```text
speedup = T_seq / T_parallel
efficiency = speedup / worker_count
```

### 5.2 Thread Mode

Thread mode는 하나의 process 내부에서 여러 pthread가 trial range를 나누어 계산한다. Thread는 같은 주소 공간을 공유하므로 생성 비용이 낮고 결과 공유가 쉽지만, shared data를 동시에 수정하면 race condition이 발생한다.

지원 synchronization 방식:

| 방식 | 동작 | 정확성 | 예상 성능 |
| --- | --- | --- | --- |
| `nosync` | shared result를 보호 없이 갱신 | 보장 불가 | 빠르게 보일 수 있으나 사용 불가 |
| `mutex` | trial마다 shared result lock | 보장 | lock contention이 큼 |
| `reduce` | worker별 local result, join 이후 병합 | 보장 | hot loop lock 제거 |

### 5.3 Process Mode

Process mode는 parent가 `fork()`로 여러 child process를 생성하고 각 child에 독립적인 trial range를 할당한다.

```text
Parent
  -> fork child processes
  -> child별 trial range 계산
  -> child-local Result 완성
  -> pipe 또는 shared memory로 결과 1회 전달
  -> waitpid()
  -> parent result merge
```

Process는 독립 주소 공간을 가지므로 thread의 shared-result race가 발생하지 않는다. 대신 결과 전달을 위해 IPC가 필요하며 fork, waitpid, read/write 또는 shared memory 접근 비용이 추가된다.

### 5.4 IPC 설계

#### Pipe

Child가 계산 완료 후 `Result` 구조체를 pipe에 기록하고 parent가 읽는다.

장점:

- 메시지 전달 흐름이 명확하다.
- Child-parent 통신 관계를 설명하기 쉽다.

비용:

- `write()`와 `read()`가 필요하다.
- 결과 전달과 descriptor 관리 비용이 발생한다.

#### Anonymous Shared Memory

`mmap(MAP_SHARED | MAP_ANONYMOUS)`으로 child별 result slot을 생성한다.

```text
slot 0: child 0만 write
slot 1: child 1만 write
...
parent: waitpid 이후 slot read
```

각 child가 자기 slot만 쓰고 parent가 종료 확인 이후 읽기 때문에 hot path에서 별도 lock이나 busy waiting이 필요 없다.

### 5.5 Hybrid Mode

Hybrid mode는 child process 내부에 pthread worker를 생성한다.

```text
Parent process
  -> Child process group
       -> pthread workers
       -> child-local synchronization 또는 reduce
       -> child-local Result 완성
       -> IPC 결과 전달
  -> Parent merge
```

Process는 큰 simulation group을 담당하고, process 내부 thread는 해당 범위를 세분화하여 계산한다. 이 구조를 통해 독립 주소 공간과 shared address space의 장단점을 한 시스템 안에서 비교할 수 있다.

### 5.6 Pipeline Mode

Pipeline mode는 작업을 batch로 분할하고 bounded queue를 통해 worker에게 동적으로 전달한다.

```text
Preprocessor
  -> TaskQueue
  -> Worker Pool
  -> Final Reduce

또는

Preprocessor
  -> TaskQueue
  -> Worker Pool
  -> MergeQueue
  -> Aggregator Thread
```

Static partition은 균등 workload에서 overhead가 작다. Queue scheduling은 lock과 condition variable 비용이 있지만 skewed workload에서 load imbalance를 줄일 수 있다.

---

## 6. Synchronization 문제와 해결 과정

### 6.1 문제 1: Nosync Race Condition

여러 pthread가 같은 histogram과 counter를 동시에 갱신하면 read-modify-write 연산이 겹쳐 lost update가 발생한다.

최신 hybrid synchronization 실험에서 `nosync`는 5회 모두 `valid=0`이었고 매 반복마다 checksum이 달랐다.

| Case | Avg Time | Valid | Checksum 상태 |
| --- | ---: | ---: | --- |
| `hybrid_2x2_nosync_shm` | `0.0312s` | `0` | 5회 모두 불일치 |

이 결과는 실행시간만으로 synchronization 전략을 선택할 수 없으며 정확성 검증이 선행되어야 함을 보여준다.

### 6.2 문제 2: Trial 단위 Mutex Contention

Mutex를 사용하면 정확성은 회복되지만 모든 trial이 하나의 shared result lock을 획득해야 한다.

Thread 2-worker 실험:

| Case | Avg Time | Avg Sync Time | Lock Count | Valid |
| --- | ---: | ---: | ---: | ---: |
| `thread_2_mutex` | `0.1794s` | `0.1077s` | `1,000,000` | `1` |
| `thread_2_reduce` | `0.0509s` | 약 `0s` | `0` | `1` |

Mutex mode에서는 lock 대기 시간이 전체 성능을 지배했다.

### 6.3 해결: Local Reduce

각 worker가 독립적인 local result를 계산하고 worker 종료 이후 한 번만 merge한다.

```text
Worker 0 -> Local Result 0
Worker 1 -> Local Result 1
...
join
-> Final Result Merge
```

Hybrid 2x2 결과:

| Case | Avg Time | Speedup | Efficiency | Valid |
| --- | ---: | ---: | ---: | ---: |
| `hybrid_2x2_mutex_shm` | `0.0563s` | `1.866x` | `46.7%` | `1` |
| `hybrid_2x2_reduce_shm` | `0.0288s` | `3.643x` | `91.1%` | `1` |

Local reduce는 mutex보다 약 `1.95x` 빠르며 정확성도 유지하였다.

### 6.4 Mutex + Condition Variable Queue

TaskQueue와 MergeQueue는 `buffer`, `head`, `tail`, `count`, `closed`를 여러 thread가 공유한다. 이 복합 상태는 mutex로 하나의 critical section으로 보호한다.

Mutex만으로는 queue가 비었거나 가득 찼을 때 효율적인 대기를 표현하기 어렵다. 따라서 다음 condition variable을 사용한다.

| Condition Variable | 역할 |
| --- | --- |
| `not_empty` | queue에 작업이 들어올 때 consumer를 깨움 |
| `not_full` | queue에 공간이 생길 때 producer를 깨움 |

이 구조는 empty/full 상태에서 busy waiting을 방지한다.

### 6.5 Semaphore 대신 Mutex + Condition Variable을 사용한 이유

Semaphore는 사용 가능한 자원 수를 표현하기 좋지만, 본 queue에서는 단순 count뿐 아니라 `head`, `tail`, `closed`, buffer 상태를 함께 원자적으로 변경해야 한다. 따라서 공유 상태 보호에는 mutex가 필요하며, empty/full 상태 대기는 condition variable이 직접적으로 표현한다.

---

## 7. 문제 인식과 구조 개선 과정

### 7.1 초기 구조의 한계

초기 Monte Carlo trial은 서로 독립적이어서 static partition만으로 쉽게 병렬화되었다. 이 구조만으로는 synchronization, IPC, scheduling, 중간 결과 병합 등 운영체제 구조의 차이를 충분히 분석하기 어려웠다.

### 7.2 개선 내용

| 단계 | 문제 | 원인 분석 | 구조 개선 | 검증 |
| --- | --- | --- | --- | --- |
| 1 | Shared result 손상 | pthread race condition | mutex 추가 | `valid=1` 회복 |
| 2 | Mutex 성능 저하 | trial별 lock contention | local reduce | mutex 대비 성능 개선 |
| 3 | Process 결과 공유 필요 | 독립 주소 공간 | pipe IPC | 정상 결과 전달 |
| 4 | Pipe 외 IPC 비교 부족 | read/write 전달 비용 | child별 shared memory slot | pipe/shm 비교 가능 |
| 5 | Process/thread 역할 결합 필요 | 단일 구조 비교 한계 | hybrid 구현 | 2x2, 2x4 비교 |
| 6 | Final reduce만으로 중간 집계 불가 | worker 종료 전 결과 없음 | merge queue + aggregator | interactive merge 구현 |
| 7 | Static partition 필요성만 보임 | 균등 workload | skewed workload + queue | scheduling 비교 가능 |
| 8 | Core 배치가 불명확 | scheduler 배치 영향 | Linux CPU affinity | core별 균일도 검증 |
| 9 | 정상 mode 회귀 가능 | 실행만 하고 결과 자동 비교 부족 | `test_modes.sh` | 8개 mode checksum 자동 검사 |

---

## 8. 실험 설계

### 8.1 환경

| 항목 | 값 |
| --- | --- |
| 실행 환경 | Docker Desktop 기반 Ubuntu 22.04 Linux VM |
| Architecture | `aarch64` |
| Docker 할당 CPU | 10 |
| Compiler | GCC, `-O2`, POSIX pthread |
| CPU 측정 | GNU `/usr/bin/time -v` |
| Core별 측정 | `mpstat -P ALL 1` |
| 정확성 | `valid`, `hist_sum`, checksum |

Docker Desktop은 Linux VM에서 실행되므로 동일 환경 내부의 상대 비교에는 사용할 수 있지만 native Linux 절대 성능으로 일반화할 수 없다.

### 8.2 Main Strong Scaling 조건

| 항목 | 값 |
| --- | --- |
| Trials | `120,000,000` |
| Steps | `50` |
| Profile | `default` |
| Pre/Post work | `0 / 0` |
| Worker 수 | `1, 2, 4, 8` |
| 반복 | 각 case 5회 |
| Affinity | 활성화 |

총 workload를 고정하고 worker 수만 증가시키는 strong scaling 방식이다.

### 8.3 정확성 판정

정상 결과는 다음 조건을 모두 만족해야 한다.

```text
valid = 1
hist_sum = trials
checksum = sequential checksum
반복 실행 checksum_count = 1
```

### 8.4 측정 지표

| Metric | 의미 |
| --- | --- |
| `time_total` | End-to-end wall-clock 시간 |
| `time_compute` | Worker 실행 구간 |
| `time_sync` | Lock/condition wait 누적값 |
| `time_ipc` | Parent에서 관측한 IPC 시간 |
| `time_merge` | 결과 병합 시간 |
| `speedup` | `T_seq / T_parallel` |
| `efficiency` | `speedup / workers` |
| CPU% | 전체 worker CPU 활용률 |
| Util/Worker | CPU% / worker 수 |
| Max RSS | GNU time이 관측한 최대 resident memory |

---

## 9. 실험 결과

### 9.1 실제 Monte Carlo Strong Scaling

| Case | Workers | Avg Time | Stdev | Speedup | Efficiency | CPU/Worker |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `seq` | 1 | `12.111s` | `0.366s` | `1.000x` | `100.0%` | `99.0%` |
| `thread_1_reduce` | 1 | `12.084s` | `0.423s` | `1.002x` | `100.2%` | `99.0%` |
| `process_1_shm` | 1 | `12.192s` | `0.275s` | `0.993x` | `99.3%` | `99.0%` |
| `thread_2_reduce` | 2 | `6.148s` | `0.173s` | `1.970x` | `98.5%` | `99.5%` |
| `process_2_shm` | 2 | `6.215s` | `0.104s` | `1.949x` | `97.4%` | `99.5%` |
| `thread_4_reduce` | 4 | `3.236s` | `0.016s` | `3.743x` | `93.6%` | `99.7%` |
| `process_4_shm` | 4 | `3.222s` | `0.014s` | `3.759x` | `94.0%` | `99.5%` |
| `hybrid_2x2_shm` | 4 | `3.222s` | `0.031s` | `3.758x` | `94.0%` | `99.5%` |
| `pipeline_4_final` | 4 | `3.275s` | `0.137s` | `3.698x` | `92.5%` | `99.8%` |
| `thread_8_reduce` | 8 | `2.390s` | `0.045s` | `5.068x` | `63.3%` | `95.7%` |
| `pipeline_8_final` | 8 | `2.177s` | `0.058s` | `5.564x` | `69.6%` | `99.7%` |

모든 14개 scaling case는 5회 반복에서 `valid_all=1`, `checksum_count=1`을 만족했다.

### 9.2 Strong Scaling 해석

1. 2-worker thread는 `1.970x` speedup과 `98.5%` efficiency로 거의 이상적이다.
2. 4-worker 주요 구조는 `92.5~94.0%` efficiency를 유지한다.
3. Process 4 shm과 hybrid 2x2는 thread 4 reduce와 거의 동일한 성능 범위에 있다.
4. Process-friendly한 coarse-grained 계산과 결과 1회 전달이 fork/IPC 비용을 충분히 상쇄했다.
5. 8-worker는 실행시간을 더 줄이지만 efficiency가 크게 감소한다.
6. CPU utilization이 높아도 speedup이 `T1/N`에 가깝지 않을 수 있다.

### 9.3 Core별 CPU Utilization 및 Memory

| Case | CPU% | Util/Worker | Core별 평균 | Min Core | Stdev | Max RSS |
| --- | ---: | ---: | --- | ---: | ---: | ---: |
| `thread_4_reduce` | `398%` | `99.5%` | `100.0|100.0|100.0|100.0` | `100.0%` | `0.00` | `1,292 KB` |
| `process_4_shm` | `398%` | `99.5%` | `100.0|100.0|100.0|100.0` | `100.0%` | `0.00` | `1,212 KB` |
| `hybrid_2x2_shm` | `397%` | `99.2%` | `100.0|99.7|100.0|100.0` | `99.7%` | `0.16` | `1,220 KB` |
| `pipeline_4_final` | `399%` | `99.8%` | `100.0|100.0|100.0|100.0` | `100.0%` | `0.00` | `1,472 KB` |

4-worker 대표 구조는 모든 대상 core를 거의 균일하게 활용했다. Max RSS는 GNU time이 실행 command에서 관측한 값이며 child process 전체의 순간별 메모리 합산값은 아니므로 보조 지표로 사용한다.

### 9.4 Process-Friendly와 Thread-Friendly 교차 비교

| Profile / Case | Workers | Avg Time | Speedup | Efficiency |
| --- | ---: | ---: | ---: | ---: |
| process-friendly seq | 1 | `0.8620s` | `1.000x` | `100.0%` |
| process-friendly process 4 shm | 4 | `0.2404s` | `3.586x` | `89.6%` |
| process-friendly thread 4 reduce | 4 | `0.2412s` | `3.573x` | `89.3%` |
| process-friendly hybrid 2x2 | 4 | `0.2421s` | `3.561x` | `89.0%` |
| thread-friendly seq | 1 | `0.0338s` | `1.000x` | `100.0%` |
| thread-friendly thread 4 reduce | 4 | `0.0098s` | `3.431x` | `85.8%` |
| thread-friendly process 4 shm | 4 | `0.0104s` | `3.236x` | `80.9%` |

Process-friendly 조건에서는 큰 독립 작업이 fork와 IPC 비용을 상대적으로 작게 만들어 세 구조의 차이가 작아졌다. Thread-friendly 조건에서는 thread 4 reduce가 process 4 shm보다 약 `5.7%` 빨랐다. 특정 구조가 항상 빠른 것이 아니라 workload granularity와 communication 비용에 따라 결과가 달라진다.

### 9.5 Final Reduce와 Interactive Merge

| 방식 | 장점 | 비용 | 적합한 상황 |
| --- | --- | --- | --- |
| Final Reduce | 구조 단순, 동기화 비용 작음 | 실행 중 중간 결과 없음 | 최종 결과만 필요한 batch 작업 |
| Interactive Merge | 실행 중 partial result 집계 | queue, lock, condition variable, aggregator 비용 | 지속 집계와 monitoring이 필요한 작업 |

Interactive merge가 작은 workload에서 느린 것은 실패가 아니다. 중간 결과 집계 기능을 얻기 위해 추가 synchronization 비용을 지불하는 구조적 trade-off이다.

---

## 10. 최종 시스템 선택

### 10.1 기본 권장 구조

현재 측정 환경과 실제 Monte Carlo workload에서는 다음 구성이 가장 현실적이다.

```text
Workers: 4
Thread: local reduce
Process: child-local compute + shared memory result slot
Hybrid: 2 processes x 2 threads + child-local reduce
Pipeline: final merge, queue가 필요한 workload에서 선택
```

### 10.2 구조별 선택 기준

| 요구사항 | 적합한 구조 |
| --- | --- |
| 낮은 생성 비용과 단순 CPU 병렬화 | Thread + local reduce |
| 주소 공간 격리와 독립 작업 | Process + result 1회 전달 |
| Process 격리와 내부 thread 병렬화 결합 | Hybrid |
| 동적 작업 분배 또는 skewed workload | Pipeline queue |
| 실행 중 partial result 집계 | Interactive merge |

---

## 11. 한계

1. 핵심 결과는 Docker Desktop Ubuntu VM에서 측정했으므로 native Linux와 절대 시간이 다를 수 있다.
2. Scaling case 실행 순서가 고정되어 thermal, cache, background load 영향이 남을 수 있다.
3. Core별 `mpstat` 결과는 대표 구조별 단일 장시간 실행이 아니라 약 3초 실행 구간을 관측한 값이다.
4. Process와 hybrid의 child 내부 lock wait, reduce, IPC write 시간을 parent CSV에 완전히 합산하지 않는다.
5. Max RSS는 전체 process tree의 동시 메모리 사용량 합계가 아니다.
6. Pipeline은 중앙 queue를 사용하므로 worker 수가 커지면 contention이 증가할 수 있다.
7. 8-worker efficiency 감소 원인을 hardware counter 수준에서 분리하지는 않았다.

### 추가 발전 방향

- Native Linux에서 동일 조건 5~10회 재측정
- Case 실행 순서 무작위화와 cooldown 적용
- `perf stat`을 이용한 context switch, cache miss, branch miss 측정
- Process/hybrid child metrics를 shared metrics table로 수집
- Worker별 deque와 work stealing 비교
- 더 긴 workload에서 core별 utilization 재측정

---

## 12. 결론

본 프로젝트는 실제 Monte Carlo 차량 추종 위험 계산을 child process, pthread, hybrid, pipeline 구조로 구현하고, 각 구조에서 발생하는 synchronization, IPC, scheduling, merge overhead를 정량적으로 비교하였다.

Shared result를 보호하지 않은 `nosync` 구조에서는 race condition으로 결과가 손상되었다. Mutex는 정확성을 회복했지만 trial 단위 lock contention 때문에 성능이 크게 감소하였다. Worker-local reduce는 shared write를 hot loop에서 제거하여 정확성을 유지하면서 높은 성능을 확보하였다.

Process mode는 fork와 IPC 비용이 있지만 child가 큰 독립 작업을 수행하고 결과를 마지막에 한 번만 전달하도록 구성하면 4-worker 조건에서 thread와 유사한 확장성을 보였다. Hybrid mode 역시 process별 큰 작업과 child 내부 pthread reduce를 결합하여 `94.0%` efficiency를 기록하였다. Pipeline mode는 queue overhead가 존재하지만 동적 scheduling과 interactive merge를 지원하며, 4-worker에서 `99.8%`의 worker당 CPU utilization을 기록하였다.

실제 Monte Carlo strong scaling 결과에서 4-worker 주요 구조는 `92.5~94.0%` efficiency, worker당 CPU utilization `99.2~99.8%`를 달성하였다. 이는 실제 프로젝트 workload 자체가 N개 core를 균일하게 활용하며 코어 수에 비례하는 성능개선에 상당히 근접했음을 보여준다.

동시에 8-worker에서는 높은 CPU utilization에도 efficiency가 감소하였다. 따라서 병렬 시스템의 성능은 CPU를 바쁘게 유지하는 것만으로 결정되지 않으며, synchronization, IPC, scheduling, merge 및 실행 환경을 함께 분석해야 한다.

---

## 13. 근거 파일 목록

### 코드

| 주제 | 파일 |
| --- | --- |
| Monte Carlo 계산 | `src/simulation.c` |
| Thread synchronization | `src/thread_mode.c` |
| Process와 IPC | `src/process_mode.c`, `src/ipc_pipe.c`, `src/ipc_shm.c` |
| Hybrid | `src/hybrid_mode.c` |
| Pipeline | `src/pipeline_mode.c` |
| Queue synchronization | `src/task_queue.c`, `src/merge_queue.c` |
| Metrics | `src/metrics.c` |
| 정확성 자동 검사 | `scripts/test_modes.sh` |

### 최신 결과

```text
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_summary.csv
results/csv/real_montecarlo_utilization_2026_06_06/real_utilization_summary.csv
results/csv/hybrid_sync_2026_06_06/hybrid_sync_summary.csv
results/csv/profile_compare_2026_06_06/profile_compare_summary.csv
```

---

## 14. 팀원별 작성 완료 조건

### 김태환

- [ ] Process mode의 parent-child 흐름 설명
- [ ] Pipe와 shared memory IPC 비교
- [ ] Child별 result slot이 lock 없이 안전한 이유 설명
- [ ] Hybrid의 process/thread 역할 분리 설명
- [ ] 최신 process/hybrid 수치와 본문 일치 확인

### 성도연

- [ ] Race condition과 mutex/reduce 해결 과정 설명
- [ ] TaskQueue와 MergeQueue의 mutex + condition variable 설명
- [ ] Semaphore를 핵심 구현에 사용하지 않은 이유 설명
- [ ] Final Reduce와 Interactive Merge 비교
- [ ] Hybrid synchronization 최신 결과 반영

### 유지원

- [ ] Monte Carlo simulation model 설명
- [ ] Strong scaling과 CPU utilization 실험 조건 정리
- [ ] Speedup, efficiency, core utilization 그래프 생성
- [ ] Docker Desktop Linux VM 환경 한계 명시
- [ ] 모든 표의 최신 CSV 수치 확인

### 정종근

- [ ] Mode별 구조 비교표 형식 통일
- [ ] 실행 명령을 직접 실행해 오탈자 확인
- [ ] `valid`, `hist_sum`, checksum 표 수치 확인
- [ ] 한글/영문 OS 용어 통일
- [ ] 그림 번호, 표 번호, 파일 경로 최종 점검

---

## 15. 필요한 그림과 표 체크리스트

### 필수 그림

- [ ] 전체 시스템 흐름도
- [ ] Thread / Process / Hybrid 구조 비교도
- [ ] Pipeline producer-consumer 구조도
- [ ] Thread 1/2/4/8 speedup 및 efficiency 그래프
- [ ] Mode별 4-worker 실행시간 비교 그래프
- [ ] Core별 utilization 그래프
- [ ] Hybrid nosync/mutex/reduce 실행시간 및 valid 비교 그래프

### 필수 표

- [ ] 실행 환경
- [ ] Mode별 역할 및 OS 개념
- [ ] 문제 → 원인 → 해결 → 검증
- [ ] Strong scaling 결과
- [ ] CPU utilization 및 Max RSS
- [ ] Hybrid synchronization 결과
- [ ] Process-friendly / thread-friendly 교차 비교
- [ ] 팀원별 역할과 실제 기여도

---

## 16. AI 사용 기록 작성 템플릿

프로젝트 가이드에 따라 AI agent를 사용한 경우 출처와 실제 입력 prompt를 기록해야 한다. 아래 표에는 실제 사용 내역만 작성하며 내용을 임의로 만들지 않는다.

| 날짜 | 사용 도구 | 입력 목적 | 실제 입력 prompt 요약 | 결과 활용 범위 | 검증 방법 | 담당자 |
| --- | --- | --- | --- | --- | --- | --- |
| YYYY-MM-DD |  |  |  |  | 코드 실행/수치 대조/수동 검수 |  |

AI가 제안한 코드나 분석은 다음 방식으로 검증했다고 명시한다.

```text
make clean && make
make test
Docker Ubuntu make test
valid / hist_sum / checksum 비교
5회 반복 실험 및 평균·표준편차 확인
```

---

## 17. 최종 통합 검수표

- [ ] 문서의 모든 수치가 최신 4개 결과 디렉터리와 일치한다.
- [ ] 과거 10,000 trials 결과를 핵심 성능 근거로 사용하지 않는다.
- [ ] 모든 정상 성능 결과가 `valid=1`, 동일 checksum인지 확인한다.
- [ ] `nosync`를 성능 개선 성공 사례로 표현하지 않는다.
- [ ] Process가 항상 thread보다 빠르다고 단정하지 않는다.
- [ ] CPU utilization과 scaling efficiency를 같은 의미로 설명하지 않는다.
- [ ] `T_sync`를 wall-clock stage처럼 단순 합산하지 않는다.
- [ ] Docker Desktop Ubuntu VM 환경임을 명시한다.
- [ ] 역할과 기여도는 실제 수행 내역과 일치시킨다.
- [ ] AI 사용 내역에는 실제 prompt와 검증 방법을 기록한다.
- [ ] 그림, 표, 코드 캡처마다 본문에서 해석을 제공한다.
- [ ] 최종 결론이 문제 발생, 원인 분석, 구조 개선, 실험 검증 흐름을 포함한다.

---

## 18. 권장 분량 배분

문서 분량은 특정 구현 설명에 치우치지 않도록 아래 비율을 권장한다.

| 영역 | 권장 비율 | 핵심 내용 |
| --- | ---: | --- |
| 시스템 개요와 목표 | 10% | Monte Carlo 모델, OS 프로젝트 목표 |
| 프로그램 구성과 실행 구조 | 20% | seq/thread/process/hybrid/pipeline |
| 문제 발생과 해결 과정 | 30% | race, mutex contention, IPC, queue, affinity |
| 실험 설계와 결과 분석 | 30% | scaling, utilization, sync, profile 비교 |
| 결론과 한계 | 10% | 최종 선택, 현실적 한계, 발전 방향 |

가장 중요한 영역은 문제 해결 과정과 실험 분석이다. 코드 기능 목록만 길게 설명하고 원인과 검증이 짧아지지 않도록 한다.

---

## 19. 팀원별 기여도 확정 템플릿

기여도는 이름별 동일 배분을 먼저 정하는 방식보다, 실제 작업 단위를 합의한 뒤 결정하는 것이 안전하다.

| 작업 항목 | 김태환 | 성도연 | 유지원 | 정종근 | 확인 근거 |
| --- | ---: | ---: | ---: | ---: | --- |
| Process / IPC / Hybrid 구현 및 분석 |  |  |  |  | commit, 코드, 개인 산출물 |
| Synchronization / Queue / Pipeline 구현 및 분석 |  |  |  |  | commit, 코드, 개인 산출물 |
| Simulation / Metrics / 실험 환경 / 데이터 분석 |  |  |  |  | script, CSV, 그래프 |
| 실행 검증 / 비교표 / 용어 및 형식 검수 |  |  |  |  | 실행 기록, 검수 내역 |
| 최종 문서 작성 및 통합 |  |  |  |  | 작성 문단, 수정 이력 |
| 합계 |  |  |  |  | 전체 합계 100% |

### 기여도 회의 방법

1. 각자 실제 수행한 작업을 코드, 문서, 실험 파일 기준으로 설명한다.
2. 공동 작업은 기여 내용을 구체적으로 나눈다.
3. 단순 참석 여부보다 구현, 검증, 분석, 문서화 결과물을 기준으로 평가한다.
4. 모든 팀원이 합의한 뒤 최종 비율을 기록한다.

---

## 20. 수행일지 템플릿

| 날짜 | 참석자 | 진행 내용 | 발견한 문제 | 결정 및 해결 방향 | 담당자 | 완료 근거 |
| --- | --- | --- | --- | --- | --- | --- |
| YYYY-MM-DD |  |  |  |  |  | commit / CSV / 문서 경로 |

수행일지는 단순히 “회의 진행”이라고 쓰지 않고 다음처럼 구체적으로 기록한다.

```text
문제:
hybrid mode가 항상 local reduce만 사용하여 synchronization 사용/미사용 비교가 불가능했다.

원인:
child 내부 pthread가 local result만 갱신하도록 고정되어 있었다.

해결:
hybrid child 내부에서 nosync, mutex, reduce를 선택할 수 있도록 구조를 확장하였다.

검증:
Docker Ubuntu에서 1,000,000 trials, 5회 반복 실행.
nosync는 valid=0, mutex/reduce는 valid=1 및 동일 checksum 확인.
```

---

## 21. 팀 회의에서 바로 결정할 항목

| 결정 항목 | 선택 기준 | 회의 결과 |
| --- | --- | --- |
| 최종 문서 편집 담당자 | 전체 문체와 표 형식을 통일할 수 있는 사람 |  |
| 최종 그림 제작 담당자 | 최신 CSV 기반 그래프 생성 가능 여부 |  |
| 코드 캡처 범위 | 문제 해결을 직접 보여주는 코드만 선택 |  |
| 개인 기여도 | 실제 결과물과 수행일지 기준 |  |
| AI 사용 기록 담당자 | 실제 prompt와 검증 기록을 취합할 사람 |  |
| 최종 실행 검증 담당자 | Docker/Linux에서 처음부터 실행 가능한 사람 |  |

### 회의 종료 조건

- [ ] 각 본문 단락 담당자가 정해졌다.
- [ ] 각 담당자의 완료 기한이 정해졌다.
- [ ] 최신 결과 외 과거 수치를 사용하지 않기로 합의했다.
- [ ] 그림과 표 제작 담당자가 정해졌다.
- [ ] 개인 기여도 확정 방식을 합의했다.
- [ ] 최종 통합 및 교차 검수 일정이 정해졌다.
