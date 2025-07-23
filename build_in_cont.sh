#!/bin/bash

target_os="ubuntu"

docker run --rm -v $(pwd):/home/builder/http-server \
    ${target_os}-builder:1.0 \
    "/home/builder/http-server/build_local.sh --src_path=/home/builder/http-server --build_type=Release --parallel_jobs=4"
