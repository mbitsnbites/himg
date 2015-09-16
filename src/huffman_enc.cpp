//-----------------------------------------------------------------------------
// HIMG, by Marcus Geelnard, 2015
//
// This is free and unencumbered software released into the public domain.
//
// See LICENSE for details.
//-----------------------------------------------------------------------------

#include "huffman_enc.h"

#include <algorithm>

#include "huffman_common.h"

namespace himg {

namespace {

// The maximum size of the tree representation (there are two additional bits
// per leaf node, representing the branches in the tree).
const int kMaxTreeDataSize = ((2 + kSymbolSize) * kNumSymbols + 7) / 8;

class OutBitstream {
 public:
  // Initialize a bitstream.
  explicit OutBitstream(uint8_t *buf)
      : m_base_ptr(buf), m_byte_ptr(buf), m_bit_pos(0) {}

  // Write bits to a bitstream.
  void WriteBits(uint32_t x, int bits) {
    // Get current stream state.
    uint8_t *buf = m_byte_ptr;
    int bit = m_bit_pos;

    // Append bits.
    // TODO(m): Optimize this!
    while (bits--) {
      *buf = (*buf & (0xff ^ (1 << bit))) | ((x & 1) << bit);
      x >>= 1;
      bit = (bit + 1) & 7;
      if (!bit) {
        ++buf;
      }
    }

    // Store new stream state.
    m_byte_ptr = buf;
    m_bit_pos = bit;
  }

  int Size() const {
    int total_bytes = static_cast<int>(m_byte_ptr - m_base_ptr);
    if (m_bit_pos > 0) {
      ++total_bytes;
    }
    return total_bytes;
  }

