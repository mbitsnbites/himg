//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "hadamard.h"

namespace himg {

namespace {

// Fast forward Hadamard transform, optionally in place.
template <int STRIDE>
void Forward8(int16_t *out, const int16_t *in) {
  int16_t a0 = in[0 * STRIDE] + in[4 * STRIDE];
  int16_t a1 = in[1 * STRIDE] + in[5 * STRIDE];
  int16_t a2 = in[2 * STRIDE] + in[6 * STRIDE];
  int16_t a3 = in[3 * STRIDE] + in[7 * STRIDE];
  int16_t a4 = in[0 * STRIDE] - in[4 * STRIDE];
  int16_t a5 = in[1 * STRIDE] - in[5 * STRIDE];
  int16_t a6 = in[2 * STRIDE] - in[6 * STRIDE];
  int16_t a7 = in[3 * STRIDE] - in[7 * STRIDE];
  int16_t b0 = a0 + a2;
  int16_t b1 = a1 + a3;
  int16_t b2 = a0 - a2;
  int16_t b3 = a1 - a3;
  int16_t b4 = a4 + a6;
  int16_t b5 = a5 + a7;
  int16_t b6 = a4 - a6;
  int16_t b7 = a5 - a7;
  out[0 * STRIDE] = b0 + b1;
  out[1 * STRIDE] = b4 + b5;
  out[2 * STRIDE] = b6 + b7;
  out[3 * STRIDE] = b2 + b3;
  out[4 * STRIDE] = b2 - b3;
  out[5 * STRIDE] = b6 - b7;
  out[6 * STRIDE] = b4 - b5;
  out[7 * STRIDE] = b0 - b1;
}

// Fast inverse Hadamard transform, optionally in place.
template <int STRIDE, int SHIFT>
void Inverse8(int16_t *out, const int16_t *in) {
  // TODO(m): Investigate if we can to do this with 16-bit precision instead.
  int32_t a0 = in[0 * STRIDE] + in[4 * STRIDE];
  int32_t a1 = in[1 * STRIDE] + in[5 * STRIDE];
  int32_t a2 = in[2 * STRIDE] + in[6 * STRIDE];
  int32_t a3 = in[3 * STRIDE] + in[7 * STRIDE];
  int32_t a4 = in[0 * STRIDE] - in[4 * STRIDE];
  int32_t a5 = in[1 * STRIDE] - in[5 * STRIDE];
  int32_t a6 = in[2 * STRIDE] - in[6 * STRIDE];
  int32_t a7 = in[3 * STRIDE] - in[7 * STRIDE];
  int32_t b0 = a0 + a2;
  int32_t b1 = a1 + a3;
  int32_t b2 = a0 - a2;
  int32_t b3 = a1 - a3;
  int32_t b4 = a4 + a6;
  int32_t b5 = a5 + a7;
  int32_t b6 = a4 - a6;
  int32_t b7 = a5 - a7;
  out[0 * STRIDE] = static_cast<int16_t>((b0 + b1) >> SHIFT);
  out[1 * STRIDE] = static_cast<int16_t>((b4 + b5) >> SHIFT);
  out[2 * STRIDE] = static_cast<int16_t>((b6 + b7) >> SHIFT);
  out[3 * STRIDE] = static_cast<int16_t>((b2 + b3) >> SHIFT);
  out[4 * STRIDE] = static_cast<int16_t>((b2 - b3) >> SHIFT);
  out[5 * STRIDE] = static_cast<int16_t>((b6 - b7) >> SHIFT);
  out[6 * STRIDE] = static_cast<int16_t>((b4 - b5) >> SHIFT);
  out[7 * STRIDE] = static_cast<int16_t>((b0 - b1) >> SHIFT);
}

}  // namespace

void Hadamard::Forward(int16_t *out, const int16_t *in) {
  // Rows.
  for (int i = 0; i < 8; ++i) {
    Forward8<1>(&out[i * 8], &in[i * 8]);
  }

  // Columns.
  for (int i = 0; i < 8; ++i) {
    Forward8<8>(&out[i], &out[i]);
  }
}

void Hadamard::Inverse(int16_t *out, const int16_t *in) {
  // Rows.
  for (int i = 0; i < 8; ++i) {
    Inverse8<1, 3>(&out[i * 8], &in[i * 8]);
  }

  // Columns.
  for (int i = 0; i < 8; ++i) {
    Inverse8<8, 3>(&out[i], &out[i]);
  }
}

}  // namespace himg
