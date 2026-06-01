# 최종 발표 PPT 구성안: 중간발표 이후 변경점과 현재 완성도

이 문서는 중간발표 이후 프로젝트가 어떻게 바뀌었는지 설명하기 위한 PPT 원고다.  
핵심 방향은 “차량 시뮬레이터를 잘 만들었다”가 아니라, “운영체제 개념인 process, thread, synchronization, queue, IPC, pipeline을 하나의 CPU-bound 실험 시스템으로 확장했다”는 점을 명확히 보여주는 것이다.

---

## Slide 1. 제목

### 제목

Monte Carlo 기반 차량 추종 위험 시뮬레이션의 병렬처리 확장

### 부제

Thread 중심 MVP에서 Process / Hybrid / Pipeline / IPC 기반 최종 구조로 확장

### 화면 구성 제안

- 왼쪽: 프로젝트 제목
- 오른쪽: 핵심 키워드 5개
  - `pthread`
  - `fork`
  - `pipe IPC`
  - `task queue`
  - `interactive merge`

### 발표 멘트

중간발표에서는 thread와 synchronization 비교가 중심이었다.  
최종 단계에서는 운영체제 프로젝트 요구사항에 맞춰 child process, IPC, hybrid 구조, task queue, pipeline merge까지 확장했다.

---

## Slide 2. 중간발표 당시 상태

### 핵심 메시지

중간발표 구현은 정확성 검증과 thread 병렬화의 기본 구조는 갖췄지만, 최종 요구사항을 모두 설명하기에는 범위가 제한적이었다.

### 표

| 항목 | 중간발표 상태 | 한계 |
| --- | --- | --- |
| Sequential baseline | 구현 완료 | 기준 성능만 제공 |
| Pthread thread mode | 구현 완료 | process 비교 없음 |
| `nosync` / `mutex` / `reduce` | 구현 완료 | sync 비교는 가능 |
| CSV 출력 | 구현 완료 | stage별 분석 부족 |
| Child process | 미구현 | 최종 요구사항 미충족 |
| IPC | 미구현 | process 간 결과 전달 설명 불가 |
| Hybrid mode | 미구현 | process/thread 역할 분리 부족 |
| Dynamic queue | 미구현 | producer-consumer 구조 부족 |

### 발표 멘트

처음 구현은 thread 실험으로는 충분했지만, 최종 보고서에서 process와 thread의 차이, IPC 비용, queue synchronization overhead를 설명하기에는 부족했다.

---

## Slide 3. 최종 구조로 바뀐 방향

### 핵심 메시지

프로젝트는 단순 병렬 계산 프로그램에서 OS 실행 구조 비교 실험 시스템으로 바뀌었다.

### 구조 비교

| 구분 | 변경 전 | 변경 후 |
| --- | --- | --- |
| 실행 모드 | `seq`, `thread` | `seq`, `thread`, `pipeline`, `process`, `hybrid` |
| 병렬 단위 | pthread worker | thread + process + process 내부 thread |
| 작업 분배 | static partition | static partition + task queue |
| 병합 방식 | local reduce | final reduce + interactive merge |
| IPC | 없음 | pipe 기반 child result 전달 |
| 성능 지표 | time 중심 | stage time, throughput, validation |
| 검증 | hist sum, checksum | 모든 모드 checksum 비교 |

### 시각화 제안

```text
Before
CLI -> seq/thread -> result

After
CLI -> preprocessing -> queue/process/thread/hybrid -> merge -> validation -> CSV
```

### 발표 멘트

변경의 핵심은 실행 구조가 다양해졌다는 점이다. 같은 Monte Carlo trial을 어떤 OS 구조로 실행하느냐에 따라 정확성, overhead, scalability가 달라지는 것을 보여줄 수 있게 되었다.

---

## Slide 4. 새로 추가된 CLI와 실행 모드

### 핵심 메시지

최종 구현에서는 실험 조건을 CLI에서 명확하게 바꿀 수 있다.

### 주요 옵션

| 옵션 | 의미 |
| --- | --- |
| `--mode seq` | sequential baseline |
| `--mode thread` | pthread static partition |
| `--mode pipeline` | task queue 기반 worker pool |
| `--mode process` | fork 기반 child process |
| `--mode hybrid` | process 내부 pthread worker |
| `--schedule static|queue` | 작업 분배 방식 |
| `--merge final|interactive` | 결과 병합 방식 |
| `--processes` | child process 수 |
| `--threads` | worker thread 수 |
| `--batch-size` | queue 작업 단위 |
| `--ipc pipe` | pipe 기반 IPC |

