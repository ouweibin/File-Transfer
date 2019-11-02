cmake_minimum_required(VERSION 2.6)
project(FileTransfer LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_C_FLAGS "-g -Wall")
set(CMAKE_C_FLAGS_DEBUG "-O0")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "C_FLAGS = " ${CMAKE_C_FLAGS} " " ${CMAKE_C_FLAGS_${BUILD_TYPE}})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
add_executable(ft filetransfer.c)

