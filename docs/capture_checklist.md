# 중간 발표 캡처 체크리스트

이 문서는 14장 발표자료에 들어갈 캡처를 **무엇을**, **어떤 명령으로**, **어느 슬라이드에**, **어떤 멘트와 함께** 넣을지 정리한 실전 체크리스트입니다.

발표자료에서는 `MVP`라는 표현을 쓰지 않습니다. 대신 다음 표현을 사용합니다.

- 중간발표 구현 범위
- 현재 구현 범위
- 최종 확장 예정 범위

## 0. 전체 캡처 전략

캡처는 5묶음으로 준비합니다.

1. 코드 구조 캡처
2. 실행/빌드 캡처
3. 실험 결과 CSV 캡처
4. Docker/Linux 캡처
5. CPU utilization 캡처

추천 파일명:

```text
captures/
01_help_cli.png
02_project_structure.png
03_simulation_code.png
04_thread_partition.png
05_pthread_create_join.png
06_sync_modes.png
07_reduce_merge.png
08_make_build.png
09_make_test.png
10_run_midterm_script.png
11_thread_scaling_csv.png
12_nosync_valid0.png
13_sync_compare.png
14_problem_cause_solution.png
15_final_extension_plan.png
16_docker_build.png
17_docker_run_experiment.png
18_cpu_pidstat.png
```

캡처 우선순위:

1. `./sim --help`
2. `src/simulation.c`의 `run_trial` loop
3. `src/thread_mode.c`의 `pthread_create`, `pthread_join`
4. `src/thread_mode.c`의 `nosync / mutex / reduce`
5. `make test`
6. `scripts/run_midterm.sh`
7. `results/csv/midterm_results.csv`
8. `nosync valid=0` 단독 실행 결과
9. Docker build 또는 Docker 내부 실행
10. `pidstat` 또는 `top`

## 1. Slide 1: 프로젝트 제목 및 핵심 정의

슬라이드 핵심:

```text
차량 시뮬레이터가 아니라 CPU-bound 병렬처리 실험이다.
```

캡처:

- 필수 캡처 없음
- 제목, 핵심 키워드, 한 문장 정의 중심

넣을 키워드:

- CPU-bound workload
- pthread-based parallel execution
- race condition
- mutex synchronization
- local reduce optimization
- final extension: child process / IPC / hybrid

발표 멘트:

```text
저희 프로젝트는 차량 시뮬레이터 자체를 만드는 것이 아니라,
운영체제의 병렬처리와 synchronization 문제를 실험하기 위한
CPU-bound 반복 계산 시스템입니다.
```

리스크 대응:

- 차량 시뮬레이션 발표처럼 보이지 않도록 첫 문장에서 OS 실험임을 명시합니다.

## 2. Slide 2: 프로젝트 목표

슬라이드 핵심:

```text
동일한 CPU-bound 작업을 실행 구조와 동기화 방식에 따라 비교한다.
```

캡처:

- 필수 캡처 없음
- 목표 표 중심

넣을 표:

| 목표 | 내용 |
|---|---|
| 병렬처리 성능 분석 | Sequential과 Thread 실행시간 비교 |
| CPU-bound 작업 구성 | Monte Carlo trial 대량 반복 |
| Synchronization 검증 | nosync / mutex / local reduce 비교 |
| 결과 정확성 검증 | total_trials, histogram_sum, checksum, valid |
| 최종 확장 | child process, IPC/shared memory, hybrid |

발표 멘트:

```text
현재 구현 범위에서는 thread 기반 병렬처리와 synchronization 문제를 먼저 검증하고,
최종 발표까지 child process와 hybrid 구조로 확장할 계획입니다.
```

## 3. Slide 3: 시스템 전체 구조

목적:

- 입력이 파일이 아니라 CLI 파라미터와 seed라는 점을 보여줍니다.
- I/O 병목이 아니라 계산량과 synchronization 방식이 실험 중심임을 방어합니다.

실행:

```sh
./sim --help
```

캡처할 부분:

```text
--mode
--trials
--steps
--threads
--sync
--seed
```

파일명:

```text
01_help_cli.png
```

발표 멘트:

```text
입력은 이미지 파일이나 외부 데이터가 아니라 CLI 파라미터와 seed입니다.
그래서 I/O 병목이 아니라 계산량과 synchronization 방식이 실험의 중심입니다.
```

