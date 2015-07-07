//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "encoder.h"

#include <algorithm>
#include <iostream>

#include "common.h"
#include "downsampled.h"
#include "hadamard.h"
#include "huffman.h"
#include "quantize.h"
#include "ycbcr.h"

namespace himg {

namespace {

void ExtractChannelBlock(int16_t *out,
                         const uint8_t *in,
                         int channel,
                         int pixel_stride,
                         int row_stride,
                         int block_width,
                         int block_height) {
  int16_t col = 0;
  int x, y;
  for (y = 0; y < block_height; y++) {
    for (x = 0; x < block_width; x++) {
      col = static_cast<int16_t>(in[channel]);
      in += pixel_stride;
      *out++ = col;
    }
    for (; x < 8; x++) {
      *out++ = col;
    }
    in += row_stride - (pixel_stride * block_width);
  }
  for (; y < 8; y++) {
    for (x = 0; x < 8; x++) {
      // We could do better here...
      *out++ = col;
    }
  }
}

}  // namespace

Encoder::Encoder() {
}

bool Encoder::Encode(const uint8_t *data,
                     int width,
                     int height,
                     int pixel_stride,
                     int num_channels,
                     int quality,
                     bool use_ycbcr) {
  m_packed_data.clear();

  m_quality = quality;
  m_use_ycbcr = use_ycbcr && (num_channels >= 3);

  // This is a RIFF file.
  EncodeRIFFStart();

  // Header data.
  EncodeHeader(width, height, num_channels);

  // Optionally convert to YCrCb.
  const uint8_t *color_space_data = data;
  std::vector<uint8_t> ycbcr_data;
  if (m_use_ycbcr) {
    ycbcr_data.resize(width * height * pixel_stride);
    YCbCr::RGBToYCbCr(
        ycbcr_data.data(), data, width, height, pixel_stride, num_channels);
    color_space_data = ycbcr_data.data();
  }

  // Lowres data.
  EncodeLowRes(color_space_data, width, height, pixel_stride, num_channels);

  // Generate the quantization configuration for the full resolution data.
  m_quantize.InitForQuality(m_quality, m_use_ycbcr);
  EncodeQuantizationConfig();
  EncodeFullResMappingFunction();

  // Full resolution data.
  EncodeFullRes(color_space_data, width, height, pixel_stride, num_channels);

  // Update the RIFF header.
  UpdateRIFFStart();

  return true;
}

void Encoder::EncodeRIFFStart() {
  m_packed_data.reserve(12);

  m_packed_data.push_back('R');
  m_packed_data.push_back('I');
  m_packed_data.push_back('F');
  m_packed_data.push_back('F');

  // The file size, which is updated once the compression process is completed.
  m_packed_data.push_back(0);
  m_packed_data.push_back(0);
  m_packed_data.push_back(0);
  m_packed_data.push_back(0);

  m_packed_data.push_back('H');
  m_packed_data.push_back('I');
  m_packed_data.push_back('M');
  m_packed_data.push_back('G');
}

void Encoder::UpdateRIFFStart() {
  uint32_t file_size = m_packed_data.size() - 8;
  m_packed_data[4] = file_size & 255;
  m_packed_data[5] = (file_size >> 8) & 255;
  m_packed_data[6] = (file_size >> 16) & 255;
  m_packed_data[7] = (file_size >> 24) & 255;
}

void Encoder::EncodeHeader(int width,
                           int height,
                           int num_channels) {
  const int header_size = 11;
  m_packed_data.reserve(m_packed_data.size() + 8 + header_size);

  m_packed_data.push_back('F');
  m_packed_data.push_back('R');
  m_packed_data.push_back('M');
  m_packed_data.push_back('T');

  m_packed_data.push_back(header_size & 255);
  m_packed_data.push_back((header_size >> 8) & 255);
  m_packed_data.push_back((header_size >> 16) & 255);
  m_packed_data.push_back((header_size >> 24) & 255);

  m_packed_data.push_back(1);  // Version
  m_packed_data.push_back(width & 255);
  m_packed_data.push_back((width >> 8) & 255);
  m_packed_data.push_back((width >> 16) & 255);
  m_packed_data.push_back((width >> 24) & 255);
  m_packed_data.push_back(height & 255);
  m_packed_data.push_back((height >> 8) & 255);
  m_packed_data.push_back((height >> 16) & 255);
  m_packed_data.push_back((height >> 24) & 255);
  m_packed_data.push_back(num_channels);
  m_packed_data.push_back(m_use_ycbcr ? 1 : 0);  // Color space (RGB / YCbCr).
}

void Encoder::EncodeLowRes(const uint8_t *data,
                           int width,
                           int height,
                           int pixel_stride,
                           int num_channels) {
  m_packed_data.push_back('L');
  m_packed_data.push_back('R');
  m_packed_data.push_back('E');
  m_packed_data.push_back('S');

  // Construct low-res (divided by 8x8) images for all channels.
  for (int chan = 0; chan < num_channels; ++chan) {
    m_downsampled.push_back(Downsampled());
    Downsampled &downsampled = m_downsampled.back();
    downsampled.SampleImage(data + chan, pixel_stride, width, height);
  }

  // Prepare an unpacked buffer all channels.
  const int num_blocks = ((width + 7) >> 3) * ((height + 7) >> 3);
  const int channel_size = num_blocks;
  const int unpacked_size = channel_size * num_channels;
  std::vector<uint8_t> unpacked_data(unpacked_size);

  // Get the low-res versions of the image fo all channels (delta encoded).
  for (int chan = 0; chan < num_channels; ++chan) {
    m_downsampled[chan].GetBlockData(unpacked_data.data() +
                                     chan * channel_size);
  }

  // Compress data.
  int packed_size = AppendPackedData(unpacked_data.data(), unpacked_size);
  std::cout << "Low resolution data: " << packed_size << " bytes.\n";
}

void Encoder::EncodeQuantizationConfig() {
  // Store the quantization data in the output buffer.
  m_packed_data.push_back('Q');
  m_packed_data.push_back('C');
  m_packed_data.push_back('F');
  m_packed_data.push_back('G');

  int config_size = m_quantize.ConfigurationSize();
  m_packed_data.push_back(config_size & 255);
  m_packed_data.push_back((config_size >> 8) & 255);
  m_packed_data.push_back((config_size >> 16) & 255);
  m_packed_data.push_back((config_size >> 24) & 255);

  int quantization_config_base = static_cast<int>(m_packed_data.size());
  m_packed_data.resize(quantization_config_base + config_size);
  m_quantize.GetConfiguration(&m_packed_data[quantization_config_base]);
}

void Encoder::EncodeFullResMappingFunction() {
  // Store the quantization data in the output buffer.
  m_packed_data.push_back('F');
  m_packed_data.push_back('M');
  m_packed_data.push_back('A');
  m_packed_data.push_back('P');

  int map_fun_size = m_quantize.MappingFunctionSize();
  m_packed_data.push_back(map_fun_size & 255);
  m_packed_data.push_back((map_fun_size >> 8) & 255);
  m_packed_data.push_back((map_fun_size >> 16) & 255);
  m_packed_data.push_back((map_fun_size >> 24) & 255);

  int map_fun_base = static_cast<int>(m_packed_data.size());
  m_packed_data.resize(map_fun_base + map_fun_size);
  m_quantize.GetMappingFunction(&m_packed_data[map_fun_base]);
}

void Encoder::EncodeFullRes(const uint8_t *data,
                            int width,
                            int height,
                            int pixel_stride,
                            int num_channels) {
  m_packed_data.push_back('F');
  m_packed_data.push_back('R');
  m_packed_data.push_back('E');
  m_packed_data.push_back('S');

  // Prepare an unpacked buffer for all channels.
  const int num_blocks = ((width + 7) >> 3) * ((height + 7) >> 3);
  const int unpacked_size = num_blocks * 64 * num_channels;
  std::vector<uint8_t> unpacked_data(unpacked_size);

  // Process all the 8x8 blocks.
  int unpacked_idx = 0;
  for (int y = 0; y < height; y += 8) {
    // Vertical block coordinate (v).
    int v = y >> 3;

    // Interleave all channels per block row.
    for (int chan = 0; chan < num_channels; ++chan) {
      // Get the low-res (divided by 8x8) image for this channel.
      Downsampled &downsampled = m_downsampled[chan];

      bool is_chroma_channel = m_use_ycbcr && (chan == 1 || chan == 2);

      for (int x = 0; x < width; x += 8) {
        // Horizontal block coordinate (u).
        int u = x >> 3;

        // Size of this block (usually 8x8, but smaller around the edges).
        int block_width = std::min(8, width - x);
        int block_height = std::min(8, height - y);

        // Copy color channel from source data.
        int16_t buf0[64];
        ExtractChannelBlock(buf0,
                            &data[(y * width + x) * pixel_stride],
                            chan,
                            pixel_stride,
                            width * pixel_stride,
                            block_width,
                            block_height);

        // Remove low-res component.
        int16_t lowres[64];
        downsampled.GetLowresBlock(lowres, u, v);
        for (int i = 0; i < 64; ++i) {
          buf0[i] -= lowres[i];
        }

        // Forward transform.
        int16_t buf1[64];
        Hadamard::Forward(buf1, buf0);

        // Quantize.
        uint8_t packed[64];
        m_quantize.Pack(packed, buf1, is_chroma_channel);

        // Store quantized data in the unpacked buffer.
        for (int i = 0; i < 64; ++i) {
          unpacked_data[unpacked_idx + u + i * downsampled.columns()] =
              packed[kIndexLUT[i]];
        }
      }

      unpacked_idx += downsampled.columns() * 64;
    }
  }

  // Compress all channels.
  int packed_size = AppendPackedData(unpacked_data.data(), unpacked_size);
  std::cout << "Full resolution data: " << packed_size << " bytes.\n";
}

int Encoder::AppendPackedData(
    const uint8_t *unpacked_data, int unpacked_size) {
  const int packed_base_idx = static_cast<int>(m_packed_data.size());
  m_packed_data.resize(packed_base_idx + 4 +
                       Huffman::MaxCompressedSize(unpacked_size));
  int packed_size =
      Huffman::Compress(m_packed_data.data() + packed_base_idx + 4,
                        unpacked_data,
                        unpacked_size);
  m_packed_data[packed_base_idx] = packed_size & 255;
  m_packed_data[packed_base_idx + 1] = (packed_size >> 8) & 255;
  m_packed_data[packed_base_idx + 2] = (packed_size >> 16) & 255;
  m_packed_data[packed_base_idx + 3] = (packed_size >> 24) & 255;
  m_packed_data.resize(packed_base_idx + 4 + packed_size);
  return packed_size;
}

}  // namespace himg
