# OS26 Final Experiment Guide

이 문서는 `ref/os26_project.pdf` 요구사항에 맞춰 최종 실험을 재현하기 위한 실행 가이드입니다.

## 1. 목적

최종 실험의 목적은 가장 빠른 모드 하나를 찾는 것이 아니라, 같은 Monte Carlo workload를 여러 OS 실행 구조로 처리했을 때 다음 차이를 정량적으로 보여주는 것입니다.

- sequential baseline 대비 speedup/efficiency
- child process의 fork/IPC overhead
- pthread scaling과 lock contention
- process + thread hybrid 구조의 장단점
- mutex + condition variable queue의 synchronization overhead
- final reduce와 interactive merge의 trade-off
- pre/post 순차 구간이 Amdahl's Law에 미치는 영향

## 2. 실행

기본 최종 실험:

```sh
chmod +x scripts/run_final.sh scripts/analyze_results.py
TRIALS=100000 STEPS=50 REPEATS=5 scripts/run_final.sh
```

더 강한 성능 분석용:

```sh
TRIALS=1000000 STEPS=100 REPEATS=5 PRE_WORK=50000 POST_WORK=10000 scripts/run_final.sh
```

결과 파일:

```text
results/csv/final_raw.csv
results/csv/final_analyzed.csv
results/csv/final_summary.md
```

## 3. 새 실험 옵션

```text
--pre-work <int>
--post-work <int>
```

이 옵션은 결과값을 바꾸지 않는 deterministic CPU work를 pre-processing/post-processing stage에 추가합니다. 목적은 단순히 trial을 바로 병렬화하는 쉬운 문제에서 벗어나, 순차 구간이 존재하는 현실적인 workload를 만들고 Amdahl's Law 분석을 가능하게 하는 것입니다.

## 4. PDF 요구사항별 대응 실험

| PDF 요구사항 | 실행 케이스 |
| --- | --- |
| parent sequential baseline | `seq` |
| single thread | `thread_1_reduce` |
| multi thread | `thread_2_reduce`, `thread_4_reduce`, `thread_8_reduce` |
| single child process | `process_1_pipe` |
| multi child process | `process_2_pipe`, `process_4_pipe` |
| process + thread hybrid | `hybrid_2x2`, `hybrid_2x4`, `hybrid_4x2` |
| synchronization 미사용 | `thread_4_nosync` |
| mutex synchronization | `thread_4_mutex` |
| reduce synchronization | `thread_4_reduce` |
| pipeline 구조 | `pipeline_final_b1000`, `pipeline_interactive_b1000` |
| batch size 변화 | `pipeline_interactive_b100`, `pipeline_interactive_b1000`, `pipeline_interactive_b10000` |

## 5. 보고서에서 사용할 지표

`final_analyzed.csv`를 기준으로 사용합니다.

| 지표 | 의미 |
| --- | --- |
| `avg_time_total` | 반복 측정 평균 실행시간 |
| `min_time_total` | 가장 좋은 실행시간 |
| `stdev_time_total` | 측정 흔들림 |
| `speedup_vs_seq_avg` | sequential 평균 대비 speedup |
| `efficiency_vs_seq_avg` | worker 수 대비 효율 |
| `avg_sequential_fraction` | pre/sync/merge/post가 차지하는 비율 추정 |
| `matches_seq_checksum` | sequential checksum과 같은지 |
| `valid_all` | 모든 반복에서 hist_sum/trials 검증을 통과했는지 |

## 6. 해석 원칙

- `valid_all=1`이고 `matches_seq_checksum=1`인 결과만 성능 비교에 사용합니다.
- `thread_4_nosync`는 빠르더라도 정확성 실패 사례로 해석합니다.
- `pipeline interactive`가 작은 workload에서 느리면 실패가 아니라 queue/condition variable/aggregator overhead의 증거로 해석합니다.
- process/hybrid가 작은 workload에서 thread보다 느리면 fork/IPC overhead가 계산 이득을 압도한 결과로 해석합니다.
- 1ms 안팎의 결과는 기능 검증용으로만 쓰고, 최종 성능 결론은 큰 workload와 반복 측정 평균으로 주장합니다.
