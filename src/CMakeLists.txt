# -*- mode: CMake; tab-width: 2; indent-tabs-mode: nil; -*-

###############################################################################
# HIMG, by Marcus Geelnard, 2017
#
# This is free and unencumbered software released into the public domain.
#
# See LICENSE for details.
###############################################################################

cmake_minimum_required(VERSION 3.0)

project(himg)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib)

find_package(Threads REQUIRED)

find_library(FreeImage_LIBRARY NAMES freeimage)
if(NOT FreeImage_LIBRARY)
  message(FATAL_ERROR "FreeImage not found!")
endif()

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark himg ${FreeImage_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

add_executable(chimg chimg.cpp)
target_link_libraries(chimg himg ${FreeImage_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

add_executable(dhimg dhimg.cpp)
target_link_libraries(dhimg himg ${FreeImage_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

