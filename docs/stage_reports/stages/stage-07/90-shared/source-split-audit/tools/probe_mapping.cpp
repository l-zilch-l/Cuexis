//  Audit probe: consumer that includes ONLY the umbrella header and names a type
//  owned by the mapping partition header.
#include <cuexis/judgement/input_boundary.hpp>

int main() {
    cuexis::judgement::InputMappingProfile profile{};
    (void)profile;
    return 0;
}
