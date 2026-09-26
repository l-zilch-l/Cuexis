// Single translation unit that compiles the minimp3 decoder bodies. The public declarations are
// visible through minimp3_config.hpp, which fixes the same build options for every user.

#define MINIMP3_IMPLEMENTATION 1
#define MINIMP3_EX_IMPLEMENTATION 1

#include "minimp3_config.hpp"
