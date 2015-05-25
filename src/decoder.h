//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef ENCODER_H_
#define ENCODER_H_

#include <cstdint>
#include <vector>

#include "downsampled.h"

namespace himg {

class Decoder {
 public:
  Decoder();

  bool Decode(const uint8_t *packed_data, int packed_size);

  const uint8_t *unpacked_data() const { return m_unpacked_data.data(); }
  int unpacked_size() const { return static_cast<int>(m_unpacked_data.size()); }

  int width() const { return m_width; }
  int height() const { return m_height; }
  int num_channels() const { return m_num_channels; }

 private:
  bool DecodeHeader();
  bool DecodeQuantizationTable();
  bool DecodeLowRes();
  bool DecodeFullRes();

  bool UncompressData(uint8_t *out, int out_size);

  std::vector<Downsampled> m_downsampled;
  std::vector<uint8_t> m_unpacked_data;

  const uint8_t *m_packed_data;
  int m_packed_size;
  int m_packed_idx;

  int m_width;
  int m_height;
  int m_num_channels;
  bool m_use_ycbcr;
  uint8_t m_quant_table[64];
};

}  // namespace himg

#endif  // ENCODER_H_
