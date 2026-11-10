# Build environment for the Linux and Windows release builds (Ubuntu 24.04; x86_64 for Linux x86_64 and Windows,
# ARM64 for Linux ARM64, which has no Windows compiler):
#   docker build --platform linux/amd64 -t cps2recomp-build tools/release
#   docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src cps2recomp-build tools/release/linux.sh
#   docker build --platform linux/arm64 -t cps2recomp-build-arm64 tools/release
#   docker run --rm --platform linux/arm64 -v "$PWD":/src -w /src cps2recomp-build-arm64 tools/release/linux.sh
FROM ubuntu:24.04
ARG TARGETARCH
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates g++ cmake ninja-build python3 curl file zip unzip git zlib1g-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
    libwayland-dev wayland-protocols libdecor-0-dev libegl-dev libgl-dev \
    libpulse-dev libasound2-dev libudev-dev libdbus-1-dev libpipewire-0.3-dev \
    $([ "$TARGETARCH" = arm64 ] || echo g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix \
        binutils-mingw-w64-x86-64 libz-mingw-w64-dev) \
    && rm -rf /var/lib/apt/lists/*
# (Rust, for librashader: tools/librashader.sh; Ubuntu's rustup, which updates itself only through apt)
ENV RUSTUP_HOME=/opt/rustup CARGO_HOME=/opt/cargo PATH=/opt/cargo/bin:$PATH
RUN apt-get update && apt-get install -y --no-install-recommends rustup \
    && rm -rf /var/lib/apt/lists/* && rustup toolchain install 1.99.1 --profile minimal --no-self-update && rustup default 1.99.1 \
    && { [ "$TARGETARCH" = arm64 ] || rustup target add x86_64-pc-windows-gnu; }
