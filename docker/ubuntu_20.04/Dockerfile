# === Stage 1: Install tools ===
FROM ubuntu:20.04 AS base
LABEL Description="ubuntu 20.04 base"

# ARG SOURCE_LIST="sources.list"
ARG INSTALL_PATH="/usr/local"

RUN useradd builder
# COPY ${SOURCE_LIST} /etc/apt/sources.list
#disable cert checking
COPY ./01-vendor-ubuntu /etc/apt/apt.conf.d/01-vendor-ubuntu

RUN apt-get update && \
    apt-get install -y --no-install-recommends apt-utils && \
    apt-get install -y --no-install-recommends build-essential git curl wget \
    make rsync autoconf python3 vim ca-certificates ninja-build

RUN apt-get install -y --no-install-recommends libssl-dev libcurl4-openssl-dev zlib1g-dev
RUN mkdir -p /tmp/pkgs

# cmake
RUN wget -O /tmp/pkgs/cmake_install.sh https://github.com/Kitware/CMake/releases/download/v3.20.2/cmake-3.20.2-linux-x86_64.sh && \
    chmod +x /tmp/pkgs/cmake_install.sh && \
    /tmp/pkgs/cmake_install.sh --prefix=/usr --skip-license

# gtest
RUN wget -O /tmp/pkgs/google-test.tar.gz https://github.com/google/googletest/releases/download/v1.16.0/googletest-1.16.0.tar.gz && \
    tar xf /tmp/pkgs/google-test.tar.gz -C /tmp/pkgs && \
    cd /tmp/pkgs/googletest-1.16.0 && \
    cmake . && \
    cmake --build . -j `nproc` && \
    cmake --install . --prefix ${INSTALL_PATH}

# zstd
RUN wget -O /tmp/pkgs/zstd.tar.gz https://github.com/facebook/zstd/archive/refs/tags/v1.5.5.tar.gz && \
    tar xf /tmp/pkgs/zstd.tar.gz -C /tmp/pkgs && \
    cd /tmp/pkgs/zstd-1.5.5 && \
    cmake -S build/cmake -DCMAKE_OSX_ARCHITECTURES="x86_64;x86_64h;arm64" \
        -DCMAKE_PREFIX_PATH=${INSTALL_PATH} \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_PATH} \
        -DZSTD_BUILD_SHARED=OFF . && \
    cmake --build . -j`nproc` && \
    cmake --install . --prefix ${INSTALL_PATH}

# boost
RUN wget -O /tmp/pkgs/boost.tar.gz https://archives.boost.io/release/1.79.0/source/boost_1_79_0.tar.gz && \
    tar xf /tmp/pkgs/boost.tar.gz -C /tmp/pkgs && \
    cd /tmp/pkgs/boost_1_79_0 && \
    ./bootstrap.sh --prefix=${INSTALL_PATH} --with-libraries=atomic,date_time,thread,test && \
    ./b2 link=static -j`nproc` install

RUN apt-get -y autoremove && \
    apt-get clean all && \
    rm -rf /tmp/*

USER builder
WORKDIR /home/builder
ENTRYPOINT ["/bin/bash", "-c"]
