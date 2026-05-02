# 2026 OS Project: Monte Carlo Car-Following Risk Simulation

숭실대학교 전자정보공학부 운영체제 과목 프로젝트를 위한 CPU-bound 병렬처리 실험 프로그램입니다.

주제는 **Child Process와 Multithread를 활용한 몬테카를로 기반 차량 추종 위험 시뮬레이션의 병렬처리 및 Synchronization 성능 분석**입니다.

현재 구현은 **중간발표 구현 범위**에 맞춰 `sequential baseline`, `pthread multi-thread`, `nosync/mutex/local reduce synchronization 비교`, `CSV 실험 자동화`, `Docker Linux 실행 환경`을 제공합니다. 최종 발표에서는 child process, process/thread hybrid, dynamic task queue, CPU/memory 분석으로 확장합니다.

## 프로젝트 목적

이 프로젝트는 실제 차량 시뮬레이터나 자율주행 시스템을 만드는 것이 아닙니다.

운영체제 수업의 핵심 주제인 다음 내용을 정량적으로 보여주기 위한 간단한 계산 모델입니다.

- CPU-bound workload 병렬화
- sequential vs pthread 실행시간 비교
- thread 수 증가에 따른 speedup 관찰
- shared data update에서 발생하는 race condition
- mutex synchronization의 정확성과 overhead
- local reduce 방식의 성능 개선 효과

각 trial은 하나의 차량 추종 상황입니다. ego 차량이 앞차를 따라가고, 앞차는 감속하며, ego 차량은 reaction time 이후 감속합니다. 매 time-step마다 위치, 속도, 상대거리, TTC를 계산하고, 충돌 여부와 최소 TTC를 기준으로 risk level을 분류합니다.

trial들은 서로 독립적이므로 thread 병렬화에 적합합니다. 파일 입력은 사용하지 않고 deterministic seed 기반 난수로 입력을 생성해 I/O 병목을 제거했습니다.

## 현재 구현 범위

| 항목 | 상태 | 설명 |
|---|---:|---|
| Sequential baseline | 완료 | 단일 thread에서 모든 trial 순차 실행 |
| Pthread multi-thread | 완료 | static partition으로 trial 범위를 thread에 분배 |
| `nosync` | 완료 | lock 없이 shared result 갱신, race condition 유도 |
| `mutex` | 완료 | 매 trial마다 mutex로 shared result 보호 |
| `reduce` | 완료 | thread-local result를 만든 뒤 main thread가 merge |
| CSV 실험 자동화 | 완료 | `scripts/run_midterm.sh` |
| Docker Linux 환경 | 완료 | Ubuntu 22.04 기반 |
| Child process mode | 최종 TODO | `fork`, shared memory/pipe aggregation 예정 |
| Hybrid mode | 최종 TODO | process + thread 조합 예정 |
| Dynamic task queue | 최종 TODO | load balancing 비교 예정 |
| CPU/memory 분석 | 최종 TODO | `pidstat`, `/usr/bin/time`, `getrusage` 확장 예정 |

## 디렉토리 구조

```text
.
├── include/              # public headers
├── src/                  # C source files
├── scripts/              # experiment scripts
├── results/
│   ├── raw/              # raw CSV output
│   └── csv/              # processed CSV output
├── docs/
│   ├── design.md         # system design
│   └── midterm_plan.md   # midterm plan
├── Makefile
├── Dockerfile
└── docker-compose.yml
```

## 중간 발표자료

중간 발표용 자료는 Markdown slide deck 형태로 정리했습니다.

```text
docs/midterm_presentation.md
docs/capture_checklist.md
docs/presentation_guide.md
docs/linux_capture_guide.md
docs/midterm_plan.md
```

- `docs/midterm_presentation.md`: 발표 슬라이드 원고
- `docs/capture_checklist.md`: 슬라이드별 캡처 명령, 파일명, 발표 멘트, 리스크 대응
- `docs/presentation_guide.md`: 슬라이드별 발표 멘트와 예상 질문 답변
- `docs/linux_capture_guide.md`: Docker Ubuntu Linux 기준 캡처 실행 절차
- `docs/midterm_plan.md`: 발표 구성과 실험 설계 요약

Markdown slide deck은 Marp 같은 도구로 PDF/PPTX로 변환할 수 있습니다. 변환 도구가 없어도 내용을 그대로 발표자료에 옮길 수 있도록 슬라이드 단위로 구성했습니다.

## 실행 모델

프로그램 실행 단위는 `./sim`입니다.

예:

```sh
./sim --mode thread --threads 4 --trials 10000 --steps 50 --sync reduce --seed 42
```

의미:

| 옵션 | 의미 |
|---|---|
| `--mode thread` | pthread 기반 병렬 실행 |
| `--threads 4` | worker thread 4개 생성 |
| `--trials 10000` | 독립적인 차량 추종 상황 10000개 계산 |
| `--steps 50` | 각 trial마다 50 time-step 계산 |
| `--sync reduce` | thread-local result를 merge하는 방식 사용 |
| `--seed 42` | 재현 가능한 deterministic 난수 사용 |

