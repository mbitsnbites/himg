###############################################################################
# HIMG, by Marcus Geelnard, 2015
#
# This is free and unencumbered software released into the public domain.
#
# See LICENSE for details.
###############################################################################

DBG_FLAGS = -g
OPT_FLAGS = -O3 -funroll-loops

CPP = g++
CPPFLAGS = -c -W -Wall $(DBG_FLAGS) $(OPT_FLAGS) -std=c++11
AR = gcc-ar
ARFLAGS = rcs
LFLAGS = $(DBG_FLAGS) $(OPT_FLAGS)

LIB_OBJS = common.o \
           decoder.o \
           downsampled.o \
           encoder.o \
           hadamard.o \
           huffman.o \
           mapper.o \
           quantize.o \
           ycbcr.o

ALL_OBJS = $(LIB_OBJS) benchmark.o chimg.o dhimg.o

.PHONY: all clean

all: chimg dhimg benchmark

clean:
	rm -rf benchmark chimg dhimg libhimg.a $(ALL_OBJS)

benchmark: benchmark.o libhimg.a
	$(CPP) $(LFLAGS) -o benchmark benchmark.o -L. -lhimg -lfreeimage

chimg: chimg.o libhimg.a
	$(CPP) $(LFLAGS) -o chimg chimg.o -L. -lhimg -lfreeimage

dhimg: dhimg.o libhimg.a
	$(CPP) $(LFLAGS) -o dhimg dhimg.o -L. -lhimg -lfreeimage

libhimg.a: $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

benchmark.o: benchmark.cpp decoder.h encoder.h
	$(CPP) $(CPPFLAGS) -o $@ $<

chimg.o: chimg.cpp encoder.h
	$(CPP) $(CPPFLAGS) -o $@ $<

dhimg.o: dhimg.cpp decoder.h
	$(CPP) $(CPPFLAGS) -o $@ $<

common.o: common.cpp common.h
	$(CPP) $(CPPFLAGS) -o $@ $<

decoder.o: decoder.cpp common.h downsampled.h decoder.h hadamard.h huffman.h mapper.h quantize.h ycbcr.h
	$(CPP) $(CPPFLAGS) -o $@ $<

downsampled.o: downsampled.cpp downsampled.h mapper.h
	$(CPP) $(CPPFLAGS) -o $@ $<

encoder.o: encoder.cpp common.h downsampled.h encoder.h hadamard.h huffman.h mapper.h quantize.h ycbcr.h
	$(CPP) $(CPPFLAGS) -o $@ $<

hadamard.o: hadamard.cpp hadamard.h
	$(CPP) $(CPPFLAGS) -o $@ $<

huffman.o: huffman.cpp huffman.h
	$(CPP) $(CPPFLAGS) -o $@ $<

mapper.o: mapper.cpp mapper.h
	$(CPP) $(CPPFLAGS) -o $@ $<

quantize.o: quantize.cpp quantize.h mapper.h
	$(CPP) $(CPPFLAGS) -o $@ $<

ycbcr.o: ycbcr.cpp ycbcr.h
	$(CPP) $(CPPFLAGS) -o $@ $<

