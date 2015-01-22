FROM ubuntu:16.04

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
        libsdl1.2debian \
        libswscale-ffmpeg3 \
        libglib2.0-0 \
        libpopt0 \
        libprotobuf-c1 \
        librabbitmq4 && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN useradd -m developer

COPY ./sdl/out/player_audio /home/developer/sdl_sensor_broker/
COPY ./sdl/lib /home/developer/sdl_sensor_broker/lib
COPY ./start_audio /home/developer/start_audio

RUN chown -R developer.developer /home/developer

USER developer

WORKDIR /home/developer/sdl_sensor_broker

CMD sh /home/developer/start_audio

