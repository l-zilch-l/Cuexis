#include <catch2/catch_test_macros.hpp>
#include <cuexis_internal/presentation_metrics.hpp>
#include <thread>

namespace cuexis::test_support {
auto metricsFromOtherTranslationUnit() noexcept -> const void*;
}

TEST_CASE("PD metrics shares one storage per thread across translation units",
          "[core][pd-metrics]") {
    using cuexis::internal::pd::threadMetrics;
    const auto* owner = &threadMetrics();
    CHECK(cuexis::test_support::metricsFromOtherTranslationUnit() == owner);
    bool sameWithinThread{}, separateFromOwner{};
    std::thread worker{[&] {
        const auto* local = &threadMetrics();
        sameWithinThread = cuexis::test_support::metricsFromOtherTranslationUnit() == local;
        separateFromOwner = local != owner;
    }};
    worker.join();
    CHECK(sameWithinThread);
    CHECK(separateFromOwner);
}
