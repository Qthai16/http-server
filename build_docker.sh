#! /bin/bash

docker run -v $(readlink -e '.'):/opt/http-server custom-ubuntu:20.04 /opt/http-server/build.sh --source_dir="/opt/http-server"