//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "quantize.h"

#include <algorithm>

namespace himg {

namespace {

const uint8_t kQuantTable[64] = {
  8, 7, 7, 8, 9, 9, 10, 10,
  8, 8, 8, 8, 9, 10, 10, 10,
  8, 8, 8, 8, 9, 10, 10, 10,
  8, 8, 8, 9, 10, 11, 10, 10,
  8, 8, 9, 10, 10, 11, 11, 10,
  9, 9, 10, 10, 10, 11, 11, 11,
  9, 10, 10, 10, 11, 11, 11, 11,
  10, 10, 10, 11, 11, 11, 11, 11
};

// This LUT is based on histogram studies.
const int16_t kDelinearizeTable[128] = {
  1, 2, 3, 4, 5, 6, 7,
  8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55,
  56, 57, 58, 59, 60, 61, 62, 63,
  64, 65, 66, 67, 68, 69, 70, 71,
  73, 74, 76, 78, 79, 81, 83, 85,
  87, 89, 92, 94, 97, 100, 103, 106,
  109, 112, 116, 120, 125, 130, 135, 140,
  146, 152, 159, 166, 173, 181, 190, 200,
  210, 221, 233, 246, 260, 276, 293, 312,
  333, 357, 383, 413, 448, 488, 535, 590,
  656, 737, 838, 968, 1141, 1386, 1767, 2471,
  5000
};

uint8_t ToSignedMagnitude(int16_t abs_x, bool negative) {
  // Special case: zero (it's quite common).
  if (!abs_x) {
    return 0;
  }

  // Look up the code.
  // TODO(m): Do binary search.
  uint8_t code;
  for (code = 0; code <= 127; ++code) {
    if (abs_x <= kDelinearizeTable[code])
      break;
  }
  if (code > 0 && code < 128) {
    if (abs_x - kDelinearizeTable[code - 1] < kDelinearizeTable[code] - abs_x)
      code--;
  }

  // Combine code and sign bit.
  if (negative)
    return (code << 1) + 1;
  else
    return code <= 126 ? ((code + 1) << 1) : 254;
}

int16_t FromSignedMagnitude(uint8_t x) {
  // Special case: zero (it's quite common).
  if (!x) {
    return 0;
  }

  if (x & 1)
    return -kDelinearizeTable[x >> 1];
  else
    return kDelinearizeTable[(x >> 1) - 1];
}

}  // namespace

void Quantize::MakeTable(uint8_t *table, uint8_t quality) {
  if (quality > 9)
    quality = 9;
  for (int i = 0; i < 64; ++i) {
    int16_t shift = static_cast<int16_t>(kQuantTable[i]) - quality;
    table[i] =
        shift >= 0 ? (shift <= 16 ? static_cast<uint8_t>(shift) : 16) : 0;
  }
}

void Quantize::Pack(uint8_t *out, const int16_t *in, const uint8_t *table) {
  for (int i = 0; i < 64; ++i) {
    uint8_t shift = *table++;
    int16_t x = *in++;
    bool negative = x < 0;
    // NOTE: We can not just shift negative numbers, since that will never
    // produce zero (e.g. -5 >> 7 == -1), so we shift the absolute value and
    // keep track of the sign.
    *out++ = ToSignedMagnitude((negative ? -x : x) >> shift, negative);
  }
}

void Quantize::Unpack(int16_t *out, const uint8_t *in, const uint8_t *table) {
  for (int i = 0; i < 64; ++i) {
    uint8_t shift = *table++;
    *out++ = FromSignedMagnitude(*in++) << shift;
  }
}

}  // namespace himg
