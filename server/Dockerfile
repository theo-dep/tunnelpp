# syntax=docker/dockerfile:1

FROM ubuntu:24.04 AS builder
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates clang cmake curl git libssl-dev ninja-build pkg-config tar zip unzip \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_ROOT=/opt/vcpkg
COPY vcpkg.json CMakeLists.txt CMakePresets.json ./
COPY common ./common
COPY server ./server
RUN cmake --preset clang-release -DTUNNELPP_BUILD_CLIENT=OFF -DTUNNELPP_BUILD_TESTS=OFF \
    && cmake --build --preset clang-release

FROM ubuntu:24.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libssl3 libstdc++6 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/clang-release/Release/tunnelpp-server /usr/local/bin/tunnelpp-server
EXPOSE 8080 8081
ENTRYPOINT ["tunnelpp-server"]
