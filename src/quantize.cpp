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
  8, 8, 8, 8, 9, 9, 9, 9,
  8, 8, 8, 8, 9, 9, 9, 9,
  8, 8, 8, 8, 9, 9, 9, 9,
  8, 8, 8, 8, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9
};

// TODO(m): Base this on histogram studies.
const int16_t kDelinearizeTable[128] = {
  1, 2, 3, 4, 5, 6, 7, 8,
  9, 10, 11, 12, 13, 14, 15, 16,
  17, 18, 19, 20, 21, 22, 23, 24,
  25, 26, 27, 28, 29, 30, 31, 32,
  33, 35, 37, 39, 41, 43, 45, 47,
  49, 51, 53, 55, 57, 59, 61, 63,
  65, 69, 73, 77, 81, 85, 89, 93,
  97, 101, 105, 109, 113, 117, 121, 125,
  129, 137, 145, 153, 161, 169, 177, 185,
  193, 201, 209, 217, 225, 233, 241, 249,
  257, 265, 273, 281, 289, 297, 305, 313,
  321, 329, 337, 345, 353, 361, 369, 377,
  385, 401, 417, 433, 449, 465, 481, 497,
  513, 529, 545, 561, 577, 593, 609, 625,
  641, 657, 673, 689, 705, 721, 737, 753,
  769, 785, 800, 820, 850, 900, 1000, 1500
};

uint8_t ToSignedMagnitude(int16_t abs_x, bool negative) {
  // Special case: zero (it's quite common).
  if (!abs_x) {
    return 0;
  }

  // Look up the code.
  // TODO(m): Do binary search, and proper rounding.
  uint8_t code;
  for (code = 0; code < 127; ++code) {
    if (abs_x < kDelinearizeTable[code + 1])
      break;
  }
  code++;  // 1 <= code <= 128

  // Combine code and sign bit.
  if (negative)
    return ((code - 1) << 1) + 1;
  else
    return code <= 127 ? (code << 1) : 254;
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
