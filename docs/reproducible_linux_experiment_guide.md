# 재현 가능한 Linux 실험 가이드

최종 성능 실험은 실제 차량 추종 Monte Carlo simulation을 고정된 총 작업량으로 실행합니다. 작은 workload는 기능 검증에만 사용하고, 성능 비교에는 각 실행이 수 초 이상 걸리는 조건을 사용합니다.

## 1. 권장 조건

```text
trials = 120000000
steps = 50
repeats = 5 이상
pre_work = 0
post_work = 0
profile = default
affinity = on
```

`pre_work`, `post_work`, dummy profile을 사용하지 않는 이유는 최종 핵심 결과가 실제 Monte Carlo 계산 구조 자체의 scaling을 보여줘야 하기 때문입니다.

## 2. Docker Desktop Ubuntu

```sh
docker build -t os-montecarlo-risk .

docker run --rm \
  -v "$PWD/results:/workspace/results" \
  os-montecarlo-risk \
  sh -lc 'TRIALS=120000000 STEPS=50 REPEATS=5 scripts/run_real_scaling.sh && python3 scripts/analyze_real_scaling.py'
```

Docker Desktop은 Linux VM 위에서 실행됩니다. 결과는 동일한 Docker 환경 내 상대 성능과 scaling 비교에 사용하며 순수 물리 Linux 절대 성능으로 표현하지 않습니다.

## 3. Windows WSL2 Ubuntu

PowerShell 관리자 권한:

```powershell
wsl --install -d Ubuntu-22.04
```

Ubuntu 터미널:

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat

make clean
make
make test

TRIALS=120000000 STEPS=50 REPEATS=5 scripts/run_real_scaling.sh
python3 scripts/analyze_real_scaling.py
TRIALS=120000000 STEPS=50 scripts/run_real_utilization.sh
```

WSL2도 VM 기반이므로 Windows background load와 할당 CPU 수를 기록합니다.

## 4. Native Linux

Ubuntu/Debian:

```sh
sudo apt update
sudo apt install -y build-essential make python3 time sysstat

make clean
make
make test

TRIALS=120000000 STEPS=50 REPEATS=5 scripts/run_real_scaling.sh
python3 scripts/analyze_real_scaling.py
TRIALS=120000000 STEPS=50 scripts/run_real_utilization.sh
```

Native Linux 결과에는 CPU 모델, physical/logical core 수, CPU governor, background load를 함께 기록합니다.

## 5. 생성 파일

```text
results/csv/real_scaling/real_scaling_raw.csv
results/csv/real_scaling/real_scaling_summary.csv
results/csv/real_utilization/sim_*.csv
results/csv/real_utilization/time_*.txt
results/csv/real_utilization/mpstat_*.txt
```

## 6. 결과 신뢰도 확인

1. 모든 정상 mode의 `valid=1`을 확인합니다.
2. 같은 workload의 checksum이 하나로 유지되는지 확인합니다.
3. 평균, 중앙값, 최소, 표준편차를 함께 봅니다.
4. CPU%를 worker 수로 나눠 worker당 평균 utilization을 계산합니다.
5. `mpstat`로 affinity 대상 core들이 균일하게 사용되는지 확인합니다.
6. VM 기반 결과와 native Linux 결과를 구분합니다.

## 7. 포함된 Docker Desktop 결과

```text
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_raw.csv
results/csv/real_montecarlo_scaling_2026_06_06_5repeat/real_scaling_summary.csv
results/csv/real_montecarlo_utilization_2026_06_06/real_utilization_summary.csv
```

현재 포함된 기준 결과의 조건은 Docker Desktop Ubuntu 22.04 Linux VM, `trials=120,000,000`, `steps=50`, 5회 반복입니다. 모든 case가 `valid=1`, 동일 checksum을 유지했습니다. 대표 4-worker 구조의 `mpstat` 검증에서는 affinity 대상 core 최소 활용률 `99.7%`, 최대 core 간 표준편차 `0.16`을 확인했습니다.
