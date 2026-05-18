# 최종 프로젝트 팀 회의 템플릿

> 목적: `os26_project.pdf` 요구사항에 맞춰 남은 작업을 명확히 나누고, 최종 발표/보고서에서 방어 가능한 실험 결과를 만들기 위한 회의 문서입니다.

---

## 1. 회의 기본 정보

| 항목 | 내용 |
| --- | --- |
| 회의 날짜 | 2026-05-18 |
| 회의 목적 | 최종 프로젝트 역할 분담, 실험 계획 확정, 보고서/PPT 책임자 지정 |
| 참석자 | 정재훈, 성도연, 유지원, 김태환, 정종근 |
| 최종 목표 | process, thread, synchronization, IPC, pipeline trade-off를 실험으로 증명 |

---

## 2. 오늘 회의에서 반드시 결정할 것

| 결정 항목 | 결정 내용 |
| --- | --- |
| 최종 실험 환경 | Docker Ubuntu Linux 기준으로 측정할지 확정 |
| 최종 실험 조건 | `TRIALS`, `STEPS`, `REPEATS`, `PRE_WORK`, `POST_WORK` 값 확정 |
| 역할 분담 | 팀원별 담당 코드/실험/문서/PPT 구역 확정 |
| 발표 흐름 | 문제 인식 → 개선 구조 → 실험 결과 → 한계와 결론 |
| 마감 기준 | 언제까지 실험 CSV, 그래프, PPT, 보고서 초안을 완성할지 확정 |

---

## 3. 현재 프로젝트 상태 요약

현재 프로젝트는 단순 Monte Carlo 병렬화에서 출발했지만, 최종 프로젝트 가이드에 맞춰 다음 구조까지 확장되었습니다.

```text
Pre-processing
  -> TaskQueue
  -> Thread / Process / Hybrid / Pipeline Workers
  -> Final Reduce or Interactive Merge
  -> Post-processing
  -> CSV Metrics
```

현재 구현된 핵심 기능:

| 항목 | 상태 |
| --- | --- |
| Sequential baseline | 구현 완료 |
| pthread thread mode | 구현 완료 |
| `nosync`, `mutex`, `reduce` 비교 | 구현 완료 |
| process mode | 구현 완료 |
| hybrid process + thread mode | 구현 완료 |
| pipeline task queue | 구현 완료 |
| interactive merge | 구현 완료 |
| pipe IPC | 구현 완료 |
| stage time metrics | 구현 완료 |
| `--pre-work`, `--post-work` | 추가 완료 |
| 반복 실험 자동화 | `scripts/run_final.sh` 추가 완료 |
| 결과 후처리 | `scripts/analyze_results.py` 추가 완료 |

아직 남은 핵심 작업:

| 항목 | 필요 이유 |
| --- | --- |
| Docker Linux에서 큰 workload 반복 실험 | 최종 성능 주장 신뢰도 확보 |
| `final_analyzed.csv` 기반 그래프 생성 | 발표/PPT 가독성 확보 |
| CPU/memory 캡처 | PDF 요구사항의 성능 분석 보강 |
| 보고서에 AI 사용 기록 추가 | 가이드 요구사항 대응 |
| 팀원별 기여도 정리 | 최종 보고서 필수 방어 요소 |

---

## 4. 교수님 피드백 반영 여부

| 피드백 | 현재 반영 | 회의에서 확인할 것 |
| --- | --- | --- |
| 바로 병렬화되어 너무 쉬움 | `pre/post stage`, `--pre-work`, `--post-work` 추가 | 최종 실험에서 이 옵션을 어느 정도로 줄지 결정 |
| Amdahl's Law가 보이는 환경 필요 | `T_pre`, `T_sync`, `T_merge`, `T_post` 측정 | sequential fraction을 보고서에 어떻게 설명할지 결정 |
| semaphore보다 mutex + 보완 구조 고민 | `mutex + condition variable` queue 구현 | semaphore 미사용 이유를 발표에서 누가 설명할지 결정 |
| final reduce만으로는 단순함 | `interactive merge`, `aggregator thread` 구현 | final vs interactive 결과를 어떤 그래프로 보여줄지 결정 |

---

## 5. 추천 역할 분담

아래 역할 분담은 “누가 더 많이 했는가”보다 “최종 제출물에서 각자 책임질 수 있는 영역” 기준입니다.

