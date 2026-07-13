FROM debian:13-slim

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
