cmake_minimum_required (VERSION 2.8)

project ( testValidateNFC )
set ( EXECUTABLE_OUTPUT_PATH ../out/ )
set ( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}" )
set ( CMAKE_PREFIX_PATH "/usr/" )

include_directories ( ../include )

ADD_EXECUTABLE (
  testValidateNFC
  ./testValidateNFC.c
)





