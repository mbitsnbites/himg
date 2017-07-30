//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "decoder.h"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "common.h"
#include "downsampled.h"
#include "hadamard.h"
#include "mapper.h"
#include "quantize.h"
#include "ycbcr.h"

namespace himg {

namespace {

uint32_t ToFourcc(const char name[4]) {
  return static_cast<uint32_t>(name[0]) |
         (static_cast<uint32_t>(name[1]) << 8) |
         (static_cast<uint32_t>(name[2]) << 16) |
         (static_cast<uint32_t>(name[3]) << 24);
}

void RestoreChannelBlock(uint8_t *out,
                         const int16_t *in,
                         int pixel_stride,
                         int row_stride,
                         int block_width,
                         int block_height) {
  for (int y = 0; y < block_height; y++) {
    if (LIKELY(block_width == 8)) {
      // Fast path.
      for (int i = 0; i < 2; ++i) {
        int16_t c1 = *in++;
        int16_t c2 = *in++;
        int16_t c3 = *in++;
        int16_t c4 = *in++;
        if (LIKELY(((c1 | c2 | c3 | c4) & 0xff00) == 0)) {
          out[0] = static_cast<uint8_t>(c1);
          out[pixel_stride] = static_cast<uint8_t>(c2);
          out[2 * pixel_stride] = static_cast<uint8_t>(c3);
          out[3 * pixel_stride] = static_cast<uint8_t>(c4);
        } else {
          out[0] = ClampTo8Bit(c1);
          out[pixel_stride] = ClampTo8Bit(c2);
          out[2 * pixel_stride] = ClampTo8Bit(c3);
          out[3 * pixel_stride] = ClampTo8Bit(c4);
        }
        out += 4 * pixel_stride;
      }
    } else {
      // Slow path.
      for (int y = 0; y < block_height; y++) {
        for (int x = 0; x < block_width; x++) {
          *out = ClampTo8Bit(*in++);
          out += pixel_stride;
        }
        in += 8 - block_width;
      }
    }
    out += row_stride - (pixel_stride * block_width);
  }
}

}  // namespace

Decoder::Decoder(int max_threads) {
  if (max_threads <= 0) {
    m_max_threads = std::thread::hardware_concurrency();
  } else {
    m_max_threads = max_threads;
  }
}

bool Decoder::Decode(const uint8_t *packed_data, int packed_size) {
  m_packed_data = packed_data;
  m_packed_size = packed_size;
  m_packed_idx = 0;

  m_unpacked_data.clear();
  m_downsampled.clear();

  // Check that this is a RIFF HIMG file.
  if (!DecodeRIFFStart()) {
    std::cout << "Not a RIFF HIMG file.\n";
    return false;
  }

  // Header data.
  if (!DecodeHeader()) {
    std::cout << "Error decoding header.\n";
    return false;
  }

  // Low resolution mapping table.
  if (!DecodeLowResMappingFunction()) {
    std::cout << "Error decoding low-res mapping function.\n";
    return false;
  }

  // Lowres data.
  if (!DecodeLowRes()) {
    std::cout << "Error decoding low-res data.\n";
    return false;
  }

  // Quantization table.
  if (!DecodeQuantizationConfig()) {
    std::cout << "Error decoding quantization configuration.\n";
    return false;
  }

  // Full resolution mapping table.
  if (!DecodeFullResMappingFunction()) {
    std::cout << "Error decoding full-res mapping function.\n";
    return false;
  }

  // Full resolution data.
  if (!DecodeFullRes()) {
    std::cout << "Error decoding full-res data.\n";
    return false;
  }

  return true;
}

bool Decoder::HasChroma() const {
  return m_use_ycbcr && m_num_channels >= 3;
}

bool Decoder::DecodeRIFFStart() {
  if (m_packed_size < 12)
    return false;

  if (m_packed_data[0] != 'R' || m_packed_data[1] != 'I' ||
      m_packed_data[2] != 'F' || m_packed_data[3] != 'F')
    return false;

  int file_size = static_cast<int>(m_packed_data[4]) |
                  (static_cast<int>(m_packed_data[5]) << 8) |
                  (static_cast<int>(m_packed_data[6]) << 16) |
                  (static_cast<int>(m_packed_data[7]) << 24);
  if (file_size + 8 != m_packed_size)
    return false;

  if (m_packed_data[8] != 'H' || m_packed_data[9] != 'I' ||
      m_packed_data[10] != 'M' || m_packed_data[11] != 'G')
    return false;

  m_packed_idx += 12;

  return true;
}

bool Decoder::DecodeHeader() {
  // Find the FRMT chunk.
  int chunk_size;
  if (!FindRIFFChunk(ToFourcc("FRMT"), &chunk_size))
    return false;
  const uint8_t *chunk_data = &m_packed_data[m_packed_idx];
  m_packed_idx += chunk_size;

  // Check the header size.
  if (chunk_size < 11)
    return false;

  // Check version.
  uint8_t version = chunk_data[0];
  if (version != 1) {
    std::cout << "Incorrect HIMG version number.\n";
    return false;
  }

  // Get image dimensions.
  m_width = static_cast<int>(chunk_data[1]) |
            (static_cast<int>(chunk_data[2]) << 8) |
            (static_cast<int>(chunk_data[3]) << 16) |
            (static_cast<int>(chunk_data[4]) << 24);
  m_height = static_cast<int>(chunk_data[5]) |
             (static_cast<int>(chunk_data[6]) << 8) |
             (static_cast<int>(chunk_data[7]) << 16) |
             (static_cast<int>(chunk_data[8]) << 24);
  m_num_channels = static_cast<int>(chunk_data[9]);
  m_use_ycbcr = chunk_data[10] != 0;

  return true;
}

bool Decoder::DecodeLowResMappingFunction() {
  // Find the LMAP chunk.
  int chunk_size;
  if (!FindRIFFChunk(ToFourcc("LMAP"), &chunk_size))
    return false;
  const uint8_t *chunk_data = &m_packed_data[m_packed_idx];
  m_packed_idx += chunk_size;

  // Restore the mapping function.
  return m_low_res_mapper.SetMappingFunction(chunk_data, chunk_size);
}

bool Decoder::DecodeLowRes() {
  // Find the LRES chunk.
  int chunk_size;
  if (!FindRIFFChunk(ToFourcc("LRES"), &chunk_size))
    return false;

  // Prepare a buffer for all channels.
  const int num_rows = (m_height + 7) >> 3;
  const int num_cols = (m_width + 7) >> 3;
  const int channel_size =
      Downsampled::BlockDataSizePerChannel(num_rows, num_cols);
  const int unpacked_size = channel_size * m_num_channels;
  std::vector<uint8_t> unpacked_data(unpacked_size);

  // Uncompress source Huffman data.
  HuffmanDec huffman_dec(m_packed_data + m_packed_idx, chunk_size, 0);
  if (!huffman_dec.Init() ||
      !huffman_dec.Uncompress(unpacked_data.data(), unpacked_size)) {
    std::cout << "Error: Invalid Huffman data.\n";
    return false;
  }
  m_packed_idx += chunk_size;

  // Initialize the downsampled version of each channel.
  for (int chan = 0; chan < m_num_channels; ++chan) {
    m_downsampled.push_back(Downsampled());
    Downsampled &downsampled = m_downsampled.back();
    downsampled.SetBlockData(unpacked_data.data() + channel_size * chan,
                             num_rows,
                             num_cols,
                             m_low_res_mapper);
  }

  return true;
}

bool Decoder::DecodeQuantizationConfig() {
  // Find the QCFG chunk.
  int chunk_size;
  if (!FindRIFFChunk(ToFourcc("QCFG"), &chunk_size))
    return false;
  const uint8_t *chunk_data = &m_packed_data[m_packed_idx];
  m_packed_idx += chunk_size;

  // Restore the configuration.
  return m_quantize.SetConfiguration(chunk_data, chunk_size, HasChroma());
}

bool Decoder::DecodeFullResMappingFunction() {
  // Find the FMAP chunk.
  int chunk_size;
  if (!FindRIFFChunk(ToFourcc("FMAP"), &chunk_size))
    return false;
  const uint8_t *chunk_data = &m_packed_data[m_packed_idx];
  m_packed_idx += chunk_size;

  // Restore the mapping function.
  return m_full_res_mapper.SetMappingFunction(chunk_data, chunk_size);
}

bool Decoder::DecodeFullRes() {
  // Find the FRES chunk.
  int chunk_size;
  if (!FindRIFFChunk(ToFourcc("FRES"), &chunk_size))
    return false;

  // Reserve space for the output data.
  m_unpacked_data.resize(m_width * m_height * m_num_channels);

  // Prepare uncompression of the Huffman data.
  const int huffman_block_size = ((m_width + 7) >> 3) * 64 * m_num_channels;
  HuffmanDec huffman_dec(m_packed_data + m_packed_idx, chunk_size, huffman_block_size);
  if (!huffman_dec.Init()) {
    std::cout << "Error: Invalid Huffman data.\n";
    return false;
  }
  m_packed_idx += chunk_size;

  // Process all the 8x8 blocks, one row at a time or several rows in parallel.
  {
    std::atomic_int next_row(0);
    std::atomic_bool success(true);

    // One worker core lambda is run in each worker thread.
    auto worker_core = [this, &huffman_dec, &next_row, &success]() {
      while (true) {
        int y = next_row.fetch_add(8, std::memory_order_relaxed);
        if (y >= m_height)
          break;
        if (!DecodeFullResBlockRow(huffman_dec, y)) {
          success = false;
          break;
        }
      }
    };

    // Start the worker threads (we start N - 1 new threads, and run one worker
    // in the current thread).
    int worker_threads = std::min((m_height + 7) / 8, m_max_threads);
    std::vector<std::thread> threads;
    for (int i = 0; i < worker_threads - 1; ++i)
      threads.push_back(std::thread(worker_core));

    // One worker is always run in the current thread.
    worker_core();

    // Wait for all the worker threads to finish.
    for (auto &thread : threads)
      thread.join();

    if (!success)
      return false;
  }

  return true;
}

bool Decoder::DecodeFullResBlockRow(const HuffmanDec &huffman_dec, int y) {
  // Determine the number of horizontal blocks.
  const int horizontal_blocks = (m_width + 7) >> 3;

  // Vertical block coordinate (v).
  int v = y >> 3;
  int block_height = std::min(8, m_height - y);

  // Prepare an unpacked buffer for all channels.
  int full_res_data_size = horizontal_blocks * m_num_channels * 64;
  std::vector<uint8_t> full_res_data(full_res_data_size);

  // Do Huffman decompression of a single block row.
  if (!huffman_dec.UncompressBlock(full_res_data.data(), full_res_data_size, v)) {
    std::cout << "Error: Invalid Huffman data.\n";
    return false;
  }

  int unpacked_idx = 0;

  // Allocate aligned working buffers (enable aligned memory access & SIMD).
  int16_t *buf0, *buf1, *lowres;
  static const int kBufferAlignment = 16;
  std::unique_ptr<int16_t> buffers(new int16_t[3 * 64 + kBufferAlignment - 1]);
  {
    intptr_t alignment_adjust =
        reinterpret_cast<intptr_t>(buffers.get()) & (kBufferAlignment - 1);
    if (alignment_adjust)
      alignment_adjust = kBufferAlignment - alignment_adjust;
    buf1 = reinterpret_cast<int16_t*>(
        ASSUME_ALIGNED16(buffers.get() + alignment_adjust));
    buf0 = reinterpret_cast<int16_t*>(ASSUME_ALIGNED16(buf1 + 64));
    lowres = reinterpret_cast<int16_t*>(ASSUME_ALIGNED16(buf0 + 64));
  }

  // All channels are inteleaved per block row.
  for (int chan = 0; chan < m_num_channels; ++chan) {
    // Get the low-res (divided by 8x8) image for this channel.
    Downsampled &downsampled = m_downsampled[chan];

    // Create an inverse index LUT for reading back the interleaved elements.
    int deinterleave_index[64];
    for (int i = 0; i < 64; ++i)
      deinterleave_index[kIndexLUT[i]] = i * horizontal_blocks;

    bool is_chroma_channel = m_use_ycbcr && (chan == 1 || chan == 2);

    for (int x = 0; x < m_width; x += 8) {
      // Horizontal block coordinate (u).
      int u = x >> 3;
      int block_width = std::min(8, m_width - x);

      // Get quantized data from the unpacked buffer.
      // NOTE: This seems to be a bottleneck on x86 (64). The irregular
      // addressing pattern and two levels of indirection seem to be the main
      // issues. Loop unrolling (e.g. -funroll-loops) helps to some extent.
      uint8_t packed[64];
      {
        const uint8_t *src = &full_res_data[unpacked_idx + u];
        for (int i = 0; i < 64; ++i)
          packed[i] = src[deinterleave_index[i]];
      }

      // De-quantize.
      m_quantize.Unpack(buf1, packed, is_chroma_channel, m_full_res_mapper);

      // Inverse transform.
      Hadamard::Inverse(buf0, buf1);

      // Add low-res component.
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

    unpacked_idx += horizontal_blocks * 64;
  }

  // Do YCbCr->RGB conversion for this block row if necessary.
  if (HasChroma()) {
    uint8_t *buf = &m_unpacked_data[y * m_width * m_num_channels];
    YCbCr::YCbCrToRGB(buf, m_width, block_height, m_num_channels);
  }

  return true;
}

bool Decoder::DecodeRIFFChunk(uint32_t *fourcc, int *size) {
  if ((m_packed_idx + 8) > m_packed_size)
    return false;

  *fourcc = static_cast<uint32_t>(m_packed_data[m_packed_idx]) |
            (static_cast<uint32_t>(m_packed_data[m_packed_idx + 1]) << 8) |
            (static_cast<uint32_t>(m_packed_data[m_packed_idx + 2]) << 16) |
            (static_cast<uint32_t>(m_packed_data[m_packed_idx + 3]) << 24);
  *size = static_cast<int>(m_packed_data[m_packed_idx + 4]) |
          (static_cast<int>(m_packed_data[m_packed_idx + 5]) << 8) |
          (static_cast<int>(m_packed_data[m_packed_idx + 6]) << 16) |
          (static_cast<int>(m_packed_data[m_packed_idx + 7]) << 24);

  m_packed_idx += 8;
  return m_packed_idx + *size <= m_packed_size;
}

bool Decoder::FindRIFFChunk(uint32_t fourcc, int *size) {
  uint32_t chunk_fourcc;
  int chunk_size;
  while (DecodeRIFFChunk(&chunk_fourcc, &chunk_size)) {
    // Did we find the requested chunk?
    if (chunk_fourcc == fourcc) {
      *size = chunk_size;
      return true;
    }

    // Unrecognized chunk. Skip to the next one.
    m_packed_idx += chunk_size;
  }

  // We didn't find the chunk.
  return false;
}

}  // namespace himg
