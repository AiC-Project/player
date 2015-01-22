
all: build-audio build-sensors build-sdl_grab

clean:
	rm -f sdl/CMakeCache.txt
	if test -f sdl/Makefile; then make --directory sdl clean; fi
	rm -f sdl/out/player_sdl_grab
	# XXX the above should not be required

build-audio:
	cd sdl && cmake . -DCMAKE_BUILD_TYPE=Release -DBUILD_SDL=FALSE
	make player_audio --directory sdl

build-sensors:
	cd sdl && cmake . -DCMAKE_BUILD_TYPE=Release -DBUILD_SDL=FALSE
	make player_sensors --directory sdl

build-sdl_grab:
	cd sdl && cmake . -DCMAKE_BUILD_TYPE=Release -DBUILD_SDL=TRUE
	make player_sdl_grab --directory sdl

debug:
	cd sdl && cmake . -DCMAKE_BUILD_TYPE=Debug -DBUILD_SDL=TRUE
	make --directory sdl VERBOSE=1

cppcheck:
	cd sdl && cmake . -DCMAKE_BUILD_TYPE=Debug -DBUILD_SDL=TRUE
	make --directory sdl cppcheck


#
# Build everything within a docker container, by sharing the source volume, then build the runtime images with the resulting binaries.
# Build dependencies are not required on the host, except for the make command and Docker.
#

docker-all: clean docker-wrapper docker-make docker-images

wrap = docker run --rm -ti -v ${CURDIR}/:/home/volume -ti aic.player-wrap

docker-wrapper:
	docker build --build-arg USER_ID=$(shell id -u) --build-arg GROUP_ID=$(shell id -g) -t aic.player-wrap -f wrap.Dockerfile .

docker-make:
	rm -f sdl/CMakeCache.txt
	$(wrap) make all

docker-images:
	docker build -t aic.audio -f audio.Dockerfile .
	docker build -t aic.sdl -f sdl.Dockerfile .
	docker build -t aic.sensors -f sensors.Dockerfile .

