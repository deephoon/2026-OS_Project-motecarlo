# Monte Carlo 기반 차량 추종 위험 시뮬레이션

운영체제 프로젝트: CPU-bound 병렬처리 및 Synchronization 성능 분석

이 프로젝트는 실제 차량 시뮬레이터를 만드는 것이 아니다. 차량 추종 상황은 운영체제 개념을 실험하기 위한 CPU-bound 반복 계산 모델로 사용된다. 핵심은 `child process`, `pthread`, `synchronization`, `race condition`, `mutex`, `condition variable`, `task queue`, `pipeline`, `IPC`, `local reduce`, stage별 성능 측정, throughput, process/thread/hybrid 성능 비교를 코드와 실험 결과로 보여주는 것이다.

## 1. 프로젝트 목적

동일한 Monte Carlo 차량 추종 위험 계산을 여러 실행 구조로 수행하고, 정확성과 성능의 trade-off를 분석한다.

| 분석 대상 | 구현 내용 |
| --- | --- |
| Sequential baseline | 단일 스레드 순차 실행 기준 성능 |
| Pthread multithread | static partition 기반 thread 병렬 실행 |
| Synchronization 비교 | `nosync`, `mutex`, `local reduce` 비교 |
| Task queue scheduling | mutex + condition variable 기반 bounded queue |
| Pipeline 구조 | pre-processing, worker, merge, post-processing stage 분리 |
| Interactive merge | worker가 batch 결과를 만들 때마다 aggregator가 병합 |
| Child process | `fork()` 기반 process mode |
| IPC | pipe 기반 child result 전달 |
| Hybrid mode | child process 내부에서 pthread worker 실행 |
| 성능 분석 | stage별 시간, throughput, validation, Amdahl’s Law 분석 지표 |

## 2. 중간발표 이후 반영한 피드백

중간발표 구현은 thread와 synchronization 비교 중심이었다. 최종 구조에서는 교수님 피드백을 반영하여 단순히 병렬 계산만 수행하는 구조가 아니라, 실제 시스템처럼 전처리, 작업 큐, 병렬 worker, 중간 병합, 후처리, IPC 병합이 드러나도록 확장했다.

| 교수님 피드백 | 반영 내용 |
| --- | --- |
| 계산 시작 즉시 병렬화되는 구조가 너무 단순함 | Pre-processing / Task queue / Merge / Post-processing stage 추가 |
| Amdahl’s Law 분석이 가능해야 함 | `T_pre`, `T_compute`, `T_sync`, `T_merge`, `T_post`, `T_total` 출력 |
| mutex만으로 queue empty/full 처리가 부족함 | `pthread_mutex_t` + `pthread_cond_t` 기반 bounded queue 구현 |
| reduce 방식이 너무 단순함 | final reduce 외에 interactive merge와 aggregator thread 추가 |
| child process가 의미 있게 사용되어야 함 | `fork()` 기반 process mode와 pipe IPC 구현 |
| process와 thread 역할이 구분되어야 함 | hybrid mode에서 process는 큰 simulation group, thread는 내부 batch 계산 담당 |
| CPU utilization과 성능 지표가 필요함 | Docker Linux, `pidstat`, `/usr/bin/time -v`, CSV 기반 성능 분석 문서화 |

## 3. 전체 시스템 구조

```text
CLI Config
   |
   v
Pre-processing Stage
   - 전체 trial 수를 TaskBatch 단위로 분할
   - batch id, trial range, seed metadata 생성
   |
   v
Task Queue
   - bounded queue
   - pthread_mutex_t로 queue state 보호
   - pthread_cond_t로 not_empty / not_full wait-signal 처리
   |
   v
Parallel Simulation Workers
   - worker thread가 TaskBatch를 pop
   - Monte Carlo trial 반복 계산
   - PartialResult 생성
   |
   v
Merge Stage
   - final reduce 또는 interactive merge
   |
   v
Post-processing Stage
   - histogram sum 검증
   - checksum 계산
   - collision probability / risk ratio 계산
   |
   v
CSV Output
```

## 4. Simulation Model

각 trial은 하나의 차량 추종 상황을 의미한다.

1. deterministic seed 기반으로 ego/front 차량 초기 상태 생성
2. ego/front 차량 속도, 거리, 반응시간, 감속도 설정
3. time-step마다 위치와 속도 업데이트
4. 상대거리, 상대속도, TTC(Time-To-Collision) 계산
5. collision 또는 risk level 판단
6. histogram, collision count, checksum에 반영

