# Monte Carlo 기반 차량 추종 위험 시뮬레이션

차량 추종 상황을 CPU-bound Monte Carlo workload로 모델링하고, 동일한 계산을 여러 운영체제 실행 구조로 처리하며 정확성과 성능 차이를 측정하는 프로젝트입니다.

핵심 구현:

`pthread` · `fork()` · `waitpid()` · `pipe` · `mmap shared memory` · `mutex` · `condition variable` · `task queue` · `local reduce` · `interactive merge` · `CPU affinity`

---

## 프로젝트 개요

이 프로젝트의 차량 모델은 병렬처리 구조를 비교하기 위한 계산 workload입니다. 핵심 질문은 다음과 같습니다.

```text
같은 Monte Carlo 계산을

1. 하나의 실행 흐름으로 처리할 때
2. 여러 pthread로 나눌 때
3. 여러 child process로 나눌 때
4. child process 내부에서 pthread를 다시 사용할 때
5. task queue와 pipeline으로 처리할 때

정확성, 실행시간, CPU 활용률, synchronization 비용,
IPC 비용과 확장성이 어떻게 달라지는가?
```

지원 실행 모드:

| Mode | 실행 구조 | 주요 비교 대상 |
| --- | --- | --- |
| `seq` | 단일 process, 단일 실행 흐름 | 기준 실행시간 |
| `thread` | 하나의 process 내부 pthread | thread scaling, synchronization |
| `process` | 여러 child process | fork 및 IPC 비용 |
| `hybrid` | 여러 child process 내부 pthread | 계층적 병렬화 |
| `pipeline` | task queue + worker pool | 동적 scheduling, queue overhead |

CPU 활용률과 strong scaling은 별도 인공 loop가 아니라 실제 Monte Carlo workload로 검증합니다. 실제 계산에서 4-worker efficiency `92.5~94.0%`, worker당 CPU 활용률 `99.2~99.8%`를 확보했기 때문에 별도의 `ideal` mode는 제거했습니다.

### 운영체제 핵심 요구사항 대응

| 검증 항목 | 프로젝트 구현 | 최신 검증 근거 |
| --- | --- | --- |
| Sequential baseline | `seq` | 고정 workload 기준시간 측정 |
| Child process 병렬처리 | `fork()`, `waitpid()`, child-local result | process 1/2/4 scaling |
| Multithread 병렬처리 | POSIX pthread, static partition | thread 1/2/4/8 scaling |
| Process + thread 조합 | child process 내부 pthread | hybrid 2x2, 2x4 |
| Synchronization 문제 | `nosync`, `mutex`, `local reduce` | race 재현 및 정확성/성능 비교 |
| IPC 비교 | pipe, anonymous `mmap` shared memory | child별 결과 1회 전달 |
| Producer-consumer | bounded queue + mutex + condition variable | pipeline final/interactive |
| Core 활용률 | Linux affinity, GNU time, `mpstat -P ALL` | 대상 core 최소 `99.7%` |
| Strong scaling | 고정된 실제 Monte Carlo workload | 4-worker efficiency `92.5~94.0%` |
| 정확성 검증 | `valid`, `hist_sum`, deterministic checksum | 정상 mode 전체 checksum 일치 |

### 문제 해결 흐름

| 발견한 문제 | 원인 | 적용한 개선 | 검증 결과 |
| --- | --- | --- | --- |
| shared result를 보호하지 않으면 결과 손상 | data race와 lost update | mutex 및 worker-local reduce 구현 | `nosync valid=0`, mutex/reduce `valid=1` |
| trial마다 mutex를 잡으면 느림 | hot loop lock contention | local result를 join 이후 병합 | hybrid reduce가 mutex보다 약 1.95배 빠름 |
| process 결과 전달 비용 | fork와 IPC, 잦은 전달 가능성 | child-local 계산 후 마지막에 결과 1회 전달 | process 4 shm efficiency `94.0%` |
| hybrid worker affinity 중복 | child별 thread ID가 다시 0부터 시작 | 전역 worker ID로 core mapping | 4-worker Util/Core `99.2%` |
| 단순 static partition만으로 scheduling 비교가 약함 | 균등 trial은 load imbalance가 작음 | skewed workload와 queue pipeline 추가 | static/queue 비교 가능 |
| final reduce만으로 중간 집계 불가 | worker 종료 전 결과가 보이지 않음 | merge queue와 aggregator thread 추가 | interactive 기능과 overhead 비교 가능 |

---

## 현재 상태

### 완료된 기능

- `seq`, `thread`, `process`, `hybrid`, `pipeline` 실행
- thread synchronization 방식 비교
  - `nosync`
  - `mutex`
  - `reduce`
- process IPC 방식 비교
  - pipe
  - anonymous shared memory
