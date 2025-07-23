#!/bin/bash

set -e

for arg in "$@"; do
    case $arg in
    --src_path=*)
        SRC_PATH="${arg#*=}"
        ;;
    --build_type=*)
        BUILD_TYPE="${arg#*=}"
        ;;
    --targets=*)
        TARGETS="${arg#*=}"
        ;;
    --parallel_jobs=*)
        PARALLEL_JOBS="${arg#*=}"
        ;;
    *)
        echo "Unknown option: "$i
        echo "Available options: --build_type=Debug/Release --targets=..."
        exit 1
        ;;
    esac
done

if [[ -z ${SRC_PATH} ]]; then
    echo "Unset source path, use current path"
    src_path=`readlink -e .`
else
    src_path=`readlink -e ${SRC_PATH}`
fi

if [[ -z "$TARGETS" ]]; then
    echo "Building all targets..."
    targets=("all")
else
    IFS=',' read -ra targets <<< "$TARGETS"
fi

if [[ -z "${PARALLEL_JOBS}" || "${PARALLEL_JOBS}" -le 0 ]]; then
    echo "Use default parallel jobs"
    PARALLEL_JOBS=$(nproc)
fi

if [[ -z ${BUILD_TYPE} ]]; then
    echo "Unset build type, use 'Debug'"
    BUILD_TYPE="Debug"
fi

if [ -f /.dockerenv ]; then # deprecated checks, but do it anyway
    # in container
    build_dir="${src_path}/build-cont-${BUILD_TYPE}"
else
    build_dir="${src_path}/build-${BUILD_TYPE}"
fi

c_compiler="/usr/bin/cc"
cxx_compiler="/usr/bin/c++"

# cat > build.env <<EOF
# build_type : ${BUILD_TYPE}
# c_compiler  : ${c_compiler}
# cxx_compiler: ${cxx_compiler}
# EOF

# cat build.env

echo "Build binaries to: ${build_dir} with ${PARALLEL_JOBS} cores"
mkdir -p ${build_dir}
# if [ ! $(pwd) == $(realpath ${build_dir}) ]; then
echo "Cleanup build directory: "${build_dir}
rm -rf ${build_dir}
# fi

cmake -G "Ninja" -B ${build_dir} -DCMAKE_C_COMPILER=${c_compiler} \
    -DCMAKE_CXX_COMPILER=${cxx_compiler} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DUSE_OPENSSL=ON -DBUILD_TEST=ON ${src_path}

cmake --build ${build_dir} -j${PARALLEL_JOBS} -t "${targets[@]}"
echo "Build successful"
