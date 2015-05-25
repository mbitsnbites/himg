//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef HUFFMAN_H_
#define HUFFMAN_H_

#include <cstdint>

namespace himg {

class Huffman {
 public:
  static int MaxCompressedSize(int uncompressed_size);

  static int Compress(uint8_t *out, const uint8_t *in, int in_size);
  static void Uncompress(uint8_t *out,
                         const uint8_t *in,
                         int in_size,
                         int out_size);
};

}  // namespace himg

#endif  // HUFFMAN_H_
