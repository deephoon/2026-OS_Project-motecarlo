# OS26 최종 실험 가이드

이 문서는 `ref/os26_project.pdf`의 핵심 요구사항인 **실제 시스템의 N-core 활용, 실행시간 감소, process/thread/hybrid 비교, 문제 해결 과정**을 검증하기 위한 실험 가이드입니다.

## 1. 핵심 검증 대상

최종 성능 근거는 인위적인 CPU loop가 아니라 실제 차량 추종 Monte Carlo simulation입니다.

```text
고정된 전체 Monte Carlo 작업
  -> seq/thread/process/hybrid/pipeline으로 실행
  -> worker 수 1/2/4/8 변경
  -> T1/N, speedup, efficiency, CPU utilization 비교
  -> valid/checksum으로 정확성 검증
```

## 2. 실제 Monte Carlo Scaling 실험

```sh
TRIALS=120000000 STEPS=50 REPEATS=5 scripts/run_real_scaling.sh
python3 scripts/analyze_real_scaling.py
```

기본 실험은 다음 구조를 비교합니다.

| 구조 | 비교 조건 |
| --- | --- |
| sequential | `seq` |
| pthread | `thread 1/2/4/8 + local reduce` |
| child process | `process 1/2/4 + shared memory IPC` |
| hybrid | `2 processes x 2/4 threads` |
| pipeline | `pipeline 1/2/4/8 + task queue + final reduce` |

결과 파일:

```text
results/csv/real_scaling/real_scaling_raw.csv
results/csv/real_scaling/real_scaling_summary.csv
```

## 3. Core Utilization 실험

```sh
TRIALS=120000000 STEPS=50 scripts/run_real_utilization.sh
```

이 스크립트는 실제 Monte Carlo의 대표 4-worker 구조를 실행하며 다음 자료를 남깁니다.

- `/usr/bin/time -v`: 전체 CPU%, memory 사용량
- `mpstat -P ALL 1`: core별 utilization
- simulation CSV: time, stage metrics, valid, checksum

## 4. 가이드 요구사항 대응

| 요구사항 | 검증 방법 |
| --- | --- |
| N개 core에서 실행시간이 T1/N에 가까워지는지 | `ideal_time`, `speedup`, `efficiency` 비교 |
| CPU utilization이 100%에 가까운지 | `avg_cpu_percent / workers`와 `mpstat` 확인 |
| child process 병렬처리 | `process_1/2/4_shm` |
| single/multi thread 비교 | `thread_1/2/4/8_reduce` |
| process + thread 조합 | `hybrid_2x2_shm`, `hybrid_2x4_shm` |
| synchronization 문제와 해결 | `nosync`, `mutex`, `local reduce`, mutex+condition variable queue |
| 예상 성능이 나오지 않을 때 구조 수정 | shared update 제거, shm IPC, affinity mapping, stage metric 수정 |

## 5. 문제 → 원인 → 해결 → 검증

| 문제 | 원인 | 해결 | 검증 |
| --- | --- | --- | --- |
| mutex mode가 느림 | trial마다 shared result lock | worker-local result + join 후 reduce | thread scaling 비교 |
| process가 thread보다 느림 | fork/IPC/주소공간 비용 | child-local 계산 후 결과 1회 shm 전달 | process 1/2/4 비교 |
| hybrid가 core를 겹쳐 사용 | child별 pthread가 같은 core 번호 사용 | global worker id 기반 affinity | hybrid CPU% 비교 |
| process stage metric 왜곡 | 전체 waitpid 시간을 sync로 기록 | fork-to-reap을 parallel compute window로 기록 | stage metric 확인 |
| 8-worker 효율 감소 | VM scheduler, worker 관리, queue/IPC 비용 | 4/8-worker 결과를 함께 제시하고 한계 분석 | efficiency와 표준편차 비교 |

## 6. 결과 해석 기준

- `valid_all=1`, `checksum_count=1`인 결과만 성능 비교에 사용합니다.
- `T_total`은 wall-clock 시간입니다.
- `T_sync`는 여러 worker의 lock/condition wait 누적값일 수 있으므로 `T_total`의 단순 구성요소로 더하지 않습니다.
- CPU utilization이 높아도 efficiency가 낮을 수 있습니다. 이 경우 core는 바쁘지만 scheduler/queue/IPC 등으로 유용한 계산 비율이 낮아진 것입니다.
- Docker Desktop 결과는 Ubuntu Linux VM 결과이며 native Linux 절대 성능으로 표현하지 않습니다.

## 7. 최종 측정 결과 위치

```text
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_raw.csv
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_summary.csv
results/csv/real_montecarlo_utilization_2026_06_06/real_utilization_summary.csv
```

대표 결과:

| Case | Workers | Speedup | Efficiency | CPU/Worker |
| --- | ---: | ---: | ---: | ---: |
| `thread_4_reduce` | 4 | `3.743x` | `93.6%` | `99.7%` |
| `process_4_shm` | 4 | `3.759x` | `94.0%` | `99.5%` |
| `hybrid_2x2_shm` | 4 | `3.758x` | `94.0%` | `99.5%` |
| `pipeline_4_final` | 4 | `3.698x` | `92.5%` | `99.8%` |

현재 실제 Monte Carlo 설계는 4-worker 조건에서 가이드가 요구하는 이상적 scaling과 core saturation에 상당히 근접합니다. `mpstat`에서도 affinity 대상 core 최소 활용률 `99.7%`, 최대 표준편차 `0.16`을 확인했습니다. 8-worker에서는 CPU utilization은 높지만 efficiency가 감소하므로 최적 worker 수와 구조적 overhead를 함께 설명해야 합니다.
