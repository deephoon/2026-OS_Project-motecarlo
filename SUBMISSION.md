# 프로젝트 실행 및 검증 가이드

이 문서는 프로젝트를 처음 확인하는 평가자가 구현 목적, 실행 방법, 정상 동작 판정 기준, 포함된 실험 결과를 빠르게 파악할 수 있도록 작성한 실행 안내서입니다.

## 1. 프로젝트 핵심

이 프로젝트는 Monte Carlo 기반 차량 추종 위험 계산을 실제 CPU-bound workload로 사용하여 다음 운영체제 실행 구조를 비교합니다.

| Mode | 사용 기술 | 확인 목적 |
| --- | --- | --- |
| `seq` | 단일 process | Sequential baseline |
| `thread` | POSIX pthread | Thread scaling과 synchronization |
| `process` | `fork()`, `waitpid()`, pipe/shared memory | Child process와 IPC |
| `hybrid` | Child process 내부 pthread | Process와 thread 결합 |
| `pipeline` | Task queue, mutex, condition variable | Producer-consumer와 동적 scheduling |

정상 실행 구조는 동일한 Monte Carlo 결과를 만들어야 합니다.

```text
valid = 1
hist_sum = trials
checksum = sequential checksum
```

`nosync`는 race condition을 의도적으로 재현하는 비교 조건이므로 invalid 결과가 정상입니다.

---

## 2. 가장 빠른 전체 검증

### 권장 방법: Docker Desktop 또는 Linux Docker

프로젝트 루트에서 아래 명령 두 개를 실행합니다.

```sh
docker build -t os-montecarlo-risk .
docker run --rm os-montecarlo-risk scripts/professor_smoke_test.sh
```

이미지가 캐시되어 있으면 수 초, 처음 빌드하는 환경에서는 패키지 설치를 포함해 수십 초에서 수 분 정도 걸릴 수 있습니다.

정상 실행 시 마지막에 다음 문장이 출력됩니다.

```text
Professor smoke test completed.
If the script reached this line, the submission is executable.
```

Smoke test는 다음 항목을 자동 확인합니다.

1. 전체 소스 clean build
2. Sequential baseline
3. pthread local reduce
4. Child process + pipe IPC
5. Child process + shared memory IPC
6. Hybrid process + pthread
7. Pipeline final merge
8. Pipeline interactive merge
9. Linux CPU affinity 옵션
10. 정상 mode의 `valid=1` 및 동일 checksum

---

## 3. Ubuntu 또는 WSL2에서 직접 실행

### 필요한 패키지

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat
```

### 빌드 및 자동 정확성 검사

```sh
make clean
make
make test
```

`make test`는 대표 정상 mode 8개를 실행합니다. 각 mode의 결과가 invalid이거나 sequential checksum과 다르면 즉시 실패합니다.

정상 출력 예:

```text
PASS seq                    checksum=...
PASS thread_reduce          checksum=...
PASS process_pipe           checksum=...
PASS process_shm            checksum=...
PASS hybrid_reduce          checksum=...
PASS hybrid_mutex           checksum=...
PASS pipeline_final         checksum=...
PASS pipeline_interactive   checksum=...
all normal modes are valid and checksum-consistent
```

---

## 4. 대표 실행 명령

아래 명령은 짧은 workload를 사용하여 각 구조를 직접 확인합니다.

### Sequential

```sh
./sim --mode seq --trials 100000 --steps 50
```

### pthread + Local Reduce

```sh
./sim --mode thread --threads 4 --sync reduce \
  --trials 100000 --steps 50
```

### Child Process + Shared Memory IPC

```sh
./sim --mode process --processes 4 --ipc shm \
  --trials 100000 --steps 50
```

### Child Process + Pipe IPC

```sh
./sim --mode process --processes 4 --ipc pipe \
  --trials 100000 --steps 50
```

### Hybrid: 2 Processes x 2 Threads

```sh
./sim --mode hybrid --processes 2 --threads 2 \
  --ipc shm --sync reduce \
  --trials 100000 --steps 50
```

### Pipeline + Final Merge

```sh
./sim --mode pipeline --threads 4 --schedule queue --merge final \
  --batch-size 1000 --queue-size 1024 \
  --trials 100000 --steps 50
```

### Pipeline + Interactive Merge

```sh
./sim --mode pipeline --threads 4 --schedule queue --merge interactive \
  --batch-size 1000 --queue-size 1024 \
  --trials 100000 --steps 50
