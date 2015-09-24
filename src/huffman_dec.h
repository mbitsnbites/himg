//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#ifndef HUFFMAN_DEC_H_
#define HUFFMAN_DEC_H_

#include <cstdint>
#include <vector>

namespace himg {

class HuffmanDec {
 public:
  HuffmanDec(const uint8_t *in, int in_size, int block_size);

  // Decode the Huffman data preamble (the tree).
  bool Init();

  // Uncompress the Huffman stream (requires that Init() has been called first).
  bool Uncompress(uint8_t *out, int out_size) const;

  // Uncompress a single block in the Huffman stream (requires that Init() has
  // been called first).
  bool UncompressBlock(uint8_t *out, int out_size, int block_no) const;

 private:
  // The maximum number of tree nodes.
  static const int kMaxTreeNodes = (261 * 2) - 1;

  // A class to help decoding binary data.
  class BitStream {
   public:
    // Initialize a bitstream.
    BitStream(const uint8_t *buf, int size);

    // Copy constructor.
    BitStream(const BitStream &other);

    // Read one bit from a bitstream.
    int ReadBit();

    // Read one bit from a bitstream, with checking.
    int ReadBitChecked();

    // Read bits from a bitstream.
    uint32_t ReadBits(int bits);

    // Read bits from a bitstream, with checking.
    uint32_t ReadBitsChecked(int bits);

    // Peek eight bits from a bitstream (read without advancing the pointer).
    uint8_t Peek8Bits() const;

    // Read 16 bits from a bitstream, byte aligned.
    uint16_t Read16BitsAligned();

    // Align the stream to a byte boundary (do nothing if already aligned).
    void AlignToByte();

    // Advance the pointer by N bits.
    void Advance(int N);

    // Advance N bytes.
    void AdvanceBytes(int N);

    // Check if we have reached the end of the buffer.
    bool AtTheEnd() const;

    const uint8_t *byte_ptr() const {
      return m_byte_ptr;
    }

    // Check if any of the Read*Checked() methods failed.
    bool read_failed() const;

   private:
    const uint8_t *m_byte_ptr;
    int m_bit_pos;
    const uint8_t *m_end_ptr;
    bool m_read_failed;
  };

  struct DecodeNode {
    DecodeNode *child_a, *child_b;
    int symbol;
  };

  struct DecodeLutEntry {
    DecodeNode *node;
    int symbol;
    int bits;
  };

  DecodeNode *RecoverTree(int *nodenum, uint32_t code, int bits);

  bool UncompressStream(uint8_t *out, int out_size, BitStream stream) const;

  DecodeNode m_nodes[kMaxTreeNodes];
  DecodeLutEntry m_decode_lut[256];

  BitStream m_stream;
  DecodeNode *m_root;

  std::vector<BitStream> m_blocks;
  int m_block_size;
  bool m_use_blocks;
};

}  // namespace himg

#endif  // HUFFMAN_DEC_H_
