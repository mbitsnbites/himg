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

struct QualityScale {
  int quality;
  int scale;
};

// This table has been tuned so that there is a relatively continuous increase
// in the resulting compressed image size based on the quality setting. For most
// images, the following quality regions apply:
//   0 - 20: Ugly and mostly pretty useless.
//  20 - 40: Useful for quick looks / previews.
//  40 - 60: Decent quality.
//  60 - 90: Nice quality.
//  90 - 100: Crazy size growth (generally not worth it).
const QualityScale kQualityToScaleTable[] = {
  {   0, 65535 },
  {  10, 32512 },
  {  20, 13568 },
  {  30,  5120 },
  {  40,  2560 },
  {  50,  1024 },
  {  60,   768 },
  {  80,   256 },
  { 100,     0 }
};

const int kQualityToScaleTableSize =
    sizeof(kQualityToScaleTable) / sizeof(kQualityToScaleTable[0]);

// Given a quality value in the range [0, 100], return a scaling factor in the
// range [0, 65535], where 65535 represents the lowest possible quality.
int QualityToScale(int quality) {
  // Look up the quality level in the quality -> scaling factor LUT.
  int idx;
  for (idx = 0; idx < kQualityToScaleTableSize - 1; ++idx) {
    if (kQualityToScaleTable[idx + 1].quality > quality)
      break;
  }
  if (idx >= kQualityToScaleTableSize - 1)
    return kQualityToScaleTable[kQualityToScaleTableSize - 1].scale;

  // Pick out the two closest table entries.
  int q1 = kQualityToScaleTable[idx].quality;
  int s1 = kQualityToScaleTable[idx].scale;
  int q2 = kQualityToScaleTable[idx + 1].quality;
  int s2 = kQualityToScaleTable[idx + 1].scale;

  // Perform linear interpolation between the two table entries.
  int q = quality;
  int denom = q2 - q1;
  return s1 + ((s2 - s1) * (q - q1) + (denom >> 1)) / denom;
}

uint8_t NearestLog2(uint16_t x) {
  uint8_t y = 0, rounding = 0;
  while (x > 1) {
    ++y;
    rounding = x & 1;
    x = x >> 1;
  }
  return y + rounding;
}

void MakeShiftTable(uint8_t *shift_table,
                    const uint8_t *base,
                    uint8_t quality) {
  const int table_scale = QualityToScale(quality);
  for (int i = 0; i < 64; ++i) {
    uint16_t coeff_scale =
        (static_cast<int>(base[i]) * table_scale + 512) >> 10;
    uint8_t shift = NearestLog2(coeff_scale);
    shift_table[i] = std::min(shift, uint8_t(15));
  }
}

}  // namespace

void Quantize::InitForQuality(uint8_t quality, bool has_chroma) {
  m_has_chroma = has_chroma;

  // Create the shift table.
  MakeShiftTable(m_shift_table, kShiftTableBase, quality);
  if (m_has_chroma)
    MakeShiftTable(m_chroma_shift_table, kChromaShiftTableBase, quality);
}

void Quantize::Pack(uint8_t *out,
                    const int16_t *in,
                    bool chroma_channel,
                    const Mapper &mapper) {
  // Select which shift table to use.
  const uint8_t *shift_table =
      chroma_channel ? m_chroma_shift_table : m_shift_table;

  for (int i = 0; i < 64; ++i) {
    uint8_t shift = shift_table[i];
    int16_t round = shift != 0 ? 1 << (shift - 1) : 0;

    int16_t x = *in++;

    // NOTE: We can not just shift negative numbers, since that will never
    // produce zero (e.g. -5 >> 7 == -1), so we shift the absolute value and
    // keep track of the sign.
    if (x < 0)
      x = -((-x + round) >> shift);
    else
      x = (x + round) >> shift;

    *out++ = mapper.MapTo8Bit(x);
  }
}

void Quantize::Unpack(int16_t *out,
                      const uint8_t *in,
                      bool chroma_channel,
                      const Mapper &mapper) const {
  // Select which shift table to use.
  const uint8_t *shift_table =
      chroma_channel ? m_chroma_shift_table : m_shift_table;

  for (int i = 0; i < 64; ++i) {
    uint8_t shift = shift_table[i];
    *out++ = mapper.UnmapFrom8Bit(*in++) << shift;
  }
}

int Quantize::ConfigurationSize() const {
  // The shift tables require 1/2 a byte (4 bits) per entry, and there are 64
  // entries per table.
  return m_has_chroma ? 64 : 32;
}

// Get the quantization configuration.
void Quantize::GetConfiguration(uint8_t *out) const {
  // Store the shift table, four bits per entry.
  for (int i = 0; i < 32; ++i)
    *out++ = (m_shift_table[i * 2] << 4) | m_shift_table[i * 2 + 1];

  if (m_has_chroma) {
    // Store the chroma channel shift table, four bits per entry.
    for (int i = 0; i < 32; ++i)
      *out++ =
          (m_chroma_shift_table[i * 2] << 4) | m_chroma_shift_table[i * 2 + 1];
  }
}

// Set the quantization configuration.
bool Quantize::SetConfiguration(
    const uint8_t *in, int config_size, bool has_chroma) {
  m_has_chroma = has_chroma;

  const int expected_size = has_chroma ? 64 : 32;
  if (config_size != expected_size)
    return false;

  // Restore the shift table, four bits per entry.
  for (int i = 0; i < 32; ++i) {
    uint8_t x = *in++;
    m_shift_table[i * 2] = x >> 4;
    m_shift_table[i * 2 + 1] = x & 15;
  }

  // Restore the chroma shift table, four bits per entry.
  if (has_chroma) {
    for (int i = 0; i < 32; ++i) {
      uint8_t x = *in++;
      m_chroma_shift_table[i * 2] = x >> 4;
      m_chroma_shift_table[i * 2 + 1] = x & 15;
    }
  }

  return true;
}

}  // namespace himg
