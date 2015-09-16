//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "huffman_dec.h"

#include <algorithm>

#include "common.h"
#include "huffman_common.h"

namespace himg {

namespace {

class InBitstream {
 public:
  // Initialize a bitstream.
  InBitstream(const uint8_t *buf, int size)
      : m_byte_ptr(buf),
        m_bit_pos(0),
        m_end_ptr(buf + size),
        m_read_failed(false) {}

  // Read one bit from a bitstream.
  int ReadBit() {
    // Get current stream state.
    const uint8_t *buf = m_byte_ptr;
    int bit = m_bit_pos;

    // Extract bit.
    int x = (*buf >> bit) & 1;
    bit = (bit + 1) & 7;
    if (!bit) {
      ++buf;
    }

    // Store new stream state.
    m_bit_pos = bit;
    m_byte_ptr = buf;

    return x;
  }

  // Read one bit from a bitstream, with checking.
  int ReadBitChecked() {
    // Check that we don't read past the end.
    if (UNLIKELY(m_byte_ptr >= m_end_ptr)) {
      m_read_failed = true;
      return 0;
    }

    // Ok, read...
    return ReadBit();
  }

  // Read bits from a bitstream.
  uint32_t ReadBits(int bits) {
    uint32_t x = 0;

    // Get current stream state.
    const uint8_t *buf = m_byte_ptr;
    int bit = m_bit_pos;

    // Extract bits.
    // TODO(m): Optimize this!
    int shift = 0;
    while (bits) {
      int bits_to_extract = std::min(bits, 8 - bit);
      bits -= bits_to_extract;

      uint8_t mask = 0xff >> (8 - bits_to_extract);
      x = x | (static_cast<uint32_t>((*buf >> bit) & mask) << shift);
      shift += bits_to_extract;

      bit += bits_to_extract;
      if (bit >= 8) {
        bit -= 8;
        ++buf;
      }
    }

    // Store new stream state.
    m_bit_pos = bit;
    m_byte_ptr = buf;

    return x;
  }

  // Read bits from a bitstream, with checking.
  uint32_t ReadBitsChecked(int bits) {
    // Check that we don't read past the end.
    int new_bit_pos = m_bit_pos + bits;
    const uint8_t *new_byte_ptr = m_byte_ptr + (new_bit_pos >> 3);
    if (UNLIKELY(new_byte_ptr > m_end_ptr ||
                 (new_byte_ptr == m_end_ptr && ((new_bit_pos & 7) > 0)))) {
      m_read_failed = true;
      return 0;
    }

    // Ok, read...
    return ReadBits(bits);
  }

  // Peek eight bits from a bitstream (read without advancing the pointer).
  uint32_t Peek8Bits() {
    uint32_t lo = *m_byte_ptr, hi = m_byte_ptr[1];
    return (((hi << 8) | lo) >> m_bit_pos) & 255;
  }

  // Advance the pointer by N bits.
  void Advance(int bits) {
    int new_bit_pos = m_bit_pos + bits;
    m_bit_pos = new_bit_pos & 7;
    m_byte_ptr += new_bit_pos >> 3;
  }

  bool AtTheEnd() const {
    // This is a rought estimate that we have reached the end of the input
    // buffer (not too short, and not too far).
    return (m_byte_ptr == m_end_ptr && m_bit_pos == 0) ||
           (m_byte_ptr == (m_end_ptr - 1) && m_bit_pos > 0);
  }

  bool read_failed() const {
    return m_read_failed;
  }