 private:
  uint8_t *m_base_ptr;
  uint8_t *m_byte_ptr;
  int m_bit_pos;
};

// Used by the encoder for building the optimal Huffman tree.
struct SymbolInfo {
  Symbol symbol;
  int count;
  uint32_t code;
  int bits;
};

struct EncodeNode {
  EncodeNode *child_a, *child_b;
  int count;
  int symbol;
};

// Calculate (sorted) histogram for a block of data.
void Histogram(const uint8_t *in, SymbolInfo *symbols, int size) {
  // Clear/init histogram.
  for (int k = 0; k < kNumSymbols; ++k) {
    symbols[k].symbol = static_cast<Symbol>(k);
    symbols[k].count = 0;
    symbols[k].code = 0;
    symbols[k].bits = 0;
  }

  // Build histogram.
  for (int k = 0; k < size;) {
    Symbol symbol = static_cast<Symbol>(in[k]);

    // Possible RLE?
    if (symbol == 0) {
      int zeros;
      for (zeros = 1; zeros < 16662 && (k + zeros) < size; ++zeros) {
        if (in[k + zeros] != 0)
          break;
      }
      if (zeros == 1) {
        symbols[0].count++;
      } else if (zeros == 2) {
        symbols[kSymTwoZeros].count++;
      } else if (zeros <= 6) {
        symbols[kSymUpTo6Zeros].count++;
      } else if (zeros <= 22) {
        symbols[kSymUpTo22Zeros].count++;
      } else if (zeros <= 278) {
        symbols[kSymUpTo278Zeros].count++;
      } else {
        symbols[kSymUpTo16662Zeros].count++;
      }
      k += zeros;
    } else {
      symbols[symbol].count++;
      k++;
    }
  }
}

// Store a Huffman tree in the output stream and in a look-up-table (a symbol
// array).
void StoreTree(EncodeNode *node,
               SymbolInfo *symbols,
               OutBitstream *stream,
               uint32_t code,
               int bits) {
  // Is this a leaf node?
  if (node->symbol >= 0) {
    // Append symbol to tree description.
    stream->WriteBits(1, 1);
    stream->WriteBits(static_cast<uint32_t>(node->symbol), kSymbolSize);

    // Find symbol index.
    int sym_idx;
    for (sym_idx = 0; sym_idx < kNumSymbols; ++sym_idx) {
      if (symbols[sym_idx].symbol == static_cast<Symbol>(node->symbol))
        break;
    }

    // Store code info in symbol array.
    symbols[sym_idx].code = code;
    symbols[sym_idx].bits = bits;
    return;
  } else {
    // This was not a leaf node.
    stream->WriteBits(0, 1);
  }

  // Branch A.
  StoreTree(node->child_a, symbols, stream, code, bits + 1);

  // Branch B.
  StoreTree(node->child_b, symbols, stream, code + (1 << bits), bits + 1);
}

// Generate a Huffman tree.
void MakeTree(SymbolInfo *sym, OutBitstream *stream) {
  // Initialize all leaf nodes.
  EncodeNode nodes[kMaxTreeNodes];
  int num_symbols = 0;
  for (int k = 0; k < kNumSymbols; ++k) {
    if (sym[k].count > 0) {
      nodes[num_symbols].symbol = static_cast<int>(sym[k].symbol);
      nodes[num_symbols].count = sym[k].count;
      nodes[num_symbols].child_a = nullptr;
      nodes[num_symbols].child_b = nullptr;
      ++num_symbols;
    }
  }

  // Build tree by joining the lightest nodes until there is only one node left
  // (the root node).
  EncodeNode *root = nullptr;
  int nodes_left = num_symbols;
  int next_idx = num_symbols;
  while (nodes_left > 1) {
    // Find the two lightest nodes.
    EncodeNode *node_1 = nullptr;
    EncodeNode *node_2 = nullptr;
    for (int k = 0; k < next_idx; ++k) {
      if (nodes[k].count > 0) {
        if (!node_1 || (nodes[k].count <= node_1->count)) {
          node_2 = node_1;
          node_1 = &nodes[k];
        } else if (!node_2 || (nodes[k].count <= node_2->count)) {
          node_2 = &nodes[k];
        }
      }
    }

    // Join the two nodes into a new parent node.
    root = &nodes[next_idx];
    root->child_a = node_1;
    root->child_b = node_2;
    root->count = node_1->count + node_2->count;
    root->symbol = -1;
    node_1->count = 0;
    node_2->count = 0;
    ++next_idx;
    --nodes_left;
  }

  // Store the tree in the output stream, and in the sym[] array (the latter is
  // used as a look-up-table for faster encoding).
  if (root) {
    StoreTree(root, sym, stream, 0, 0);
  } else {
    // Special case: only one symbol => no binary tree.
    root = &nodes[0];
    StoreTree(root, sym, stream, 0, 1);
  }
}

}  // namespace

int HuffmanEnc::MaxCompressedSize(int uncompressed_size) {
  return uncompressed_size + kMaxTreeDataSize;
}

int HuffmanEnc::Compress(uint8_t *out, const uint8_t *in, int in_size) {
  // Do we have anything to compress?
  if (in_size < 1)
    return 0;

  // Initialize bitstream.
  OutBitstream stream(out);

  // Calculate and sort histogram for input data.
  SymbolInfo symbols[kNumSymbols];
  Histogram(in, symbols, in_size);

  // Build Huffman tree.
  MakeTree(symbols, &stream);

  // Sort histogram - first symbol first (bubble sort).
  // TODO(m): Quick-sort.
  bool swaps;
  do {
    swaps = false;
    for (int k = 0; k < kNumSymbols - 1; ++k) {
      if (symbols[k].symbol > symbols[k + 1].symbol) {
        SymbolInfo tmp = symbols[k];
        symbols[k] = symbols[k + 1];
        symbols[k + 1] = tmp;
        swaps = true;
      }
    }
  } while (swaps);

  // Encode input stream.
  for (int k = 0; k < in_size;) {
    uint8_t symbol = in[k];

    // Possible RLE?
    if (symbol == 0) {
      int zeros;
      for (zeros = 1; zeros < 16662 && (k + zeros) < in_size; ++zeros) {
        if (in[k + zeros] != 0)
          break;
      }
      if (zeros == 1) {
        stream.WriteBits(symbols[0].code, symbols[0].bits);
      } else if (zeros == 2) {
        stream.WriteBits(symbols[kSymTwoZeros].code,
                         symbols[kSymTwoZeros].bits);
      } else if (zeros <= 6) {
        uint32_t count = static_cast<uint32_t>(zeros - 3);
        stream.WriteBits(symbols[kSymUpTo6Zeros].code,
                         symbols[kSymUpTo6Zeros].bits);
        stream.WriteBits(count, 2);
      } else if (zeros <= 22) {
        uint32_t count = static_cast<uint32_t>(zeros - 7);
        stream.WriteBits(symbols[kSymUpTo22Zeros].code,
                         symbols[kSymUpTo22Zeros].bits);
        stream.WriteBits(count, 4);
      } else if (zeros <= 278) {
        uint32_t count = static_cast<uint32_t>(zeros - 23);
        stream.WriteBits(symbols[kSymUpTo278Zeros].code,
                         symbols[kSymUpTo278Zeros].bits);
        stream.WriteBits(count, 8);
      } else {
        uint32_t count = static_cast<uint32_t>(zeros - 279);
        stream.WriteBits(symbols[kSymUpTo16662Zeros].code,
                         symbols[kSymUpTo16662Zeros].bits);
        stream.WriteBits(count, 14);
      }
      k += zeros;
    } else {
      stream.WriteBits(symbols[symbol].code, symbols[symbol].bits);
      k++;
    }
  }

  // Calculate size of output data.
  return stream.Size();
}

}  // namespace himg
