# syntax=docker/dockerfile:1

# Stage 1: build tunnelpp-server with CMake + vcpkg + clang
FROM ubuntu:25.10 AS builder

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        clang \
        clang-tools \
        cmake \
        curl \
        git \
        libssl-dev \
        linux-libc-dev \
        make \
        ninja-build \
        pkg-config \
        tar \
        unzip \
        zip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Check out vcpkg at exactly the commit pinned by "builtin-baseline" in vcpkg.json
COPY vcpkg.json ./
RUN baseline="$(sed -n 's/.*"builtin-baseline": *"\([0-9a-f]\{40\}\)".*/\1/p' vcpkg.json)" \
    && git init /opt/vcpkg \
    && git -C /opt/vcpkg remote add origin https://github.com/microsoft/vcpkg.git \
    && git -C /opt/vcpkg fetch --depth 1 origin "$baseline" \
    && git -C /opt/vcpkg checkout --detach FETCH_HEAD \
    && /opt/vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg

# Copy the build files before the sources to keep cached the expensive vcpkg layer
COPY CMakeLists.txt CMakePresets.json ./
COPY common ./common
COPY server ./server

# Server only: the client is a CLI distributed through npm, no tests
RUN cmake --preset clang -DTUNNELPP_BUILD_CLIENT=OFF -DTUNNELPP_BUILD_TESTS=OFF \
    && cmake --build --preset clang-release

# Stage 2: minimal runtime image, the server binary plus its shared libraries
FROM ubuntu:25.10

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        libssl3 \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/clang/server/Release/tunnelpp-server /usr/local/bin/tunnelpp-server

# Public HTTP listener and the admin listener
EXPOSE 8080 8081

ENTRYPOINT ["tunnelpp-server"]
