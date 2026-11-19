# The image of the CI jobs: everything that the jobs used to install again and
# again with apt (the build dependencies, the tools and clang-tidy 22 from
# apt.llvm.org). The workflow ci-image.yml builds it and publishes it to
# ghcr.io; ci.yml runs its jobs in it. Rebuilt on a change of this file and
# every week, for the security updates of Ubuntu.
FROM ubuntu:24.04

LABEL org.opencontainers.image.source="https://github.com/ondratu/tiny-shell"
LABEL org.opencontainers.image.description="Build and check tools of the tiny-shell CI"

ENV DEBIAN_FRONTEND=noninteractive

# Clang 19 is the one of Ubuntu (the jobs used it before, and the size limit
# is measured with it), so it is installed before the repository of LLVM is
# added; that one is only for clang-tidy 22. libstdc++-14-dev is there because
# the runners of GitHub have it, Clang uses the newest libstdc++ and the size
# of the binary depends on it.
RUN printf 'Acquire::Retries "5";\n' > /etc/apt/apt.conf.d/80-retries \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
        ca-certificates wget gnupg \
        build-essential pkg-config git gh ccache cppcheck \
        libx11-dev libxft-dev libxrender-dev libxext-dev libxrandr-dev libxinerama-dev \
        libfontconfig-dev xvfb xauth libgtest-dev libpam0g-dev \
        clang-19 llvm-19 libclang-rt-19-dev libstdc++-14-dev \
 && wget -qO /etc/apt/trusted.gpg.d/apt.llvm.org.asc --tries=5 --retry-connrefused --waitretry=10 \
        https://apt.llvm.org/llvm-snapshot.gpg.key \
 && echo "deb http://apt.llvm.org/noble/ llvm-toolchain-noble-22 main" > /etc/apt/sources.list.d/llvm.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends clang-tidy-22 \
 && rm -rf /var/lib/apt/lists/*
