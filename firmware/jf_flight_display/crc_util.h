// A small CRC-32 for detecting corrupt data in flash. No Arduino headers: host-tested.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace jf {

// CRC-32 (IEEE 802.3, the zlib polynomial), computed bit by bit: slow, but needs no table and runs on a few hundred bytes.
inline uint32_t crc32(const void* data, size_t n) {
  const unsigned char* p = static_cast<const unsigned char*>(data);
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    crc ^= p[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

}  // namespace jf
