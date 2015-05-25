# HIMG - Hadamard compressed image

HIMG is a work-in-progress lossy image compression format. It is similar to
JPEG, but rather than using the discrete cosine transform (DCT) it uses the
Hadamard transform (plus it does some things slightly differently, for instance
the Huffman / RLE encoding).

The main difference between the DCT and the Hadamard transform is that the
latter can be implemented with integer additions and subtractions only, and thus
requires no multiplications. As such, HIMG is very suitable for low end hardware
with slow (or missing) multiplication instructions.

