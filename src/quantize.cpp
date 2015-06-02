//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "quantize.h"

#include <algorithm>
#include <iostream>

namespace himg {

namespace {

// Note: Shamelessly borrowed from libjpeg 6a (needs tuning).
const uint8_t kShiftTableBase[64] = {
  16, 11, 10, 16, 24, 40, 51, 61,
  12, 12, 14, 19, 26, 58, 60, 55,
  14, 13, 16, 24, 40, 57, 69, 56,
  14, 17, 22, 29, 51, 87, 80, 62,
  18, 22, 37, 56, 68, 109, 103, 77,
  24, 35, 55, 64, 81, 104, 113, 92,
  49, 64, 78, 87, 103, 121, 120, 101,
  72, 92, 95, 98, 112, 100, 103, 99
};

// Note: Inspired by libjpeg 6a.
const uint8_t kChromaShiftTableBase[64] = {
  17, 18, 24, 47, 100, 110, 115, 120,
  18, 21, 26, 66, 100, 110, 118, 121,
  24, 26, 56, 100, 100, 110, 120, 122,
  47, 66, 100, 100, 100, 110, 120, 123,
  100, 100, 100, 100, 100, 110, 120, 124,
  110, 110, 110, 110, 110, 110, 110, 123,
  120, 120, 120, 120, 120, 110, 100, 122,
  124, 124, 126, 126, 125, 123, 122, 105
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

uint8_t NearestLog2(uint16_t x) {
  uint8_t y = 0, rounding = 0;
  while (x > 1) {
    ++y;
    rounding = x & 1;
    x = x >> 1;
  }
  return y + rounding;
}

void MakeShiftTable(
    uint8_t *shift_table, const uint8_t *base, uint8_t quality) {
  for (int i = 0; i < 64; ++i) {
    // quality is in the range [0, 255].
    uint16_t scale = (static_cast<uint16_t>(base[i]) *
                      static_cast<uint16_t>(255 - quality) + 4) >> 3;
    uint8_t shift = NearestLog2(scale);
    shift_table[i] = shift <= 16 ? static_cast<uint8_t>(shift) : 16;
  }
}

}  // namespace

void Quantize::InitForQuality(uint8_t quality) {
  // Create the shift table.
  MakeShiftTable(m_shift_table, kShiftTableBase, quality);
  MakeShiftTable(m_chroma_shift_table, kChromaShiftTableBase, quality);

  // Create the delinearization table.
  // TODO(m): Base this on the shift table.
  for (int i = 0; i < 128; ++i)
    m_delinearization_table[i] = kDelinearizeTable[i];
}

void Quantize::Pack(uint8_t *out, const int16_t *in, bool chroma_channel) {
  // Select which shift table to use.
  const uint8_t *shift_table =
      chroma_channel ? m_chroma_shift_table : m_shift_table;

  for (int i = 0; i < 64; ++i) {
    uint8_t shift = shift_table[i];
    int16_t x = *in++;
    bool negative = x < 0;
    // NOTE: We can not just shift negative numbers, since that will never
    // produce zero (e.g. -5 >> 7 == -1), so we shift the absolute value and
    // keep track of the sign.
    *out++ = ToSignedMagnitude((negative ? -x : x) >> shift, negative);
  }
}

void Quantize::Unpack(int16_t *out, const uint8_t *in, bool chroma_channel) {
  // Select which shift table to use.
  const uint8_t *shift_table =
      chroma_channel ? m_chroma_shift_table : m_shift_table;

  for (int i = 0; i < 64; ++i) {
    uint8_t shift = shift_table[i];
    *out++ = FromSignedMagnitude(*in++) << shift;
  }
}

int Quantize::ConfigurationSize() const {
  // The shift tables requires 4 bits per entry, and there are 2 * 64 entries.
  int size = 2 * 32;

  // The delinearization table requires one byte that tells how many items can
  // be represented with a single byte, plus the actual single- and double-byte
  // items.
  int single_byte_items = NumberOfSingleByteDelinearizationItems();
  size += 1 + single_byte_items + 2 * (128 - single_byte_items);

  return size;
}

// Get the quantization configuration.
void Quantize::GetConfiguration(uint8_t *out) {
  // Store the shift table, four bits per entry.
  for (int i = 0; i < 32; ++i)
    *out++ = (m_shift_table[i * 2] << 4) | m_shift_table[i * 2 + 1];

  // Store the chroma channel shift table, four bits per entry.
  for (int i = 0; i < 32; ++i)
    *out++ =
        (m_chroma_shift_table[i * 2] << 4) | m_chroma_shift_table[i * 2 + 1];

  // Store the delinearization table.
  int single_byte_items = NumberOfSingleByteDelinearizationItems();
  *out++ = static_cast<uint8_t>(single_byte_items);
  int i;
  for (i = 0; i < single_byte_items; ++i)
    *out++ = static_cast<uint8_t>(m_delinearization_table[i]);
  for (; i < 128; ++i) {
    uint16_t x = m_delinearization_table[i];
    *out++ = static_cast<uint8_t>(x & 255);
    *out++ = static_cast<uint8_t>(x >> 8);
  }
}

// Set the quantization configuration.
bool Quantize::SetConfiguration(const uint8_t *in, int config_size) {
  if (config_size < 64)
    return false;

  // Restore the shift table, four bits per entry.
  for (int i = 0; i < 32; ++i) {
    uint8_t x = *in++;
    m_shift_table[i * 2] = x >> 4;
    m_shift_table[i * 2 + 1] = x & 15;
  }

  // Restore the chroma shift table, four bits per entry.
  for (int i = 0; i < 32; ++i) {
    uint8_t x = *in++;
    m_chroma_shift_table[i * 2] = x >> 4;
    m_chroma_shift_table[i * 2 + 1] = x & 15;
  }

  // Restore the delinearization table.
  int single_byte_items = static_cast<int>(*in++);
  int actual_size = 64 + 1 + single_byte_items + 2 * (128 - single_byte_items);
  if (actual_size != config_size)
    return false;
  int i;
  for (i = 0; i < single_byte_items; ++i)
    m_delinearization_table[i] = static_cast<uint16_t>(*in++);
  for (; i < 128; ++i) {
    m_delinearization_table[i] =
        static_cast<uint16_t>(in[0]) | (static_cast<uint16_t>(in[1]) << 8);
    in += 2;
  }

  std::cout << "Quantization shift table:\n";
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      std::cout << static_cast<int>(m_shift_table[i * 8 + j]) << " ";
    }
    std::cout << "\n";
  }

