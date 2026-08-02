#include <cuexis/core/error.hpp>

#include <iostream>

int main() {
    const cuexis::core::Error error{"consumer.core", "Core component is available"};
    if (error.code() != "consumer.core") {
        return 1;
    }
    std::cout << "Cuexis Core external consumer passed\n";
    return 0;
}