- task queue와 merge queue
- mutex + condition variable 기반 producer-consumer 동기화
- final reduce와 interactive merge
- uniform 및 skewed workload
- Linux CPU affinity
- stage metrics와 queue/IPC counters
- checksum 및 histogram 기반 정확성 검증
- `make test`에서 모든 정상 mode의 `valid=1`과 동일 checksum 자동 검사
- 실제 Monte Carlo strong scaling 자동 측정
- `/usr/bin/time -v` 및 `mpstat` 기반 CPU 활용률 측정

### 냉정한 현재 평가

| 항목 | 평가 |
| --- | --- |
| 정확성 | 강함. 정상 mode는 반복 측정에서 모두 동일 checksum과 `valid=1` 유지 |
| 2-worker scaling | 매우 좋음. thread 기준 efficiency `98.5%` |
| 4-worker scaling | 좋음. 주요 구조 efficiency `92.5~94.0%` |
| 4-core utilization | 매우 좋음. 대표 구조의 worker당 평균 CPU 사용률 `99.2~99.8%` |
| core 사용 균일도 | 좋음. affinity 대상 core 최소 활용률 `99.7%`, 최대 표준편차 `0.16` |
| 8-worker scaling | 제한적. CPU는 계속 사용하지만 efficiency가 `62.3~69.6%`로 감소 |
| process/hybrid 구조 | 4-worker 조건에서는 안정적이나 구조 복잡도는 thread보다 큼 |
| synchronization 비교 | hybrid에서도 `nosync/mutex/reduce` 비교 가능. reduce가 정확성과 성능을 함께 확보 |
| profile 교차 비교 | process/thread 양쪽에서 동일 profile을 실행해 구조 차이를 비교 |
| 측정 환경 | Docker Desktop Linux VM 결과이므로 native Linux 절대 성능으로 일반화할 수 없음 |
| 반복 측정 설계 | 5회 반복은 완료했지만 case 실행 순서가 고정되어 thermal/cache 순서 효과가 남을 수 있음 |
| `mpstat` 근거 | core별 활용률은 강하지만 대표 구조별 단일 실행이며 약 3초 구간을 관측한 결과 |

현실적인 결론은 다음과 같습니다.

- 실제 Monte Carlo 구조는 **4-worker까지 이상적인 시간 감소에 상당히 근접**합니다.
- 8-worker에서도 core는 거의 모두 사용하지만, 실행시간은 `T1/8`에 가깝게 줄지 않습니다.
- CPU utilization이 높다는 사실만으로 scaling이 이상적이라는 뜻은 아닙니다.
- 8-worker 효율 저하는 VM scheduler, logical core 특성, queue 관리, process/thread 관리 비용이 유용한 계산 비율을 낮추기 때문입니다.
- 현재 환경의 실용적인 worker 수는 대체로 4개입니다.

---

## 빠른 실행

### Docker Ubuntu

```sh
docker build -t os-montecarlo-risk .
docker run --rm os-montecarlo-risk sh -lc 'make test && \
  ./sim --mode process --processes 4 --ipc shm --trials 10000 --steps 30 && \
  ./sim --mode hybrid --processes 2 --threads 2 --ipc shm --trials 10000 --steps 30'
```

정상 simulation row의 `valid` 값이 `1`이면 정확성 검증을 통과한 것입니다.
`make test`는 대표 정상 mode 8개를 실행하고 하나라도 invalid이거나 checksum이 다르면 실패합니다.

### Ubuntu 또는 WSL2

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat

make clean
make
make test
```

macOS에서도 기본 기능은 실행할 수 있지만, Linux CPU affinity와 GNU `/usr/bin/time -v`, `mpstat` 검증은 Docker, WSL2 또는 Linux에서 실행해야 합니다.

---

## 시스템 구조

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
   |
   +-- hist_sum validation
   +-- checksum validation
   +-- stage metrics
   +-- CSV output
```

### Monte Carlo trial

각 trial은 독립적인 seed를 사용합니다.

```text
trial index
  -> deterministic seed
  -> 차량 속도, 거리, 반응 시간, 감속력 생성
  -> time step별 위치와 속도 계산
  -> 최소 TTC 및 충돌 여부 계산
  -> risk histogram에 결과 1회 반영
```

trial index 기반 seed를 사용하므로 worker 수와 실행 구조가 달라져도 최종 결과를 동일하게 재현할 수 있습니다.

---

## 실행 모드

### Sequential

```text
Parent process
  -> 모든 trial 순차 실행
  -> Result 생성
```

병렬 실행의 speedup과 efficiency를 계산하기 위한 기준입니다.

```sh
./sim --mode seq --trials 1000000 --steps 50
```

### Thread

```text
Single process
  -> pthread worker 0
  -> pthread worker 1
  -> ...
  -> join
  -> local result merge
```

