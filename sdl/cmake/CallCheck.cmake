#
# - Enable Valgrind Check
#
#   Build a Valgrind build:
#     cmake -DCMAKE_BUILD_TYPE=Valgrind ..
#     make
#     make _targetname
#
#

# Check prereqs
FIND_PROGRAM( VALGRIND_PATH valgrind )

IF(NOT VALGRIND_PATH)
    MESSAGE(FATAL_ERROR "valgrind not found! Aborting...")
ENDIF() # NOT VALGRIND_PATH

SET(VALGRIND_OPTIONS "")

SET(CMAKE_CXX_FLAGS_MEMCHECK
    "-g -O0 -fprofile-arcs "
    CACHE STRING "Flags used by the C++ compiler during valgrind builds."
    FORCE )
SET(CMAKE_C_FLAGS_MEMCHECK
    "-g -O0 -fprofile-arcs"
    CACHE STRING "Flags used by the C compiler during valgrind builds."
    FORCE )
SET(CMAKE_EXE_LINKER_FLAGS_MEMCHECK
    ""
    CACHE STRING "Flags used for linking binaries during valgrind builds."
    FORCE )
SET(CMAKE_SHARED_LINKER_FLAGS_MEMCHECK
    ""
    CACHE STRING "Flags used by the shared libraries linker during valgrind builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_MEMCHECK
    CMAKE_C_FLAGS_MEMCHECK
    CMAKE_EXE_LINKER_FLAGS_MEMCHECK
    CMAKE_SHARED_LINKER_FLAGS_MEMCHECK )

IF ( NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Valgrind"))
  MESSAGE( WARNING "Valgrind results with an optimized (non-Debug) build may be misleading" )
ENDIF() # NOT CMAKE_BUILD_TYPE STREQUAL "Debug"


# Param _targetname     The name of new the custom make target
# Param list of target tests
FUNCTION(SETUP_TARGET_FOR_MEMCHECK _targetname _test)

    IF(NOT VALGRIND_PATH)
        MESSAGE(FATAL_ERROR "valgrind not found! Aborting...")
    ENDIF() # NOT VALGRIND_PATH

    # Setup target
    ADD_CUSTOM_TARGET(${_targetname} ALL

    COMMENT "Cachegrind run complete for test=${_test} valgrind=${VALGRIND_PATH} target=${_targetname}"
    # Run tests
    COMMAND ${VALGRIND_PATH} --tool=callgrind  --dump-instr=yes
                             --log-file="log/log_callgrind.txt"
                             --callgrind-out-file="log/callgrind.out"
                             ${_test} ${VMIP} 800 600 160 sensor ${CAM}


    )

ENDFUNCTION() # SETUP_TARGET_FOR_MEMCHECK
