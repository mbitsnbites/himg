//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef DOWNSAMPLED_H_
#define DOWNSAMPLED_H_

#include <cstdint>
#include <vector>

#include "mapper.h"

namespace himg {

class Downsampled {
 public:
  Downsampled();

  void SampleImage(const uint8_t *pixels, int stride, int width, int height);

  void GetLowresBlock(int16_t *out, int u, int v) const;

  static int BlockDataSizePerChannel(int rows, int columns);

  void GetBlockData(uint8_t *out, const Mapper &mapper) const;
  void SetBlockData(
      const uint8_t *in, int rows, int columns, const Mapper &mapper);

  int rows() const { return m_rows; }

  int columns() const { return m_columns; }

  int Size() const { return m_rows * m_columns; }

 private:
  int m_rows;
  int m_columns;
  std::vector<uint8_t> m_data;
};

}  // namespace himg

#endif  // DOWNSAMPLED_H_