### 발표 멘트

이제 실행 모드, process 수, thread 수, batch size, merge 방식을 CLI로 바꿀 수 있다. 그래서 같은 workload를 여러 OS 구조에서 재현 가능하게 비교할 수 있다.

---

## Slide 5. Thread mode 변경점

### 핵심 메시지

Thread mode는 기존 구조를 유지하되, 최종 비교의 기준점 역할을 한다.

### 내용

| Sync mode | 구현 방식 | 의미 |
| --- | --- | --- |
| `nosync` | shared result lock 없이 갱신 | race condition 관찰 |
| `mutex` | 매 trial마다 lock/unlock | 정확하지만 lock contention 발생 |
| `reduce` | thread-local result 후 merge | 정확성과 성능의 균형 |

### 시각화 제안

```text
Thread 0 -> local_result[0]
Thread 1 -> local_result[1]
Thread 2 -> local_result[2]
Thread 3 -> local_result[3]
             |
             v
        final merge
```

### 발표 멘트

Thread mode의 핵심 결론은 reduce 방식이다. hot loop에서 shared write를 제거하기 때문에 mutex보다 overhead가 작고, nosync와 달리 결과도 정확하다.

---

## Slide 6. Pipeline mode 추가

### 핵심 메시지

Pipeline mode는 producer-consumer 구조와 condition variable synchronization을 보여주기 위해 추가했다.

### 구조

```text
Preprocess
  -> TaskBatch 생성
  -> TaskQueue push
  -> Worker threads pop
  -> Monte Carlo 계산
  -> PartialResult 생성
  -> Merge stage
  -> Validation / CSV
```

### 구현 포인트

| 구성 요소 | 역할 |
| --- | --- |
| `TaskQueue` | batch 단위 작업 저장 |
| `pthread_mutex_t` | queue 상태 보호 |
| `pthread_cond_t not_empty` | worker 대기/깨우기 |
| `pthread_cond_t not_full` | producer 대기/깨우기 |
| `batch_size` | queue granularity 조절 |

### 발표 멘트

Pipeline mode는 단순히 thread를 많이 만드는 구조가 아니라, 작업을 batch로 나누고 queue를 통해 worker에게 공급한다. 이 과정에서 mutex와 condition variable의 비용을 실험할 수 있다.

---

## Slide 7. Final reduce vs Interactive merge

### 핵심 메시지

두 병합 방식은 모두 정확하지만 overhead 구조가 다르다.

### 비교 표

| 항목 | Final reduce | Interactive merge |
| --- | --- | --- |
| 병합 시점 | worker 종료 후 마지막에 병합 | worker가 batch를 끝낼 때마다 병합 |
| 추가 thread | 없음 | aggregator thread 사용 |
| queue 사용 | task queue 중심 | task queue + merge queue |
| 장점 | 단순하고 overhead 작음 | 실행 중간 결과 병합 가능 |
| 단점 | 결과 병합이 마지막에 몰림 | mutex/condvar/context switch 비용 증가 |
| 해석 | 작은 workload에 유리 | 구조 설명에는 좋지만 항상 빠르지는 않음 |

### 발표 멘트

Interactive merge는 더 고급 구조처럼 보이지만, 작은 workload에서는 오히려 느릴 수 있다. merge queue 접근과 aggregator thread wake-up 비용이 계산 이득보다 커질 수 있기 때문이다.

---

## Slide 8. Process mode 추가

### 핵심 메시지

Process mode는 child process와 IPC overhead를 보여주기 위해 추가했다.

### 구조

```text
Parent process
  -> fork child 0
  -> fork child 1
  -> ...
  -> pipe read
  -> waitpid
  -> result merge

Child process
  -> assigned trial range 계산
  -> Result payload pipe write
  -> _exit
```

### 비교 포인트

| 항목 | Thread | Process |
| --- | --- | --- |
| 메모리 | 주소 공간 공유 | 주소 공간 분리 |
| 생성 비용 | 낮음 | 상대적으로 큼 |
| 결과 전달 | shared memory/local reduce | pipe IPC |
| 안정성 | race 주의 | 격리성 높음 |
| overhead | lock/scheduling | fork/IPC/waitpid |

