# Reproducible Linux Experiment Guide

이 문서는 최종 보고서용 성능 결과를 더 신뢰성 있게 만들기 위한 실행 기준과, Windows WSL / 일반 Linux / Docker Linux 환경에서 같은 실험을 재현하는 방법을 정리합니다.

## 1. 이번에 적용한 신뢰도 보강

기존 10,000 trials 결과는 실행 시간이 1ms 안팎이라 OS scheduler noise의 영향을 크게 받습니다. 그래서 최종 보고서용 실험은 아래 기준으로 다시 측정합니다.

| 항목 | 기준 |
| --- | --- |
| 최소 trials | `100000` |
| 권장 trials | `1000000` |
| steps | `50` 이상 |
| 반복 횟수 | `5`회 이상 |
| 환경 | Docker Ubuntu Linux 또는 고정된 Linux/WSL 환경 |
| 결과 해석 기준 | `final_analyzed.csv`의 평균/최소/표준편차/speedup/efficiency |

주의: `--pre-work`는 실행 1회당 고정 pre-processing 부하로 동작합니다. batch size 비교에서 특정 batch가 부당하게 유리해지지 않도록 보정했습니다.

## 2. Docker Ubuntu Linux 실험 결과

실행 환경:

```text
Docker image: ubuntu:22.04
Compiler: gcc with -O2 -pthread
Architecture: Docker Desktop Linux container
```

실행 명령:

```sh
docker build -t os-montecarlo-risk .

docker run --rm \
  -v "$PWD/results/csv/docker_1m:/workspace/results/csv" \
  os-montecarlo-risk \
  sh -lc 'TRIALS=1000000 STEPS=50 REPEATS=5 PRE_WORK=50000 POST_WORK=10000 OUT_DIR=results/csv scripts/run_final.sh'
```

생성 파일:

```text
results/csv/docker_1m/final_raw.csv
results/csv/docker_1m/final_analyzed.csv
results/csv/docker_1m/final_summary.md
```

## 3. 1,000,000 trials 결과 요약

조건:

```text
trials = 1000000
steps = 50
repeats = 5
pre_work = 50000
post_work = 10000
sequential baseline avg = 0.099473s
```

상위 성능 결과:

| Case | Avg time | Speedup | Efficiency | Valid | Checksum matches seq |
| --- | ---: | ---: | ---: | ---: | ---: |
| `thread_8_reduce` | `0.020732s` | `4.798` | `0.600` | 1 | 1 |
| `hybrid_2x4` | `0.023281s` | `4.273` | `0.534` | 1 | 1 |
| `hybrid_4x2` | `0.023545s` | `4.225` | `0.528` | 1 | 1 |
| `thread_4_reduce` | `0.029020s` | `3.428` | `0.857` | 1 | 1 |
| `pipeline_final_b1000` | `0.029900s` | `3.327` | `0.832` | 1 | 1 |

동기화 비교:

| Case | Avg time | Speedup | Valid | Checksum matches seq | 해석 |
| --- | ---: | ---: | ---: | ---: | --- |
| `thread_4_reduce` | `0.029020s` | `3.428` | 1 | 1 | 정확성과 성능 균형이 좋음 |
| `thread_4_mutex` | `0.132535s` | `0.751` | 1 | 1 | 매 trial lock으로 contention 큼 |
| `thread_4_nosync` | `0.040384s` | `2.463` | 0 | 0 | 빠를 수 있지만 결과가 틀림 |

process / hybrid 비교:

| Case | Avg time | Speedup | Efficiency | 해석 |
| --- | ---: | ---: | ---: | --- |
| `process_1_pipe` | `0.103178s` | `0.964` | `0.964` | 단일 child는 fork/IPC overhead로 seq보다 약함 |
| `process_2_pipe` | `0.057633s` | `1.726` | `0.863` | 계산량이 커지면 process 병렬화 효과 발생 |
| `process_4_pipe` | `0.030225s` | `3.291` | `0.823` | multi-process 병렬화 효과가 명확함 |
| `hybrid_2x4` | `0.023281s` | `4.273` | `0.534` | process 격리 + 내부 thread 병렬화 효과 |
| `hybrid_4x2` | `0.023545s` | `4.225` | `0.528` | 비슷한 worker 수에서 hybrid trade-off 확인 가능 |

