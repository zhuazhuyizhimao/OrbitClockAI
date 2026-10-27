#pragma once
#include <stdint.h>

namespace clock_audio {
bool begin();
// Nonblocking. Busy/unavailable requests are discarded, never queued.
bool play();
const char *status();
int lastError();
uint32_t playCount();
}
