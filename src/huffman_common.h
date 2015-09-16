//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef HUFFMAN_COMMON_H_
#define HUFFMAN_COMMON_H_

#include <cstdint>

namespace himg {

namespace {

// A symbol is a 9-bit unsigned number.
typedef uint16_t Symbol;
const int kSymbolSize = 9;
const int kNumSymbols = 261;

// Special symbols for RLE.
const Symbol kSymTwoZeros = 256;        // 2            (0 bits)
const Symbol kSymUpTo6Zeros = 257;      // 3 - 6        (2 bits)
const Symbol kSymUpTo22Zeros = 258;     // 7 - 22       (4 bits)
const Symbol kSymUpTo278Zeros = 259;    // 23 - 278     (8 bits)
const Symbol kSymUpTo16662Zeros = 260;  // 279 - 16662  (14 bits)

// The maximum number of nodes in the Huffman tree (branch nodes + leaf nodes).
const int kMaxTreeNodes = (kNumSymbols * 2) - 1;

}  // namespace

}  // namespace himg

#endif  // HUFFMAN_COMMON_H_
