# Docker 1M Shared Memory / Skewed Workload Validation

측정일: 2026-06-01

이 문서는 `--ipc shm`과 `--workload skewed` 추가 구현 후, Docker Ubuntu Linux 환경에서 다시 측정한 최종 검증 결과입니다.

## 실행 조건

| 항목 | 값 |
| --- | --- |
| Environment | Docker Ubuntu 22.04 |
| Trials | `1,000,000` |
| Steps | `50` |
| Repeats | `5` |
| Seed | `42` |
| Pre work | `50,000` |
| Post work | `10,000` |
| Uniform output | `results/csv/docker_1m_shm_uniform/final_analyzed.csv` |
| Skewed output | `results/csv/docker_1m_shm_skewed/final_analyzed.csv` |

실행 명령:

```sh
docker build -t os-montecarlo-risk .

docker run --rm \
  -v "$PWD/results:/workspace/results" \
  -e TRIALS=1000000 \
  -e STEPS=50 \
  -e REPEATS=5 \
  -e WORKLOAD=uniform \
  -e SKEW_FACTOR=8 \
  -e PRE_WORK=50000 \
  -e POST_WORK=10000 \
  -e OUT_DIR=results/csv/docker_1m_shm_uniform \
  os-montecarlo-risk sh scripts/run_final.sh

docker run --rm \
  -v "$PWD/results:/workspace/results" \
  -e TRIALS=1000000 \
  -e STEPS=50 \
  -e REPEATS=5 \
  -e WORKLOAD=skewed \
  -e SKEW_FACTOR=8 \
  -e PRE_WORK=50000 \
  -e POST_WORK=10000 \
  -e OUT_DIR=results/csv/docker_1m_shm_skewed \
  os-montecarlo-risk sh scripts/run_final.sh
```

## Uniform Workload 요약

Sequential baseline 평균은 `0.094601s`입니다.

| Case | Avg time | Speedup | Efficiency | Valid | Checksum match |
| --- | ---: | ---: | ---: | ---: | ---: |
| `thread_8_reduce` | 0.021111 | 4.481 | 0.560 | 1 | 1 |
| `hybrid_2x4_shm` | 0.021997 | 4.301 | 0.538 | 1 | 1 |
| `hybrid_2x4` | 0.022467 | 4.211 | 0.526 | 1 | 1 |
| `thread_4_reduce` | 0.027202 | 3.478 | 0.869 | 1 | 1 |
| `process_4_shm` | 0.028469 | 3.323 | 0.831 | 1 | 1 |
| `process_4_pipe` | 0.030282 | 3.124 | 0.781 | 1 | 1 |
| `pipeline_final_b1000` | 0.029459 | 3.211 | 0.803 | 1 | 1 |
| `pipeline_interactive_b1000` | 0.030199 | 3.133 | 0.783 | 1 | 1 |
| `thread_4_nosync` | 0.034111 | 2.773 | 0.693 | 0 | 0 |

해석:

- 균등 workload에서는 `thread_8_reduce`가 가장 빠릅니다.
- `shm`은 pipe보다 약간 빠르게 나왔지만 차이는 크지 않습니다. 이 프로젝트의 `Result` 구조체가 작기 때문에 IPC 복사 비용보다 `fork/waitpid`와 계산 시간이 더 크게 보입니다.
- `pipeline_interactive_b1000`은 `pipeline_final_b1000`보다 약간 느립니다. 이는 실패가 아니라 merge queue, condition variable, aggregator thread overhead가 관찰된 결과입니다.
- `thread_4_nosync`는 성능값이 있어도 `valid=0`, checksum mismatch이므로 실패 사례로만 사용해야 합니다.

## Skewed Workload 요약

Sequential baseline 평균은 `0.128468s`입니다.