`reduce` 방식은 각 worker가 독립적인 local result를 계산한 후 join 이후 한 번만 병합합니다. Hot loop에서 shared write와 lock을 제거하기 때문에 현재 구현에서 가장 실용적인 thread 방식입니다.

```sh
./sim --mode thread --threads 4 --sync reduce \
  --trials 1000000 --steps 50
```

### Process

```text
Parent process
  -> fork child processes
  -> child별 독립 trial range 계산
  -> child별 local Result 생성
  -> pipe 또는 shared memory로 결과 1회 전달
  -> waitpid
  -> parent result merge
```

Child는 계산 중 parent의 result를 갱신하지 않습니다. Shared memory mode에서도 child마다 독립 slot을 사용하고 parent는 `waitpid()` 이후 읽습니다.

```sh
./sim --mode process --processes 4 --ipc shm \
  --trials 1000000 --steps 50
```

### Hybrid

```text
Parent process
  -> child process groups
       -> process 내부 pthread workers
       -> process-local reduce
       -> IPC result 전달
  -> parent merge
```

Hybrid worker의 affinity core는 다음 전역 worker ID로 계산합니다.

```text
global_worker_id = process_id * threads_per_process + thread_id
```

이 방식은 각 child 내부 thread가 동일한 core에 겹쳐 배치되는 문제를 방지합니다.

Child 내부 pthread도 `nosync`, `mutex`, `reduce`를 선택할 수 있습니다. `nosync`는 process 내부 shared address space에서 race를 재현하고, `mutex`는 정확하지만 trial별 lock contention을 발생시키며, `reduce`는 child 내부 thread-local result를 마지막에 병합합니다.

```sh
./sim --mode hybrid --processes 2 --threads 2 --ipc shm --sync reduce \
  --trials 1000000 --steps 50 --affinity on
```

### Pipeline

```text
Preprocessor
  -> TaskQueue
  -> Worker pool
  -> Final reduce

또는

Preprocessor
  -> TaskQueue
  -> Worker pool
  -> MergeQueue
  -> Aggregator
```

Pipeline은 batch 단위로 작업을 분배합니다. 균등 workload에서는 queue overhead가 추가될 수 있지만, 불균등 workload에서는 static partition보다 load balancing에 유리할 수 있습니다.

```sh
./sim --mode pipeline --threads 4 \
  --schedule queue --merge final \
  --batch-size 100000 --queue-size 1024 \
  --trials 1000000 --steps 50
```

---

## Synchronization 설계

### `nosync`

여러 thread가 shared result를 보호 없이 수정합니다.

- lock overhead 없음
- data race 발생
- `hist_sum != trials` 또는 checksum mismatch 가능
- 정확한 결과로 사용할 수 없음

### `mutex`

Shared result 갱신 전후에 mutex를 사용합니다.

- 정확성 유지
- trial마다 lock을 획득해 contention 증가
- synchronization 비용을 직접 확인하기 위한 비교 방식

### `reduce`

Worker마다 local result를 만들고 마지막에 병합합니다.

- hot loop에서 shared write 제거
- lock contention 최소화
- 정확성과 성능의 균형이 가장 좋음

### Mutex + Condition Variable Queue

Task queue와 merge queue는 다음 상태를 mutex로 보호합니다.

```text
head
tail
count
closed
```

Mutex만으로는 queue가 비었거나 가득 찼을 때 worker를 효율적으로 대기시킬 수 없습니다. 이를 보완하기 위해 condition variable을 사용합니다.

```text
not_empty: consumer 대기 및 wake-up
not_full: producer 대기 및 wake-up
```

---

## Final Reduce와 Interactive Merge

| 방식 | 구조 | 장점 | 비용 |
| --- | --- | --- | --- |
| final | worker 종료 후 한 번에 병합 | 단순하고 빠름 | 실행 중 중간 결과 없음 |
| interactive | partial result를 merge queue에 전달 | 실행 중 결과 집계 가능 | queue, lock, aggregator 비용 |

Interactive merge가 final reduce보다 느린 것은 구현 실패가 아닙니다. 중간 결과를 계속 집계하는 기능을 얻는 대신 synchronization과 context switching 비용을 지불하는 구조입니다.

---

## IPC 설계

### Pipe

```text
Child local Result
  -> write()
  -> pipe
  -> parent read()
  -> merge
```

메시지 전달 구조가 명확하지만 result 전달 시 read/write 비용이 발생합니다.

### Shared Memory

```text
Shared Result Table
  slot 0: child 0 전용
  slot 1: child 1 전용
  ...
```

각 child는 자기 slot만 쓰고 parent는 child 종료 이후 읽습니다. 이 구조에서는 결과 전달 과정에 별도 lock이나 busy waiting이 필요하지 않습니다.

