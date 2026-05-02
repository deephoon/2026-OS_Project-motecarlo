---
marp: true
title: Monte Carlo CPU-bound Parallel Simulation
paginate: true
---

# 1. 프로젝트 제목 및 핵심 정의

## Monte Carlo 기반 CPU-bound 병렬 시뮬레이션의 Thread/Synchronization 성능 분석

본 프로젝트는 차량 시뮬레이터 구현이 아니라, CPU-bound 반복 계산을 이용해 운영체제의 병렬처리와 동기화 문제를 실험하는 시스템이다.

핵심 키워드

- CPU-bound workload
- pthread-based parallel execution
- race condition
- mutex synchronization
- local reduce optimization
- final extension: child process / IPC / hybrid

---

# 2. 프로젝트 목표

동일한 CPU-bound 작업을 실행 구조와 동기화 방식에 따라 비교하고, 정확성과 성능의 trade-off를 분석한다.

| 목표 | 내용 |
|---|---|
| 병렬처리 성능 분석 | Sequential과 Thread 실행시간 비교 |
| CPU-bound 작업 구성 | Monte Carlo trial을 대량 반복 |
| Synchronization 검증 | nosync / mutex / local reduce 비교 |
| 결과 정확성 검증 | total_trials, histogram_sum, checksum, valid 확인 |
| 최종 확장 | child process, IPC/shared memory, hybrid 구조 구현 예정 |

현재 구현 범위에서는 thread 기반 병렬처리와 synchronization 문제를 먼저 검증하고, 최종 발표까지 child process와 hybrid 구조로 확장한다.

---

# 3. 시스템 전체 구조

입력 파라미터를 기반으로 독립적인 Monte Carlo trial을 생성하고, 실행 방식과 동기화 방식에 따른 결과 정확성과 성능을 비교한다.

```text
[Input Parameters]
trials, steps, threads, sync_mode, seed
        |
        v
[Simulation Engine]
Monte Carlo trial 반복 수행
        |
        v
[Execution Mode]
Sequential / Thread
        |
        v
[Synchronization Mode]
nosync / mutex / local reduce
        |
        v
[Result Validation]
total_trials, histogram_sum, checksum, valid
        |
        v
[Performance Analysis]
execution time, speedup, efficiency
        |
        v
[Final Extension]
child process / IPC / hybrid
```

---

# 4. Monte Carlo 시뮬레이션 모델

## Simulation Model: Independent Trial-based Workload

각 trial은 독립적인 계산 단위이며, thread별로 나누어 병렬 처리할 수 있다.

Trial 계산 흐름

1. seed 기반 난수로 차량 상태 생성
2. ego/front 차량 속도, 거리, 반응시간 설정
3. time-step마다 위치와 속도 업데이트
4. relative_distance, relative_speed, TTC 계산
5. collision 또는 risk level 판단
6. histogram과 collision_count에 반영

| Risk Level | 기준 |
|---|---|
| Collision | `relative_distance <= 0` |
| High | `TTC < 1.5` |
| Medium | `TTC < 3.0` |
| Low | `TTC < 5.0` |
| Safe | 그 외 |

---

# 5. 현재 구현 범위와 최종 확장 범위

중간발표에서는 Thread와 Synchronization 검증을 완료했고, 최종 발표에서는 Process / IPC / Hybrid 구조로 확장한다.

| 구분 | 현재 상태 | 최종 확장 |
|---|---|---|
| Sequential baseline | 구현 완료 | 기준 성능 유지 |
| Pthread thread mode | 구현 완료 | thread 수별 실험 확장 |
| nosync / mutex / reduce | 구현 완료 | sync overhead 정밀 분석 |
| CSV 결과 저장 | 구현 완료 | 그래프 자동화 |
| Child process | 확장 예정 | fork 기반 process mode |
| IPC / shared memory | 확장 예정 | child 결과 병합 |
| Hybrid mode | 확장 예정 | process + thread 구조 |
| Dynamic task queue | 확장 예정 | worker idle 감소 분석 |

