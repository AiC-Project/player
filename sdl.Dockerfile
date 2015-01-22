FROM ubuntu:16.04

# We manually link libGL.so.1 because it is only provided by libgl1-mesa-dev

RUN apt-get -y update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        busybox \
        strace \
        telnet \
        net-tools \
        iputils-ping \
        mtr \
        amqp-tools \
        libav-tools \
        libavcodec-ffmpeg56 \
        libavformat-ffmpeg56 \
        libsdl2-2.0 \
        libswscale-ffmpeg3 \
        libglib2.0-0 \
        libxv1 \
        libgl1-mesa-dri \
        libpopt0 \
        libgl1-mesa-glx \
        libprotobuf-c1 \
        librabbitmq4 \
        mesa-utils \
        libasan2 && \
    ln -s /usr/lib/x86_64-linux-gnu/mesa/libGL.so.1 /usr/lib/x86_64-linux-gnu/libGL.so && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN useradd -m developer

COPY ./sdl/out/player_sdl_grab /home/developer/sdl_sensor_broker/
COPY ./sdl/lib /home/developer/sdl_sensor_broker/lib
COPY ./start_sdl /home/developer/start_sdl

RUN chown -R developer.developer /home/developer

USER developer

WORKDIR /home/developer/sdl_sensor_broker

ENV DISPLAY xorg:0.0

CMD sh /home/developer/start_sdl