Risk level은 다음과 같다.

| Risk Level | 기준 |
| --- | --- |
| Collision | `relative_distance <= 0` |
| High | `TTC < 1.5` |
| Medium | `TTC < 3.0` |
| Low | `TTC < 5.0` |
| Safe | 그 외 |

trial은 서로 독립적이다. 따라서 thread/process 단위로 나누어 병렬 처리하기 적합하다. 실행 순서가 달라도 같은 trial은 같은 난수를 사용하도록 seed는 trial index 기반으로 생성한다.

```text
trial_seed = base_seed ^ (trial_index * 2654435761u)
```

## 5. 실행 모드

| Mode | CLI | 설명 |
| --- | --- | --- |
| Sequential | `--mode seq` | 단일 thread baseline |
| Thread static | `--mode thread --schedule static` | pthread static partition |
| Pipeline queue | `--mode pipeline --schedule queue` | task queue + worker pool + merge queue |
| Process | `--mode process` | parent가 child process를 fork하고 pipe로 결과 수신 |
| Hybrid | `--mode hybrid` | child process 내부에서 pthread worker 실행 |

## 6. Synchronization 비교

| Sync Mode | 구현 방식 | 목적 |
| --- | --- | --- |
| `nosync` | shared `Result`를 lock 없이 직접 갱신 | race condition 관찰 |
| `mutex` | 매 trial마다 mutex lock/unlock | 정확성 보장, lock contention 확인 |
| `reduce` | worker-local result에 누적 후 merge | hot loop의 shared write 제거 |

`nosync`는 의도적으로 안전하지 않은 모드다. 여러 thread가 동시에 `histogram`, `total_trials`, `collision_count`를 갱신하기 때문에 lost update가 발생할 수 있다. 이 경우 `hist_sum != trials`, `valid=0`이 나올 수 있으며, 이는 synchronization 필요성을 보여주는 실험 결과다.

## 7. Final Reduce와 Interactive Merge

| Merge Mode | 설명 |
| --- | --- |
| `--merge final` | 모든 worker가 끝난 뒤 main thread가 partial result를 한 번에 병합 |
| `--merge interactive` | worker가 batch를 끝낼 때마다 merge queue에 `PartialResult`를 push하고 aggregator thread가 계속 병합 |

interactive merge는 현실적인 producer-consumer 구조를 보여주기 위한 방식이다. 계산 중간에 결과가 계속 병합되므로 pipeline 구조 설명에 적합하다. 단, merge queue synchronization overhead가 추가되므로 항상 더 빠른 구조는 아니며, final reduce와 비교 분석해야 한다.

## 8. Task Queue 설계

`TaskQueue`와 `MergeQueue`는 bounded queue로 구현했다.

사용한 동기화 primitive:

```text
pthread_mutex_t mutex
pthread_cond_t not_empty
pthread_cond_t not_full
```

설계 이유:

- mutex는 queue의 `head`, `tail`, `count`, `closed` 상태를 보호한다.
- condition variable은 queue가 비어 있거나 가득 찬 경우 worker/producer를 sleep 상태로 보낸다.
- semaphore는 사용하지 않았다. queue 상태 보호와 wait/signal 조건 표현이 모두 필요하므로, 이번 구현에서는 mutex + condition variable이 더 직접적이고 설명하기 쉽다.

## 9. Process Mode

Process mode는 `fork()`와 pipe IPC를 사용한다.

```text
[Parent Process]
   |
   +-- fork child 0
   +-- fork child 1
   +-- ...
   |
   +-- read Result from pipe
   +-- waitpid()
   +-- result_merge()

[Child Process]
   |
   +-- assigned trial range 계산
   +-- Result를 pipe에 write
   +-- _exit(0)
```

현재 IPC는 `Result` 구조체를 pipe로 전달한다. 동일한 binary 내부에서 parent/child가 같은 구조체 layout을 사용하므로 프로젝트 실험용으로 충분하다. shared memory IPC는 TODO로 남겨두었다.

## 10. Hybrid Mode

Hybrid mode는 process와 thread의 역할을 분리한다.

| 구성 요소 | 역할 |
| --- | --- |
| Parent process | child process 생성, pipe 결과 수신, 최종 merge |
| Child process | 큰 simulation group 담당 |
| Thread worker | child process 내부 trial range 병렬 계산 |
| Pipe IPC | child result를 parent에 전달 |

