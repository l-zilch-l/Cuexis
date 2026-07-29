#pragma once

// The C++ shared preview intentionally crosses matching-toolchain STL boundaries.

#if defined(_MSC_VER)
#define CUEXIS_ABI_WARNING_PUSH __pragma(warning(push)) __pragma(warning(disable : 4251))
#define CUEXIS_ABI_WARNING_POP __pragma(warning(pop))
#else
#define CUEXIS_ABI_WARNING_PUSH
#define CUEXIS_ABI_WARNING_POP
#endif