각 trial은 다음 과정을 수행합니다.

```text
trial seed 생성
  -> ego/front 차량 초기 조건 생성
  -> time-step 반복
  -> relative distance, relative speed, TTC 계산
  -> collision 또는 risk level 분류
  -> Result histogram에 누적
```

위험도 bucket은 다음 5개입니다.

```text
RISK_SAFE
RISK_LOW
RISK_MEDIUM
RISK_HIGH
RISK_COLLISION
```

## Synchronization 실험

### 1. `nosync`

여러 thread가 하나의 shared `Result`를 lock 없이 갱신합니다.

```text
global_result.total_trials += 1
global_result.histogram[risk] += 1
global_result.collision_count += collided
```

이 연산들은 atomic하지 않으므로 lost update가 발생할 수 있습니다. 따라서 `hist_sum != trials`, `valid=0`이 나올 수 있습니다. 이 모드는 빠를 수 있지만 결과를 신뢰할 수 없습니다.

### 2. `mutex`

trial 하나가 끝날 때마다 mutex로 shared result 갱신을 보호합니다.

```text
pthread_mutex_lock
  global_result update
pthread_mutex_unlock
```

정확하지만 모든 thread가 같은 critical section에 자주 진입하므로 lock contention과 synchronization overhead가 발생합니다.

### 3. `reduce`

각 thread가 자기 `local_result`에만 결과를 누적합니다. 모든 worker가 종료된 뒤 main thread가 local result들을 merge합니다.

```text
thread 0 -> local_result[0]
thread 1 -> local_result[1]
thread 2 -> local_result[2]
thread 3 -> local_result[3]

after pthread_join:
global_result = merge(local_result[0..N-1])
```

hot loop에서 shared write와 lock을 제거하므로 정확성과 성능을 함께 기대할 수 있습니다.

## Build

```sh
make
make test
make clean
```

빌드 옵션:

```text
gcc -std=c11 -O2 -Wall -Wextra -pthread -Iinclude
```

실행 파일:

```text
./sim
```

## CLI

지원 옵션:

```text
--mode <seq|thread>
--trials <int>
--steps <int>
--threads <int>
--sync <nosync|mutex|reduce>
--seed <int>
--verbose
--help
```

기본값:

```text
mode    = seq
trials  = 100000
steps   = 50
threads = 4
sync    = reduce
seed    = 42
```

실행 예시:

```sh
./sim --mode seq --trials 1000000 --steps 50 --seed 42
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync reduce --seed 42
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync mutex --seed 42
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync nosync --seed 42
```

## CSV 출력

기본 출력은 자동화하기 쉬운 CSV 한 줄입니다.

```text
mode,sync,threads,trials,steps,time_sec,speedup,total_trials,collision_count,hist_sum,checksum,valid
```

| 필드 | 설명 |
|---|---|
| `mode` | `seq` 또는 `thread` |
| `sync` | `nosync`, `mutex`, `reduce` |
| `threads` | thread 개수 |
| `trials` | 실행한 trial 수 |
| `steps` | trial당 time-step 수 |
| `time_sec` | wall-clock 실행시간 |
| `speedup` | sequential time / current time |
| `total_trials` | 집계된 trial 수 |
| `collision_count` | 충돌 발생 trial 수 |
| `hist_sum` | risk histogram 합 |
| `checksum` | 결과 비교용 checksum |
| `valid` | `total_trials == trials && hist_sum == trials`이면 1 |

단일 `./sim` 실행에서는 `speedup=1.0` placeholder가 출력됩니다. `scripts/run_midterm.sh`는 sequential 첫 실행 시간을 기준으로 speedup을 다시 계산해 최종 CSV에 저장합니다.

## 실험 자동화

권한 부여:

```sh
chmod +x scripts/run_midterm.sh scripts/clean_results.sh
```

빠른 확인:

```sh
TRIALS=10000 scripts/run_midterm.sh
```

중간 발표용 기본 실험:

```sh
scripts/run_midterm.sh
```

결과 파일:

```text
results/raw/midterm_raw.csv
results/csv/midterm_results.csv
```

자동화 스크립트는 다음 실험을 수행합니다.

| 실험 | 목적 |
|---|---|
| sequential baseline | 기준 실행시간 측정 |
| reduce, threads 1/2/4/8 | thread 수 증가에 따른 speedup 분석 |
| nosync/mutex/reduce | synchronization 방식 비교 |
| steps 10/50/100 | CPU-bound 계산 강도 변화 분석 |

## 현재 실험 결과 예시

아래 값은 로컬 환경에서 `TRIALS=1000000 STEPS=50 scripts/run_midterm.sh`로 얻은 예시입니다. 실제 발표용 최종 수치는 Docker Linux에서 다시 측정하는 것을 권장합니다.

