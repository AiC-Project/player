# AiCPlayer - README

The AiC Player is the interface between sensors emitters and
the virtual android machine. It is split into several executables:

- The most important one, which we can call "player", does hardware-accelerated
  rendering for the android UI, using the remote rendering libs (mostly vanilla)
  from the google emulator. Because it has access to the graphical interface, it
  also contains code required to record screenshots and videos from the graphical
  output.
- The "player_audio" is a separate executable which connects to a custom port on
  the virtual machine in order to receive a raw PCM stream of the audio output of
  android. Its job is to copy that stream to a local ffserver instance, in order
  to transcode it and present it elsewhere (for example a web interface).
- The "player_sensors"’ job is to dispatch protocol buffers containing sensor data
  from an AMQP queue to the different TCP ports for those sensors on the VM.
  Since each sensor has its dedicated AMQP queue, player_sensors does not need
  to filter or even read the content of the protocol buffer, only to transfer it.

# Building

---------

TODO: Docker instructions

---------

- Building and running only work under a moderately modern linux distribution
- Building the whole package requires libffmpeg>=2.8, pthreads, libx11, glib 2.0, libsdl2 (2.0.4), [protobuf-c](https://github.com/protobuf-c/protobuf-c), [rabbitmq-c](https://github.com/alanxz/rabbitmq-c) (0.7.1)

In apt terms, this gives us (as of ubuntu 16.04):
  
    apt install \
        libav-tools \
        libavcodec-ffmpeg56 \
        libavformat-ffmpeg56 \
        libsdl2-2.0 \
        libswscale-ffmpeg3 \
        libglib2.0-0 \
        libprotobuf-c1 \
        librabbitmq4 \
        libx11-6
  
and the matching -dev packages for the headers.

- A C compiler, either gcc or clang are okay

- Compile the sources:

  
    cmake -DBUILD_SDL=1 -DCMAKE_BUILD_TYPE=Release
    make
  

## Obtaining the OpenGL rendering libs

The following libs are **required** in order to get the OpenGL rendering working:

- lib64EGL_translator.so 
- lib64GLES_CM_translator.so
- lib64GLES_V2_translator.so
- lib64OpenglRender.so 

Because if those libs are not available, then the graphical player will
not run, and android will not boot fully unless it is configured to boot
without hardware acceleration.

To build them, you need to apply a patch to the AOSP sources in order to
be able to receive rotation events, then build it using the scripts available
to build the emulator (which may differ between android versions).

# Usage

Runtime parameters are given through environment variables, which are
all mandatory. Running with docker-compose is recommended (and easier)
in order to have all executables running at the same time with the right
parameters.

## Generic options

Option                      | Use
---                         | ---
AIC_PLAYER_AMQP_USERNAME    | Username to access the AMQP service
AIC_PLAYER_AMQP_PASSWORD    | Password to access the AMQP service
AIC_PLAYER_AMQP_HOST        | Host (IP or hostname) of the AMQP server
AIC_PLAYER_VM_HOST          | Host (IP or hostname) of the virtual machine
AIC_PLAYER_VM_ID            | Virtual Machine ID, used to select the AMQP queue

## Graphical player options

Option                      | Use
---                         | ---
AIC_PLAYER_DPI              | Pixel density of the graphical view
AIC_PLAYER_WIDTH            | Width of the graphical view, in pixels
AIC_PLAYER_HEIGHT           | Height of the graphical view, in pixels
AIC_PLAYER_ENABLE_RECORD    | Toggle recording interface (AMQP + from the VM)
AIC_PLAYER_PATH_RECORD      | Path of the recorded files

## Sensors player options

Option                      | Use
---                         | ---
AIC_PLAYER_ENABLE_SENSORS   | Enable the misc sensors like accelerometer, thermometer, luxmeter…
AIC_PLAYER_ENABLE_GPS       | Enable the GPS sensor
AIC_PLAYER_ENABLE_GSM       | Enable the GSM sensor
AIC_PLAYER_ENABLE_BATTERY   | Enable the battery sensor
AIC_PLAYER_ENABLE_NFC       | Enable the NFC sensor

Each option is **required** and the executables will abort if one is not found.


## Record files and videos locally:

 - Press F7 to start recording a video and F8 to stop recording it
 - Press F6 to take a snapshot

## Remote commands

Remote commands/sensor data are sent through AMQP queues; read
the specific message documentation for more information.

Queue name                       | Effect
---                              | --- 
android-events.{vm_id}.battery   | Forward a battery payload
android-events.{vm_id}.sensors   | Forward a sensors payload
android-events.{vm_id}.gps       | Forward a GPS payload
android-events.{vm_id}.gsm       | Forward a GSM payload
android-events.{vm_id}.nfc       | Forward a NFC payload
android-events.{vm_id}.recording | Toggle video recording or take a screenshot

# Other topics

## Documentation

The documentation was produced using doxygen, it can be either build through
the "make doc" target produced with cmake, or manually using "doxygen -g Doxyfile".

## Code formatting:

Code is formatted with clang-format and the configuration file .clang-format is
in the sdl/ directory.

## Static analysis with clang/scan-build:

You can easily use scan-build with cmake, you only have to prefix your "cmake" and
"make" invocations with scan-build, and it will automatically switch compilers and
perform static analysis on the source files.

## Tests

Building with tests requires [cmockery](https://github.com/google/cmockery) in addition
to the other dependencies.

Building is about the same as without testing:

  
    cmake -DBUILD_SDL=1 -DCMAKE_BUILD_TYPE=Debug -DWITH_TEST=1
    make
  

This will produce test executables in the out/ directory.
