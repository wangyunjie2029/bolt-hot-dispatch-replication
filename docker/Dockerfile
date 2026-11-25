# syntax=docker/dockerfile:1
# ---------------------------------------------------------------------------
# Stage 1: build llvm-bolt from a pinned LLVM mainline commit with the
#          Hot Dispatch Replication pass applied on top.
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS builder

ARG LLVM_COMMIT=88658a4729f40f3b3bfb86d87d9d460e5098d510
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates \
        zlib1g-dev libzstd-dev libedit-dev libncurses-dev python3 \
    && rm -rf /var/lib/apt/lists/*

# Fetch exactly the pinned commit (blob-less clone keeps the layer small).
RUN git init /src \
 && cd /src \
 && git remote add origin https://github.com/llvm/llvm-project.git \
 && git fetch --depth 1 origin ${LLVM_COMMIT} \
 && git checkout FETCH_HEAD

COPY patch/hot-dispatch-replication.patch /tmp/pass.patch
RUN cd /src && git apply --stat /tmp/pass.patch && git apply /tmp/pass.patch

RUN cmake -G Ninja -S /src/llvm -B /build \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_ENABLE_ASSERTIONS=OFF \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        -DLLVM_ENABLE_PROJECTS=bolt \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLLVM_INCLUDE_BENCHMARKS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_ENABLE_ZLIB=ON -DLLVM_ENABLE_ZSTD=ON \
 && ninja -C /build llvm-bolt

# ---------------------------------------------------------------------------
# Stage 2: runtime image - llvm-bolt + workloads + experiment scripts.
# ---------------------------------------------------------------------------
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        clang-18 gcc g++ make binutils python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/bin/llvm-bolt /opt/bolt/bin/llvm-bolt
COPY bench/ /opt/bench/
COPY scripts/ /opt/scripts/
COPY docs/ /opt/docs/
COPY patch/ /opt/patch/

ENV BOLT=/opt/bolt/bin/llvm-bolt \
    BENCH=/opt/bench \
    WORK=/work

WORKDIR /work
ENTRYPOINT ["/bin/bash"]
