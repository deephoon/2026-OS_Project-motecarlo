# 실행 및 검증 가이드

이 문서는 프로젝트를 처음 받은 사람이 실제 Monte Carlo 시스템을 바로 빌드하고 검증하기 위한 안내서입니다.

## 1. 가장 빠른 검증

Docker가 설치되어 있다면 프로젝트 루트에서 아래 두 명령을 실행합니다.

```sh
docker build -t os-montecarlo-risk .
docker run --rm os-montecarlo-risk scripts/professor_smoke_test.sh
```

마지막에 아래 문장이 나오면 빌드와 대표 실행 모드 검증이 완료된 것입니다.

```text
Professor smoke test completed.
```

smoke test는 실제 차량 추종 Monte Carlo workload로 `seq`, `thread`, `process`, `hybrid`, `pipeline`, pipe/shm IPC, final/interactive merge, CPU affinity 옵션을 확인합니다. 정상 simulation row는 `valid=1`이어야 합니다.

## 2. Linux 또는 WSL2에서 직접 실행

Ubuntu 기준:

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat

make clean
make
make test
scripts/professor_smoke_test.sh
```

이 프로젝트는 `pthread`, `fork()`, `waitpid()`, `pipe`, `mmap`, `sched_setaffinity()`를 사용하므로 Windows native 환경보다 Docker Desktop 또는 WSL2 Ubuntu를 권장합니다.

## 3. 실제 Monte Carlo 핵심 실행

```sh
# sequential baseline
./sim --mode seq --trials 1000000 --steps 50

# pthread + local reduce
./sim --mode thread --threads 4 --sync reduce \
  --trials 1000000 --steps 50 --affinity on

# child process + shared memory IPC
./sim --mode process --processes 4 --ipc shm \
  --trials 1000000 --steps 50 --affinity on

# child process 내부 pthread
./sim --mode hybrid --processes 2 --threads 2 --ipc shm \
  --trials 1000000 --steps 50 --affinity on

# queue 기반 pipeline
./sim --mode pipeline --threads 4 --schedule queue --merge final \
  --trials 1000000 --steps 50 --affinity on
```

## 4. 최종 Scaling 실험 재현

실제 Monte Carlo 총 작업량을 고정하고 worker 수를 변경합니다. 기본값은 오래 실행될 수 있습니다.

```sh
TRIALS=120000000 STEPS=50 REPEATS=5 scripts/run_real_scaling.sh

RAW_PATH=results/csv/real_scaling/real_scaling_raw.csv \
SUMMARY_PATH=results/csv/real_scaling/real_scaling_summary.csv \
python3 scripts/analyze_real_scaling.py
```

빠른 재현 확인:

```sh
TRIALS=100000 STEPS=30 REPEATS=2 scripts/run_real_scaling.sh
python3 scripts/analyze_real_scaling.py
```

생성 파일:

```text
results/csv/real_scaling/real_scaling_raw.csv
results/csv/real_scaling/real_scaling_summary.csv
```

## 5. Core별 Utilization 측정

`mpstat`와 `/usr/bin/time -v`를 사용해 실제 Monte Carlo 실행의 CPU 활용률을 기록합니다.

```sh
TRIALS=120000000 STEPS=50 scripts/run_real_utilization.sh

RAW_PATH=results/csv/real_scaling/real_scaling_raw.csv \
UTIL_DIR=results/csv/real_utilization \
UTIL_SUMMARY_PATH=results/csv/real_utilization/real_utilization_summary.csv \
python3 scripts/analyze_real_scaling.py
```

`mpstat_*.txt`에는 core별 utilization이, `time_*.txt`에는 전체 CPU%와 메모리 사용량이 기록됩니다.

## 6. 포함된 최종 결과

최종 실제 Monte Carlo 측정 결과:

```text
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_raw.csv
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_summary.csv
results/csv/real_montecarlo_utilization_2026_06_06/real_utilization_summary.csv
```

대표 4-worker 구조는 speedup `3.698~3.759x`, efficiency `92.5~94.0%`, worker당 평균 utilization `99.5~99.8%`입니다. `mpstat` 검증에서 affinity 대상 core의 최소 활용률은 `99.7%`, 최대 core 간 표준편차는 `0.16`이었습니다.

이 값은 Docker Desktop Ubuntu 22.04 Linux VM에서 측정했습니다. 같은 환경 내 상대 성능과 scaling 분석에 사용하며 native Linux 절대 성능으로 해석하지 않습니다.
