cmake_minimum_required (VERSION 2.8)
include(cmake/cppcheck.cmake)

project ( AiCVM_PlayerSensorBroker )
set ( EXECUTABLE_OUTPUT_PATH out/ )
set ( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}" )
set ( CMAKE_PREFIX_PATH "/usr/" )


if(BUILD_SDL)
	INCLUDE(FindPkgConfig)
	PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)
	include_directories(${SDL2_INCLUDE_DIRS})
	find_package(X11 )
endif()

FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GLIB REQUIRED glib-2.0)

find_package(Threads REQUIRED)
# Find protobuf-c
find_library(PROTOBUFC_LIB NAMES protobuf-c HINTS /usr/lib64 /usr/lib)

if (NOT (PROTOBUFC_LIB))
    MESSAGE(FATAL_ERROR "Failed to find protobuf-c.  Please install protobuf-c.  PROTOBUFC_LIB=${PROTOBUFC_LIB}")
else()
    MESSAGE(STATUS "PROTOBUFC_LIB:        ${PROTOBUFC_LIB}")
endif()

include_directories (
  include
  ${GLIB_INCLUDE_DIRS}
)

if(BUILD_SDL)
ADD_EXECUTABLE (
  player_sdl_grab
  ./src/main.c
  ./src/config_env.c
  ./src/socket.c
  ./src/amqp_listen.c
  ./src/protobuf_framing.c
  ./src/host_gl.c
  ./src/grabber.c
  ./src/logger.c
  ./src/sdl_events.c
  ./src/sdl_translate.c
  ./src/dump_trace.c
  ./src/recording.pb-c.c
)
endif()

ADD_LIBRARY ( libOpenglRender SHARED IMPORTED )
SET_TARGET_PROPERTIES ( libOpenglRender PROPERTIES IMPORTED_LOCATION lib/lib64OpenglRender.so)

ADD_LIBRARY ( lib64EGL_translator SHARED IMPORTED )
SET_TARGET_PROPERTIES ( lib64EGL_translator PROPERTIES IMPORTED_LOCATION lib/lib64EGL_translator.so)

ADD_LIBRARY ( lib64GLES_V2_translator SHARED IMPORTED )
SET_TARGET_PROPERTIES ( lib64GLES_V2_translator PROPERTIES IMPORTED_LOCATION lib/lib64GLES_V2_translator.so)

ADD_LIBRARY ( lib64GLES_CM_translator SHARED IMPORTED )
SET_TARGET_PROPERTIES ( lib64GLES_CM_translator PROPERTIES IMPORTED_LOCATION lib/lib64GLES_CM_translator.so      )


SET(LIB_OPENGLRENDER "libOpenglRender" "lib64EGL_translator" "lib64GLES_V2_translator" "lib64GLES_CM_translator")
SET(LIB_RABBITMQ rabbitmq)
SET (FFMPEG_LIBRARIES   avcodec   avformat   avutil swscale swresample )
#SET(FFMPEG_LIBRARIES ${FFMPEG_LIBAVCODEC} ${FFMPEG_LIBAVFORMAT} ${FFMPEG_LIBAVUTIL})

if(BUILD_SDL)
TARGET_LINK_LIBRARIES (
  player_sdl_grab
  ${SDL2_LIBRARIES}
  ${LIB_OPENGLRENDER}
  ${LIB_RABBITMQ}
  ${CMAKE_THREAD_LIBS_INIT}
  ${X11_LIBRARIES}
  ${FFMPEG_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${PROTOBUFC_LIB}
)
endif()


if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	set (CMAKE_C_FLAGS "--std=gnu99 ${CMAKE_C_FLAGS}")
endif()
      
##target_link_libraries(AiCVM_PlayerSensorBroker )
LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/res)

MESSAGE( STATUS "X11_LIBRARIES:        " ${X11_LIBRARIES} )
MESSAGE( STATUS "CXX compiler:         " ${CMAKE_CXX_COMPILER})
MESSAGE( STATUS "C compiler:           " ${CMAKE_C_COMPILER})


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -pthread")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wfatal-errors -g")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -fsanitize=address -fstack-protector -DFORTIFY_SOURCE=2 -fno-omit-frame-pointer")
endif()


########Generate c/java files from *.proto
file(GLOB proto_packages "${CMAKE_CURRENT_BINARY_DIR}/proto/*.proto")

message(STATUS "Generating c files from *.proto")
execute_process(
		COMMAND protoc-c -I=${CMAKE_CURRENT_BINARY_DIR}/proto/ --c_out=${CMAKE_CURRENT_BINARY_DIR}/src ${proto_packages}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/src/recording.pb-c.h ${CMAKE_CURRENT_BINARY_DIR}/include/recording.pb-c.h)
file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/src/nfc.pb-c.h ${CMAKE_CURRENT_BINARY_DIR}/include/nfc.pb-c.h)
file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/src/sensors_packet.pb-c.h ${CMAKE_CURRENT_BINARY_DIR}/include/sensors_packet.pb-c.h)
message(STATUS "Manage proto : Done")
######Special command
add_custom_target ( rmcache @echo removing file CMakeCache.txt)