pipeline / merge 비교:

| Case | Avg time | Speedup | 해석 |
| --- | ---: | ---: | --- |
| `pipeline_final_b1000` | `0.029900s` | `3.327` | final reduce가 가장 단순하고 안정적 |
| `pipeline_interactive_b1000` | `0.033507s` | `2.969` | 중간 병합 가능하지만 queue/aggregator overhead 존재 |
| `pipeline_interactive_b100` | `0.063280s` | `1.572` | batch가 너무 작으면 queue 접근이 많아져 느림 |
| `pipeline_interactive_b10000` | `0.030529s` | `3.258` | queue overhead는 작지만 load balancing은 약해질 수 있음 |

## 4. 보고서에서 주장 가능한 결론

- `valid_all=1`이고 `matches_seq_checksum=1`인 mode만 성능 비교에 사용했습니다.
- `thread_8_reduce`는 이번 Linux Docker 조건에서 가장 빠른 평균 실행시간을 보였습니다.
- `thread_4_reduce`는 speedup은 `thread_8_reduce`보다 낮지만 efficiency가 더 높아, worker 사용 효율 측면에서는 좋은 기준점입니다.
- `thread_4_mutex`는 정확하지만 매 trial마다 lock을 잡기 때문에 성능이 크게 나빠집니다.
- `thread_4_nosync`는 빠르게 보일 수 있으나 `valid=0`, checksum mismatch이므로 synchronization 필요성을 보여주는 실패 사례입니다.
- process는 작은 workload에서는 overhead가 크지만, 1,000,000 trials에서는 `process_4_pipe`가 `3.291x` speedup을 보여 child process 병렬화 효과를 설명할 수 있습니다.
- hybrid는 구조가 복잡하지만 `hybrid_2x4`, `hybrid_4x2` 모두 `4x` 이상의 speedup을 보여 process/thread 결합의 장점을 설명할 수 있습니다.
- interactive merge는 현실적인 중간 병합 구조이지만, final reduce보다 synchronization overhead가 추가됩니다.

## 5. CPU / Memory 측정 예시

대표 실행:

```sh
docker run --rm os-montecarlo-risk sh -lc \
  '/usr/bin/time -v ./sim --mode thread --threads 8 \
    --trials 1000000 --steps 50 --sync reduce \
    --pre-work 50000 --post-work 10000 --metrics-detail 1'
```

측정 예시:

```text
Percent of CPU this job got: 700%
Maximum resident set size: 1344 KB
valid: 1
hist_sum: 1000000
```

보고서에는 CPU 사용률과 최대 RSS를 함께 캡처하면 좋습니다. 단, Docker Desktop 환경에서는 Linux VM 위에서 실행되므로 실제 물리 Linux와 수치가 다를 수 있습니다.

## 6. Windows에서 실행하는 방법

### 방법 A: WSL2 Ubuntu 권장

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

결과:

```text
results/csv/final_raw.csv
results/csv/final_analyzed.csv
results/csv/final_summary.md
```

### 방법 B: Windows Docker Desktop

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

Windows에서는 native MinGW보다 WSL2 또는 Docker를 권장합니다. 이 프로젝트는 `pthread`, `fork`, `pipe`, `waitpid` 같은 POSIX/Linux API를 사용하기 때문에 Windows native C 환경에서는 그대로 실행하기 어렵습니다.

## 7. 일반 Linux에서 실행하는 방법

Ubuntu/Debian:

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

## 8. 최종 보고서에 넣을 실험 신뢰도 문장

```text
최종 성능 분석은 10,000 trials의 기능 검증 결과가 아니라,
Docker Ubuntu Linux 환경에서 100,000 및 1,000,000 trials 조건을 5회 반복 측정한 결과를 기준으로 수행했다.
각 mode는 valid flag와 checksum을 sequential baseline과 비교하여 정확성을 먼저 검증했고,
그 후 평균 실행시간, 최소 실행시간, 표준편차, speedup, efficiency를 계산했다.
따라서 단일 실행이나 1ms 수준의 측정값에 의존하지 않고,
반복 측정 기반으로 process/thread/synchronization/pipeline trade-off를 해석했다.
```