| 팀원 | 추천 담당 | 구체 작업 | 최종 산출물 |
| --- | --- | --- | --- |
| 정재훈 | 총괄 / 통합 / 발표 흐름 | 전체 구조 점검, README/보고서 흐름 정리, 최종 발표 스토리 구성 | 최종 README, 발표 시나리오, 전체 결론 |
| 성도연 | synchronization / pipeline | `TaskQueue`, `MergeQueue`, mutex+condvar, final vs interactive merge 설명 | sync/pipeline 설명 자료, 코드 캡처, overhead 해석 |
| 유지원 | simulation / metrics / Amdahl 분석 | simulation model 설명, `T_pre/T_compute/T_sync/T_merge/T_post` 해석, speedup/efficiency 그래프 | Amdahl 분석 표, stage time 그래프 |
| 김태환 | process / hybrid / IPC | `fork`, `pipe`, `waitpid`, process vs thread, hybrid 구조 설명 | process/hybrid 비교표, IPC overhead 해석 |
| 정종근 | 실험 결과 정리 보조 / 발표자료 보조 | 생성된 CSV 확인, 표 정리, 그래프 이미지 삽입, 오탈자/형식 확인 | 결과 표 정리본, PPT 시각자료 보조 |

### 정종근님 역할 배정 기준

정종근님은 외국분이라 어려운 코드 수정이나 복잡한 한국어 보고서 논리 작성보다는, **명확한 입력과 출력이 있는 작업**을 맡기는 것이 현실적입니다.

추천 작업:

| 작업 | 이유 |
| --- | --- |
| `final_analyzed.csv`에서 주요 수치 복사/정리 | 숫자 중심이라 언어 부담이 적음 |
| 그래프 이미지 PPT에 배치 | 시각 작업 중심이라 부담이 낮음 |
| 실행 결과의 `valid_all`, `matches_seq_checksum` 확인 | 체크리스트형 작업이라 명확함 |
| 발표자료 영어 키워드 정리 | 외국인 팀원 강점을 살릴 수 있음 |
| 팀 회의록 간단 정리 | 복잡한 기술 판단 없이 기록 중심 |

피해야 할 작업:

| 작업 | 이유 |
| --- | --- |
| `process/hybrid` 코드 수정 | fork/IPC 디버깅 난이도가 높음 |
| Amdahl's Law 핵심 해석 작성 | 한국어 보고서 논리와 수식 설명이 필요함 |
| 최종 결론 문장 작성 | 교수님 피드백과 프로젝트 맥락 이해가 많이 필요함 |
| race condition 디버깅 | 재현성과 설명 난이도가 높음 |

---

## 6. 오늘 확정할 실험 조건

기본 권장값:

```sh
TRIALS=100000 STEPS=50 REPEATS=5 \
PRE_WORK=50000 POST_WORK=10000 scripts/run_final.sh
```

최종 보고서용 강한 조건:

```sh
TRIALS=1000000 STEPS=100 REPEATS=5 \
PRE_WORK=50000 POST_WORK=10000 scripts/run_final.sh
```

결과 파일:

```text
results/csv/final_raw.csv
results/csv/final_analyzed.csv
results/csv/final_summary.md
```

회의에서 결정:

| 항목 | 결정값 |
| --- | --- |
| 최종 `TRIALS` |  |
| 최종 `STEPS` |  |
| 최종 `REPEATS` |  |
| 최종 `PRE_WORK` |  |
| 최종 `POST_WORK` |  |
| 실험 실행 담당자 |  |
| 그래프 생성 담당자 |  |

---

## 7. 최종 보고서/PPT 구성안

| 파트 | 핵심 내용 | 담당 |
| --- | --- | --- |
| 1. 문제 정의 | 단순 병렬화가 너무 쉬웠고 OS 분석 요소가 부족했다 | 정재훈 |
| 2. 전체 구조 | pre → queue → worker → merge → post | 정재훈 |
| 3. Thread & Sync | nosync/mutex/reduce 비교, race condition | 성도연 |
| 4. Pipeline & Merge | final reduce vs interactive merge | 성도연 |
| 5. Simulation & Metrics | Monte Carlo workload, stage time, checksum | 유지원 |
| 6. Amdahl Analysis | sequential fraction, speedup, efficiency | 유지원 |
| 7. Process & IPC | fork, pipe, waitpid, process overhead | 김태환 |
| 8. Hybrid | process 내부 thread 구조와 trade-off | 김태환 |
| 9. 실험 결과 표/그래프 | `final_analyzed.csv` 기반 시각화 | 정종근 |
| 10. 결론 | 가장 빠른 구조보다 OS trade-off 분석이 핵심 | 정재훈 |

---

## 8. 팀원별 오늘 할 일 체크리스트

### 정재훈

- [ ] 전체 발표 흐름 확정
- [ ] README와 최종 보고서 구조 맞추기
- [ ] 팀원별 산출물 취합
- [ ] 최종 결론 문장 작성

### 성도연

