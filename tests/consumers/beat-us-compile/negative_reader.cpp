//  Negative translation unit: the compiled-artifact Reader header belongs to the future Reader
//  target and must not be visible to the source compile consumer. The build is expected to fail at
//  this include.

#include <cuexis/gameplay_packed/compiled_capsule.hpp>

int main() {
    return 0;
}
