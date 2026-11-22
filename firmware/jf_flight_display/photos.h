// Photos of the actual aircraft from planespotters.net (thumbnails, cached in RAM).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace jf {

struct Photo {
  char reg[12];       // registration the photo belongs to
  uint8_t* jpg;       // JPEG bytes (heap), nullptr when no photo is available
  size_t len;
  char credit[28];    // photographer
  bool tried;         // looked up already (also when no photo was found)
};

// The cached photo for a registration, or nullptr when there is none (yet).
const Photo* photoFor(const char* reg);
// Looks the registration up (blocking, ~1-3 s) unless it was tried before.
// Returns true when a new photo became available during this call.
bool photoPrefetch(const char* reg);

}  // namespace jf