### 발표 멘트

Process mode는 thread보다 빠르기 위한 구조라기보다, 운영체제에서 process isolation과 IPC 비용을 실험하기 위한 구조다. 작은 workload에서는 fork와 pipe 비용 때문에 thread보다 불리할 수 있다.

---

## Slide 9. Hybrid mode 추가

### 핵심 메시지

Hybrid mode는 process와 thread의 역할을 분리해 보여준다.

### 구조

```text
Parent
  -> child process 0
       -> thread 0
       -> thread 1
       -> local merge
       -> pipe write
  -> child process 1
       -> thread 0
       -> thread 1
       -> local merge
       -> pipe write
  -> parent final merge
```

### 역할 분리

| 구성 | 역할 |
| --- | --- |
| Parent process | child 생성, pipe 수신, 최종 merge |
| Child process | 큰 simulation group 담당 |
| Thread worker | child 내부 trial range 병렬 계산 |
| Pipe IPC | child result를 parent로 전달 |

### 발표 멘트

Hybrid mode는 구조적으로 가장 복잡하다. process와 thread를 모두 사용하기 때문에 설명력은 좋지만, workload가 충분히 크지 않으면 overhead가 커질 수 있다.

---

## Slide 10. CSV 지표 변경점

### 핵심 메시지

출력이 단순 실행시간에서 stage별 분석 가능한 CSV로 바뀌었다.

### 주요 필드

| 필드 | 의미 |
| --- | --- |
| `time_total` | 전체 실행 시간 |
| `time_pre` | batch 생성 등 전처리 |
| `time_compute` | worker 계산 구간 |
| `time_sync` | queue/lock/wait overhead |
| `time_merge` | partial result 병합 |
| `time_post` | validation/checksum |
| `throughput_batches_per_sec` | 초당 batch 처리량 |
| `hist_sum` | histogram 합 |
| `checksum` | 결과 재현성 확인 |
| `valid` | 결과 정확성 flag |

### 주의할 점

- 현재 단일 실행의 `speedup`, `efficiency`는 placeholder다.
- 최종 보고서에서는 sequential row의 `time_total`을 기준으로 후처리 계산해야 한다.

### 발표 멘트

이전에는 단순히 빠른지 느린지만 볼 수 있었다. 지금은 어느 stage에서 시간이 쓰이는지, queue와 merge overhead가 어느 정도인지 분석할 수 있다.

---

## Slide 11. 실험 자동화 변경점

### 핵심 메시지

최종 실험용 스크립트가 추가되어 여러 실행 모드를 같은 형식의 CSV로 모을 수 있다.

### 스크립트

```sh
scripts/run_final.sh
```

### 수행하는 실험

| 실험 | 목적 |
| --- | --- |
| Sequential | baseline |
| Thread 1/2/4/8 | thread scaling |
| Pipeline final vs interactive | merge 방식 비교 |
| Batch size 100/1000/10000 | queue granularity 분석 |
| Process 2/4 | child process overhead 분석 |
| Hybrid 2x2 / 2x4 | process + thread 조합 분석 |

### 산출물

```text
results/csv/final_results.csv
```

### 발표 멘트

최종 실험은 수동으로 하나씩 실행하지 않고, 같은 seed와 조건으로 자동 수집한다. 다만 평균과 표준편차 summary는 아직 자동화되어 있지 않으므로 보고서 작성 시 후처리가 필요하다.

---

## Slide 12. 정확성 검증 방식

### 핵심 메시지

병렬 구조가 달라도 같은 trial index는 같은 seed를 사용하므로 결과가 같아야 한다.

### 검증 기준

| 검증 항목 | 의미 |
| --- | --- |
| `total_trials == trials` | 요청한 trial 수만큼 계산했는가 |
| `hist_sum == trials` | 모든 risk bucket 합이 trial 수와 같은가 |
| `valid=1` | 기본 정합성 통과 |
| `checksum 동일` | 실행 모드가 달라도 동일 결과 |

### 발표 멘트

성능이 아무리 좋아도 valid가 0이면 의미가 없다. 그래서 이 프로젝트에서는 성능보다 먼저 hist_sum, checksum, valid로 정확성을 확인한다.

---

