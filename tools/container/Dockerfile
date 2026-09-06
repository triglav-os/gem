# Optional, pinned Linux build environment; all compilers run inside Docker.
FROM ubuntu:24.04@sha256:33ceb71981b602c1a7443a53469e4dba065f7503eab3078a2d7a57a2ab987517
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config libsdl2-dev imagemagick python3 \
    clang-18 clang-format-18 gdb ca-certificates git \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
