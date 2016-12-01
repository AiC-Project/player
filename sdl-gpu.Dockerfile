FROM ubuntu:16.04

# -----------------------------------------------
# from https://github.com/NVIDIA/nvidia-docker/blob/opengl/ubuntu-14.04/opengl/runtime/Dockerfile

##FROM ubuntu:14.04
##MAINTAINER NVIDIA CORPORATION <digits@nvidia.com>

LABEL com.nvidia.volumes.needed="nvidia_driver"

RUN apt-get update && apt-get install -y --no-install-recommends --force-yes \
        libxau6 \
        libxdmcp6 \
        libxcb1 \
        libxext6 \
        libx11-6 \
        xauth && \
    rm -rf /var/lib/apt/lists/*

RUN echo "/usr/local/nvidia/lib" >> /etc/ld.so.conf.d/nvidia.conf && \
    echo "/usr/local/nvidia/lib64" >> /etc/ld.so.conf.d/nvidia.conf

RUN mkdir -p /opt/nvidia/lib && \
    ln -s /usr/local/nvidia/lib /opt/nvidia/lib/i386-linux-gnu && \
    ln -s /usr/local/nvidia/lib64 /opt/nvidia/lib/x86_64-linux-gnu

ENV PATH /usr/local/nvidia/bin:${PATH}
##ENV LD_LIBRARY_PATH /usr/local/nvidia/lib:/usr/local/nvidia/lib64:${LD_LIBRARY_PATH}
##ENV LD_PRELOAD /opt/nvidia/\$LIB/libGL.so.1:${LD_PRELOAD}

# -----------------------------------------------
# from https://github.com/NVIDIA/nvidia-docker/blob/opengl/ubuntu-14.04/opengl/virtualgl/Dockerfile

##FROM opengl:runtime
##MAINTAINER NVIDIA CORPORATION <digits@nvidia.com>

RUN apt-get update && apt-get install -y --no-install-recommends --force-yes \
        ca-certificates \
        curl \
        libxv1 && \
    rm -rf /var/lib/apt/lists/*

RUN curl -fsSL -o virtualgl.deb https://sourceforge.net/projects/virtualgl/files/2.5/virtualgl_2.5_amd64.deb/download && \
    dpkg -i virtualgl.deb && rm virtualgl.deb

ENV PATH /opt/VirtualGL/bin/:${PATH}

##ENTRYPOINT ["/opt/VirtualGL/bin/vglrun"]

# -----------------------------------------------


# need to wipe these variables otherwise things will panic while building
ENV LD_PRELOAD ""
ENV LD_LIBRARY_PATH ""

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
        libpopt0 \
        libprotobuf-c1 \
        librabbitmq4 \
        mesa-utils \
        libasan2 && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN useradd -m developer

COPY ./sdl/out/player_sdl_grab /home/developer/sdl_sensor_broker/
COPY ./sdl/lib /home/developer/sdl_sensor_broker/lib
COPY ./start_sdl-gpu /home/developer/start_sdl-gpu

RUN chown -R developer.developer /home/developer

USER developer

WORKDIR /home/developer/sdl_sensor_broker

ENV DISPLAY xorg:0.0
ENV LD_LIBRARY_PATH /usr/local/nvidia/lib:/usr/local/nvidia/lib64:${LD_LIBRARY_PATH}
ENV LD_PRELOAD /opt/nvidia/lib/x86_64-linux-gnu/libGL.so.1:${LD_PRELOAD}

CMD sh /home/developer/start_sdl-gpu

