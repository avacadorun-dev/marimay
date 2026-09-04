FROM docker.gitea.com/runner-images:ubuntu-latest@sha256:fd911d7417bfbf0f454530e447da95b58001e1df41bbc5e1a8dd35d432575aae

USER root

RUN sed -i \
      's|http://archive.ubuntu.com/ubuntu|http://ru.archive.ubuntu.com/ubuntu|g' \
      /etc/apt/sources.list.d/ubuntu.sources; \
    export DEBIAN_FRONTEND=noninteractive; \
    apt-get -o Acquire::ForceIPv4=true \
      -o Acquire::Retries=10 \
      -o Acquire::http::Timeout=30 \
      -o Acquire::https::Timeout=30 \
      -o Dpkg::Use-Pty=0 update; \
    apt-get -o Acquire::ForceIPv4=true \
      -o Acquire::Retries=10 \
      -o Acquire::http::Timeout=30 \
      -o Acquire::https::Timeout=30 \
      -o Dpkg::Use-Pty=0 install --yes --no-install-recommends \
        build-essential \
        clang \
        cmake \
        libclang-rt-18-dev \
        llvm-18 \
        python3; \
    command -v ar; \
    command -v clang; \
    command -v cmake; \
    command -v gcc; \
    command -v llvm-symbolizer-18; \
    command -v make; \
    command -v python3; \
    rm -rf /var/lib/apt/lists/*

LABEL org.opencontainers.image.title="Marimay Gitea C17 CI toolchain"
LABEL org.opencontainers.image.description="Pinned Linux x86_64 compiler and sanitizer environment for Marimay CI"
