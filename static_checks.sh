#!/bin/bash

clang_tidy_bin="/usr/bin/clang-tidy"
build_dir="cmake-build-debug"

run-clang-tidy -clang-tidy-binary "${clang_tidy_bin}" -p "${build_dir}" -quiet