- [ ] `task_queue.c`, `merge_queue.c`, `pipeline_mode.c` 핵심 코드 캡처
- [ ] mutex + condition variable 사용 이유 정리
- [ ] final reduce vs interactive merge 비교 설명 작성
- [ ] 작은 workload에서 interactive merge가 느릴 수 있는 이유 정리

### 유지원

- [ ] simulation model 설명 정리
- [ ] `time_total`, `T_pre`, `T_compute`, `T_sync`, `T_merge`, `T_post` 의미 정리
- [ ] `final_analyzed.csv`에서 speedup/efficiency 표 확인
- [ ] Amdahl's Law 해석 문장 작성

### 김태환

- [ ] `process_mode.c`, `hybrid_mode.c`, `ipc_pipe.c` 핵심 코드 캡처
- [ ] process 1/2/4 비교 결과 정리
- [ ] hybrid 2x2, 2x4, 4x2 비교 결과 정리
- [ ] fork/IPC overhead 해석 작성

### 정종근

- [ ] `final_summary.md`에서 fastest case 표 확인
- [ ] `final_analyzed.csv`에서 주요 표를 PPT용으로 정리
- [ ] `valid_all`, `matches_seq_checksum`가 1인지 체크
- [ ] 그래프 또는 표 이미지 PPT에 삽입
- [ ] 발표자료 영어 키워드 또는 짧은 설명 문장 정리

---

## 9. 회의 진행 순서

### 0분-5분: 목표 재확인

```text
우리 목표는 자동차 시뮬레이터를 잘 만드는 것이 아니라,
process/thread/synchronization/IPC/pipeline의 성능 trade-off를 보여주는 것이다.
```

### 5분-15분: 현재 구현 상태 공유

- 구현 완료된 mode 확인
- `make test` 통과 여부 확인
- checksum이 모든 mode에서 같은지 확인

### 15분-30분: 실험 조건 확정

- Docker Linux에서 실행할지 결정
- `TRIALS`, `STEPS`, `REPEATS`, `PRE_WORK`, `POST_WORK` 확정
- 누가 실험을 돌릴지 결정

### 30분-45분: 역할 분담 확정

- 위 추천 역할표를 기준으로 담당자 확정
- 각자 제출할 산출물 형식 정하기
- 정종근님은 CSV/표/PPT 보조 중심으로 배정

### 45분-60분: 보고서/PPT 흐름 확정

- 발표 스토리 순서 확정
- 필요한 그래프 목록 확정
- 마감 시간 확정

---

## 10. 최종 그래프 추천 목록

| 그래프 | 목적 | 담당 |
| --- | --- | --- |
| Thread 수별 `avg_time_total` | thread scaling 확인 | 유지원 |
| Thread 수별 speedup/efficiency | 병렬화 효율 확인 | 유지원 |
| nosync/mutex/reduce 비교 | synchronization 필요성 설명 | 성도연 |
| process 1/2/4 비교 | child process overhead 설명 | 김태환 |
| hybrid 조합 비교 | process+thread trade-off 설명 | 김태환 |
| final vs interactive merge | merge 방식 trade-off 설명 | 성도연 |
| batch size별 pipeline 성능 | queue granularity 영향 설명 | 정종근 |
| stage time stacked bar | Amdahl's Law 설명 | 유지원 |

---

## 11. 최종 결론 초안

```text
이 프로젝트는 Monte Carlo 차량 추종 위험 계산을 workload로 사용했지만,
핵심 목적은 차량 모델이 아니라 OS 실행 구조 비교이다.

초기 구조는 trial들이 독립적이라 병렬화가 너무 쉬웠기 때문에,
pre-processing, post-processing, task queue, merge queue, IPC, synchronization을 추가해
실제 시스템에 가까운 처리 구조로 확장했다.

실험 결과는 thread reduce가 작은 CPU-bound workload에서 가장 실용적일 수 있음을 보여주지만,
process, hybrid, pipeline, interactive merge는 각각 격리성, 구조적 확장성,
실시간 병합 가능성이라는 장점과 overhead를 함께 가진다.

따라서 최종 결론은 가장 빠른 방식 하나가 아니라,
process/thread/synchronization/IPC/pipeline 구조별 trade-off를 정량적으로 분석했다는 점이다.
```

---

## 12. 회의 후 액션 아이템

| 작업 | 담당 | 마감 | 완료 |
| --- | --- | --- | --- |
| Docker Linux 최종 실험 실행 |  |  | [ ] |
| `final_analyzed.csv` 검토 |  |  | [ ] |
| 그래프 생성 |  |  | [ ] |
| 코드 캡처 정리 |  |  | [ ] |
| PPT 초안 작성 |  |  | [ ] |
| 보고서 초안 작성 |  |  | [ ] |
| AI 사용 기록 정리 |  |  | [ ] |
| 최종 리허설 |  |  | [ ] |

