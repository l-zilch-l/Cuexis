//  Negative translation unit: the base Chart header must not be visible to the source compile
//  consumer. The build is expected to fail at this include.

#include <cuexis/chart/chart_document.hpp>

int main() {
    return 0;
}