---

## Scheduling과 Workload

### Uniform

모든 trial의 계산량이 거의 동일합니다.

```sh
--workload uniform
```

이 조건에서는 static partition이 단순하고 효율적입니다.

### Skewed

일부 trial에 추가 CPU work를 부여합니다.

```sh
--workload skewed --skew-factor 8
```

결과 반영은 trial당 한 번만 수행하므로 정확성 검증은 유지됩니다. Skewed workload는 static partition의 load imbalance와 queue scheduling 효과를 확인하기 위한 진단 조건입니다.

### Workload Profiles

```sh
--profile default
--profile process_friendly
--profile thread_friendly
```

| Profile | 동작 |
| --- | --- |
| `default` | 실제 Monte Carlo 기본 계산 |
| `process_friendly` | 큰 독립 작업과 결과 1회 전달을 강조 |
| `thread_friendly` | thread-local reduce의 낮은 overhead를 강조 |

Profile은 실행 구조별 특성을 비교하기 위한 진단 옵션입니다. 최종 scaling 결과는 `default` profile과 `inner_work=0` 조건을 사용합니다.

같은 profile을 process와 thread 양쪽에서 실행하는 이유는 특정 구조가 항상 빠르다고 주장하기 위해서가 아닙니다. 큰 독립 작업에서는 fork/IPC 비용이 상대적으로 작아지고, 작은 작업에서는 shared address space와 thread-local reduce의 낮은 비용이 더 중요해지는지 확인하기 위한 조건입니다.

---

## Metrics

상세 CSV 출력은 다음 지표를 포함합니다.

| Metric | 의미 |
| --- | --- |
| `time_total` | end-to-end wall-clock 시간 |
| `time_pre` | batch 생성 및 준비 |
| `time_compute` | parallel worker 실행 구간 |
| `time_sync` | lock 및 condition wait 누적값 |
| `time_ipc` | parent에서 관측한 IPC 시간 |
| `time_merge` | partial result 병합 시간 |
| `time_post` | validation 및 checksum 계산 |
| `lock_wait_count` | lock 관측 횟수 |
| `cond_wait_count` | condition wait 횟수 |
| `ipc_bytes` | 전달한 IPC byte 수 |
| GNU time Max RSS | 실행 command에서 관측한 최대 resident memory |
| `valid` | 결과 정확성 검증 |
| `checksum` | 실행 구조 간 결과 일치 검증 |

`time_sync`는 여러 worker의 대기 시간을 누적한 값입니다. 따라서 모든 stage 값을 더해도 `time_total`과 일치하지 않을 수 있습니다. `time_sync`는 wall-clock 구간이 아니라 synchronization 부담을 비교하는 보조 지표입니다.

---

## 최종 실제 Monte Carlo 결과

측정 조건:

| 항목 | 값 |
| --- | --- |
| 환경 | Docker Desktop Ubuntu 22.04 Linux VM |
| Architecture | `aarch64` |
| 할당 CPU | 10 |
| Trials | `120,000,000` |
| Steps | `50` |
| Profile | `default` |
| Pre/Post work | `0 / 0` |
| 반복 횟수 | 각 case 5회 |
| Affinity | 활성화 |

### 실험 검증 체계

성능 수치는 한 번 실행한 최솟값이 아니라 다음 검증 단계를 모두 통과한 결과만 사용합니다.

| 검증 단계 | 확인 내용 | 통과 기준 | 최신 결과 |
| --- | --- | --- | --- |
| 자동 기능 검증 | 대표 정상 mode 8개 실행 | 모두 `valid=1`, 동일 checksum | `make test` 통과 |
| 반복 정확성 검증 | Scaling 14개 case를 각 5회 실행 | `valid_all=1`, `checksum_count=1` | 전체 통과 |
| Strong scaling 검증 | 동일한 총 Monte Carlo workload에서 worker 수만 변경 | `T1/N`, speedup, efficiency 비교 | 4-worker efficiency `92.5~94.0%` |
| CPU saturation 검증 | GNU time으로 전체 CPU% 측정 | worker당 CPU가 100%에 근접 | `99.2~99.8%` |
| Core 균일도 검증 | `mpstat -P ALL 1`로 affinity 대상 core 측정 | core별 사용률 편차가 작아야 함 | 최소 `99.7%`, 최대 표준편차 `0.16` |
| Race condition 재현 | Hybrid/thread `nosync` 반복 실행 | invalid 및 checksum 불일치 발생 | 5회 모두 invalid |
| 해결 구조 검증 | 동일 조건에서 mutex/reduce 실행 | 정확성 회복 및 overhead 비교 | 둘 다 valid, reduce가 mutex보다 약 `1.95x` 빠름 |
| Workload 교차 검증 | 동일 profile을 process/thread/hybrid에서 실행 | 모든 정상 case checksum 일치 | 17개 case 모두 통과 |

