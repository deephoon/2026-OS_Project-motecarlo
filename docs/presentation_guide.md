# 중간 발표 진행 가이드

이 문서는 발표자가 실제 발표 중 어떤 순서와 논리로 말해야 하는지 정리한 가이드입니다.

## 발표 핵심 기조

반드시 다음 프레임으로 발표합니다.

```text
차량 시뮬레이션 발표 X
CPU-bound 병렬처리 실험 발표 O

thread만 구현한 발표 X
현재 thread/sync 검증 완료,
최종 process/hybrid 확장 계획 명확 O

코드 구현 소개 X
운영체제 개념 분석 O
```

강조할 OS 개념:

- thread
- synchronization
- race condition
- mutex
- local reduce
- speedup
- CPU utilization
- lock contention
- scheduling overhead

## 1분 오프닝 멘트

```text
저희 프로젝트는 차량 시뮬레이터 자체를 만드는 것이 아니라,
운영체제의 병렬처리와 synchronization 문제를 실험하기 위한
CPU-bound 반복 계산 시스템입니다.

초기에는 이미지 처리 방식도 검토했지만,
단순 blur는 연산량보다 I/O와 메모리 접근 영향이 커질 수 있다고 판단했습니다.
그래서 파일 입력 없이 seed 기반으로 독립적인 Monte Carlo trial을 대량 생성하고,
thread 수와 synchronization 방식에 따라 실행시간과 결과 정확성을 비교하는 구조로 설계했습니다.

이번 중간발표 구현 범위에서는 sequential baseline,
pthread 기반 thread mode, nosync/mutex/local reduce 비교를 완료했고,
최종 발표까지 child process, IPC, hybrid 구조로 확장할 계획입니다.
```

## 슬라이드별 발표 포인트

### Slide 1

핵심:

- 차량 시뮬레이터가 아니다.
- CPU-bound 병렬처리 실험이다.

멘트:

```text
제목의 차량 추종 모델은 계산 workload를 만들기 위한 도메인이고,
실제 발표의 중심은 thread, synchronization, race condition, speedup 분석입니다.
```

### Slide 2

핵심:

- 목표는 성능과 정확성의 trade-off 분석.

멘트:

```text
같은 Monte Carlo 계산을 sequential, thread, sync 방식별로 실행하고,
단순히 빠른지만 보는 것이 아니라 결과가 정확한지도 같이 검증합니다.
```

### Slide 3

핵심:

- CLI parameter와 seed가 입력.
- 파일 I/O 제거.

멘트:

```text
입력 파일을 읽지 않기 때문에 이미지 read/write 병목이 실험 결과를 왜곡하지 않습니다.
```

### Slide 4

핵심:

- trial 독립성.
- time-step 반복 계산.

멘트:

```text
trial 간 의존성이 없기 때문에 thread별로 구간을 나누기 쉽고,
반복 계산량은 steps와 trials로 조절할 수 있습니다.
```

### Slide 5

핵심:

- 현재 구현 범위와 최종 확장 범위를 분리.

멘트:

```text
child process는 최종 프로젝트 요구사항이므로 확장 예정 범위로 명확히 두었습니다.
이번 발표에서는 thread와 synchronization 검증을 먼저 완료한 상태를 보여드립니다.
```

### Slide 6

핵심:

- 병렬화 단위는 trial range.
- 공유 자원은 result/histogram.

멘트:

```text
thread가 계산 자체는 독립적으로 수행하지만,
결과를 하나의 histogram에 합치는 순간 synchronization 문제가 발생합니다.
```

### Slide 7

핵심:

- nosync / mutex / reduce의 목적이 다르다.

멘트:

```text
nosync는 일부러 문제를 보여주는 기준이고,
mutex는 correctness를 보장하는 방식,
reduce는 lock overhead를 줄이는 구조 개선입니다.
```

### Slide 8

핵심:

- 실제 구현 증거.
- Docker Linux 재현성.

멘트:

```text
최종 측정은 Docker Linux에서 수행하도록 준비했고,
Makefile과 스크립트로 같은 실험을 반복 실행할 수 있습니다.
```

### Slide 9

핵심:

- test vector가 다양해야 분석 가능.

멘트:

```text
thread 수, sync 방식, steps를 바꿔서 단일 결과가 아니라 여러 조건에서 원인을 분석할 수 있게 구성했습니다.
```

### Slide 10

핵심:

- thread scaling.
- 이상적 speedup과 차이 분석.

멘트:

```text
thread 수가 증가하면 실행시간이 감소하지만,
이상적 T/N과 완전히 같지는 않습니다.
이는 thread 생성, scheduling, 물리 core 수, workload 크기 때문입니다.
```

### Slide 11

핵심:

- nosync valid=0.
- race condition 증명.

멘트:

