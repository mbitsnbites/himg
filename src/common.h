//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef COMMON_H_
#define COMMON_H_

#include <cstdint>

// Branch optimization macros.
#if defined(__GNUC__)
# define LIKELY(expr) __builtin_expect(!!(expr), 1)
# define UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#else
# define LIKELY(expr) (expr)
# define UNLIKELY(expr) (expr)
#endif

namespace himg {

// Indexing of an 8x8 block.
extern const uint8_t kIndexLUT[64];

// Clamp a 16-bit value to an 8-bit unsigned value.
inline uint8_t ClampTo8Bit(int16_t x) {
  return x >= 0 ? (x <= 255 ? static_cast<uint8_t>(x) : 255) : 0;
}

}  // namespace himg

#endif  // COMMON_H_