 private:
  const uint8_t *m_byte_ptr;
  int m_bit_pos;
  const uint8_t *m_end_ptr;
  bool m_read_failed;
};

// Used by the encoder for building the optimal Huffman tree.
struct SymbolInfo {
  Symbol symbol;
  int count;
  uint32_t code;
  int bits;
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

// Recover a Huffman tree from a bitstream.
DecodeNode *RecoverTree(DecodeNode *nodes,
                        InBitstream *stream,
                        int *nodenum,
                        DecodeLutEntry *lut,
                        uint32_t code,
                        int bits) {
  // Pick a node from the node array.
  DecodeNode *this_node = &nodes[*nodenum];
  *nodenum = *nodenum + 1;

  // Clear the node.
  this_node->symbol = -1;
  this_node->child_a = nullptr;
  this_node->child_b = nullptr;

  // Is this a leaf node?
  bool is_leaf = stream->ReadBitChecked() != 0;
  if (UNLIKELY(stream->read_failed()))
    return nullptr;
  if (is_leaf) {
    // Get symbol from tree description and store in lead node.
    int symbol = static_cast<int>(stream->ReadBitsChecked(kSymbolSize));
    if (UNLIKELY(stream->read_failed()))
      return nullptr;

    this_node->symbol = symbol;

    if (bits <= 8) {
      // Fill out the LUT for this symbol, including all permutations of the
      // upper bits.
      uint32_t dups = 256 >> bits;
      for (uint32_t i = 0; i < dups; ++i) {
        DecodeLutEntry *lut_entry = &lut[(i << bits) | code];
        lut_entry->node = nullptr;
        lut_entry->bits = bits;
        lut_entry->symbol = symbol;
      }
    }

    return this_node;
  }

  if (bits == 8) {
    // Add a non-terminated entry in the LUT (i.e. one that points into the tree
    // rather than giving a symbol).
    DecodeLutEntry *lut_entry = &lut[code];
    lut_entry->node = this_node;
    lut_entry->bits = 8;
    lut_entry->symbol = 0;
  }

  // Get branch A.
  this_node->child_a = RecoverTree(nodes, stream, nodenum, lut, code, bits + 1);
  if (UNLIKELY(!this_node->child_a))
    return nullptr;

  // Get branch B.
  this_node->child_b =
      RecoverTree(nodes, stream, nodenum, lut, code + (1 << bits), bits + 1);
  if (UNLIKELY(!this_node->child_b))
    return nullptr;

  return this_node;
}

}  // namespace

bool HuffmanDec::Uncompress(uint8_t *out,
                            const uint8_t *in,
                            int in_size,
                            int out_size) {
  // Do we have anything to decompress?
  if (in_size < 1)
    return out_size == 0;

  // Initialize bitstream.
  InBitstream stream(in, in_size);

  // Recover Huffman tree.
  int node_count = 0;
  DecodeNode nodes[kMaxTreeNodes];
  DecodeLutEntry decode_lut[256];
  DecodeNode *root = RecoverTree(nodes, &stream, &node_count, decode_lut, 0, 0);
  if (UNLIKELY(!root))
    return false;

  // Decode input stream.
  uint8_t *buf = out;
  const uint8_t *buf_end = out + out_size;

  // We do the majority of the decoding in a fast, unchecked loop.
  // Note: The longest supported code + RLE encoding is 32 + 14 bits ~= 6 bytes.
  const uint8_t *buf_fast_end = buf_end - 6;
  while (buf < buf_fast_end) {
    int symbol;

    // Peek 8 bits from the stream and use it to look up a potential symbol in
    // the LUT (codes that are eight bits or shorter are very common, so we have
    // a high hit rate in the LUT).
    const auto &lut_entry = decode_lut[stream.Peek8Bits()];
    stream.Advance(lut_entry.bits);
    if (LIKELY(lut_entry.node == nullptr)) {
      // Fast case: We found the symbol in the LUT.
      symbol = lut_entry.symbol;
    } else {
      // Slow case: Traverse the tree from 8 bits code length until we find a
      // leaf node.
      DecodeNode *node = lut_entry.node;
      while (node->symbol < 0) {
        // Get next node.
        if (stream.ReadBit())
          node = node->child_b;
        else
          node = node->child_a;
      }
      symbol = node->symbol;
    }

    // Decode as RLE or plain copy.
    if (LIKELY(symbol <= 255)) {
      // Plain copy.
      *buf++ = static_cast<uint8_t>(symbol);
    } else {
      // Symbols >= 256 are RLE tokens.
      int zero_count;
      switch (symbol) {
        case kSymTwoZeros: {
          zero_count = 2;
          break;
        }
        case kSymUpTo6Zeros: {
          zero_count = static_cast<int>(stream.ReadBits(2)) + 3;
          break;
        }
        case kSymUpTo22Zeros: {
          zero_count = static_cast<int>(stream.ReadBits(4)) + 7;
          break;
        }
        case kSymUpTo278Zeros: {
          zero_count = static_cast<int>(stream.ReadBits(8)) + 23;
          break;
        }
        case kSymUpTo16662Zeros: {
          zero_count = static_cast<int>(stream.ReadBits(14)) + 279;
          break;
        }
        default: {
          // Note: This should never happen -> abort!
          return false;
        }
      }

      if (UNLIKELY(buf + zero_count > buf_end))
        return false;
      std::fill(buf, buf + zero_count, 0);
      buf += zero_count;
    }
  }

  // ...and we do the tail of the decoding in a slower, checked loop.
  while (buf < buf_end) {
    // Traverse the tree until we find a leaf node.
    DecodeNode *node = root;
    while (node->symbol < 0) {
      // Get next node.
      if (stream.ReadBitChecked())
        node = node->child_b;
      else
        node = node->child_a;

      if (UNLIKELY(stream.read_failed()))
        return false;
    }
    int symbol = node->symbol;

    // Decode as RLE or plain copy.
    if (LIKELY(symbol <= 255)) {
      // Plain copy.
      *buf++ = static_cast<uint8_t>(symbol);
    } else {
      // Symbols >= 256 are RLE tokens.
      int zero_count;
      switch (symbol) {
        case kSymTwoZeros: {
          zero_count = 2;
          break;
        }
        case kSymUpTo6Zeros: {
          zero_count = static_cast<int>(stream.ReadBitsChecked(2)) + 3;
          break;
        }
        case kSymUpTo22Zeros: {
          zero_count = static_cast<int>(stream.ReadBitsChecked(4)) + 7;
          break;
        }
        case kSymUpTo278Zeros: {
          zero_count = static_cast<int>(stream.ReadBitsChecked(8)) + 23;
          break;
        }
        case kSymUpTo16662Zeros: {
          zero_count = static_cast<int>(stream.ReadBitsChecked(14)) + 279;
          break;
        }
        default: {
          // Note: This should never happen -> abort!
          return false;
        }
      }

      if (UNLIKELY(stream.read_failed() || buf + zero_count > buf_end))
        return false;
      std::fill(buf, buf + zero_count, 0);
      buf += zero_count;
    }
  }

  return stream.AtTheEnd();
}

}  // namespace himg
