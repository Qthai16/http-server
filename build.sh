#! /bin/bash

if [ $# -eq 0 ]; then
    echo "No source files path provided, using current directory..."
    source_dir="."
fi

for i in "$@"
do
case $i in
    --source_dir=*)
        source_dir="${i#*=}"
    ;;
    *)
        # unknown option
        echo "Unknown option: "$i
        echo "Available options: --source_dir=/path_to_source"
    ;;
esac
done

source_dir_canonical=$(readlink -e "${source_dir}")

mkdir -p ${source_dir_canonical}/build-docker
cd ${source_dir_canonical}/build-docker
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_OPENSSL=OFF -DUSE_LIBCURL=OFF ..
make -j 8