add_custom_command(
    COMMENT "Cleaning Cmake Cache"
    COMMAND find
    ARGS ${CMAKE_CURRENT_BINARY_DIR} -name "CMakeCache.txt" | xargs rm -rf
    TARGET  rmcache
)
########


### temp NFC command to send tag ###
message(STATUS "TO TEST NFC TAG use : cmake -DBUILD_NFC=ON . ")
if (BUILD_NFC)
    add_subdirectory(testNFC)
endif (BUILD_NFC)
### ###


##########################################################################
# DOXYGEN
# add a target to generate API documentation with Doxygen
find_package(Doxygen)
option(BUILD_DOCUMENTATION "Create and install the HTML/MAN/XML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if(BUILD_DOCUMENTATION)
    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is needed to build the documentation.")
    endif()

    set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
    #install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html DESTINATION share/doc)

     add_custom_target(cleandoc
            COMMAND ${CMAKE_COMMAND} -E remove_directory  ${CMAKE_CURRENT_BINARY_DIR}/doc/
            COMMENT "cleaning Doxygen documentation"
        )

    add_custom_target ( viewdoc @echo view doc)

    add_custom_command(
        COMMENT "Launching firefox to vizualize doc"
        COMMAND firefox /home/abruxelle/workspace/sdl_sensor_broker/sdl/doc/html/index.html
        TARGET  viewdoc
    )

    add_custom_target(whole
        COMMENT "Generating EXEC + DOC and view doc"
        COMMAND make
        COMMAND make doc
        COMMAND make viewdoc
    )

endif()
##########################################################################







##########################################################################
# VALGRIND
if (("${CMAKE_BUILD_TYPE}" STREQUAL "Valgrind") AND (NOT WIN32))
	MESSAGE(COMMENT "valgrind found! ")

    LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
    INCLUDE(cmake/MemoryCheck)
    SETUP_TARGET_FOR_MEMCHECK(test_valgrind ${PROJECT_BINARY_DIR}/out/AiCVM_PlayerSensorBroker )

##########################################################################
# HELLGRIND
    INCLUDE(cmake/ThreadsCheck)
    SETUP_TARGET_FOR_MEMCHECK(test_hellgrind ${PROJECT_BINARY_DIR}/out/AiCVM_PlayerSensorBroker )

##########################################################################
# CACHEGRIND
    INCLUDE(cmake/CacheCheck)
    SETUP_TARGET_FOR_MEMCHECK(test_cachegrind ${PROJECT_BINARY_DIR}/out/AiCVM_PlayerSensorBroker )

##########################################################################
# CALLGRIND
    SETUP_TARGET_FOR_MEMCHECK(test_callgrind ${PROJECT_BINARY_DIR}/out/AiCVM_PlayerSensorBroker )
	# add top-level testing dir
	#enable_testing()
endif()



enable_testing()
include(CTest)


##############player_audio#################
###########################################
ADD_EXECUTABLE (
    player_audio
    ./src/player_audio.c
    ./src/socket.c
    ./src/logger.c
    ./src/dump_trace.c
    ./src/config_env.c
)

TARGET_LINK_LIBRARIES (
    player_audio
    ${GLIB_LIBRARIES}
    ${FFMPEG_LIBRARIES}
)
###########################################


###############player_sensors##############
###########################################
if (NOT WITH_TEST)
ADD_EXECUTABLE (
  player_sensors
  ./src/sensors.c
  ./src/player_nfc.c
  ./src/config_env.c
  ./src/socket.c
  ./src/amqp_listen.c
  ./src/protobuf_framing.c
  ./src/logger.c
  ./src/dump_trace.c
)

TARGET_LINK_LIBRARIES (
  player_sensors
  ${LIB_RABBITMQ}
  ${CMAKE_THREAD_LIBS_INIT}
  ${GLIB_LIBRARIES}
  ${PROTOBUFC_LIB}
)
endif()

##ctest_build
if (WITH_TEST)
    add_definitions( -DWITH_TESTING )
    include(cmake/FindCMockery.cmake)
    include_directories ("${PROJECT_SOURCE_DIR}/testPlayer" ${CMOCKERY_INCLUDE_DIRS} )
    add_executable(testSensors
                    ./testPlayer/testSensors.c
                    ./testPlayer/mockVMSensors.c
                    ./testPlayer/mockVMNfcd.c
                    ./testPlayer/mockVMLibNci.c
                    ./testPlayer/amqp_send.c
                    ./src/player_nfc.c
                    ./src/sensors.c
                    ./src/config_env.c
                    ./src/socket.c
                    ./src/amqp_listen.c
                    ./src/protobuf_framing.c
                    ./src/logger.c
                    ./src/sensors_packet.pb-c.c
                    ./src/nfc.pb-c.c
                   )
    target_link_libraries(testSensors
                            ${CMOCKERY_LIBRARY}
                            ${LIB_RABBITMQ}
                            ${CMAKE_THREAD_LIBS_INIT}
                            ${FFMPEG_LIBRARIES}
                            ${GLIB_LIBRARIES}
                            ${PROTOBUFC_LIB})
    add_dependencies(testSensors testSensors)
    add_test(testSensors ./out/testSensors)
endif()
###########################################
