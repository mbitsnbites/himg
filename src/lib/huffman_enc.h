//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef HUFFMAN_ENC_H_
#define HUFFMAN_ENC_H_

#include <cstdint>

namespace himg {

class HuffmanEnc {
 public:
  static int MaxCompressedSize(int uncompressed_size);

  static int Compress(uint8_t *out,
                      const uint8_t *in,
                      int in_size,
                      int block_size);
};

}  // namespace himg

#endif  // HUFFMAN_ENC_H_
