//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef HUFFMAN_DEC_H_
#define HUFFMAN_DEC_H_

#include <cstdint>

namespace himg {

class HuffmanDec {
 public:
  static bool Uncompress(uint8_t *out,
                         const uint8_t *in,
                         int in_size,
                         int out_size);
};

}  // namespace himg

#endif  // HUFFMAN_DEC_H_