## 4. Slide 4: Monte Carlo 시뮬레이션 모델

목적:

- CPU-bound 반복 계산 구조를 보여줍니다.
- 차량 모델 자체가 아니라 독립 trial 기반 workload임을 보여줍니다.

파일:

```text
src/simulation.c
```

추천 명령:

```sh
nl -ba src/simulation.c | sed -n '120,150p'
```

캡처할 부분:

```text
RiskLevel run_trial(...)
for (int step = 0; step < time_steps; ++step)
advance_one_step(...)
compute_ttc(...)
collision 판단
classify_risk(...)
```

파일명:

```text
03_simulation_code.png
```

발표 멘트:

```text
각 trial 내부에서 time-step 반복 계산을 수행합니다.
파일을 읽는 것이 아니라 차량 상태를 갱신하고 TTC를 계산하는 CPU-bound loop입니다.
```

## 5. Slide 5: 현재 구현 범위와 최종 확장 범위

목적:

- 실제 구현 산출물이 있다는 것을 보여줍니다.
- child process 미구현 리스크를 “최종 확장 예정 범위”로 방어합니다.

추천 명령:

```sh
find . -maxdepth 2 -type f | sort
```

너무 길면 아래 항목이 보이도록 캡처합니다.

```text
./Dockerfile
./Makefile
./README.md
./docs/design.md
./docs/midterm_presentation.md
./include/config.h
./include/result.h
./include/thread_mode.h
./scripts/run_midterm.sh
./src/main.c
./src/simulation.c
./src/sequential.c
./src/thread_mode.c
```

파일명:

```text
02_project_structure.png
```

발표 멘트:

```text
현재 구현 범위는 sequential, pthread thread mode, synchronization 비교,
실험 자동화, Docker 실행 환경까지입니다.
child process와 hybrid는 최종 확장 범위로 분리했습니다.
```

## 6. Slide 6: Thread 기반 병렬처리 구조

목적:

- pthread 기반 병렬처리를 보여줍니다.
- 병렬화 단위가 Monte Carlo trial range임을 보여줍니다.

### 캡처 1: partition 코드

```sh
nl -ba src/thread_mode.c | sed -n '60,90p'
```

캡처할 부분:

```text
partition_work(...)
start_idx
end_idx
```

파일명:

```text
04_thread_partition.png
```

### 캡처 2: pthread_create / pthread_join

```sh
nl -ba src/thread_mode.c | sed -n '103,138p'
```

캡처할 부분:

```text
pthread_create(...)
pthread_join(...)
```

파일명:

```text
05_pthread_create_join.png
```

발표 멘트:

```text
전체 trial 수를 thread 수로 나누고, 각 thread가 자기 range를 계산합니다.
병렬화 단위는 차량이 아니라 Monte Carlo trial range입니다.
```

## 7. Slide 7: Synchronization 실험 설계

목적:

- nosync / mutex / reduce의 차이를 코드와 구조로 보여줍니다.
- race condition과 해결 전략을 OS 개념 중심으로 설명합니다.

### 캡처 1: sync mode별 갱신 방식

```sh
nl -ba src/thread_mode.c | sed -n '20,45p'
```

캡처할 부분:

```text
SYNC_REDUCE
result_add_trial(&arg->local_result, ...)
SYNC_MUTEX
pthread_mutex_lock(...)
pthread_mutex_unlock(...)
SYNC_NOSYNC
result_add_trial(arg->global_result, ...)
```

파일명:

```text
06_sync_modes.png
```

### 캡처 2: local reduce merge

```sh
nl -ba src/thread_mode.c | sed -n '141,148p'
```

파일명:

```text
07_reduce_merge.png
```

발표 멘트:

```text
nosync는 global result를 바로 갱신합니다.
mutex는 매 trial마다 lock/unlock을 수행합니다.
reduce는 thread-local result에 먼저 저장하고, join 이후 merge합니다.
```

## 8. Slide 8: 프로그램 구성 및 실행 환경

목적:

- 실제 C/pthread/Makefile 기반 구현을 증명합니다.
- Docker Linux 재현성을 보여줍니다.

### 캡처 1: build

```sh
make clean
make
```

캡처할 부분:

```text
gcc
-std=c11
-O2
-Wall
-Wextra
-pthread
-Iinclude
-o sim
```

파일명:

```text
08_make_build.png
```

### 캡처 2: smoke test

```sh
make test
```

캡처할 부분:

