#!/bin/bash
docker build --build-arg INSTALL_PATH=/usr/local \
    -f ubuntu_20.04/Dockerfile \
    -t "ubuntu-builder:1.0" .