즉, process는 큰 작업 단위 분리와 IPC 병합을 보여주고, thread는 process 내부의 세부 계산 병렬화를 담당한다.

## 11. CLI 옵션

```text
--mode <seq|thread|pipeline|process|hybrid>
--schedule <static|queue>
--merge <final|interactive>
--trials <int>
--steps <int>
--threads <int>
--processes <int>
--batch-size <int>
--queue-size <int>
--sync <nosync|mutex|reduce>
--ipc <pipe|shm>
--enable-pipeline <0|1>
--metrics-detail <0|1>
--seed <int>
--verbose
--help
```

기본값:

```text
mode=thread
schedule=static
merge=final
trials=100000
steps=50
threads=4
processes=2
batch_size=1000
queue_size=1024
sync=reduce
ipc=pipe
seed=42
metrics_detail=1
```

## 12. 빌드 방법

```sh
make clean
make
make test
```

컴파일 옵션:

```text
gcc -std=c11 -O2 -Wall -Wextra -pthread -Iinclude
```

## 13. 실행 예시

Sequential baseline:

```sh
./sim --mode seq --trials 10000 --steps 30 --seed 42
```

Thread static reduce:

```sh
./sim --mode thread --threads 4 --trials 10000 --steps 30 --sync reduce --seed 42
```

Race condition 확인:

```sh
./sim --mode thread --threads 4 --trials 10000 --steps 30 --sync nosync --seed 42
```

Pipeline + interactive merge:

```sh
./sim --mode pipeline --schedule queue --merge interactive --threads 4 \
  --trials 10000 --steps 30 --batch-size 1000 --queue-size 1024 --seed 42
```

Process mode:

```sh
./sim --mode process --processes 2 --trials 10000 --steps 30 --ipc pipe --seed 42
```

Hybrid mode:

```sh
./sim --mode hybrid --processes 2 --threads 2 --trials 10000 --steps 30 --ipc pipe --seed 42
```

## 14. 최종 실험 자동화

```sh
chmod +x scripts/run_final.sh
TRIALS=10000 STEPS=30 scripts/run_final.sh
```

결과 파일:

```text
results/csv/final_results.csv
```

최종 발표/보고서용 측정은 Docker Linux에서 더 큰 workload로 수행한다.

```sh
TRIALS=1000000 STEPS=50 scripts/run_final.sh
```

실험 항목:

| 실험 | 목적 |
| --- | --- |
| Sequential | 기준 성능 |
| Thread 1/2/4/8 | thread scaling |
| Pipeline final vs interactive | merge 방식 비교 |
| Batch size 100/1000/10000 | queue granularity 분석 |
| Process 2/4 | child process 성능 |
| Hybrid 2x2 / 2x4 | process + thread 조합 |

## 15. CSV 출력 필드

`final_results.csv`는 다음 필드를 출력한다.

```text
mode,schedule,merge,sync,processes,threads,trials,steps,batch_size,queue_size,
time_total,time_pre,time_compute,time_sync,time_merge,time_post,
speedup,efficiency,sequential_fraction_estimate,compute_ratio,
sync_overhead_ratio,merge_overhead_ratio,throughput_batches_per_sec,
total_trials,collision_count,hist_sum,checksum,valid,notes
```

주요 필드 의미:

| 필드 | 의미 |
| --- | --- |
| `time_total` | 전체 실행 시간 |
| `time_pre` | batch 생성 등 전처리 시간 |
| `time_compute` | worker 계산 구간 시간 |
| `time_sync` | queue wait/lock 등 synchronization 비용 |
| `time_merge` | partial result 병합 시간 |
| `time_post` | validation/checksum 등 후처리 시간 |
| `sequential_fraction_estimate` | Amdahl’s Law 분석용 순차 구간 추정 |
| `compute_ratio` | 전체 시간 중 계산 구간 비율 |
| `sync_overhead_ratio` | 전체 시간 중 동기화 비용 비율 |
| `merge_overhead_ratio` | 전체 시간 중 병합 비용 비율 |
| `throughput_batches_per_sec` | 초당 처리 batch 수 |
| `hist_sum` | histogram 합계 |
| `checksum` | 결과 재현성 확인용 checksum |
| `valid` | `hist_sum == trials` 여부 |

현재 프로그램 단일 실행에서는 `speedup`, `efficiency`를 placeholder로 둔다. 보고서에서는 sequential row를 기준으로 후처리 계산한다.

```text
speedup = T_seq / T_parallel
efficiency = speedup / worker_count
```

Amdahl’s Law 분석용 지표:

