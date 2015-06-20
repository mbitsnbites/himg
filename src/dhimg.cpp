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

#include "decoder.h"

int main(int argc, const char **argv) {
  if (argc < 3) {
    std::cout << "Usage: " << argv[0] << " image outfile" << std::endl;
    return 0;
  }

  // Load the packed data from a file.
  std::vector<uint8_t> packed_data;
  {
    // Open file.
    std::ifstream f(argv[1], std::ifstream::in | std::ofstream::binary);
    if (!f.good()) {
      std::cout << "Unable to read file " << argv[1] << std::endl;
      return -1;
    }

    // Get file size.
    f.seekg(0, std::ifstream::end);
    int file_size = static_cast<int>(f.tellg());
    f.seekg(0, std::ifstream::beg);
    std::cout << "File size: " << file_size << std::endl;

    // Read the file data into our buffer.
    packed_data.resize(file_size);
    f.read(reinterpret_cast<char *>(packed_data.data()), file_size);
  }

  // Decode the image.
  himg::Decoder decoder;
  if (!decoder.Decode(packed_data.data(), packed_data.size())) {
    std::cout << "Unable to decode image." << std::endl;
    return -1;
  }

  // Write the decoded image to a file using FreeImage.
  {
    FreeImage_Initialise();

    FIBITMAP *bitmap = FreeImage_ConvertFromRawBits(
        const_cast<BYTE *>(decoder.unpacked_data()),
        decoder.width(),
        decoder.height(),
        decoder.num_channels() * decoder.width(),
        decoder.num_channels() * 8,
        0xff0000,
        0x00ff00,
        0x0000ff,
        false);
    FreeImage_Save(FIF_PNG, bitmap, argv[2]);
    FreeImage_Unload(bitmap);

    FreeImage_DeInitialise();
  }

  return 0;
}
