# Final Validation Report

이 문서는 최종 제출 전 프로젝트 약점을 보강하기 위해 수행한 검증 결과와 보고서/PPT 반영 포인트를 정리합니다.

## 1. 기능 검증

`make test` 통과.

| Mode | Trials | Steps | Valid | Hist sum | Checksum |
| --- | ---: | ---: | ---: | ---: | ---: |
| `seq` | 10000 | 30 | 1 | 10000 | `9158329899332878926` |
| `thread reduce` | 10000 | 30 | 1 | 10000 | `9158329899332878926` |
| `pipeline interactive` | 10000 | 30 | 1 | 10000 | `9158329899332878926` |

해석:

- 정상 synchronization mode에서는 sequential baseline과 같은 checksum을 재현했다.
- `thread_4_nosync`는 최종 반복 실험에서 `valid_all=0`, checksum mismatch로 확인되며 race condition 설명용 실패 사례로 사용한다.

## 2. 기본 성능 실험

조건:

```text
Docker Ubuntu Linux container
TRIALS=1000000
STEPS=50
REPEATS=5
PRE_WORK=50000
POST_WORK=10000
```

핵심 결과:

| Case | Avg time | Speedup | Efficiency | Valid | Checksum |
| --- | ---: | ---: | ---: | ---: | ---: |
| `thread_8_reduce` | `0.020732s` | `4.798x` | `0.600` | 1 | match |
| `hybrid_2x4` | `0.023281s` | `4.273x` | `0.534` | 1 | match |
| `hybrid_4x2` | `0.023545s` | `4.225x` | `0.528` | 1 | match |
| `thread_4_reduce` | `0.029020s` | `3.428x` | `0.857` | 1 | match |
| `pipeline_final_b1000` | `0.029900s` | `3.327x` | `0.832` | 1 | match |

해석:

- 이 조건에서는 `thread_8_reduce`가 가장 빠른 평균 실행시간을 보였다.
- `thread_4_reduce`는 절대 실행시간은 더 길지만 efficiency가 높아 실용적인 thread 병렬화 기준점이다.
- process/hybrid는 fork/IPC overhead가 있으나 큰 workload에서는 병렬화 효과를 보인다.

## 3. Amdahl Stress 실험

조건:

```text
Docker Ubuntu Linux container
TRIALS=1000000
STEPS=50
REPEATS=5
PRE_WORK=50000000
POST_WORK=10000000
```

핵심 결과:

| Case | Avg time | Speedup | Efficiency | Sequential fraction |
| --- | ---: | ---: | ---: | ---: |
| `seq` | `0.249434s` | `1.000x` | `1.000` | `0.613` |
| `thread_2_reduce` | `0.203409s` | `1.226x` | `0.613` | `0.756` |
| `thread_4_reduce` | `0.181329s` | `1.376x` | `0.344` | `0.847` |
| `thread_8_reduce` | `0.172864s` | `1.443x` | `0.180` | `0.886` |

해석:

- 기본 실험에서 `thread_8_reduce` speedup은 `4.798x`였다.
- 순차 pre/post 구간을 크게 만든 stress 실험에서는 `thread_8_reduce` speedup이 `1.443x`로 제한되었다.
- 이는 병렬화할 수 없는 순차 구간이 커질수록 전체 speedup이 제한된다는 Amdahl's Law를 보여준다.

## 4. CPU / Memory 측정

측정 명령은 Docker Ubuntu Linux container에서 `/usr/bin/time -v`로 수행했다.

| Case | CPU percent | Wall time | User time | System time | Max RSS |
| --- | ---: | ---: | ---: | ---: | ---: |
| `seq` | `98%` | `0:00.18` | `0.17s` | `0.00s` | `1212 KB` |
| `thread_8_reduce` | `434%` | `0:00.04` | `0.19s` | `0.00s` | `1276 KB` |
| `hybrid_2x4` | `664%` | `0:00.02` | `0.16s` | `0.00s` | `1212 KB` |

주의:

- `/usr/bin/time -v` 단일 실행 결과는 resource usage 캡처용이다.
- 최종 성능 순위와 speedup은 5회 반복 평균인 `final_analyzed.csv`를 기준으로 해석한다.
- Docker Desktop은 Linux VM 위에서 실행되므로 순수 물리 Linux의 절대 시간과 다를 수 있다.

## 5. 생성된 그래프

그래프는 `results/graphs/`에 SVG로 생성했다.

| 파일 | 목적 |
| --- | --- |
| `thread_speedup_efficiency.svg` | thread scaling과 efficiency 감소 |
| `sync_compare.svg` | nosync/mutex/reduce 비교 |
| `process_hybrid_compare.svg` | process/hybrid overhead와 병렬화 효과 |
| `pipeline_merge_compare.svg` | final reduce와 interactive merge 비교 |
| `stage_time_stacked.svg` | stage별 실행 시간 구성 |
| `amdahl_stress_speedup.svg` | 기본 실험 vs Amdahl stress speedup 비교 |
| `amdahl_stress_stage_time.svg` | stress 조건에서 pre/post 순차 구간 시각화 |

## 6. 최종 보고서 반영 문장

```text
최종 성능 분석은 Docker Ubuntu Linux 환경에서 1,000,000 trials 조건을 5회 반복 측정한 결과를 기준으로 수행했다. 각 mode는 valid flag와 checksum을 sequential baseline과 비교하여 정확성을 먼저 검증했고, 이후 평균 실행시간, 최소 실행시간, 표준편차, speedup, efficiency를 계산했다.
```

```text
PRE_WORK/POST_WORK를 증가시킨 Amdahl stress 실험에서는 thread_8_reduce의 speedup이 기본 실험의 4.798x에서 1.443x로 감소했다. 이는 병렬화할 수 없는 순차 구간이 커질수록 전체 speedup이 제한된다는 Amdahl's Law를 보여준다.
```

```text
본 프로젝트의 목적은 가장 빠른 실행 구조 하나를 찾는 것이 아니라, process, thread, synchronization, IPC, pipeline 구조가 정확성, overhead, scalability에 어떤 trade-off를 만드는지 정량적으로 비교하는 것이다.
```
