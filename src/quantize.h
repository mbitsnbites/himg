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
  // Initialize quantization data for a given quality level (0-9).
  void InitForQuality(uint8_t quality);

  // Pack to clamped signed magnitude based on the shift table.
  void Pack(uint8_t *out, const int16_t *in, bool chroma_channel);

  // Unpack to 16-bit twos complement based on the shift table.
  void Unpack(int16_t *out, const uint8_t *in, bool chroma_channel);

  // Get the required size of the quantization configuration (in bytes).
  int ConfigurationSize() const;

  // Get the quantization configuration.
  void GetConfiguration(uint8_t *out);

  // Set the quantization configuration.
  bool SetConfiguration(const uint8_t *in, int config_size);

 private:
  int NumberOfSingleByteDelinearizationItems() const;
  uint8_t ToSignedMagnitude(int16_t abs_x, bool negative);
  int16_t FromSignedMagnitude(uint8_t x);

  uint8_t m_shift_table[64];
  uint8_t m_chroma_shift_table[64];
  uint16_t m_delinearization_table[128];
};

}  // namespace himg

#endif  // QUANTIZE_H_
