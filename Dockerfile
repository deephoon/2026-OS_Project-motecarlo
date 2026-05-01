FROM ubuntu:22.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        make \
        time \
        procps \
        sysstat \
        htop \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . .
RUN make clean
RUN make

CMD ["/bin/bash"]
