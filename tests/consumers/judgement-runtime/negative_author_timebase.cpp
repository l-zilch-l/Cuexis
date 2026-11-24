// Negative unit: the old author timebase header belongs to the compile side and must not be
// visible from a consumer of the judgement runtime. A successful compile means the header leaked
// through the runtime include roots.
#include <cuexis/judgement/timebase.hpp>

int main() {
    return 0;
}
