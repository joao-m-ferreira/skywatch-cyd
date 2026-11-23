// Elapsed-time helpers for millis() values (32 bits, they wrap after 49 days). No Arduino headers: host-tested.
#pragma once
#include <stdint.h>

namespace jf {

// Milliseconds from `then` to `now` as a signed number: negative when `then` is slightly in the future (a timestamp
// taken after `now` was read, for example after a fetch that took a second), where an unsigned subtraction would wrap
// to about 49 days. Correct across the wrap of millis() as long as the real gap is under 24 days.
inline int32_t elapsedMs(uint32_t now, uint32_t then) { return static_cast<int32_t>(now - then); }

// True if more than `ms` milliseconds have passed since `then`.
inline bool olderThan(uint32_t now, uint32_t then, uint32_t ms) { return elapsedMs(now, then) > static_cast<int32_t>(ms); }

}  // namespace jf
