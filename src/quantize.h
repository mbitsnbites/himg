//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef QUANTIZE_H_
#define QUANTIZE_H_

#include <cstdint>

namespace himg {

class Quantize {
 public:
  // Initialize quantization data for a given quality level (0-100).
  void InitForQuality(uint8_t quality, bool has_chroma);

  // Pack to clamped signed magnitude based on the shift table.
  void Pack(uint8_t *out, const int16_t *in, bool chroma_channel);

  // Unpack to 16-bit twos complement based on the shift table.
  void Unpack(int16_t *out, const uint8_t *in, bool chroma_channel);

  // Get the required size for the quantization configuration (in bytes).
  int ConfigurationSize() const;

  // Get the quantization configuration.
  void GetConfiguration(uint8_t *out) const;

  // Set the quantization configuration.
  bool SetConfiguration(const uint8_t *in, int config_size, bool has_chroma);

  // Get the required size for the mapping function (in bytes).
  int MappingFunctionSize() const;

  // Get the mapping function.
  void GetMappingFunction(uint8_t *out) const;

  // Set the mapping function.
  bool SetMappingFunction(const uint8_t *in, int map_fun_size);

 private:
  int NumberOfSingleByteMappingItems() const;
  uint8_t MapTo8Bit(int16_t abs_x, bool negative) const;
  int16_t UnmapFrom8Bit(uint8_t x) const;

  bool m_has_chroma;
  uint8_t m_shift_table[64];
  uint8_t m_chroma_shift_table[64];
  uint16_t m_mapping_table[128];
};

}  // namespace himg

#endif  // QUANTIZE_H_
