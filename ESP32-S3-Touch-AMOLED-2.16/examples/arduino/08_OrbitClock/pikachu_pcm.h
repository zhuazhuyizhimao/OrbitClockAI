#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { PIKACHU_SAMPLE_RATE = 16000, PIKACHU_SAMPLE_COUNT = 35200 };
extern const int16_t pikachu_pcm[PIKACHU_SAMPLE_COUNT];
#ifdef __cplusplus
}
#endif