---

# 6. Thread 기반 병렬처리 구조

Monte Carlo trial range를 thread별로 나누어 병렬 계산한다.

```text
[Main Thread]
- Config parsing
- Result initialization
- pthread_create()
        |
        v
[Thread 0] trial range A 처리
[Thread 1] trial range B 처리
[Thread 2] trial range C 처리
[Thread 3] trial range D 처리
        |
        v
[pthread_join()]
        |
        v
[Result Merge / Validation]
```

| 항목 | 내용 |
|---|---|
| 병렬화 단위 | Monte Carlo trial range |
| 분할 방식 | Static partition |
| Thread 수 | 1 / 2 / 4 / 8 |
| 공유 자원 | global result, histogram |
| 주요 문제 | 동시 update 시 race condition |

---

# 7. Synchronization 실험 설계

같은 병렬 작업도 공유 자원을 어떻게 갱신하느냐에 따라 정확성과 성능이 달라진다.

| Sync Mode | 구현 방식 | 목적 |
|---|---|---|
| nosync | global result 직접 갱신 | race condition 관찰 |
| mutex | 매 trial마다 lock/unlock | 정확성 보장 |
| local reduce | thread-local result 후 merge | 정확성 유지 + lock overhead 감소 |

```text
nosync:
  global_histogram[risk]++

mutex:
  lock
  global_histogram[risk]++
  unlock

reduce:
  local_histogram[risk]++
  thread 종료 후 merge
```

---

# 8. 프로그램 구성 및 실행 환경

C, pthread, Makefile, Docker Linux 환경에서 재현 가능한 실험 구조를 구성했다.

```text
os-montecarlo-risk/
├── src/
│   ├── main.c
│   ├── simulation.c
│   ├── sequential.c
│   ├── thread_mode.c
│   ├── sync.c
│   ├── metrics.c
│   └── result.c
├── include/
├── scripts/
│   └── run_midterm.sh
├── results/
└── Dockerfile
```

| 항목 | 내용 |
|---|---|
| Language | C |
| Thread Library | pthread |
| Build | Makefile |
| Environment | Docker Ubuntu Linux |
| Measurement | clock_gettime, CSV logging |

---

# 9. 실험 조건 및 Test Vector

동일한 작업에서 thread 수, sync 방식, steps 값을 바꿔 성능과 정확성을 비교한다.

| 변수 | 값 |
|---|---|
| trials | 10,000 / 100,000 / 1,000,000 |
| steps | 10 / 50 / 100 |
| threads | 1 / 2 / 4 / 8 |
| sync | nosync / mutex / reduce |
| seed | 42 fixed |

| Case | 조건 | 목적 |
|---|---|---|
| E1 | Sequential | baseline |
| E2 | Thread 1/2/4/8 + reduce | thread scalability |
| E3 | Thread 4 + nosync | race condition 확인 |
| E4 | Thread 4 + mutex | 정확성 확보 |
| E5 | Thread 4 + reduce | 동기화 최적화 |
| E6 | steps 10/50/100 | CPU-bound 강도 변화 |

---

# 10. 실험 결과 1: Thread 병렬처리 성능

Thread 수 증가에 따른 실행시간과 speedup을 비교하고, 실제 speedup이 이상적 speedup과 다른 이유를 분석한다.

`TRIALS=1000000`, `STEPS=50`

| Mode | Threads | Sync | Time(sec) | Speedup | Valid |
|---|---:|---|---:|---:|---:|
| seq | 1 | - | 0.114392 | 1.00x | 1 |
| thread | 1 | reduce | 0.102064 | 1.120787 | 1 |
| thread | 2 | reduce | 0.053912 | 2.121828 | 1 |
| thread | 4 | reduce | 0.032194 | 3.553209 | 1 |
| thread | 8 | reduce | 0.020967 | 5.455812 | 1 |

분석

