//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef MAPPER_H_
#define MAPPER_H_

#include <cstdint>

namespace himg {

class Mapper {
 public:
  Mapper();

  // Get the required size for the mapping function (in bytes).
  int MappingFunctionSize() const;

  // Get the mapping function.
  void GetMappingFunction(uint8_t *out) const;

  // Set the mapping function.
  bool SetMappingFunction(const uint8_t *in, int map_fun_size);

  // Map a 16-bit value to an 8-bit value.
  uint8_t MapTo8Bit(int16_t x) const;

  // Unmap an 8-bit value to a 16-bit.
  int16_t UnmapFrom8Bit(uint8_t x) const {
    return m_mapping_table[static_cast<int8_t>(x)];
  }

 protected:
  int NumberOfSingleByteMappingItems() const;

  int16_t *m_mapping_table;
  int16_t m_mapping_table_full[256];
};

class LowResMapper : public Mapper {
 public:
  // Generate a low-res image mapping table for the given quality.
  void InitForQuality(int quality);
};

class FullResMapper : public Mapper {
 public:
  // Generate a full-res image mapping table for the given quality.
  void InitForQuality(int quality);
};

}  // namespace himg

#endif  // MAPPER_H_
