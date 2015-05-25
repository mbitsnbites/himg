//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include <cstdint>
#include <fstream>
#include <iostream>

#include <FreeImage.h>

#include "encoder.h"

namespace {

const int kDefaultQuality = 3;
const bool kUseYCbCr = true;
const int kNumChannels = 3;

}  // namespace

int main(int argc, const char **argv) {
  if (argc < 3) {
    std::cout << "Usage: " << argv[0] << " image outfile" << std::endl;
    return 0;
  }
  const char *input_file = argv[1];
  const char *output_file = argv[2];

  FreeImage_Initialise();

  // Load the source image using FreeImage.
  FIBITMAP *bitmap;
  {
    FREE_IMAGE_FORMAT format = FreeImage_GetFileType(input_file);
    if (format == FIF_UNKNOWN) {
      std::cerr << "Unknown file format for " << input_file << std::endl;
      return -1;
    }
    FIBITMAP *bitmap_tmp = FreeImage_Load(format, input_file);
    if (!bitmap_tmp) {
      std::cerr << "Unable to load " << input_file << std::endl;
      return -1;
    }

    // Convert the image to 32-bit RGBA.
    bitmap = FreeImage_ConvertTo32Bits(bitmap_tmp);
    FreeImage_Unload(bitmap_tmp);
  }

  // Encode the image.
  himg::Encoder encoder;
  {
    int width = FreeImage_GetWidth(bitmap);
    int height = FreeImage_GetHeight(bitmap);
    int pixel_stride = 4;
    uint8_t *data = reinterpret_cast<uint8_t *>(FreeImage_GetBits(bitmap));
    encoder.Encode(data,
                   width,
                   height,
                   pixel_stride,
                   kNumChannels,
                   kDefaultQuality,
                   kUseYCbCr);
  }
  std::cout << "Compressed size: " << encoder.packed_size() << std::endl;

  // We're done with the FreeImage bitmap.
  FreeImage_Unload(bitmap);

  // Write packed data to a file.
  {
    std::ofstream f(output_file, std::ofstream::out | std::ofstream::binary);
    f.write(reinterpret_cast<const char *>(encoder.packed_data()),
            encoder.packed_size());
  }

  FreeImage_DeInitialise();

  return 0;
}
