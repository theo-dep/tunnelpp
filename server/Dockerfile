# syntax=docker/dockerfile:1

# The image packages the release binary instead of compiling the server: the download URL
# must match the asset names of the "build" matrix in .github/workflows/release.yml.
# Base and assets are both ubuntu-26.04: the glibc the binary was linked against must not be
# newer than the one in the base image.
FROM ubuntu:26.04

ARG DEBIAN_FRONTEND=noninteractive

# Release version to package (e.g. 1.2.3): the server binary is downloaded from the
# corresponding GitHub release, so it must already be published.
ARG VERSION

# Set automatically by BuildKit, maps TARGETARCH (amd64/arm64) to the asset architecture
ARG TARGETARCH

RUN if [ -z "$VERSION" ]; then \
        echo "VERSION build-arg is required (e.g. --build-arg VERSION=1.2.3)" >&2; exit 1; \
    fi

RUN case "$TARGETARCH" in \
        amd64) asset_arch=x64 ;; \
        arm64) asset_arch=arm64 ;; \
        *) echo "unsupported architecture: $TARGETARCH" >&2 && exit 1 ;; \
    esac \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
           ca-certificates \
           curl \
           libssl3 \
           libstdc++6 \
       && rm -rf /var/lib/apt/lists/* \
    && curl -fsSL -o /tmp/tunnelpp-server.tar.gz \
           "https://github.com/theo-dep/tunnelpp/releases/download/v${VERSION}/tunnelpp-server-linux-${asset_arch}.tar.gz" \
    && tar -xzf /tmp/tunnelpp-server.tar.gz -C /tmp \
    && install -m 755 /tmp/tunnelpp-server /usr/local/bin/tunnelpp-server \
    && rm -rf /tmp/tunnelpp-server.tar.gz /tmp/tunnelpp-server

# Public HTTP listener port (informational; override at runtime with TUNNELPP_HTTP_PORT
# and map with -p). The admin listener was removed.
ARG HTTP_PORT=8080
EXPOSE ${HTTP_PORT}

ENTRYPOINT ["tunnelpp-server"]