## Slide 13. 업로드 결과에서 확인된 현실적인 문제

### 핵심 메시지

구현은 많이 확장됐지만, 10,000 trials 수준의 결과만으로 성능 결론을 내리기는 어렵다.

### 문제점

| 문제 | 이유 | 대응 |
| --- | --- | --- |
| 실행시간이 너무 짧음 | 1ms 안팎이면 scheduler noise 영향 큼 | 100,000 이상 trials 사용 |
| interactive merge가 느릴 수 있음 | queue/aggregator overhead | final reduce와 비교 |
| process가 thread보다 느릴 수 있음 | fork/pipe/waitpid 비용 | overhead tradeoff로 해석 |
| hybrid가 과설계일 수 있음 | process + thread 비용 동시 발생 | 큰 workload에서 재측정 |
| speedup placeholder | 단일 실행에서 기준값 없음 | CSV 후처리로 계산 |

### 발표 멘트

냉정하게 보면 지금 프로젝트는 구조 구현은 많이 됐지만, 성능 결론은 아직 조심해야 한다. 작은 workload에서는 복잡한 구조가 오히려 느릴 수 있다.

---

## Slide 14. 최종 보고서에서 가져갈 결론

### 핵심 메시지

최종 결론은 “어떤 모드가 무조건 빠르다”가 아니라, OS 구조별 trade-off다.

### 결론 정리

| 구조 | 결론 |
| --- | --- |
| `thread + reduce` | 가장 실용적인 baseline |
| `mutex` | 정확하지만 lock contention이 있음 |
| `nosync` | race condition 설명에 적합 |
| `pipeline final` | queue 구조에서 단순하고 안정적 |
| `pipeline interactive` | 실시간 병합 가능하지만 overhead 큼 |
| `process` | 격리성은 좋지만 IPC 비용 존재 |
| `hybrid` | 설명력은 좋지만 workload가 작으면 과설계 |

### 발표 멘트

운영체제 관점에서 중요한 것은 단순 실행시간 순위가 아니라, 각 구조가 어떤 비용을 만들고 어떤 상황에서 의미가 있는지 설명하는 것이다.

---

## Slide 15. 앞으로 해야 할 일

### 핵심 메시지

최종 제출 전에는 성능 측정의 신뢰도를 높여야 한다.

### 추가 보강 완료/남은 작업

| 우선순위 | 작업 | 이유 |
| --- | --- | --- |
| 1 | Docker Linux에서 큰 workload 재측정 | 완료. 단, 최종 보고서 전 동일 환경 재확인 권장 |
| 2 | speedup/efficiency 후처리 | 완료. `scripts/analyze_results.py` 사용 |
| 3 | 5회 이상 반복 측정 | 완료. `REPEATS=5` 기준 |
| 4 | pidstat, `/usr/bin/time -v` 캡처 | 부분 완료. 최종 제출 전 캡처 이미지 정리 필요 |
| 5 | 그래프 생성 | 완료. `scripts/make_final_graphs.py` 사용 |
| 6 | shared memory IPC 비교 | 구현 완료. `--ipc pipe/shm` 비교 가능 |

### 발표 멘트

현재는 구조 구현과 기능 검증은 된 상태다. 남은 핵심은 더 큰 workload에서 반복 측정하고, speedup과 efficiency를 후처리해서 보고서에 넣는 것이다.

---

## Slide 16. 한 장 요약

### 핵심 메시지

중간발표 이후 프로젝트는 thread 실험에서 OS 병렬처리 구조 비교 실험으로 확장되었다.

### 요약 문장

- 기존: `seq/thread` 중심의 synchronization 비교
- 변경: `process`, `hybrid`, `pipeline`, `interactive merge`, `pipe/shared memory IPC`, `skewed workload` 추가
- 검증: `hist_sum`, `checksum`, `valid`로 모든 모드 정확성 확인 가능
- 분석: stage time과 throughput으로 overhead 위치를 설명 가능
- 남은 일: 최종 Linux 환경에서 같은 명령으로 재측정하고 CPU/memory 캡처를 보고서에 붙이는 것

### 발표 마무리 멘트

이 프로젝트의 최종 가치는 자동차 모델 자체가 아니라, 동일한 CPU-bound 작업을 여러 OS 실행 구조로 바꿔가며 정확성과 overhead를 비교할 수 있다는 점이다.
