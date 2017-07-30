//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef YCBCR_H_
#define YCBCR_H_

#include <cstdint>

namespace himg {

class YCbCr {
 public:
  static void RGBToYCbCr(uint8_t *out,
                         const uint8_t *in,
                         int width,
                         int height,
                         int pixel_stride,
                         int num_channels);

  static void YCbCrToRGB(uint8_t *buf,
                         int width,
                         int height,
                         int num_channels);
};

}  // namespace himg

#endif // YCBCR_H_
