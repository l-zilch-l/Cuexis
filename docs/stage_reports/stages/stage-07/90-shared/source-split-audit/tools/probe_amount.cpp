//  Audit probe: consumer that includes ONLY the umbrella header and requires the
//  complete type owned by one of its partition headers.
#include <cuexis/judgement/input_boundary.hpp>

int main() {
    static_assert(sizeof(cuexis::judgement::AmountSpec) > 0);
    return 0;
}