```text
sequential_fraction_estimate = (T_pre + T_sync + T_merge + T_post) / T_total
compute_ratio = T_compute / T_total
sync_overhead_ratio = T_sync / T_total
merge_overhead_ratio = T_merge / T_total
throughput = processed_batches / T_total
```

## 16. Docker Linux 실행 방법

최종 측정 기준은 Docker Ubuntu Linux다. macOS에서 개발할 수는 있지만, 발표용 수치는 Linux 컨테이너에서 다시 측정하는 것을 권장한다.

Docker image build:

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
TRIALS=10000 STEPS=30 scripts/run_final.sh
```

docker compose:

```sh
docker compose build
docker compose run --rm os-sim
make
TRIALS=10000 STEPS=30 scripts/run_final.sh
```

Docker image에는 다음 도구가 포함된다.

| 도구 | 용도 |
| --- | --- |
| `gcc`, `make` | C build |
| `procps` | `top`, `ps` |
| `sysstat` | `pidstat` |
| `/usr/bin/time` | memory / user time / system time 측정 |
| `htop` | optional monitoring |

## 17. CPU Utilization 캡처

긴 workload를 실행한 상태에서 별도 터미널로 CPU 사용률을 캡처한다.

터미널 1:

```sh
./sim --mode pipeline --schedule queue --merge interactive --threads 4 \
  --trials 10000000 --steps 100 --batch-size 1000 --queue-size 1024 --seed 42
```

터미널 2:

```sh
pidstat -u -r -C sim 1
```

또는:

```sh
top
```

memory usage까지 확인:

```sh
/usr/bin/time -v ./sim --mode hybrid --processes 2 --threads 4 \
  --trials 1000000 --steps 100 --ipc pipe --seed 42
```

## 18. 결과 해석 방향

| 관찰 결과 | 해석 |
| --- | --- |
| thread 수가 증가할수록 time 감소 | CPU-bound trial 병렬화 효과 |
| 8 threads에서 효율이 낮아짐 | physical core 수, scheduling overhead, context switching 영향 |
| `nosync`에서 `valid=0` | shared result update race condition |
| `mutex`가 정확하지만 느림 | per-trial lock contention |
| `reduce`가 정확하고 빠름 | hot loop에서 shared write 제거 |
| batch size가 너무 작으면 느림 | queue synchronization overhead 증가 |
| batch size가 너무 크면 load balancing 저하 | worker idle 가능성 증가 |
| process가 thread보다 느릴 수 있음 | fork/IPC/waitpid overhead |
| hybrid 성능이 workload에 따라 달라짐 | process overhead와 thread 병렬성의 trade-off |

## 19. Trouble Shooting

| 문제 | 해결 |
| --- | --- |
| `Permission denied` | `chmod +x scripts/run_final.sh` |
| `make: command not found` | `apt-get install -y build-essential make` |
| Docker 결과 파일 권한 문제 | project image 사용 또는 host directory 권한 조정 |
| `nosync` 결과가 invalid | 정상 동작이다. race condition 관찰용 모드 |
| `--ipc shm` 실행 실패 | shared memory는 TODO 확장 항목 |
| 8 threads가 더 느림 | core 수 한계, scheduling overhead, context switching 분석 필요 |

## 20. 현재 구현 완료 범위

| 항목 | 상태 |
| --- | --- |
| Sequential baseline | 완료 |
| Pthread static thread mode | 완료 |
| nosync / mutex / reduce | 완료 |
| Stage별 시간 측정 | 완료 |
| Pre-processing / Post-processing | 완료 |
| TaskBatch | 완료 |
| mutex + condition variable task queue | 완료 |
| queue 기반 pipeline | 완료 |
| final reduce | 완료 |
| interactive merge | 완료 |
| process mode | 완료 |
| pipe IPC | 완료 |
| hybrid mode | 기본 구현 완료 |
| run_final.sh | 완료 |
| Docker Linux 환경 | 완료 |
| capture checklist | 완료 |

## 21. TODO 및 제한사항

| 항목 | 상태 |
| --- | --- |
| shared memory IPC | TODO |
| semaphore vs mutex 비교 | TODO |
| double buffering | TODO |
| perf stat 자동화 | TODO |
| work stealing | TODO |
| 그래프 자동 생성 | TODO |

현재 구현은 최종보고서에서 설명 가능한 수준의 process/thread/synchronization/pipeline/IPC 구조를 우선 완성하는 데 초점을 맞췄다.