```text
./sim --mode seq ...
seq,reduce,...
./sim --mode thread ...
thread,reduce,...
```

파일명:

```text
09_make_test.png
```

발표 멘트:

```text
C와 pthread 기반으로 구현했고 Makefile로 동일한 빌드와 테스트를 재현할 수 있습니다.
```

## 9. Slide 9: 실험 조건 및 Test Vector

목적:

- test vector가 자동화되어 있다는 것을 보여줍니다.
- thread 수, sync 방식, 계산 강도를 독립 변수로 바꿨다는 점을 보여줍니다.

실행:

```sh
nl -ba scripts/run_midterm.sh | sed -n '1,50p'
```

캡처할 부분:

```text
TRIALS=${TRIALS:-1000000}
STEPS=${STEPS:-50}
for threads in 1 2 4 8
sync nosync / mutex / reduce
steps 10 50 100
```

파일명:

```text
10_run_midterm_script.png
```

발표 멘트:

```text
thread 수, synchronization 방식, steps 값을 바꿔
동일 workload를 여러 조건에서 비교하도록 자동화했습니다.
```

## 10. Slide 10: Thread 병렬처리 성능

목적:

- thread 수 증가에 따른 time과 speedup을 보여줍니다.

### 실험 실행

```sh
TRIALS=1000000 STEPS=50 scripts/run_midterm.sh
```

### 결과 보기

```sh
cat results/csv/midterm_results.csv
```

캡처할 행:

```text
seq,reduce,4,1000000,50,...
thread,reduce,1,1000000,50,...
thread,reduce,2,1000000,50,...
thread,reduce,4,1000000,50,...
thread,reduce,8,1000000,50,...
```

보기 좋게 출력:

```sh
awk -F, 'NR==1 || ($1=="seq") || ($1=="thread" && $2=="reduce" && $5==50 && NR<=6) {print}' results/csv/midterm_results.csv
```

파일명:

```text
11_thread_scaling_csv.png
```

발표 멘트:

```text
thread 수가 증가할수록 실행시간이 감소하는 경향을 보입니다.
다만 이상적 T/N과 완전히 같지는 않고,
thread overhead와 scheduling overhead가 영향을 줍니다.
```

그래프:

- threads vs time_sec
- threads vs speedup

## 11. Slide 11: Synchronization 정확성 비교

목적:

- synchronization 필요성을 결과값으로 증명합니다.
- `nosync valid=0` 캡처를 가장 크게 배치합니다.

### nosync 단독 실행

```sh
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync nosync --seed 42
```

캡처할 부분:

```text
thread,nosync,4,1000000,50,...
total_trials 값이 1000000이 아님
hist_sum 값이 1000000이 아님
valid=0
```

파일명:

```text
12_nosync_valid0.png
```

### mutex / reduce 비교

```sh
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync mutex --seed 42
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync reduce --seed 42
```

캡처할 부분:

```text
mutex valid=1
reduce valid=1
```

파일명:

```text
13_sync_compare.png
```

발표 멘트:

```text
nosync에서는 여러 thread가 공유 counter와 histogram을 동시에 갱신하면서 값이 유실됩니다.
그래서 hist_sum이 trials와 일치하지 않고 valid=0이 됩니다.
mutex와 reduce에서는 valid=1로 정확성이 회복됩니다.
```

## 12. Slide 12: 문제 -> 원인 -> 해결 분석

목적:

- 결과가 단순 수치 나열이 아니라 OS 개념 분석으로 이어진다는 것을 보여줍니다.

추천 구성:

```text
왼쪽: nosync valid=0 CSV 캡처
오른쪽: src/thread_mode.c의 mutex/reduce 코드 캡처
```

캡처 명령:

```sh
nl -ba src/thread_mode.c | sed -n '20,45p'
```

파일명:

```text
14_problem_cause_solution.png
```

발표 멘트:

```text
문제는 nosync에서 결과가 깨지는 것입니다.
원인은 shared result를 여러 thread가 동시에 갱신하는 race condition입니다.
해결은 mutex 또는 local reduce이며, reduce는 lock overhead까지 줄입니다.
```

## 13. Slide 13: 최종 확장 계획

목적:

- child process 미구현 리스크를 방어합니다.
- process / IPC / hybrid 확장 구조를 구체적으로 제시합니다.

추천 명령:

```sh
sed -n '100,115p' docs/design.md
rg -n "최종|process|hybrid|dynamic" README.md docs/design.md
```