```

---

## 5. Synchronization 문제 확인

같은 hybrid 구조에서 synchronization 방식만 변경하여 race condition과 해결 효과를 확인할 수 있습니다.

```sh
# 의도적인 race condition: valid=0이 발생할 수 있음
./sim --mode hybrid --processes 2 --threads 2 --ipc shm --sync nosync \
  --trials 1000000 --steps 50

# 정확하지만 trial마다 lock을 사용하는 방식
./sim --mode hybrid --processes 2 --threads 2 --ipc shm --sync mutex \
  --trials 1000000 --steps 50

# 정확성과 성능을 함께 확보하는 local reduce
./sim --mode hybrid --processes 2 --threads 2 --ipc shm --sync reduce \
  --trials 1000000 --steps 50
```

포함된 5회 반복 결과:

| Case | Avg Time | Valid | 해석 |
| --- | ---: | ---: | --- |
| Hybrid nosync | `0.0312s` | `0` | Race condition으로 결과 손상 |
| Hybrid mutex | `0.0563s` | `1` | 정확하지만 lock contention 발생 |
| Hybrid reduce | `0.0288s` | `1` | 정확하며 mutex보다 약 `1.95x` 빠름 |

---

## 6. 포함된 최신 실험 결과

모든 핵심 결과는 Docker Desktop 기반 Ubuntu 22.04 Linux VM에서 측정했습니다. 동일 환경 내 구조별 상대 비교와 scaling 분석에 사용하며, native Linux 절대 성능으로 해석하지 않습니다.

### 실제 Monte Carlo Strong Scaling

조건:

```text
Trials = 120,000,000
Steps = 50
Repeats = 5
Affinity = on
```

| Case | Workers | Avg Time | Speedup | Efficiency | Util/Worker |
| --- | ---: | ---: | ---: | ---: | ---: |
| `seq` | 1 | `12.111s` | `1.000x` | `100.0%` | `99.0%` |
| `thread_4_reduce` | 4 | `3.236s` | `3.743x` | `93.6%` | `99.7%` |
| `process_4_shm` | 4 | `3.222s` | `3.759x` | `94.0%` | `99.5%` |
| `hybrid_2x2_shm` | 4 | `3.222s` | `3.758x` | `94.0%` | `99.5%` |
| `pipeline_4_final` | 4 | `3.275s` | `3.698x` | `92.5%` | `99.8%` |
| `pipeline_8_final` | 8 | `2.177s` | `5.564x` | `69.6%` | `99.7%` |

검증 결과:

```text
14개 scaling case x 5회 반복
valid_all = 1
checksum_count = 1
```

핵심 해석:

- 실제 Monte Carlo 구조는 4-worker까지 `T1/4`에 상당히 근접합니다.
- 8-worker에서도 CPU는 거의 포화되지만 efficiency는 감소합니다.
- CPU utilization이 높다는 사실과 실행시간이 선형적으로 감소한다는 것은 서로 다른 의미입니다.

### Core별 Utilization

| Case | CPU% | Util/Worker | 대상 Core 최소 사용률 | Core 표준편차 |
| --- | ---: | ---: | ---: | ---: |
| `thread_4_reduce` | `398%` | `99.5%` | `100.0%` | `0.00` |
| `process_4_shm` | `398%` | `99.5%` | `100.0%` | `0.00` |
| `hybrid_2x2_shm` | `397%` | `99.2%` | `99.7%` | `0.16` |
| `pipeline_4_final` | `399%` | `99.8%` | `100.0%` | `0.00` |

---

## 7. 최신 실험 재현

최종 조건은 실행 시간이 오래 걸릴 수 있습니다. 먼저 아래 빠른 조건으로 스크립트 동작을 확인하는 것을 권장합니다.

### Docker에서 결과를 호스트에 저장하는 방법

Container 종료 후에도 새 결과를 보존하려면 프로젝트의 `results` 디렉터리를 mount합니다.

```sh
docker run --rm \
  -v "$PWD/results:/workspace/results" \
  os-montecarlo-risk \
  sh -lc 'TRIALS=100000 STEPS=30 REPEATS=2 scripts/run_real_scaling.sh'
```

생성된 결과는 호스트의 `results/csv/real_scaling/`에서 확인할 수 있습니다.

### 실제 Monte Carlo Scaling

빠른 확인:

```sh
TRIALS=100000 STEPS=30 REPEATS=2 scripts/run_real_scaling.sh

RAW_PATH=results/csv/real_scaling/real_scaling_raw.csv \
SUMMARY_PATH=results/csv/real_scaling/real_scaling_summary.csv \
python3 scripts/analyze_real_scaling.py
```

최종 조건:

```sh
TRIALS=120000000 STEPS=50 REPEATS=5 scripts/run_real_scaling.sh
```

### Core별 Utilization

```sh
TRIALS=120000000 STEPS=50 scripts/run_real_utilization.sh

