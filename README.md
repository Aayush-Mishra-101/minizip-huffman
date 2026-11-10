# MiniZip: Huffman Coding File Compressor

OS Lab Project (Operating Systems, COCSC301) by Aayush Mishra (2025UCS1706).

A command-line lossless file compressor/decompressor in C++17 using Huffman coding.
Works on any file type (text, source code, binary). No third-party dependencies.

## OS concepts demonstrated
- File I/O through the kernel (open, read, write, lseek, close via C++ streams)
- Binary-mode byte streams and file offset rewinding (two-pass processing)
- Buffered, bit-level output (`BitWriter`)
- Dynamic memory management (heap-allocated tree, explicit `freeTree`)
- Exit status and defensive error handling

## Build
```bash
g++ -std=c++17 -O2 -o minizip minizip.cpp
```

## Usage
```bash
./minizip -c input.txt output.huff      # compress
./minizip -d output.huff restored.txt   # decompress
cmp input.txt restored.txt              # verify (no output = identical)
```

## Archive format
| Offset | Size | Field |
|---|---|---|
| 0 | 1024 bytes | 256 x uint32 byte frequencies |
| 1024 | 4 bytes | original size (uint32) |
| 1028 | variable | Huffman bit stream, MSB first, last byte zero-padded |

## Limitations
- Fixed 1028-byte header: tiny files grow, incompressible data grows slightly.
- 32-bit counters (max 4 GiB input); no checksum or magic number.
- Empty files produce no archive.

Full report: [docs/Huffman_Compressor_OS_Report.pdf](docs/Huffman_Compressor_OS_Report.pdf)