| Case | Avg time | Speedup | Efficiency | Valid | Checksum match |
| --- | ---: | ---: | ---: | ---: | ---: |
| `hybrid_4x2` | 0.035298 | 3.640 | 0.455 | 1 | 1 |
| `hybrid_2x4_shm` | 0.035857 | 3.583 | 0.448 | 1 | 1 |
| `hybrid_2x4` | 0.035894 | 3.579 | 0.447 | 1 | 1 |
| `pipeline_final_b1000` | 0.036921 | 3.480 | 0.870 | 1 | 1 |
| `thread_8_reduce` | 0.037207 | 3.453 | 0.432 | 1 | 1 |
| `pipeline_interactive_b1000` | 0.037946 | 3.386 | 0.846 | 1 | 1 |
| `thread_4_reduce` | 0.060584 | 2.120 | 0.530 | 1 | 1 |
| `process_4_shm` | 0.061322 | 2.095 | 0.524 | 1 | 1 |
| `process_4_pipe` | 0.062909 | 2.042 | 0.511 | 1 | 1 |
| `thread_4_nosync` | 0.063101 | 2.036 | 0.509 | 0 | 0 |

해석:

- skewed workload에서는 static 4-thread reduce가 `0.060584s`로 크게 불리해졌습니다.
- 같은 skewed 조건에서 `pipeline_final_b1000`은 `0.036921s`로 개선됩니다. queue scheduling이 불균등 batch를 동적으로 분배하면서 static partition의 약점을 줄인 결과로 해석할 수 있습니다.
- `pipeline_interactive_b1000`은 final merge보다 약간 느리지만, 실시간 partial merge 구조와 synchronization overhead를 보여주는 비교군으로 의미가 있습니다.
- hybrid 계열이 가장 빠르게 나온 것은 process별 큰 단위 분할과 child 내부 thread 병렬화가 skewed 조건에서도 비교적 잘 작동했기 때문입니다. 다만 Docker Desktop VM 환경의 scheduler 영향이 있으므로 절대 순위로 과장하면 안 됩니다.

## 검증 결론

| 검증 항목 | 결론 |
| --- | --- |
| `--ipc shm` correctness | 통과. `process_*_shm`, `hybrid_2x4_shm` 모두 `valid=1`, checksum match |
| `--workload skewed` correctness | 통과. dummy CPU work만 증가시키므로 checksum 유지 |
| synchronization failure evidence | 유지. `thread_4_nosync`는 `valid=0`, checksum mismatch |
| pipeline scheduling evidence | 강화. skewed workload에서 `thread_4_reduce`보다 `pipeline_final_b1000`이 유리 |
| IPC comparison evidence | 강화. pipe와 shm을 같은 process/hybrid 조건에서 비교 가능 |

## 보고서에 쓸 수 있는 냉정한 문장

> 추가 실험 결과, shared memory IPC는 모든 process/hybrid 조건에서 정확성을 유지했고 pipe IPC와 비교 가능한 결과를 제공했다. 다만 전달되는 `Result` 구조체가 작기 때문에 IPC 방식 자체보다 `fork/waitpid` 및 계산 분할 효과가 더 크게 나타났다.

> skewed workload에서는 static partition의 약점이 드러났다. `thread_4_reduce`는 uniform 조건보다 상대적으로 불리해졌고, queue 기반 `pipeline_final_b1000`은 동적 batch 분배를 통해 더 나은 결과를 보였다. 이는 pipeline이 항상 빠르다는 뜻이 아니라, workload imbalance가 있을 때 queue scheduling의 필요성을 보여주는 근거다.

> `thread_4_nosync`는 두 조건 모두에서 checksum mismatch가 발생했으므로 성능 비교 대상이 아니라 race condition 실패 사례로 사용해야 한다.

## 한계

- Docker Desktop 기반 결과는 순수 물리 Linux 결과가 아닙니다. 최종 보고서에는 “Docker Ubuntu Linux VM 환경”이라고 명시해야 합니다.
- `time_sync`는 queue mode에서 worker별 lock/condition wait가 누적될 수 있어 wall-clock stage와 직접 합산하면 안 됩니다.
- `time_ipc`는 parent가 결과를 읽는 구간 중심입니다. child가 pipe write에 소비한 시간까지 완전히 합산하려면 child metrics shared table이 추가로 필요합니다.