- Thread 수 증가에 따라 실행시간은 감소하는 경향을 보인다.
- 실제 speedup은 이상적인 T/N과 차이가 있다.
- 원인: thread overhead, scheduling overhead, physical core 수 한계, workload 크기.

---

# 11. 실험 결과 2: Synchronization 정확성 비교

nosync에서 valid=0이 되는 결과를 통해 synchronization 필요성을 확인하고, mutex/reduce로 정확성을 회복한다.

`TRIALS=1000000`, `STEPS=50`, `threads=4`

| Sync Mode | Valid | Histogram Sum | Time(sec) | 해석 |
|---|---:|---:|---:|---|
| nosync | 0 | 800794 | 0.036567 | race condition 발생 |
| mutex | 1 | 1000000 | 0.136491 | 정확하지만 lock overhead |
| reduce | 1 | 1000000 | 0.031272 | 정확성 + 성능 개선 |

강조

```text
nosync에서 valid=0이 되는 결과는
여러 thread가 공유 counter와 histogram을 동시에 갱신하면서
race condition이 발생했음을 보여준다.
```

---

# 12. 결과 분석: 문제 -> 원인 -> 해결

실험 결과를 문제, 원인, 해결 방식으로 연결해 운영체제 관점에서 분석한다.

| 발견한 문제 | 원인 | 해결 방법 | 검증 |
|---|---|---|---|
| Sequential 실행이 느림 | 하나의 core만 사용 | pthread 병렬화 | thread 수별 time 비교 |
| nosync 결과 불일치 | race condition | mutex 적용 | valid=1 |
| mutex 성능 저하 | lock contention | local reduce | 실행시간 비교 |
| 8 thread 효율 저하 가능 | context switch / core 수 한계 | efficiency 분석 | speedup 비교 |
| process 미구현 | 중간 구현 범위 | 최종 단계 확장 | fork/IPC 계획 |

핵심 OS 개념

- race condition
- mutex synchronization
- lock contention
- thread scheduling overhead
- speedup and efficiency

---

# 13. 최종 발표까지 확장 계획

## Final Extension Plan: Process / IPC / Hybrid

```text
[Current Implementation]
Sequential + Thread + Sync 비교
        |
        v
[Step 1: Child Process]
Parent가 fork()로 child 생성
각 child가 simulation group 처리
        |
        v
[Step 2: IPC / Shared Memory]
child result를 parent가 병합
T_merge 측정
        |
        v
[Step 3: Hybrid]
각 child process 내부에 thread pool 구성
process-local reduce 후 parent merge
        |
        v
[Final Analysis]
process vs thread vs hybrid
CPU utilization / memory usage / speedup / efficiency
```

| 구성 | 역할 |
|---|---|
| Parent Process | child 생성, 결과 병합, 성능 측정 |
| Child Process | 큰 simulation group 처리 |
| Thread | child 내부 batch 병렬 계산 |
| IPC / Shared Memory | child 결과 전달 |
| Local Reduce | thread 결과 병합 최적화 |

---

# 14. 역할분담 및 결론

| 팀원 | 담당 | 현재 산출물 | 최종 작업 |
|---|---|---|---|
| 정재훈 | 총괄 / 발표 / 통합 | 설계, 발표자료 | hybrid 통합 |
| 성도연 | thread / sync | pthread, mutex, reduce | task queue |
| 유지원 | simulation / metrics | run_trial, baseline | 실험 확장 |
| 김태환 | result 검증 / process | histogram, checksum | process mode |
| 정종근 | 실험 자동화 / 보고서 | CSV, 그래프 | 결과 분석 |

결론

1. 현재 구현 범위에서는 sequential, pthread, nosync/mutex/reduce 비교를 구현했다.
2. nosync 결과를 통해 synchronization 필요성을 확인했다.
3. mutex와 reduce를 비교하여 정확성과 성능 trade-off를 분석했다.
4. 최종 발표까지 child process, IPC, hybrid 구조로 확장한다.

