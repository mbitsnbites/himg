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

namespace {

// We have five different ways to predict the current sample based on its
// neighbours (see PredictSample() for details).
const int kNumPredictors = 5;

// A macro block is a collection of low-res samples. An optimal predictor is
// selected for each macro block.
const int kMacroBlockSize = 16;

int NumMacroBlocks(int blocks) {
  return (blocks + kMacroBlockSize - 1) / kMacroBlockSize;
}

int16_t ClampTo8Bit(int16_t x) {
  return std::max(int16_t(0), std::min(x, int16_t(255)));
}

uint8_t EncodePredictor(int predictor) {
  return static_cast<uint8_t>(predictor - 2);
}

int DecodePredictor(uint8_t encoded_predictor) {
  return static_cast<int>(encoded_predictor + 2);
}

int16_t PredictSample(int16_t s1, int16_t s2, int16_t s3, int predictor) {
  switch (predictor) {
    default:
    case 0:
      // A mix between methods 3 & 4.
      return ClampTo8Bit((3 * (s2 + s3) - 2 * s1 + 2) >> 2);
    case 1:
      // Sample above.
      return s2;
    case 2:
      // Sample to the left.
      return s3;
    case 3:
      // Average of the two closest samples (above & left).
      return (s2 + s3 + 1) >> 1;
    case 4:
      // Linear extrapolation of s1, s2 & s3.
      return ClampTo8Bit(s2 + s3 - s1);
  }
}

}  // namespace

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
  left[4] = (left[0] + left[8] + 1) >> 1;
  left[2] = (left[0] + left[4] + 1) >> 1;
  left[6] = (left[4] + left[8] + 1) >> 1;
  left[1] = (left[0] + left[2] + 1) >> 1;
  left[3] = (left[2] + left[4] + 1) >> 1;
  left[5] = (left[4] + left[6] + 1) >> 1;
  left[7] = (left[6] + left[8] + 1) >> 1;
  right[0] = x12;
  right[8] = x22;
  right[4] = (right[0] + right[8] + 1) >> 1;
  right[2] = (right[0] + right[4] + 1) >> 1;
  right[6] = (right[4] + right[8] + 1) >> 1;
  right[1] = (right[0] + right[2] + 1) >> 1;
  right[3] = (right[2] + right[4] + 1) >> 1;
  right[5] = (right[4] + right[6] + 1) >> 1;
  right[7] = (right[6] + right[8] + 1) >> 1;

  // Liner interpolation to produce the eight rows of the block.
  for (int y = 0; y < 8; ++y) {
    int16_t a0 = left[y];
    int16_t a8 = right[y];
    int16_t a4 = (a0 + a8 + 1) >> 1;
    int16_t a2 = (a0 + a4 + 1) >> 1;
    int16_t a6 = (a4 + a8 + 1) >> 1;
    int16_t a1 = (a0 + a2 + 1) >> 1;
    int16_t a3 = (a2 + a4 + 1) >> 1;
    int16_t a5 = (a4 + a6 + 1) >> 1;
    int16_t a7 = (a6 + a8 + 1) >> 1;
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

int Downsampled::BlockDataSizePerChannel(int rows, int columns) {
  const int macro_rows = NumMacroBlocks(rows);
  const int macro_columns = NumMacroBlocks(columns);
  return macro_rows * macro_columns + rows * columns;
}

void Downsampled::GetBlockData(uint8_t *out, const Mapper &mapper) const {
  const int macro_rows = NumMacroBlocks(m_rows);
  const int macro_columns = NumMacroBlocks(m_columns);

  // Determine the best predictor for each macro block.
  uint8_t *predictor_selection = out;
  {
    int predictor_error[kNumPredictors];
    for (int mv = 0; mv < macro_rows; ++mv) {
      int v0 = mv * kMacroBlockSize;
      for (int mu = 0; mu < macro_columns; ++mu) {
        int u0 = mu * kMacroBlockSize;

        // Clear the prediction error vector.
        for (int i = 0; i < kNumPredictors; ++i)
          predictor_error[i] = 0;

        // Iterate over all the pixels of this macro block.
        for (int dv = 0; dv < kMacroBlockSize; ++dv) {
          int v = v0 + dv;
          if (v >= m_rows)
            break;

          for (int du = 0; du < kMacroBlockSize; ++du) {
            int u = u0 + du;
            if (u >= m_columns)
              break;

            // Extract the three neighbour samples that we use for
            // prediction.
            int16_t s1, s2, s3;
            if (du > 0 && dv > 0) {
              s1 =
                  static_cast<int16_t>(m_data[(v - 1) * m_columns + u - 1]);
              s2 = static_cast<int16_t>(m_data[(v - 1) * m_columns + u]);
              s3 = static_cast<int16_t>(m_data[v * m_columns + u - 1]);
            } else if (du > 0) {
              s1 = s2 = s3 =
                  static_cast<int16_t>(m_data[v * m_columns + u - 1]);
            } else if (dv > 0) {
              s1 = s2 = s3 =
                  static_cast<int16_t>(m_data[(v - 1) * m_columns + u]);
            } else {
              s1 = s2 = s3 = 128;
            }

            // Try all the predictors.
            for (int predictor = 0; predictor < kNumPredictors;
                 ++predictor) {
              // Calculate the prediction error for this predictor.
              int16_t predicted = PredictSample(s1, s2, s3, predictor);
              int16_t actual =
                  static_cast<int16_t>(m_data[v * m_columns + u]);
              int delta = static_cast<int>(actual - predicted);
              int err = delta * delta;

              // Accumulate the prediction error for this predictor.
              predictor_error[predictor] += err;
            }
          }
        }

        // Select the best predictor for this macro block.
        int best_predictor = 0;
        int best_error = predictor_error[0];
        for (int predictor = 1; predictor < kNumPredictors; ++predictor) {
          if (predictor_error[predictor] < best_error) {
            best_predictor = predictor;
            best_error = predictor_error[predictor];
          }
        }

        // Output the predictor.
        *out++ = EncodePredictor(best_predictor);
      }
    }
  }

  // We use a temporary working buffer for the two most recent lines in the
  // macro block.
  uint8_t work_buf[kMacroBlockSize * 2];
  uint8_t *lines[2] = {&work_buf[0], &work_buf[kMacroBlockSize]};

  // Iterate over all macro blocks.
  for (int mv = 0; mv < macro_rows; ++mv) {
    int v0 = mv * kMacroBlockSize;
    for (int mu = 0; mu < macro_columns; ++mu) {
      int u0 = mu * kMacroBlockSize;

      // Get the predictor for this macro block.
      int predictor =
          DecodePredictor(predictor_selection[mv * macro_columns + mu]);

      // Iterate over all the pixels of this macro block.
      for (int dv = 0; dv < kMacroBlockSize; ++dv) {
        int v = v0 + dv;
        if (v >= m_rows)
          break;

        for (int du = 0; du < kMacroBlockSize; ++du) {
          int u = u0 + du;
          if (u >= m_columns)
            break;

          // Extract the three neighbour samples that we use for prediction.
          int16_t s1, s2, s3;
          if (du > 0 && dv > 0) {
            s1 = static_cast<int16_t>(lines[0][du - 1]);
            s2 = static_cast<int16_t>(lines[0][du]);
            s3 = static_cast<int16_t>(lines[1][du - 1]);
          } else if (du > 0) {
            s1 = s2 = s3 = static_cast<int16_t>(lines[1][du - 1]);
          } else if (dv > 0) {
            s1 = s2 = s3 = static_cast<int16_t>(lines[0][du]);
          } else {
            s1 = s2 = s3 = 128;
          }

          // Predict the current sample.
          int16_t predicted = PredictSample(s1, s2, s3, predictor);

          // Calculate the delta to the prediction.
          int16_t actual = static_cast<int16_t>(m_data[v * m_columns + u]);
          int16_t delta = actual - predicted;
          uint8_t delta8 = mapper.MapTo8Bit(delta);

          // Compensate actual value for quantization (i.e. mimic the decoder).
          actual = predicted + mapper.UnmapFrom8Bit(delta8);
          actual = std::max(int16_t(0), std::min(actual, int16_t(255)));
          lines[1][du] = static_cast<uint8_t>(actual);

          // Output the quantized delta value.
          *out++ = delta8;
        }

        std::swap(lines[0], lines[1]);
      }
    }
  }
}

void Downsampled::SetBlockData(
    const uint8_t *in, int rows, int columns, const Mapper &mapper) {
  const int macro_rows = NumMacroBlocks(rows);
  const int macro_columns = NumMacroBlocks(columns);

  m_rows = rows;
  m_columns = columns;
  m_data.resize(m_rows * m_columns);

  // The per macro-block predictor selection comes first (one byte per macro
  // block).
  const uint8_t *predictor_selection = in;
  in += macro_rows * macro_columns;

  // Reconstruct samples (integrate deltas) for all macro blocks.
  for (int mv = 0; mv < macro_rows; ++mv) {
    int v0 = mv * kMacroBlockSize;
    for (int mu = 0; mu < macro_columns; ++mu) {
      int u0 = mu * kMacroBlockSize;

      // Get the predictor for this macro block.
      int predictor =
          DecodePredictor(predictor_selection[mv * macro_columns + mu]);

      // Iterate over all the pixels of this macro block.
      for (int dv = 0; dv < kMacroBlockSize; ++dv) {
        int v = v0 + dv;
        if (v >= m_rows)
          break;

        for (int du = 0; du < kMacroBlockSize; ++du) {
          int u = u0 + du;
          if (u >= m_columns)
            break;

          // Extract the three neighbour samples that we use for prediction.
          int16_t s1, s2, s3;
          if (du > 0 && dv > 0) {
            s1 = static_cast<int16_t>(m_data[(v - 1) * m_columns + u - 1]);
            s2 = static_cast<int16_t>(m_data[(v - 1) * m_columns + u]);
            s3 = static_cast<int16_t>(m_data[v * m_columns + u - 1]);
          } else if (du > 0) {
            s1 = s2 = s3 = static_cast<int16_t>(m_data[v * m_columns + u - 1]);
          } else if (dv > 0) {
            s1 = s2 = s3 =
                static_cast<int16_t>(m_data[(v - 1) * m_columns + u]);
          } else {
            s1 = s2 = s3 = 128;
          }

          // Predict the current sample.
          int16_t predicted = PredictSample(s1, s2, s3, predictor);

          // Restore actual value.
          int16_t delta = mapper.UnmapFrom8Bit(*in++);
          int16_t actual = predicted + delta;
          actual = std::max(int16_t(0), std::min(actual, int16_t(255)));

          // Output the restored sample value.
          m_data[v * m_columns + u] = static_cast<uint8_t>(actual);
        }
      }
    }
  }
}

}  // namespace himg
