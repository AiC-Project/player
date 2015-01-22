FROM ubuntu:16.04

RUN apt-get -y update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential \
        cmake \
        doxygen \
        cppcheck \
        libavformat-dev \
        libswscale-dev \
        libsdl2-dev \
        libprotobuf-c-dev \
        libprotobuf-c1 \
        protobuf-c-compiler \
        libasan2 \
        librabbitmq4 \
        librabbitmq-dev \
        libgl1-mesa-dev && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ARG USER_ID
ARG GROUP_ID

RUN addgroup --gid $GROUP_ID developer
RUN adduser --uid $USER_ID --gid $GROUP_ID --disabled-login --gecos "" developer
USER developer
WORKDIR /home/volume

