#!/bin/bash

echo Building docker images...

if [ $# -eq 0 ]; then
    echo "No path to docker image files provided. Using current directory..."
fi

if [ -z "$@" ]; then
	path="."
else
	path="$@"
fi

if [ ! -f $path'/Dockerfile' ]; then
	echo "Dockerfile does not exist!"
	exit 1
fi

docker build -t "custom-ubuntu:20.04" $path'/x64'
if [ $? -ne 0 ]; then
    echo "Failed to build docker image"
    exit 2
fi

echo "Exporting Docker Ubuntu 20.04 x64"
mkdir -p ${path}/images
docker save "custom-ubuntu:20.04" | gzip -c > ${path}'/images/custom-ubuntu-20.04.tar.gz'
if [ $? -ne 0 ]; then
    echo "Failed to save docker image to tarball"
    exit 3
fi

echo "Done creating docker"
exit 0