RAW_PATH=results/csv/real_scaling/real_scaling_raw.csv \
UTIL_DIR=results/csv/real_utilization \
UTIL_SUMMARY_PATH=results/csv/real_utilization/real_utilization_summary.csv \
python3 scripts/analyze_real_scaling.py
```

### Hybrid Synchronization 비교

```sh
TRIALS=1000000 STEPS=50 REPEATS=5 scripts/run_hybrid_sync.sh
```

### Process-Friendly / Thread-Friendly 교차 비교

```sh
TRIALS=100000 STEPS=50 REPEATS=5 scripts/run_profile_compare.sh
python3 scripts/analyze_profile_compare.py
```

---

## 8. 결과 파일 위치

제출본에는 최신 검증 결과만 포함되어 있습니다.

```text
results/csv/
├── real_montecarlo_scaling_2026_06_06_5repeat/
├── real_montecarlo_utilization_2026_06_06/
├── hybrid_sync_2026_06_06/
└── profile_compare_2026_06_06/
```

| 파일 | 내용 |
| --- | --- |
| `real_scaling_summary.csv` | Mode별 평균시간, speedup, efficiency, CPU utilization |
| `real_utilization_summary.csv` | Core별 활용률, Max RSS, 정확성 |
| `hybrid_sync_summary.csv` | Nosync/mutex/reduce 정확성과 성능 비교 |
| `profile_compare_summary.csv` | Process-friendly/thread-friendly 교차 비교 |

---

## 9. 주요 문서와 코드 위치

| 위치 | 내용 |
| --- | --- |
| `README.md` | 전체 프로젝트 설계와 결과 분석 |
| `docs/final_report_draft.md` | 프로젝트 문제 해결 과정과 실험 분석 상세 초안 |
| `src/thread_mode.c` | Thread synchronization과 local reduce |
| `src/process_mode.c` | Child process 실행과 parent merge |
| `src/hybrid_mode.c` | Child process 내부 pthread |
| `src/pipeline_mode.c` | Task queue, worker pool, aggregator |
| `src/task_queue.c`, `src/merge_queue.c` | Mutex + condition variable queue |
| `src/ipc_pipe.c`, `src/ipc_shm.c` | Pipe/shared memory IPC |
| `scripts/test_modes.sh` | 정상 mode 자동 정확성 검사 |

---

## 10. 실행 환경별 주의사항

### Docker Desktop

- Docker에 할당한 CPU 수가 worker 수보다 적으면 scaling 결과가 달라집니다.
- 포함된 결과는 Docker Desktop Ubuntu Linux VM 결과입니다.

### WSL2

- `mpstat`와 affinity 결과는 WSL2 scheduler와 할당 자원의 영향을 받습니다.

### Native Linux

- CPU affinity, GNU time, `mpstat`를 가장 직접적으로 사용할 수 있습니다.
- 포함된 Docker 결과와 절대시간이 다를 수 있습니다.

### macOS

- 기본 빌드와 simulation 실행은 가능할 수 있지만 Linux 전용 affinity와 GNU time, `mpstat` 검증에는 적합하지 않습니다.

---

## 11. 문제 해결

| 문제 | 해결 방법 |
| --- | --- |
| `make` 또는 `gcc` 없음 | Ubuntu에서 `build-essential make` 설치 |
| `mpstat` 없음 | `sudo apt install sysstat` |
| `/usr/bin/time -v` 미지원 | Ubuntu/Docker/WSL의 GNU time 사용 |
| Affinity warning | 제한된 container CPU set 또는 Linux 외 환경 확인 |
| `nosync`가 invalid | 의도한 race condition 재현 결과 |
| Docker 결과가 포함 결과와 다름 | Docker CPU 할당, background load, architecture 확인 |

---

## 12. 정상 제출본 구조

```text
.
├── README.md
├── SUBMISSION.md
├── Dockerfile
├── docker-compose.yml
├── Makefile
├── include/
├── src/
├── scripts/
│   ├── professor_smoke_test.sh
│   ├── test_modes.sh
│   ├── run_real_scaling.sh
│   ├── run_real_utilization.sh
│   ├── run_hybrid_sync.sh
│   └── run_profile_compare.sh
├── docs/
│   └── final_report_draft.md
└── results/csv/
    ├── real_montecarlo_scaling_2026_06_06_5repeat/
    ├── real_montecarlo_utilization_2026_06_06/
    ├── hybrid_sync_2026_06_06/
    └── profile_compare_2026_06_06/
```
