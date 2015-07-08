//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "mapper.h"

#include <algorithm>
#include <utility>

namespace himg {

namespace {

// This is a hand-tuned mapping table that seems to give good results.
const int16_t kLowResMappingTable[128] = {
  0, 1, 2, 3, 4, 5, 6, 7,
  8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47,
  48, 49, 50, 51, 52, 53, 54, 55,
  56, 57, 58, 59, 60, 62, 63, 64,
  65, 67, 68, 70, 71, 73, 74, 76,
  78, 79, 81, 83, 85, 87, 89, 91,
  93, 95, 97, 99, 102, 104, 106, 109,
  111, 114, 117, 119, 122, 125, 128, 131,
  134, 137, 140, 143, 146, 150, 153, 156,
  160, 164, 167, 171, 175, 178, 182, 186,
  190, 195, 199, 203, 207, 212, 216, 221,
  226, 230, 235, 240, 245, 250, 255, 999 // FIXME!
};

const std::pair<int, int> kLowResMapScaleTable[] = {
  {   0, 120 },
  {   5,  90 },
  {  10,  70 },
  {  20,  40 },
  {  30,  32 },
  {  40,  26 },
  {  50,  20 },
  { 100,  16 }
};

// Given a quality value in the range [0, 100], return an interpolated scaling
// factor.
int QualityToScale(int quality,
                   const std::pair<int, int> *table,
                   int table_size) {
  // Look up the quality level in the quality -> scaling factor LUT.
  int idx;
  for (idx = 0; idx < table_size - 1; ++idx) {
    if (table[idx + 1].first > quality)
      break;
  }
  if (idx >= table_size - 1)
    return table[table_size - 1].second;

  // Pick out the two closest table entries.
  int q1 = table[idx].first;
  int s1 = table[idx].second;
  int q2 = table[idx + 1].first;
  int s2 = table[idx + 1].second;

  // Perform linear interpolation between the two table entries.
  int q = quality;
  int denom = q2 - q1;
  return s1 + ((s2 - s1) * (q - q1) + (denom >> 1)) / denom;
}

}  // namespace

void LowResMapper::InitForQuality(int quality) {
  // Determine ramp factor based on the quality setting. The ramp factor is in
  // 1/16ths.
  int16_t index_scale = QualityToScale(quality,
                                       kLowResMapScaleTable,
                                       sizeof(kLowResMapScaleTable) /
                                          sizeof(kLowResMapScaleTable[0]));

  // Generate the mapping table based on the index scale.
  for (int16_t i = 0; i < 128; ++i) {
    int16_t index = (i * index_scale + 8) >> 4;
    index = std::min(index, int16_t(127));
    m_mapping_table[i] = kLowResMappingTable[index];
  }
}

int Mapper::MappingFunctionSize() const {
  // The mapping table requires one byte that tells how many items can be
  // represented with a single byte, plus the actual single- and double-byte
  // items.
  int single_byte_items = NumberOfSingleByteMappingItems();
  return 1 + single_byte_items + 2 * (128 - single_byte_items);
}

void Mapper::GetMappingFunction(uint8_t *out) const {
  // Store the mapping table.
  int single_byte_items = NumberOfSingleByteMappingItems();
  *out++ = static_cast<uint8_t>(single_byte_items);
  int i;
  for (i = 0; i < single_byte_items; ++i)
    *out++ = static_cast<uint8_t>(m_mapping_table[i]);
  for (; i < 128; ++i) {
    uint16_t x = m_mapping_table[i];
    *out++ = static_cast<uint8_t>(x & 255);
    *out++ = static_cast<uint8_t>(x >> 8);
  }
}

bool Mapper::SetMappingFunction(const uint8_t *in, int map_fun_size) {
  if (map_fun_size < 1)
    return false;

  // Restore the mapping table.
  int single_byte_items = static_cast<int>(*in++);
  int actual_size = 1 + single_byte_items + 2 * (128 - single_byte_items);
  if (actual_size != map_fun_size)
    return false;
  int i;
  for (i = 0; i < single_byte_items; ++i)
    m_mapping_table[i] = static_cast<uint16_t>(*in++);
  for (; i < 128; ++i) {
    m_mapping_table[i] =
        static_cast<uint16_t>(in[0]) | (static_cast<uint16_t>(in[1]) << 8);
    in += 2;
  }

  return true;
}

uint8_t Mapper::MapTo8Bit(int16_t x) const {
  if (!x)
    return 0;

  uint16_t abs_x = static_cast<uint16_t>(std::abs(x));

  // TODO(m): Use binary search instead of O(n) search.
  uint8_t mapped;
  for (mapped = 1; mapped < 127; ++mapped) {
    if (abs_x < m_mapping_table[mapped]) {
      if ((abs_x - m_mapping_table[mapped - 1]) <
          (m_mapping_table[mapped] - abs_x)) {
        --mapped;
      }
      break;
    }
  }

  return x >= 0 ? mapped : static_cast<uint8_t>(-static_cast<int8_t>(mapped));
}

int16_t Mapper::UnmapFrom8Bit(uint8_t x) const {
  if (!x)
    return 0;

  bool negative = (x & 128) != 0;
  if (negative) {
    x = static_cast<uint8_t>(-static_cast<int8_t>(x));
  }

  int16_t value = static_cast<int16_t>(m_mapping_table[x]);
  return negative ? -value : value;
}

int Mapper::NumberOfSingleByteMappingItems() const {
  int single_byte_items;
  for (single_byte_items = 0; single_byte_items < 128; ++single_byte_items) {
    if (m_mapping_table[single_byte_items] >= 256)
      break;
  }
  return single_byte_items;
}

}  // namespace himg