  return true;
}

int Quantize::NumberOfSingleByteDelinearizationItems() const {
  int single_byte_items;
  for (single_byte_items = 0; single_byte_items < 128; ++single_byte_items) {
    if (m_delinearization_table[single_byte_items] >= 256)
      break;
  }
  return single_byte_items;
}

uint8_t Quantize::ToSignedMagnitude(int16_t abs_x, bool negative) {
  // Special case: zero (it's quite common).
  if (!abs_x) {
    return 0;
  }

  // Look up the code.
  // TODO(m): Do binary search.
  uint8_t code;
  for (code = 0; code <= 127; ++code) {
    if (abs_x <= m_delinearization_table[code])
      break;
  }
  if (code > 0 && code < 128) {
    if (abs_x - m_delinearization_table[code - 1] <
        m_delinearization_table[code] - abs_x)
      code--;
  }

  // Combine code and sign bit.
  if (negative)
    return (code << 1) + 1;
  else
    return code <= 126 ? ((code + 1) << 1) : 254;
}

int16_t Quantize::FromSignedMagnitude(uint8_t x) {
  // Special case: zero (it's quite common).
  if (!x) {
    return 0;
  }

  if (x & 1)
    return -m_delinearization_table[x >> 1];
  else
    return m_delinearization_table[(x >> 1) - 1];
}

}  // namespace himg
