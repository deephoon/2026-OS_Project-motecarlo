# Linux / Docker 캡처 실행 가이드

이 문서는 발표자료에 사용할 캡처를 **Linux 환경에서 재현 가능하게 찍는 방법**을 정리합니다. 최종 성능 측정은 macOS보다 Docker Ubuntu Linux에서 수행한 결과를 우선 사용합니다.

## 1. Docker 이미지 빌드 캡처

프로젝트 루트에서 실행합니다.

```sh
docker build -t os-montecarlo-risk .
```

캡처 포인트:

- Ubuntu 기반 이미지 사용
- `build-essential`, `make`, `time`, `procps`, `sysstat` 설치
- `RUN make` 성공

추천 캡처 파일:

```text
captures/16_docker_build.png
```

발표 멘트:

```text
Docker Ubuntu Linux 환경을 구성해 macOS 개발 환경과 분리된 Linux 기준 실험을 수행할 수 있도록 했습니다.
```

## 2. Docker 컨테이너 실행 캡처

결과 파일을 호스트에도 남기려면 volume mount 방식이 좋습니다.

```sh
docker run --rm -it -v "$PWD":/workspace -w /workspace os-montecarlo-risk
```

컨테이너 안에서:

```sh
make clean
make
make test
```

캡처 포인트:

- 컨테이너 내부 shell
- `make clean`
- `make`
- `make test`

추천 캡처 파일:

```text
captures/17_docker_run_experiment.png
```

## 3. Docker 내부 중간 발표 실험 캡처

컨테이너 안에서 실행합니다.

```sh
TRIALS=1000000 STEPS=50 scripts/run_midterm.sh
cat results/csv/midterm_results.csv
```

캡처 포인트:

- sequential baseline
- thread reduce 1/2/4/8
- nosync / mutex / reduce
- steps 10/50/100
- `valid` column

발표자료 배치:

- Slide 10: thread scaling 결과
- Slide 11: synchronization 정확성 결과

## 4. nosync valid=0 단독 캡처

컨테이너 안에서 실행합니다.

```sh
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync nosync --seed 42
```

캡처 포인트:

```text
mode=thread
sync=nosync
trials=1000000
hist_sum != 1000000
valid=0
```

추천 캡처 파일:

```text
captures/12_nosync_valid0.png
```

발표 멘트:

```text
nosync는 lock 없이 shared result를 갱신하므로 race condition으로 인해 histogram sum이 trials와 일치하지 않습니다.
```

## 5. mutex / reduce 정확성 비교 캡처

컨테이너 안에서 실행합니다.

```sh
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync mutex --seed 42
./sim --mode thread --threads 4 --trials 1000000 --steps 50 --sync reduce --seed 42
```

캡처 포인트:

```text
mutex valid=1
reduce valid=1
checksum 동일
reduce가 mutex보다 빠른 경향
```

추천 캡처 파일:

```text
captures/13_sync_compare.png
```

## 6. CPU utilization 캡처

컨테이너 안에서 긴 workload를 실행합니다.

터미널 1:

```sh
docker run --rm -it -v "$PWD":/workspace -w /workspace os-montecarlo-risk
```

컨테이너 안:

```sh
make
./sim --mode thread --threads 4 --trials 50000000 --steps 100 --sync reduce --seed 42
```

터미널 2:

```sh
docker ps
```

같은 컨테이너에 접속:

```sh
docker exec -it <container_id> bash
```

컨테이너 안에서:

```sh
pidstat -u -r -C sim 1
```

대체:

```sh
top
```

캡처 포인트:

- `sim` 프로세스의 CPU 사용률
- thread 실행 중 CPU 사용률이 올라간 장면
- 가능하면 4 threads 기준 높은 CPU 사용률

추천 캡처 파일:

```text
captures/18_cpu_pidstat.png
```

주의:

- workload가 너무 짧으면 `pidstat`를 켜기 전에 실행이 끝납니다.
- 이 경우 `trials`를 `50000000` 이상으로 늘립니다.
- Docker Desktop을 사용한다면 Docker에 할당된 CPU core 수를 확인합니다.

## 7. Docker Compose 방식

Compose를 쓰는 경우:

```sh
docker compose build
docker compose run --rm os-sim
```

컨테이너 안:

```sh
make clean
make
TRIALS=1000000 STEPS=50 scripts/run_midterm.sh
cat results/csv/midterm_results.csv
```

장점:

- repo의 `docker-compose.yml` 구성을 보여주기 좋음
- volume mount가 자동으로 적용됨

## 8. 캡처 시 터미널 화면 정리 팁

캡처 전에 터미널을 넓게 만듭니다.

```sh
clear
```

출력이 너무 길면 필요한 부분만 잘라 봅니다.

```sh
sed -n '1,12p' results/csv/midterm_results.csv
```

코드는 line number와 함께 캡처합니다.

```sh
nl -ba src/thread_mode.c | sed -n '20,45p'
nl -ba src/simulation.c | sed -n '120,150p'
```

## 9. Linux 캡처 결과를 발표자료에 넣는 기준

우선순위:

1. Docker Linux 결과
2. macOS 로컬 결과
3. 코드/스크립트 캡처

발표자료에는 다음처럼 표기합니다.

```text
Environment: Docker Ubuntu Linux
Trials: 1,000,000
Steps: 50
Seed: 42
```

macOS 결과를 넣는 경우:

```text
해당 결과는 로컬 개발 환경에서 측정한 예시이며,
최종 보고서에서는 Docker Linux 기준 반복 측정 결과로 대체할 예정입니다.
```