| mode | sync | threads | trials | steps | time_sec | speedup | hist_sum | valid |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| seq | reduce | 4 | 1000000 | 50 | 0.114392 | 1.000000 | 1000000 | 1 |
| thread | reduce | 1 | 1000000 | 50 | 0.102064 | 1.120787 | 1000000 | 1 |
| thread | reduce | 2 | 1000000 | 50 | 0.053912 | 2.121828 | 1000000 | 1 |
| thread | reduce | 4 | 1000000 | 50 | 0.032194 | 3.553209 | 1000000 | 1 |
| thread | reduce | 8 | 1000000 | 50 | 0.020967 | 5.455812 | 1000000 | 1 |
| thread | nosync | 4 | 1000000 | 50 | 0.036567 | 3.128285 | 800794 | 0 |
| thread | mutex | 4 | 1000000 | 50 | 0.136491 | 0.838092 | 1000000 | 1 |
| thread | reduce | 4 | 1000000 | 100 | 0.048148 | 2.375841 | 1000000 | 1 |

관찰:

- `reduce`는 thread 수가 증가할수록 실행시간이 감소하는 경향을 보입니다.
- `nosync`는 빠르지만 `hist_sum=800794`, `valid=0`으로 결과가 깨졌습니다.
- `mutex`는 정확하지만 매 trial마다 lock/unlock이 들어가 `reduce`보다 느립니다.
- checksum은 `seq`, `mutex`, `reduce`에서 동일하게 유지되어 deterministic seed와 결과 재현성이 확인됩니다.

## Docker Linux 실행

macOS에서도 컴파일되지만 최종 성능 측정은 Linux 기준이 더 적합합니다.

이미지 빌드:

```sh
docker build -t os-montecarlo-risk .
```

컨테이너 실행:

```sh
docker run --rm -it os-montecarlo-risk
```

컨테이너 내부:

```sh
make clean
make
TRIALS=10000 scripts/run_midterm.sh
```

호스트 디렉토리에 결과를 남기려면:

```sh
docker run --rm -it -v "$PWD":/workspace -w /workspace os-montecarlo-risk
```

Docker Compose:

```sh
docker compose build
docker compose run --rm os-sim
```

컨테이너 안에서:

```sh
make clean
make
TRIALS=10000 scripts/run_midterm.sh
```

CPU 관찰 도구:

```sh
top
pidstat
/usr/bin/time
htop
```

Dockerfile에는 `procps`, `sysstat`, `htop`, `time`을 포함했습니다.

## 프로젝트 가이드와의 연결

PDF 프로젝트 가이드는 child process, multiple threads, synchronization을 활용하고 다양한 조건에서 성능을 분석할 것을 요구합니다.

현재 구현 범위는 중간 발표 범위에 맞춰 다음 항목을 충족합니다.

- 같은 작업을 sequential과 pthread로 실행해 실행시간 비교
- 1/2/4/8 thread 조건으로 core 활용 효과 관찰
- synchronization이 필요한 shared result update 상황 정의
- `nosync`로 race condition 유도
- `mutex`로 race condition 해결
- `reduce`로 synchronization overhead를 줄이는 구조 개선 제시
- CSV 기반 정량 결과와 checksum/valid 기반 결과 검증 제공

최종 발표까지 다음 항목을 추가해야 합니다.

- child process 기반 병렬처리 구현
- child process vs thread 비교
- single child process vs single thread 비교
- multi child process vs multi thread 비교
- process/thread hybrid mode 구현
- CPU utilization과 memory usage 분석
- 팀원별 역할분담, 수행일지, 기여도 정리

## AI 사용 기록

프로젝트 구현 및 README 정리에 OpenAI Codex 기반 AI agent를 사용했습니다.

사용 목적:

- C/pthread 기반 현재 구현 코드 작성
- Makefile, Dockerfile, 실험 스크립트 작성
- 프로젝트 가이드 PDF 기준 적합성 점검
- README 및 문서 정리

보고서 제출 시에는 프로젝트 가이드 요구사항에 맞춰 사용한 AI 도구, 사용 범위, 주요 입력 prompt를 별도 항목으로 정리해야 합니다.

## Troubleshooting

`Permission denied`:

```sh
chmod +x scripts/run_midterm.sh scripts/clean_results.sh
```

`make: command not found`:

```sh
apt-get update
apt-get install -y build-essential make
```

Docker 결과 파일 권한 문제:

```sh
sudo chown -R "$USER" results
```

8 threads에서 성능이 기대보다 낮은 경우:

- physical core 수보다 thread가 많을 수 있음
- Docker Desktop에 할당된 CPU core 수가 제한적일 수 있음
- context switching과 scheduling overhead가 증가할 수 있음
- workload가 너무 작으면 thread 생성 비용이 계산 이득보다 클 수 있음

`nosync` 결과가 틀리는 경우:

정상입니다. race condition을 보여주기 위한 실험 모드입니다. 발표에서는 `valid=0`, `hist_sum < trials`를 synchronization 필요성의 근거로 설명하면 됩니다.
