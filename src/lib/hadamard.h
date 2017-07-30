//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef HADAMARD_H_
#define HADAMARD_H_

#include <cstdint>

namespace himg {

class Hadamard {
 public:
  // Forward Hadamard transform (no scaling).
  static void Forward(int16_t *out, const int16_t *in);

  // Inverse Hadamard transform, including divide by 64.
  static void Inverse(int16_t *out, const int16_t *in);
};

}  // namespace himg

#endif  // HADAMARD_H_
