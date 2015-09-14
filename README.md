# HIMG - Hadamard compressed image

HIMG is a work-in-progress lossy image compression format. It is similar to
JPEG, but rather than using the discrete cosine transform (DCT) it uses the
Hadamard transform (plus it does some things slightly differently, for instance
the Huffman / RLE encoding).

The main difference between the DCT and the Hadamard transform is that the
latter can be implemented with integer additions and subtractions only, and thus
requires no multiplications. As such, HIMG is very suitable for low end hardware
with slow (or missing) multiplication instructions.

## Goals

The first goal was to crate an image codec that is suitable for low end hardware
(e.g. CPUs without floating point capabilities and slow or lacking
multiplication / division instructions). Thus, HIMG is designed as an
integer-only image codec involving no multiplications nor divisions in the core
loops.

Another goal is that the image codec should give similar quality and compression
ratio as JPEG. Since HIMG uses a simpler transform compared to JPEG it has a
natural disadvantage, but in most cases HIMG comes pretty close to JPEG.

A third goal is that HIMG should be as fast as possible on modern hardware with
a high degree of hardware parallelism. To this end, the data format is designed
with parallelism in mind.
