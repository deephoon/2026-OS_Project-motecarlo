# Midterm Presentation Plan

## 발표 목표

중간 발표의 핵심은 **단순 이미지 blur의 한계를 인식하고, I/O와 메모리 스트리밍 영향이 작은 CPU-bound Monte Carlo 모델로 재설계했다는 점**을 명확히 보여주는 것이다.

최종 프로젝트 전체 요구사항은 child process, multithread, synchronization 비교까지 포함하지만, 이번 중간 발표는 다음 범위를 구현 결과 중심으로 발표한다.

- sequential baseline
- pthread multi-thread mode
- `nosync`, `mutex`, `reduce` synchronization 비교
- CSV 기반 정량 실험
- Docker Linux 실행 환경

Child process, hybrid mode, dynamic task queue, CPU/memory utilization 정밀 분석은 최종 발표 확장 계획으로 분리해 설명한다.

## 교수님 피드백 반영 방향

교수님 피드백의 본질:

- 단순 blur는 연산이 너무 가볍다.
- 이미지 크기만 키우면 I/O, 메모리 접근, cache miss 영향이 커진다.
- 프로젝트는 N개 core를 사용했을 때 CPU utilization이 높게 유지되는 workload여야 한다.
- process와 thread가 각각 의미 있는 역할을 가져야 한다.

우리의 대응:

- 이미지 파일 입력을 제거한다.
- deterministic seed 기반으로 trial 입력을 생성한다.
- trial 내부 time-step 반복 계산으로 CPU-bound 비중을 키운다.
- `trials`와 `steps`를 통해 계산량을 조절한다.
- 현재 중간 단계에서는 pthread parallelism과 synchronization 문제를 먼저 증명한다.
- 최종 단계에서 child process와 hybrid 구조를 추가한다.

## 발표자료 구성

1. Title
   - 프로젝트명
   - 팀원/역할

2. 프로젝트 가이드 요구사항 요약
   - child process + multithread + synchronization
   - 4개 이상 core에서 N개 core 활용
   - 정량 성능 분석
   - 문제 인식, 원인 분석, 구조 개선

3. 교수님 피드백 반영
   - 단순 이미지 blur의 문제
   - CPU-bound Monte Carlo 모델로 전환한 이유

4. Monte Carlo 차량 추종 모델 설명
   - 실제 차량 시뮬레이터가 아니라 OS 실험 모델
   - trial 계산 흐름
   - risk classification

5. System flow diagram
   - CLI config
   - sequential mode
   - thread mode
   - sync mode
   - CSV output

6. Thread 구조
   - static partition
   - deterministic per-trial seed
   - reproducibility

7. Synchronization 비교
   - nosync
   - mutex
   - reduce

8. 실험 설계
   - sequential baseline
   - thread scaling
   - sync comparison
   - CPU-bound intensity

9. 실험 결과
   - thread 수별 time/speedup
   - sync mode별 time/valid
   - steps 변화에 따른 time

10. 교수님 질문 대비
   - CPU-bound 근거
   - I/O bottleneck 제거 근거
   - cache/memory 관점 방어
   - child process 최종 확장 계획

11. 한계와 최종 확장
   - process mode
   - hybrid mode
   - dynamic task queue
   - CPU/memory 분석

12. 결론

## 실험 명령

Smoke test:

```sh
make clean
make
make test
```

중간 발표 실험:

```sh
TRIALS=1000000 STEPS=50 scripts/run_midterm.sh
```

CPU utilization 캡처용 긴 실행:

```sh
./sim --mode thread --threads 4 --trials 10000000 --steps 100 --sync reduce --seed 42
```

다른 터미널에서:

```sh
pidstat -u -r -C sim 1
```

또는:

```sh
top
```

## 현재 실험 결과 요약

`TRIALS=1000000`, `STEPS=50` 기준 로컬 결과:

| 실험 | 핵심 결과 |
|---|---|
| sequential | `time_sec=0.114392`, `valid=1` |
| reduce 1 thread | `time_sec=0.102064`, `speedup=1.120787` |
| reduce 2 threads | `time_sec=0.053912`, `speedup=2.121828` |
| reduce 4 threads | `time_sec=0.032194`, `speedup=3.553209` |
| reduce 8 threads | `time_sec=0.020967`, `speedup=5.455812` |
| nosync 4 threads | `hist_sum=800794`, `valid=0` |
| mutex 4 threads | `time_sec=0.136491`, `valid=1` |
| reduce 4 threads | `time_sec=0.031272`, `valid=1` |

발표 포인트:

- `reduce`는 thread 수가 증가할수록 실행시간이 감소한다.
- `nosync`는 race condition 때문에 결과가 깨진다.
- `mutex`는 정확하지만 lock overhead가 크다.
- `reduce`는 shared write를 hot loop에서 제거해 정확성과 성능을 함께 확보한다.

## 그래프 후보

- threads 수에 따른 `time_sec`
- threads 수에 따른 `speedup`
- sync mode별 `time_sec`
- sync mode별 `hist_sum` 또는 `valid`
- steps 10/50/100 변화에 따른 `time_sec`

## 현재 한계

- child process는 아직 구현하지 않았다.
- CPU utilization은 아직 수동 캡처 중심이다.
- memory usage와 user/system time 분석은 최종 단계에서 보강한다.
- 실제 차량 동역학 정확도보다 OS 병렬처리 실험성이 우선이다.

## 최종 발표까지 남은 작업

- child process mode 구현
- single child process vs single thread 비교
- multi process vs multi thread 비교
- process + thread hybrid mode 구현
- dynamic task queue 구현
- phase timing 추가
  - `T_generate`
  - `T_compute`
  - `T_sync`
  - `T_merge`
  - `T_output`
- `pidstat`, `/usr/bin/time`, `getrusage` 기반 CPU/memory 지표 수집
- 팀원별 역할분담, 수행일지, AI 사용 기록 정리

