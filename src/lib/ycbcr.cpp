//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "ycbcr.h"

#include "common.h"

namespace himg {

// We use a multiplier-less approximation:
//   Y  = (R + 2G + B) / 4
//   Cb = B - G
//   Cr = R - G
//
//   G = Y - (Cb + Cr) / 4
//   B = G + Cb
//   R = G + Cr

void YCbCr::RGBToYCbCr(uint8_t *out,
                       const uint8_t *in,
                       int width,
                       int height,
                       int pixel_stride,
                       int num_channels) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // Convert RGB -> YCbCr.
      int16_t r = static_cast<int16_t>(in[0]);
      int16_t g = static_cast<int16_t>(in[1]);
      int16_t b = static_cast<int16_t>(in[2]);
      int16_t y = (r + 2 * g + b + 2) >> 2;
      int16_t cb = (b - g + 256) >> 1;
      int16_t cr = (r - g + 256) >> 1;
      out[0] = static_cast<uint8_t>(y);
      out[1] = static_cast<uint8_t>(cb);
      out[2] = static_cast<uint8_t>(cr);

      // Append remaining channels as-is (e.g. alpha).
      for (int chan = 3; chan < num_channels; ++chan) {
        out[chan] = in[chan];
      }

      in += pixel_stride;
      out += pixel_stride;
    }
  }
}

void YCbCr::YCbCrToRGB(uint8_t *buf,
                       int width,
                       int height,
                       int num_channels) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // Convert YCbCr -> RGB.
      int16_t y = static_cast<int16_t>(buf[0]);
      int16_t cb = (static_cast<int16_t>(buf[1]) << 1) - 255;
      int16_t cr = (static_cast<int16_t>(buf[2]) << 1) - 255;
      int16_t g = y - ((cb + cr + 2) >> 2);
      int16_t b = g + cb;
      int16_t r = g + cr;
      if (LIKELY(((r | g | b) & 0xff00) == 0)) {
        buf[0] = static_cast<uint8_t>(r);
        buf[1] = static_cast<uint8_t>(g);
        buf[2] = static_cast<uint8_t>(b);
      } else {
        buf[0] = ClampTo8Bit(r);
        buf[1] = ClampTo8Bit(g);
        buf[2] = ClampTo8Bit(b);
      }

      // Note: Remaining channels are kept as-is (e.g. alpha).

      buf += num_channels;
    }
  }
}

}  // namespace himg
