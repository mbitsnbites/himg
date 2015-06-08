//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "downsampled.h"

#include <algorithm>

namespace himg {

Downsampled::Downsampled() : m_rows(0), m_columns(0) {
}

void Downsampled::SampleImage(const uint8_t *pixels,
                              int stride,
                              int width,
                              int height) {
  // Divide by 8x8, rounding up.
  m_rows = (height + 7) >> 3;
  m_columns = (width + 7) >> 3;

  // Calculate average color for each 8x8 block.
  std::vector<uint8_t> average;
  average.reserve(m_rows * m_columns);
  for (int v = 0; v < m_rows; ++v) {
    int y_min = std::max(0, v * 8 - 3);
    int y_max = std::min(height - 1, v * 8 + 4);
    for (int u = 0; u < m_columns; ++u) {
      int x_min = std::max(0, u * 8 - 3);
      int x_max = std::min(width - 1, u * 8 + 4);
      uint16_t sum = 0;
      for (int y = y_min; y <= y_max; ++y) {
        for (int x = x_min; x <= x_max; ++x) {
          sum += pixels[(y * width + x) * stride];
        }
      }
      int total_count = (x_max - x_min + 1) * (y_max - y_min + 1);
      average.push_back(
          static_cast<uint8_t>((sum + (total_count >> 1)) / total_count));
    }
  }

  // Compensate blocks for lienear interpolation (phase shift 1/16 pixels up &
  // to the left).
  m_data.reserve(m_columns * m_rows);
  for (int v = 0; v < m_rows; ++v) {
    int row1 = std::max(0, v - 1);
    int row2 = v;
    for (int u = 0; u < m_columns; ++u) {
      int col1 = std::max(0, u - 1);
      int col2 = u;
      uint16_t x11 = static_cast<uint16_t>(average[row1 * m_columns + col1]);
      uint16_t x12 = static_cast<uint16_t>(average[row1 * m_columns + col2]);
      uint16_t x21 = static_cast<uint16_t>(average[row2 * m_columns + col1]);
      uint16_t x22 = static_cast<uint16_t>(average[row2 * m_columns + col2]);
      uint16_t a1 = (1 * x11 + 15 * x12 + 8) >> 4;
      uint16_t a2 = (1 * x21 + 15 * x22 + 8) >> 4;
      m_data.push_back(static_cast<uint8_t>((1 * a1 + 15 * a2 + 8) >> 4));
    }
  }
}

void Downsampled::GetLowresBlock(int16_t *out, int u, int v) {
  // Pick out the four values in the corners of the block.
  int row1 = v;
  int row2 = std::min(m_rows - 1, v + 1);
  int col1 = u;
  int col2 = std::min(m_columns - 1, u + 1);
  int16_t x11 = static_cast<int16_t>(m_data[row1 * m_columns + col1]);
  int16_t x12 = static_cast<int16_t>(m_data[row1 * m_columns + col2]);
  int16_t x21 = static_cast<int16_t>(m_data[row2 * m_columns + col1]);
  int16_t x22 = static_cast<int16_t>(m_data[row2 * m_columns + col2]);

  // Liner interpolation to produce the left and right columns of the block.
  int16_t left[9], right[9];
  left[0] = x11;
  left[8] = x21;
  left[4] = (left[0] + left[8]) >> 1;
  left[2] = (left[0] + left[4]) >> 1;
  left[6] = (left[4] + left[8]) >> 1;
  left[1] = (left[0] + left[2]) >> 1;
  left[3] = (left[2] + left[4]) >> 1;
  left[5] = (left[4] + left[6]) >> 1;
  left[7] = (left[6] + left[8]) >> 1;
  right[0] = x12;
  right[8] = x22;
  right[4] = (right[0] + right[8]) >> 1;
  right[2] = (right[0] + right[4]) >> 1;
  right[6] = (right[4] + right[8]) >> 1;
  right[1] = (right[0] + right[2]) >> 1;
  right[3] = (right[2] + right[4]) >> 1;
  right[5] = (right[4] + right[6]) >> 1;
  right[7] = (right[6] + right[8]) >> 1;

  // Liner interpolation to produce the eight rows of the block.
  for (int y = 0; y < 8; ++y) {
    int16_t a0 = left[y];
    int16_t a8 = right[y];
    int16_t a4 = (a0 + a8) >> 1;
    int16_t a2 = (a0 + a4) >> 1;
    int16_t a6 = (a4 + a8) >> 1;
    int16_t a1 = (a0 + a2) >> 1;
    int16_t a3 = (a2 + a4) >> 1;
    int16_t a5 = (a4 + a6) >> 1;
    int16_t a7 = (a6 + a8) >> 1;
    out[0] = a0;
    out[1] = a1;
    out[2] = a2;
    out[3] = a3;
    out[4] = a4;
    out[5] = a5;
    out[6] = a6;
    out[7] = a7;
    out += 8;
  }
}

void Downsampled::GetBlockData(uint8_t *out) const {
  for (int v = 0; v < m_rows; ++v) {
    for (int u = 0; u < m_columns; ++u) {
      uint8_t predicted;
      if (u > 0 && v > 0) {
        predicted = m_data[v * m_columns + u - 1] +
                    m_data[(v - 1) * m_columns + u] -
                    m_data[(v - 1) * m_columns + u - 1];
      } else if (u > 0) {
        predicted = m_data[v * m_columns + u - 1];
      } else if (v > 0) {
        predicted = m_data[(v - 1) * m_columns + u];
      } else {
        predicted = 0;
      }
      *out++ = m_data[v * m_columns + u] - predicted;
    }
  }
}

void Downsampled::SetBlockData(const uint8_t *in, int rows, int columns) {
  m_rows = rows;
  m_columns = columns;
  m_data.resize(m_rows * m_columns);
  for (int v = 0; v < m_rows; ++v) {
    for (int u = 0; u < m_columns; ++u) {
      uint8_t predicted;
      if (u > 0 && v > 0) {
        predicted = m_data[v * m_columns + u - 1] +
                    m_data[(v - 1) * m_columns + u] -
                    m_data[(v - 1) * m_columns + u - 1];
      } else if (u > 0) {
        predicted = m_data[v * m_columns + u - 1];
      } else if (v > 0) {
        predicted = m_data[(v - 1) * m_columns + u];
      } else {
        predicted = 0;
      }
      m_data[v * m_columns + u] = *in++ + predicted;
    }
  }
}

}  // namespace himg
