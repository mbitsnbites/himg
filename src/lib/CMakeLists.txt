# -*- mode: CMake; tab-width: 2; indent-tabs-mode: nil; -*-

###############################################################################
# HIMG, by Marcus Geelnard, 2017
#
# This is free and unencumbered software released into the public domain.
#
# See LICENSE for details.
###############################################################################

# TODO(m): Turn on more warnings!

set(himg_sources
    common.cpp
    decoder.cpp
    downsampled.cpp
    encoder.cpp
    hadamard.cpp
    huffman_dec.cpp
    huffman_enc.cpp
    mapper.cpp
    quantize.cpp
    ycbcr.cpp
    )
add_library(himg ${himg_sources})
target_include_directories(himg PUBLIC .)