자동 기능 검증은 실행 성공만 확인하지 않습니다. [scripts/test_modes.sh](scripts/test_modes.sh)가 sequential checksum을 기준으로 thread, process pipe/shm, hybrid mutex/reduce, pipeline final/interactive의 `valid`와 checksum을 비교하며, 하나라도 다르면 `make test`가 실패합니다.

최신 결과 묶음의 역할:

| 결과 디렉터리 | 검증 목적 |
| --- | --- |
| `real_montecarlo_scaling_2026_06_06_5repeat` | 실제 Monte Carlo의 1/2/4/8-worker strong scaling |
| `real_montecarlo_utilization_2026_06_06` | CPU%, core별 균일도, Max RSS |
| `hybrid_sync_2026_06_06` | Hybrid 내부 race condition과 mutex/reduce 해결 효과 |
| `profile_compare_2026_06_06` | Process-friendly/thread-friendly 동일 workload 교차 비교 |

### Strong Scaling

| Case | Workers | Avg time | Stdev | Speedup | Efficiency | CPU% | Util/Worker |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `seq` | 1 | `12.111s` | `0.366s` | `1.000x` | `100.0%` | `99.0%` | `99.0%` |
| `thread_2_reduce` | 2 | `6.148s` | `0.173s` | `1.970x` | `98.5%` | `199.0%` | `99.5%` |
| `thread_4_reduce` | 4 | `3.236s` | `0.016s` | `3.743x` | `93.6%` | `398.8%` | `99.7%` |
| `process_4_shm` | 4 | `3.222s` | `0.014s` | `3.759x` | `94.0%` | `398.2%` | `99.5%` |
| `hybrid_2x2_shm` | 4 | `3.222s` | `0.031s` | `3.758x` | `94.0%` | `398.2%` | `99.5%` |
| `pipeline_4_final` | 4 | `3.275s` | `0.137s` | `3.698x` | `92.5%` | `399.0%` | `99.8%` |
| `thread_8_reduce` | 8 | `2.390s` | `0.045s` | `5.068x` | `63.3%` | `765.8%` | `95.7%` |
| `pipeline_8_final` | 8 | `2.177s` | `0.058s` | `5.564x` | `69.6%` | `797.6%` | `99.7%` |

모든 14개 scaling case는 5회 반복에서 다음 조건을 만족했습니다.

```text
valid_all = 1
checksum_count = 1
checksum = 10938743413492098894
hist_sum = trials = 120000000
```

### 이상적 시간 대비 실제 손실

`ideal time`은 sequential 평균시간을 worker 수로 나눈 `T1/N`입니다. 실제 실행시간과의 차이는 worker 생성, scheduling, synchronization, queue, IPC, merge 및 VM 실행 환경의 영향을 포함합니다.

| Case | Ideal time | Actual avg | Ideal 대비 증가 | 해석 |
| --- | ---: | ---: | ---: | --- |
| `thread_4_reduce` | `3.028s` | `3.236s` | `6.9%` | Local reduce로 lock 비용을 줄여 이상적 시간에 근접 |
| `process_4_shm` | `3.028s` | `3.222s` | `6.4%` | Fork/IPC가 있지만 큰 독립 작업과 결과 1회 전달로 상쇄 |
| `hybrid_2x2_shm` | `3.028s` | `3.222s` | `6.4%` | Process와 thread 계층 비용이 4-worker에서는 제한적 |
| `pipeline_4_final` | `3.028s` | `3.275s` | `8.2%` | Queue scheduling 비용으로 다른 4-worker 구조보다 손실 증가 |
| `thread_8_reduce` | `1.514s` | `2.390s` | `57.9%` | 추가 worker 대비 scheduler와 실행 환경 overhead가 커짐 |
| `pipeline_8_final` | `1.514s` | `2.177s` | `43.8%` | CPU는 포화됐지만 queue 및 worker 관리 비용으로 선형 scaling 실패 |

CPU utilization과 scaling efficiency는 서로 다른 지표입니다. `pipeline_8_final`은 worker당 CPU `99.7%`를 기록했지만 efficiency는 `69.6%`입니다. 즉, 모든 core가 바쁘더라도 그 시간이 모두 유용한 Monte Carlo 계산시간 감소로 연결되지는 않습니다.

결과 파일:

```text
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_raw.csv
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_summary.csv
```

### Core별 Utilization

