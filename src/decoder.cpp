//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "decoder.h"

#include <algorithm>
#include <iostream>

#include "common.h"
#include "downsampled.h"
#include "hadamard.h"
#include "huffman.h"
#include "quantize.h"

namespace himg {

namespace {

uint8_t ClampTo8Bit(int16_t x) {
  return x >= 0 ? (x <= 255 ? static_cast<uint8_t>(x) : 255) : 0;
}

void RestoreChannelBlock(uint8_t *out,
                         const int16_t *in,
                         int pixel_stride,
                         int row_stride,
                         int block_width,
                         int block_height) {
  for (int y = 0; y < block_height; y++) {
    for (int x = 0; x < block_width; x++) {
      *out = ClampTo8Bit(*in++);
      out += pixel_stride;
    }
    in += 8 - block_width;
    out += row_stride - (pixel_stride * block_width);
  }
}

void YCbCrToRGB(uint8_t *buf,
                int width,
                int height,
                int num_channels) {
  // We use a multiplier-less approximation:
  //   Y  = (R + 2G + B) / 4
  //   Cb = B - G
  //   Cr = R - G
  //
  //   G = Y - (Cb + Cr) / 4
  //   B = G + Cb
  //   R = G + Cr
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // Convert YCbCr -> RGB.
      int16_t y = static_cast<int16_t>(buf[0]);
      int16_t cb = (static_cast<int16_t>(buf[1]) << 1) - 255;
      int16_t cr = (static_cast<int16_t>(buf[2]) << 1) - 255;
      int16_t g = y - ((cb + cr + 2) >> 2);
      int16_t b = g + cb;
      int16_t r = g + cr;
      buf[0] = ClampTo8Bit(r);
      buf[1] = ClampTo8Bit(g);
      buf[2] = ClampTo8Bit(b);

      // Note: Remaining channels are kept as-is (e.g. alpha).

      buf += num_channels;
    }
  }
}

}  // namespace

Decoder::Decoder() {
}

bool Decoder::Decode(const uint8_t *packed_data, int packed_size) {
  m_packed_data = packed_data;
  m_packed_size = packed_size;
  m_packed_idx = 0;

  m_unpacked_data.clear();

  // Header data.
  if (!DecodeHeader()) {
    std::cout << "Error decoding header.\n";
    return false;
  }

  // Quantization table.
  if (!DecodeQuantizationTable()) {
    std::cout << "Error decoding quantization table.\n";
    return false;
  }

  // Lowres data.
  if (!DecodeLowRes()) {
    std::cout << "Error decoding low-res image.\n";
    return false;
  }

  // Full resolution data.
  if (!DecodeFullRes()) {
    std::cout << "Error decoding full-res image.\n";
    return false;
  }

  return true;
}

bool Decoder::DecodeHeader() {
  // Check magic ID and version.
  if (m_packed_size < 15) {
    return false;
  }
  if (m_packed_data[0] != 'H' || m_packed_data[1] != 'I' ||
      m_packed_data[2] != 'M' || m_packed_data[3] != 'G' ||
      m_packed_data[4] != 1) {
    std::cout << "Incorrect magic ID or version number.\n";
    return false;
  }

  // Get image dimensions.
  m_width = static_cast<int>(m_packed_data[5]) |
            (static_cast<int>(m_packed_data[6]) << 8) |
            (static_cast<int>(m_packed_data[7]) << 16) |
            (static_cast<int>(m_packed_data[8]) << 24);
  m_height = static_cast<int>(m_packed_data[9]) |
             (static_cast<int>(m_packed_data[10]) << 8) |
             (static_cast<int>(m_packed_data[11]) << 16) |
             (static_cast<int>(m_packed_data[12]) << 24);
  m_num_channels = static_cast<int>(m_packed_data[13]);
  m_use_ycbcr = m_packed_data[14] != 0;

  std::cout << "Image dimensions: " << m_width << "x" << m_height << " ("
            << m_num_channels << " channels).\n";

  m_packed_idx = 15;

  return true;
}

bool Decoder::DecodeQuantizationTable() {
  // The quantization table is RLE encoded (we could do even better, but this
  // typically takes less than 10 bytes, so...).
  uint16_t table_idx = 0;
  for (int i = 0; i < 64 && m_packed_idx < m_packed_size - 1;) {
    uint8_t x = m_packed_data[m_packed_idx++];
    int count = static_cast<int>(m_packed_data[m_packed_idx++]);
    i += count;
    for (; count > 0 && table_idx < 64; --count) {
      m_quant_table[kIndexLUT[table_idx++]] = x;
    }
    if (count > 0)
      return false;
  }

  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      std::cout << static_cast<int>(m_quant_table[i * 8 + j]) << " ";
    }
    std::cout << "\n";
  }

  return table_idx == 64;
}

