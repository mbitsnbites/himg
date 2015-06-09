//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <FreeImage.h>

#include "encoder.h"

namespace {

const int kDefaultQuality = 90;  // 0 = min quality, 100 = max quality.
const int kNumChannels = 3;

bool ArgToInt(const char *arg, int *result) {
  std::string arg_str(arg);
  try {
    *result = std::stoi(arg_str);
    return true;
  } catch (...) {
    std::cout << "Invalid integer expression: " << arg << "\n";
    *result = 0;
  }
  return false;
}

struct Options {
  Options() {
    use_ycbcr = true;
    quality = kDefaultQuality;
    input_file = nullptr;
    output_file = nullptr;
  }

  bool Parse(int argc, const char **argv) {
    std::vector<const char *> file_names;

    bool success = true;
    for (int k = 1; k < argc && success; ++k) {
      const char *arg = argv[k];

      if (arg[0] == '-') {
        // Parse options (starting with '-').
        if (std::strcmp(arg, "-rgb") == 0) {
          use_ycbcr = false;
        } else if (std::strcmp(arg, "-q") == 0) {
          if (k + 1 < argc && ArgToInt(argv[++k], &quality)) {
            success = quality >= 0 && quality <= 100;
            if (!success)
              std::cout << "Invalid quality level: " << quality << "\n";
          } else {
            success = false;
          }
        } else {
          std::cout << "Invalid option: " << arg << "\n";
          success = false;
        }
      } else {
        // Non-options are file names.
        file_names.push_back(arg);
      }
    }

    if (!success || file_names.size() != 2) {
      std::cout << "Usage: " << argv[0] << " [options] image outfile\n";
      std::cout << "Options:\n";
      std::cout << " -q <quality> Set the quality (0-100)\n";
      std::cout << " -rgb         Use RGB color space (instead of YCbCr)\n";
      return false;
    }

    input_file = file_names[0];
    output_file = file_names[1];

    return true;
  }

  bool use_ycbcr;
  int quality;
  const char *input_file;
  const char *output_file;
};

}  // namespace

int main(int argc, const char **argv) {
  Options options;
  if (!options.Parse(argc, argv)) {
    return 0;
  }

  FreeImage_Initialise();

  // Load the source image using FreeImage.
  FIBITMAP *bitmap;
  {
    FREE_IMAGE_FORMAT format = FreeImage_GetFileType(options.input_file);
    if (format == FIF_UNKNOWN) {
      std::cerr << "Unknown file format for " << options.input_file
                << std::endl;
      return -1;
    }
    FIBITMAP *bitmap_tmp = FreeImage_Load(format, options.input_file);
    if (!bitmap_tmp) {
      std::cerr << "Unable to load " << options.input_file << std::endl;
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
                   options.quality,
                   options.use_ycbcr);
  }
  std::cout << "Compressed size: " << encoder.packed_size() << std::endl;

  // We're done with the FreeImage bitmap.
  FreeImage_Unload(bitmap);

  // Write packed data to a file.
  {
    std::ofstream f(options.output_file,
                    std::ofstream::out | std::ofstream::binary);
    f.write(reinterpret_cast<const char *>(encoder.packed_data()),
            encoder.packed_size());
  }

  FreeImage_DeInitialise();

  return 0;
}