캡처할 부분:

```text
process_mode
fork
shared memory
pipe
hybrid
dynamic task queue
CPU utilization
memory footprint
```

파일명:

```text
15_final_extension_plan.png
```

발표 멘트:

```text
최종 발표에서는 process가 큰 simulation group을 맡고,
각 process 내부에서 thread가 batch를 병렬 계산하는 hybrid 구조로 확장할 계획입니다.
```

## 14. Slide 14: 역할분담 및 결론

캡처:

- 필수 캡처 없음
- 역할분담 표와 결론 중심

점검:

- 팀원 이름이 실제 팀원과 일치하는지 확인합니다.
- 수행일지의 역할과 발표자료 역할이 일치해야 합니다.
- 보고서 기여도 총합은 100%가 되도록 정리합니다.

발표 멘트:

```text
현재 구현 범위에서는 sequential, pthread, nosync/mutex/reduce 비교를 구현했습니다.
nosync 결과를 통해 synchronization 필요성을 확인했고,
mutex와 reduce 비교를 통해 정확성과 성능 trade-off를 분석했습니다.
최종 발표까지 child process, IPC, hybrid 구조로 확장하겠습니다.
```

## 15. Docker/Linux 캡처

Docker 빌드:

```sh
docker build -t os-montecarlo-risk .
```

캡처할 부분:

```text
FROM ubuntu:22.04
build-essential
make
gcc
RUN make
```

파일명:

```text
16_docker_build.png
```

Docker 실행:

```sh
docker run --rm -it os-montecarlo-risk
```

컨테이너 안:

```sh
make clean
make
TRIALS=1000000 STEPS=50 scripts/run_midterm.sh
```

파일명:

```text
17_docker_run_experiment.png
```

발표 멘트:

```text
macOS에서도 개발 가능하지만,
최종 성능 측정은 Docker Linux에서 수행하도록 환경을 준비했습니다.
```

## 16. CPU utilization 캡처

이 캡처는 발표자료에 꼭 넣지 않더라도 교수님 질문 대비용으로 준비합니다.

터미널 1:

```sh
./sim --mode thread --threads 4 --trials 10000000 --steps 100 --sync reduce --seed 42
```

터미널 2:

```sh
pidstat -u -r -C sim 1
```

대체:

```sh
top
```

캡처할 부분:

```text
sim 프로세스 CPU 사용률
%CPU
threads 실행 중 CPU가 올라가는 장면
```

너무 빨리 끝나면 workload를 키웁니다.

```sh
./sim --mode thread --threads 4 --trials 50000000 --steps 100 --sync reduce --seed 42
```

파일명:

```text
18_cpu_pidstat.png
```

발표 멘트:

```text
CPU utilization은 최종 발표에서 더 정량적으로 수집할 예정이며,
현재는 thread 실행 중 CPU 사용률이 올라가는 것을 확인하는 수준으로 캡처했습니다.
```

## 17. 발표자료 배치 요약

추천 배치:

```text
Slide 3: ./sim --help 캡처 작게
Slide 4: simulation.c run_trial loop 캡처
Slide 6: pthread_create / pthread_join 캡처
Slide 7: nosync / mutex / reduce 코드 캡처
Slide 8: make 또는 Docker 캡처
Slide 9: run_midterm.sh 캡처
Slide 10: thread scaling CSV + 그래프
Slide 11: nosync valid=0 캡처 크게
Slide 12: 문제-원인-해결 표 + sync 코드 일부
Slide 13: 확장 계획 구조도
```

가장 중요한 캡처:

```text
Slide 11의 nosync valid=0
```

이 캡처가 있어야 “왜 synchronization이 필요한가?”를 코드가 아니라 결과로 보여줄 수 있습니다.

## 18. 발표자료에서 제외하거나 구두로만 처리할 내용

| 내용 | 처리 방식 |
|---|---|
| 기존 이미지 블러링 주제의 자세한 문제점 | Slide 1~2에서 짧게 구두 설명 |
| 프로젝트 가이드 요구사항 대응표 전체 | 직접 표로 넣지 않고 전체 구성에 녹임 |
| 교수님 피드백 전문 | 슬라이드에 넣지 않음 |
| 캐시 재사용 관련 긴 설명 | 질문 나오면 답변 |
| 이미지 필터 대안 분석 | 발표자료 제외 |