bool Decoder::DecodeLowRes() {
  // Prepare a buffer for all channels.
  const int num_rows = (m_height + 7) >> 3;
  const int num_cols = (m_width + 7) >> 3;
  const int num_blocks = num_rows * num_cols;
  const int channel_size = num_blocks;
  const int unpacked_size = channel_size * m_num_channels;
  std::vector<uint8_t> unpacked_data(unpacked_size);

  // Uncompress source data.
  if (!UncompressData(unpacked_data.data(), unpacked_size))
    return false;

  // Initialize the downsampled version of each channel.
  for (int chan = 0; chan < m_num_channels; ++chan) {
    m_downsampled.push_back(Downsampled());
    Downsampled &downsampled = m_downsampled.back();
    downsampled.SetBlockData(
        unpacked_data.data() + channel_size * chan, num_rows, num_cols);
  }

  std::cout << "Decoded lowres image " << num_cols << "x" << num_rows << std::endl;

  return true;
}

bool Decoder::DecodeFullRes() {
  // Reserve space for the output data.
  m_unpacked_data.resize(m_width * m_height * m_num_channels);

  // Prepare an unpacked buffer for all channels.
  const int num_blocks = ((m_width + 7) >> 3) * ((m_height + 7) >> 3);
  const int unpacked_size = num_blocks * 64 * m_num_channels;
  std::vector<uint8_t> unpacked_data(unpacked_size);

  // Uncompress all channels using Huffman.
  if (!UncompressData(unpacked_data.data(), unpacked_size))
    return false;
  std::cout << "Uncompressed full res data.\n";

  // Process all the 8x8 blocks.
  int unpacked_idx = 0;
  for (int y = 0; y < m_height; y += 8) {
    // Vertical block coordinate (v).
    int v = y >> 3;
    int block_height = std::min(8, m_height - y);

    // TODO(m): Do Huffman decompression of a single block row at a time.

    // All channels are inteleaved per block row.
    for (int chan = 0; chan < m_num_channels; ++chan) {
      // Get the low-res (divided by 8x8) image for this channel.
      Downsampled &downsampled = m_downsampled[chan];

      for (int x = 0; x < m_width; x += 8) {
        // Horizontal block coordinate (u).
        int u = x >> 3;
        int block_width = std::min(8, m_width - x);

        // Get quantized data from the unpacked buffer.
        uint8_t packed[64];
        for (int i = 0; i < 64; ++i) {
          packed[kIndexLUT[i]] =
              unpacked_data[unpacked_idx + u + i * downsampled.columns()];
        }

        // De-quantize.
        int16_t buf1[64];
        Quantize::Unpack(buf1, packed, m_quant_table);

        // Inverse transform.
        int16_t buf0[64];
        Hadamard::Inverse(buf0, buf1);

        // Add low-res component.
        int16_t lowres[64];
        downsampled.GetLowresBlock(lowres, u, v);
        for (int i = 0; i < 64; ++i) {
          buf0[i] += lowres[i];
        }

        // Copy color channel to destination data.
        RestoreChannelBlock(
            &m_unpacked_data[(y * m_width + x) * m_num_channels + chan],
            buf0,
            m_num_channels,
            m_width * m_num_channels,
            block_width,
            block_height);
      }

      unpacked_idx += downsampled.columns() * 64;
    }

    // Do YCbCr->RGB conversion for this block row if necessary.
    if (m_use_ycbcr) {
      uint8_t *buf = &m_unpacked_data[y * m_width * m_num_channels];
      YCbCrToRGB(buf, m_width, block_height, m_num_channels);
    }
  }

  return true;
}

bool Decoder::UncompressData(uint8_t *out, int out_size) {
  // Get the input data size (packed size).
  if ((m_packed_idx + 4) > m_packed_size)
    return false;
  int in_size = static_cast<int>(m_packed_data[m_packed_idx]) |
                (static_cast<int>(m_packed_data[m_packed_idx + 1]) << 8) |
                (static_cast<int>(m_packed_data[m_packed_idx + 2]) << 16) |
                (static_cast<int>(m_packed_data[m_packed_idx + 3]) << 24);
  m_packed_idx += 4;

  // Uncompress data.
  if ((m_packed_idx + in_size) > m_packed_size)
    return false;
  std::cout << "Unpacking " << in_size << " Huffman compressed bytes."
            << std::endl;
  Huffman::Uncompress(out, m_packed_data + m_packed_idx, in_size, out_size);
  m_packed_idx += in_size;

  return true;
}

}  // namespace himg
