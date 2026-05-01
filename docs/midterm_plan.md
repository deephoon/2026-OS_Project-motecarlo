# Midterm Presentation Plan

## 발표 목표

이 중간 발표의 목표는 차량 시뮬레이션 자체가 아니라 OS 병렬처리 실험 모델을 구현했다는 점을 보여주는 것입니다. 같은 Monte Carlo workload를 sequential, pthread, synchronization 방식별로 실행하고 실행시간과 결과 검증 지표를 비교합니다.

## 구현 완료 범위

- Sequential baseline
- Pthread multi-thread mode
- Static partition 기반 trial 분할
- `nosync`, `mutex`, `reduce` synchronization 비교
- deterministic per-trial seed
- CSV output
- `scripts/run_midterm.sh` 실험 자동화
- Docker Linux build environment

## 실험 목록

1. Sequential baseline
   - `./sim --mode seq --trials 1000000 --steps 50 --seed 42`

2. Thread scaling with local reduce
   - threads 1, 2, 4, 8
   - 같은 trials/steps/seed에서 실행시간 비교

3. Synchronization comparison
   - `nosync`
   - `mutex`
   - `reduce`
   - `hist_sum`, `valid`, `time_sec` 비교

4. CPU-bound intensity
   - steps 10, 50, 100
   - 계산량이 커질수록 thread overhead가 상대적으로 줄어드는지 확인

## 그래프 후보

- threads 수에 따른 `time_sec`
- threads 수에 따른 speedup
- sync mode별 `time_sec`
- sync mode별 `hist_sum` 또는 `valid`
- steps 변화에 따른 `time_sec`

## 발표 포인트

- Trial 단위 독립성 때문에 thread 병렬화가 자연스럽다.
- `nosync`는 빠를 수 있지만 결과가 틀릴 수 있다.
- `mutex`는 정확하지만 lock contention 때문에 느릴 수 있다.
- `reduce`는 shared write를 hot loop에서 제거해 정확성과 성능을 동시에 얻는다.
- deterministic per-trial seed로 thread scheduling과 결과 재현성을 분리했다.

## 현재 한계

- 실제 차량 동역학이 아니라 OS 실험용 단순 모델이다.
- speedup은 CSV에서 직접 계산해야 한다.
- process/hybrid/dynamic queue는 아직 구현하지 않았다.
- CPU utilization과 memory 분석은 문서화 TODO 상태다.

## 최종 발표까지 남은 작업

- child process mode 구현
- process 결과 aggregation 방식 비교
- hybrid process/thread mode 구현
- dynamic task queue 구현
- `pidstat`, `time`, `getrusage` 기반 CPU/memory 지표 추가
- Docker Linux에서 반복 측정 후 그래프 작성
