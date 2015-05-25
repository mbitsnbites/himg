//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef QUANTIZE_H_
#define QUANTIZE_H_

#include <cstdint>

namespace himg {

class Quantize {
 public:
  static void MakeTable(uint8_t *table, uint8_t quality);

  // Pack to clamped signed magnitude based on the shift table.
  static void Pack(uint8_t *out, const int16_t *in, const uint8_t *table);

  // Unpack to 16-bit twos complement based on the shift table.
  static void Unpack(int16_t *out, const uint8_t *in, const uint8_t *table);
};

}  // namespace himg

#endif  // QUANTIZE_H_
