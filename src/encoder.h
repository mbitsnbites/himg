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
#include "quantize.h"

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
  void EncodeRIFFStart();
  void UpdateRIFFStart();
  void EncodeHeader(int width, int height, int num_channels);
  void EncodeLowRes(const uint8_t *data,
                    int width,
                    int height,
                    int pixel_stride,
                    int num_channels);
  void EncodeQuantizationConfig();
  void EncodeFullResMappingFunction();
  void EncodeFullRes(const uint8_t *data,
                     int width,
                     int height,
                     int pixel_stride,
                     int num_channels);

  int AppendPackedData(const uint8_t *unpacked_data, int unpacked_size);

  int m_quality;
  bool m_use_ycbcr;
  Quantize m_quantize;
  std::vector<Downsampled> m_downsampled;
  std::vector<uint8_t> m_packed_data;
};

}  // namespace himg

#endif  // ENCODER_H_
