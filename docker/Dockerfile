FROM ubuntu:20.04
MAINTAINER Thai Pham <phamquangthai97@gmail.com>
LABEL Description="Ubuntu x64"

#This is a workaround due to ICAP blocking http://ubuntu.com breaking apt-get update
COPY ./sources.list /etc/apt/sources.list
#disable cert checking
COPY ./01-vendor-ubuntu /etc/apt/apt.conf.d/01-vendor-ubuntu

COPY ./setup_build_env.sh /opt
RUN adduser --disabled-password --gecos "" builder
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections
RUN /opt/setup_build_env.sh

WORKDIR /opt
