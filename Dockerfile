FROM node:22-bookworm-slim AS node

FROM debian:13-slim

COPY --from=node /usr/local/ /usr/local/
ENV NPM_CONFIG_CACHE=/tmp/rtlplayground-npm-cache

RUN apt-get update && apt-get install -y \
    make \
    gcc \
    sdcc \
    xxd \
    python3 \
    libjson-c-dev \
    golang-go \
    git \
    && rm -rf /var/lib/apt/lists/*

# git safe.directory for mounted repos (Makefile uses git describe)
RUN git config --global --add safe.directory /workspace

WORKDIR /workspace

CMD ["bash"]