| Case | CPU% | Util/Worker | Core 0~3 평균 | Min | Stdev | Max RSS |
| --- | ---: | ---: | --- | ---: | ---: | ---: |
| `thread_4_reduce` | `398%` | `99.5%` | `100.0|100.0|100.0|100.0` | `100.0%` | `0.00` | `1,292 KB` |
| `process_4_shm` | `398%` | `99.5%` | `100.0|100.0|100.0|100.0` | `100.0%` | `0.00` | `1,212 KB` |
| `hybrid_2x2_shm` | `397%` | `99.2%` | `100.0|99.7|100.0|100.0` | `99.7%` | `0.16` | `1,220 KB` |
| `pipeline_4_final` | `399%` | `99.8%` | `100.0|100.0|100.0|100.0` | `100.0%` | `0.00` | `1,472 KB` |

Max RSS는 GNU `/usr/bin/time -v`가 실행 command에 대해 기록한 값입니다. 여러 child process의 순간별 RSS를 모두 합산한 값은 아니므로 메모리의 절대 우열보다, 동일 환경에서 구조별 대략적인 footprint를 비교하는 보조 지표로 사용합니다.

결과 파일:

```text
results/csv/real_montecarlo_utilization_2026_06_06/real_utilization_summary.csv
results/csv/real_montecarlo_utilization_2026_06_06/mpstat_*.txt
results/csv/real_montecarlo_utilization_2026_06_06/time_*.txt
```

### 결과 해석

1. **4-worker까지는 scaling이 강합니다.**

   주요 구조가 `92.5~94.0%` efficiency를 기록했습니다.

2. **Process와 hybrid도 실제 workload에서 충분히 경쟁력이 있습니다.**

   Shared memory 결과 전달과 coarse-grained 계산으로 `process_4_shm`, `hybrid_2x2_shm` 모두 `94.0%` efficiency를 기록했습니다.

3. **Pipeline은 queue 비용이 있어도 core를 충분히 활용합니다.**

   `pipeline_4_final`은 worker당 CPU utilization `99.8%`를 기록했습니다.

4. **8-worker는 CPU saturation과 scaling이 분리됩니다.**

   `pipeline_8_final`은 CPU `797.6%`를 사용하지만 efficiency는 `69.6%`입니다. Core를 바쁘게 유지하는 것과 실행시간이 이상적으로 감소하는 것은 같은 의미가 아닙니다.

5. **현재 측정 환경의 실용적인 worker 수는 4개입니다.**

   8-worker는 실행시간을 더 줄이지만 추가 worker 대비 효율이 크게 감소합니다.

6. **Docker Desktop 결과에는 환경 한계가 있습니다.**

   Linux VM 내부 상대 비교에는 유효하지만, 결과를 다른 CPU나 native Linux의 절대 성능으로 일반화할 수 없습니다.

7. **현재 수치만으로 모든 환경에서 같은 순위를 보장할 수 없습니다.**

   `process_4_shm`, `hybrid_2x2_shm`, `thread_4_reduce`의 평균시간 차이는 수십 ms 수준입니다. 다른 CPU, native Linux, Docker CPU 할당, background load에 따라 순위가 바뀔 수 있습니다.

### 결과 신뢰도와 주장 범위

| 주장 가능한 내용 | 근거 |
| --- | --- |
| 정상 실행 구조는 동일한 계산 결과를 만든다 | 5회 반복 `valid_all=1`, 단일 checksum |
| 실제 Monte Carlo 구조가 4개 core를 균일하게 사용한다 | worker당 CPU `99.2~99.8%`, core 최소 `99.7%` |
| 4-worker까지 코어 수 비례 성능에 상당히 근접한다 | speedup `3.698~3.759x`, efficiency `92.5~94.0%` |
| Local reduce가 mutex보다 적합하다 | 동일 정확성에서 hybrid reduce가 약 `1.95x` 빠름 |
| Process와 thread의 유불리는 workload에 따라 달라진다 | Process-friendly/thread-friendly 교차 비교 |

| 현재 결과만으로 단정하면 안 되는 내용 | 이유 |
| --- | --- |
| Process가 모든 환경에서 thread보다 빠르다 | 4-worker 평균 차이가 작고 환경에 따라 순위가 바뀔 수 있음 |
| 8-worker가 효율적인 기본값이다 | 실행시간은 감소하지만 efficiency가 크게 하락 |
| Docker Desktop 결과가 native Linux 절대 성능과 같다 | Linux VM scheduler와 CPU 할당의 영향을 받음 |
| Max RSS가 전체 process tree 메모리 합계다 | GNU time이 관측한 command 기준 최대 RSS임 |
| `T_sync + T_compute + ... = T_total`이다 | `T_sync`는 여러 worker 대기시간의 누적 지표일 수 있음 |

### Hybrid Synchronization 비교

측정 조건은 Docker Desktop Ubuntu 22.04 VM, `trials=1,000,000`, `steps=50`, `2 processes x 2 threads`, shared memory IPC, 각 5회입니다.

