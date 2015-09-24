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

HuffmanDec::BitStream::BitStream(const uint8_t *buf, int size)
    : m_byte_ptr(buf),
      m_bit_pos(0),
      m_end_ptr(buf + size),
      m_read_failed(false) {
}

HuffmanDec::BitStream::BitStream(const BitStream &other)
    : m_byte_ptr(other.m_byte_ptr),
      m_bit_pos(other.m_bit_pos),
      m_end_ptr(other.m_end_ptr),
      m_read_failed(other.m_read_failed) {
}

int HuffmanDec::BitStream::ReadBit() {
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

int HuffmanDec::BitStream::ReadBitChecked() {
  // Check that we don't read past the end.
  if (UNLIKELY(m_byte_ptr >= m_end_ptr)) {
    m_read_failed = true;
    return 0;
  }

  // Ok, read...
  return ReadBit();
}

uint32_t HuffmanDec::BitStream::ReadBits(int bits) {
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

uint32_t HuffmanDec::BitStream::ReadBitsChecked(int bits) {
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

uint8_t HuffmanDec::BitStream::Peek8Bits() const {
  uint8_t lo = m_byte_ptr[0], hi = m_byte_ptr[1];
  return ((hi << 8) | lo) >> m_bit_pos;
}

uint16_t HuffmanDec::BitStream::Read16BitsAligned() {
  // TODO(m): Check that we don't read past the end.

  AlignToByte();
  uint16_t lo = m_byte_ptr[0], hi = m_byte_ptr[1];
  m_byte_ptr += 2;
  return (hi << 8) | lo;
}

void HuffmanDec::BitStream::AlignToByte() {
  if (LIKELY(m_bit_pos)) {
    m_bit_pos = 0;
    ++m_byte_ptr;
  }
}
void HuffmanDec::BitStream::Advance(int N) {
  int new_bit_pos = m_bit_pos + N;
  m_bit_pos = new_bit_pos & 7;
  m_byte_ptr += new_bit_pos >> 3;
}

void HuffmanDec::BitStream::AdvanceBytes(int N) {
  // TODO(m): Check that we don't read past the end.

  m_byte_ptr += N;
}

bool HuffmanDec::BitStream::AtTheEnd() const {
  // This is a rought estimate that we have reached the end of the input
  // buffer (not too short, and not too far).
  return (m_byte_ptr == m_end_ptr && m_bit_pos == 0) ||
         (m_byte_ptr == (m_end_ptr - 1) && m_bit_pos > 0);
}

bool HuffmanDec::BitStream::read_failed() const {
  return m_read_failed;
}

// Recover a Huffman tree from a bitstream.
HuffmanDec::DecodeNode *HuffmanDec::RecoverTree(int *nodenum,
                                                uint32_t code,
                                                int bits) {
  // Pick a node from the node array.
  DecodeNode *this_node = &m_nodes[*nodenum];
  *nodenum = *nodenum + 1;
  if (UNLIKELY(*nodenum) >= kMaxTreeNodes)
    return nullptr;

  // Clear the node.
  this_node->symbol = -1;
  this_node->child_a = nullptr;
  this_node->child_b = nullptr;

  // Is this a leaf node?
  bool is_leaf = m_stream.ReadBitChecked() != 0;
  if (UNLIKELY(m_stream.read_failed()))
    return nullptr;
  if (is_leaf) {
    // Get symbol from tree description and store in lead node.
    int symbol = static_cast<int>(m_stream.ReadBitsChecked(kSymbolSize));
    if (UNLIKELY(m_stream.read_failed()))
      return nullptr;

    this_node->symbol = symbol;

    if (bits <= 8) {
      // Fill out the LUT for this symbol, including all permutations of the
      // upper bits.
      uint32_t dups = 256 >> bits;
      for (uint32_t i = 0; i < dups; ++i) {
        DecodeLutEntry *lut_entry = &m_decode_lut[(i << bits) | code];
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
    DecodeLutEntry *lut_entry = &m_decode_lut[code];
    lut_entry->node = this_node;
    lut_entry->bits = 8;
    lut_entry->symbol = 0;
  }

  // Get branch A.
  this_node->child_a = RecoverTree(nodenum, code, bits + 1);
  if (UNLIKELY(!this_node->child_a))
    return nullptr;

  // Get branch B.
  this_node->child_b = RecoverTree(nodenum, code + (1 << bits), bits + 1);
  if (UNLIKELY(!this_node->child_b))
    return nullptr;

  return this_node;
}

HuffmanDec::HuffmanDec(const uint8_t *in, int in_size, int block_size)
    : m_stream(in, in_size), m_root(nullptr) {
  m_block_size = block_size > 0 ? block_size : in_size;
  m_use_blocks = m_block_size < in_size;
}

bool HuffmanDec::Init() {
  // Only allow Init() to run once.
  if (m_root)
    return false;

  // Recover Huffman tree.
  int node_count = 0;
  m_root = RecoverTree(&node_count, 0, 0);
  if (m_root == nullptr)
    return false;
  if (m_use_blocks)
    m_stream.AlignToByte();

  // Recover the individual blocks.
  if (m_use_blocks) {
    BitStream tmp_stream(m_stream);
    while (!tmp_stream.AtTheEnd()) {
      uint16_t packed_block_size = tmp_stream.Read16BitsAligned();
      // TODO(m): Check size of block stream.
      m_blocks.push_back(BitStream(tmp_stream.byte_ptr(), packed_block_size));
      tmp_stream.AdvanceBytes(packed_block_size);
    }
  }

  return true;
}

bool HuffmanDec::Uncompress(uint8_t *out, int out_size) const {
  // Has Init() been run successfully?
  if (!m_root || m_use_blocks)
    return false;

  return UncompressStream(out, out_size, m_stream);
}

bool HuffmanDec::UncompressBlock(uint8_t *out,
                                 int out_size,
                                 int block_no) const {
  // Has Init() been run successfully?
  if (!m_root || !m_use_blocks)
    return false;

  if (block_no < 0 || block_no > static_cast<int>(m_blocks.size()))
    return false;

  return UncompressStream(out, out_size, m_blocks[block_no]);
}

bool HuffmanDec::UncompressStream(
    uint8_t *out, int out_size, BitStream stream) const {
  // Do we have anything to decompress?
  if (m_stream.AtTheEnd())
    return out_size == 0;

  // Decode input stream.
  uint8_t *buf = out;
  const uint8_t *buf_end = out + out_size;

  // Performance hack: I really don't like this, but by putting the decode LUT
  // in the local stack frame, we're noticeably faster than when using the
  // in-object LUT (g++ 4.9).
  // TODO(m): Find a better solution.
  DecodeLutEntry decode_lut[256];
  std::copy(&m_decode_lut[0], &m_decode_lut[0] + 256, &decode_lut[0]);

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
    DecodeNode *node = m_root;
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
