#pragma once

// One configuration for every translation unit that sees minimp3. The scalar build keeps the
// decoder free of platform SIMD paths so canonical bytes do not depend on the target ISA.
// MINIMP3_FIXED_POINT is deliberately not defined: MP3 and Vorbis share one documented
// float-to-S16 rounding rule.

#define MINIMP3_NO_SIMD 1

#include <minimp3/minimp3_ex.h>