```text
이 슬라이드가 synchronization 필요성을 보여주는 핵심입니다.
nosync에서는 thread들이 공유 counter를 동시에 갱신해 값이 유실되고,
histogram sum이 trials와 일치하지 않습니다.
```

### Slide 12

핵심:

- 문제 -> 원인 -> 해결.

멘트:

```text
결과가 단순히 좋다 나쁘다가 아니라,
각 현상을 운영체제 개념으로 원인 분석하고 해결 방식과 연결했습니다.
```

### Slide 13

핵심:

- child process 미구현 리스크 방어.
- 최종 구조 구체화.

멘트:

```text
최종 발표에서는 process가 큰 simulation group을 맡고,
각 process 내부에서 thread가 batch를 병렬 계산하는 hybrid 구조로 확장할 계획입니다.
```

### Slide 14

핵심:

- 역할분담.
- 현재 산출물과 최종 작업.

멘트:

```text
역할은 단순 업무명이 아니라 현재 산출물과 최종 확장 작업 기준으로 나누었습니다.
```

## 예상 질문과 답변

### Q1. 왜 차량 시뮬레이션인가?

```text
차량 모델 자체가 목적은 아닙니다.
독립적인 trial을 많이 만들 수 있고, 각 trial 안에서 time-step 반복 계산을 수행할 수 있어서
CPU-bound 병렬처리 실험 workload로 사용했습니다.
```

### Q2. CPU-bound라는 근거는?

```text
파일 입력이 없고, CLI와 seed만으로 trial 입력을 생성합니다.
각 trial은 steps만큼 위치, 속도, TTC 계산을 반복하므로 계산량은 trials와 steps로 증가합니다.
```

### Q3. I/O 병목은 어떻게 제거했나?

```text
이미지 파일 read/write가 없습니다.
출력도 CSV 한 줄이므로 전체 시간 대부분은 simulation compute와 synchronization/merge에 해당합니다.
```

### Q4. cache 재사용은 어떻게 설명할 것인가?

```text
이미지처럼 큰 배열을 한 번 읽고 버리는 streaming 구조가 아닙니다.
각 worker는 작은 차량 상태와 local result를 반복 사용하므로 메모리 bandwidth보다 arithmetic loop 중심입니다.
```

### Q5. child process가 아직 없는데 가이드에 부합하는가?

```text
중간발표 구현 범위는 multithread 수행 결과입니다.
가이드의 최종 요구사항인 child process와 hybrid 구조는 Slide 13의 구조대로 최종 발표까지 확장할 계획입니다.
```

### Q6. nosync가 빠르면 좋은 것 아닌가?

```text
아닙니다.
nosync는 valid=0으로 결과가 틀립니다.
성능 분석에서는 실행시간뿐 아니라 total_trials, hist_sum, checksum으로 correctness도 함께 봐야 합니다.
```

### Q7. mutex가 왜 느린가?

```text
매 trial마다 lock/unlock을 수행하기 때문에 여러 thread가 critical section에서 대기합니다.
이 lock contention 때문에 정확하지만 reduce보다 느립니다.
```

### Q8. reduce가 왜 좋은가?

```text
thread-local result에 먼저 누적하므로 hot loop에서 공유 자원 접근이 없습니다.
thread 종료 후 main thread가 merge하므로 정확성을 유지하면서 lock overhead를 줄입니다.
```

### Q9. speedup이 이상적이지 않으면 실패인가?

```text
아닙니다.
프로젝트 가이드도 문제 인식과 원인 분석을 요구합니다.
speedup이 T/N과 다른 이유를 thread overhead, scheduling overhead, core 수 한계로 분석하는 것이 발표의 핵심입니다.
```

## 발표자료에서 피해야 할 표현

피할 표현:

- MVP
- 차량 시뮬레이터 구현
- thread만 구현
- 단순히 코드 작성

대체 표현:

- 중간발표 구현 범위
- 현재 구현 범위
- 최종 확장 예정 범위
- CPU-bound 병렬처리 실험
- synchronization 성능 및 정확성 분석

## 캡처 준비 가이드

발표자료에 넣을 캡처는 다음 문서를 기준으로 준비합니다.

```text
docs/capture_checklist.md
docs/linux_capture_guide.md
```

우선순위:

1. `nosync valid=0` 결과 캡처
2. `pthread_create` / `pthread_join` 코드 캡처
3. `nosync / mutex / reduce` 코드 캡처
4. `TRIALS=1000000 STEPS=50 scripts/run_midterm.sh` 결과 CSV
5. Docker Linux 실행 캡처
6. `pidstat` 또는 `top` CPU utilization 캡처

발표자료에 넣을 캡처는 Linux/Docker 결과를 우선 사용하고, macOS 로컬 결과는 개발 중간 확인용으로만 사용합니다.