| Case | Avg time | Speedup | Efficiency | Valid | Checksum 상태 |
| --- | ---: | ---: | ---: | ---: | --- |
| `hybrid_2x2_nosync_shm` | `0.0312s` | `3.367x` | `84.2%` | `0` | 5회 모두 서로 다름 |
| `hybrid_2x2_mutex_shm` | `0.0563s` | `1.866x` | `46.7%` | `1` | seq와 일치 |
| `hybrid_2x2_reduce_shm` | `0.0288s` | `3.643x` | `91.1%` | `1` | seq와 일치 |

`nosync`는 빠르게 보이지만 trial 수와 histogram update를 잃으므로 사용할 수 없습니다. `mutex`는 정확하지만 child 내부 shared result를 trial마다 잠가 contention이 큽니다. `reduce`는 정확성을 유지하면서 mutex보다 약 `1.95x` 빠르게 측정되어 현재 hybrid의 기본 전략으로 적합합니다.

결과 파일:

```text
results/csv/hybrid_sync_2026_06_06/hybrid_sync_raw.csv
results/csv/hybrid_sync_2026_06_06/hybrid_sync_summary.csv
```

### Workload Profile 교차 비교

측정 조건은 Docker Desktop Ubuntu 22.04 VM, `trials=100,000`, `steps=50`, 각 5회입니다. Profile은 결과를 바꾸지 않는 dummy integer work만 추가하며 모든 정상 case가 `valid=1`, 동일 checksum을 유지했습니다.

| Profile / Case | Workers | Avg time | Speedup | Efficiency |
| --- | ---: | ---: | ---: | ---: |
| process-friendly seq | 1 | `0.8620s` | `1.000x` | `100.0%` |
| process-friendly process 4 shm | 4 | `0.2404s` | `3.586x` | `89.6%` |
| process-friendly thread 4 reduce | 4 | `0.2412s` | `3.573x` | `89.3%` |
| process-friendly hybrid 2x2 | 4 | `0.2421s` | `3.561x` | `89.0%` |
| thread-friendly seq | 1 | `0.0338s` | `1.000x` | `100.0%` |
| thread-friendly thread 4 reduce | 4 | `0.0098s` | `3.431x` | `85.8%` |
| thread-friendly process 4 shm | 4 | `0.0104s` | `3.236x` | `80.9%` |

Process-friendly 조건에서는 작업이 충분히 커져 process, thread, hybrid의 차이가 작아졌고 process 4 shm이 근소하게 가장 빨랐습니다. Thread-friendly 조건에서는 thread 4 reduce가 process 4 shm보다 약 `5.7%` 빨랐습니다. 이 결과는 process나 thread 중 하나가 항상 우월한 것이 아니라, 작업 크기와 IPC/동기화 비율에 따라 선택이 달라져야 함을 보여줍니다.

결과 파일:

```text
results/csv/profile_compare_2026_06_06/profile_compare_raw.csv
results/csv/profile_compare_2026_06_06/profile_compare_summary.csv
```

---

## 실험 재현

### 실제 Monte Carlo 5회 Scaling

```sh
TRIALS=120000000 STEPS=50 REPEATS=5 \
scripts/run_real_scaling.sh

python3 scripts/analyze_real_scaling.py
```

기본 출력:

```text
results/csv/real_scaling/real_scaling_raw.csv
results/csv/real_scaling/real_scaling_summary.csv
```

### 실제 Monte Carlo Core Utilization

```sh
TRIALS=120000000 STEPS=50 \
scripts/run_real_utilization.sh
```

이 스크립트는 `thread_4_reduce`, `process_4_shm`, `hybrid_2x2_shm`, `pipeline_4_final`을 실행하고 `mpstat`와 GNU time 결과를 저장합니다.

### Hybrid Synchronization 비교

```sh
TRIALS=1000000 STEPS=50 REPEATS=5 \
scripts/run_hybrid_sync.sh
```

### Profile 비교

```sh
TRIALS=100000 STEPS=50 REPEATS=5 \
scripts/run_profile_compare.sh

python3 scripts/analyze_profile_compare.py
```

---

## 주요 CLI 옵션

```text
--mode <seq|thread|pipeline|process|hybrid>
--threads <int>
--processes <int>
--sync <nosync|mutex|reduce>
--ipc <pipe|shm>
--schedule <static|queue>
--merge <final|interactive>
--trials <int>
--steps <int>
--batch-size <int>
--queue-size <int>
--workload <uniform|skewed>
--skew-factor <int>
--profile <default|process_friendly|thread_friendly>
--inner-work <int>
--pre-work <int>
--post-work <int>
--affinity <on|off>
--core-count <int>
--metrics-detail <0|1>
```

전체 옵션:

```sh
./sim --help
```

---

## 실행 환경

### Windows

