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

class Encoder {
 public:
  Encoder();

  bool Encode(const uint8_t *data,
              int width,
              int height,
              int pixel_stride,
              int num_channels,
              int quality,
              bool use_ycbcr);

  const uint8_t *packed_data() const { return m_packed_data.data(); }

  int packed_size() const { return static_cast<int>(m_packed_data.size()); }

 private:
  void EncodeHeader(int width, int height, int num_channels, bool use_ycrcb);
  void EncodeQuantizationTable(const uint8_t *table);
  void EncodeLowRes(const uint8_t *data,
                    int width,
                    int height,
                    int pixel_stride,
                    int num_channels);
  void EncodeFullRes(const uint8_t *data,
                     int width,
                     int height,
                     int pixel_stride,
                     int num_channels,
                     uint8_t *quant_table);

  int AppendPackedData(const uint8_t *unpacked_data, int unpacked_size);

  std::vector<Downsampled> m_downsampled;
  std::vector<uint8_t> m_packed_data;
};

}  // namespace himg

#endif  // ENCODER_H_