Windows native 환경보다 WSL2 Ubuntu 또는 Docker Desktop을 권장합니다.

```powershell
wsl --install -d Ubuntu-22.04
```

WSL2 Ubuntu 내부:

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat
make clean
make
make test
```

### Linux

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat

make clean
make
make test
```

### Docker

```sh
docker build -t os-montecarlo-risk .
docker run --rm -it os-montecarlo-risk
```

Docker Desktop은 Linux VM 자원 설정의 영향을 받습니다. Worker scaling을 비교할 때는 Docker에 할당한 CPU 수와 memory를 고정해야 합니다.

---

## 프로젝트 구조

```text
.
├── include/
│   ├── config.h
│   ├── metrics.h
│   ├── affinity.h
│   ├── task_queue.h
│   └── merge_queue.h
├── src/
│   ├── main.c
│   ├── simulation.c
│   ├── sequential.c
│   ├── thread_mode.c
│   ├── process_mode.c
│   ├── hybrid_mode.c
│   ├── pipeline_mode.c
│   ├── task_queue.c
│   ├── merge_queue.c
│   ├── ipc_pipe.c
│   ├── ipc_shm.c
│   ├── affinity.c
│   └── metrics.c
├── scripts/
│   ├── run_real_scaling.sh
│   ├── run_real_utilization.sh
│   ├── analyze_real_scaling.py
│   ├── test_modes.sh
│   ├── run_hybrid_sync.sh
│   ├── run_profile_compare.sh
│   └── analyze_profile_compare.py
├── docs/
│   └── final_report_draft.md
├── results/csv/
│   ├── real_montecarlo_scaling_2026_06_06_5repeat/
│   ├── real_montecarlo_utilization_2026_06_06/
│   ├── hybrid_sync_2026_06_06/
│   └── profile_compare_2026_06_06/
├── Makefile
├── Dockerfile
└── docker-compose.yml
```

---

## 남아 있는 한계와 개선 방향

### 1. Native Linux 교차 측정

현재 핵심 결과는 Docker Desktop Ubuntu VM에서 측정했습니다. Native Linux에서 동일한 5회 실험을 수행하면 VM scheduler 영향을 분리할 수 있습니다.

### 2. 측정 순서 무작위화와 반복 확대

현재 scaling 실험은 case 실행 순서가 고정되어 있습니다. Case 순서를 무작위화하고 10회 이상 반복하며 실행 사이 cooldown을 추가하면 thermal throttling, cache 상태, background load의 영향을 줄일 수 있습니다.

### 3. Core Utilization 장시간 측정

현재 `mpstat` 결과는 대표 구조별 단일 실행과 약 3초 구간을 기준으로 합니다. 더 큰 workload 또는 반복 실행으로 10초 이상 측정하면 core 균일도 근거가 더 안정적입니다.

### 4. 8-worker 효율 개선

8-worker에서 CPU utilization은 높지만 efficiency가 낮습니다. 다음 항목을 추가로 확인할 가치가 있습니다.

- physical core와 logical core 구분
- Docker CPU quota와 scheduler 영향
- batch size 조정
- queue contention
- worker 수 자동 선택

### 5. Process 및 Hybrid 내부 Metrics

현재 process/hybrid의 `time_compute`는 fork-to-reap wall interval입니다. Hybrid sync별 정확성과 end-to-end 성능은 비교할 수 있지만, child 내부 lock wait와 reduce 시간을 parent CSV로 합산하지는 않습니다. 더 세밀한 분석에는 shared metrics table이 필요합니다.

### 6. Work Stealing

현재 pipeline은 중앙 task queue를 사용합니다. Worker별 deque와 work stealing을 적용하면 skewed workload의 load balancing을 개선할 수 있지만, 구조 복잡도와 synchronization 비용도 증가합니다.

### 7. Hardware Counter 측정

`perf stat`을 추가하면 context switch, cache miss, branch miss를 이용해 8-worker efficiency 감소 원인을 더 구체적으로 분석할 수 있습니다.

---

## Troubleshooting

| 문제 | 확인할 내용 |
| --- | --- |
| `make: command not found` | `build-essential`, `make` 설치 |
| script permission 오류 | `chmod +x scripts/*.sh` |
| `mpstat` 없음 | `sysstat` 설치 |
| `/usr/bin/time -v` 미지원 | GNU time이 있는 Ubuntu/Docker/WSL에서 실행 |
| affinity warning | Linux 외 환경 또는 제한된 container CPU set 확인 |
| `nosync` 결과 invalid | 의도된 race condition 비교 결과 |
| 8-worker efficiency 감소 | core 구성, Docker 할당 CPU, scheduler, queue overhead 확인 |
| Docker와 native 결과 차이 | VM 기반 결과와 native Linux 결과를 분리해 해석 |
