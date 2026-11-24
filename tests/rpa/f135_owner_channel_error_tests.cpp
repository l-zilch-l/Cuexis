// F135: OwnerChannel::poll copies optional<Result<Reply>> out of the preallocated slot while the
// slot is retained. An interned Error must survive that copy with zero allocations and without
// throwing while every allocation fails.

#include "../../app/execution_host/owner_channel.hpp"
#include "../support/f135_allocation_probe.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace {

using cuexis::core::Error;
using cuexis::core::ErrorStage;
using cuexis::core::Result;
using cuexis::playback::detail::OwnerChannel;

auto internedFailure() -> Error {
    return Error::interned(cuexis::core::intern("host.rpa.allocation_failed"),
                           cuexis::core::intern("Reserved owner channel failure"),
                           ErrorStage::query);
}

} // namespace

TEST_CASE("F135 OwnerChannel poll copies interned errors with zero allocations",
          "[rpa][f135][channel]") {
    OwnerChannel<int, Result<int>> channel{2, 0};
    const auto posted = channel.post(1, false);
    REQUIRE(posted);
    const auto command = channel.take();
    REQUIRE(command);
    channel.complete(*posted, cuexis::core::unexpected(internedFailure()));

    std::uint64_t totalAttempts = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        bool threw = false;
        bool outerHasValue = false;
        bool innerHasValue = true;
        bool interned = false;
        std::string_view code;
        ErrorStage stage = ErrorStage::none;
        std::uint64_t attempts = 0;
        {
            const f135_probe::FailScope scope;
            try {
                auto polled = channel.poll(*posted);
                outerHasValue = polled.has_value();
                if (outerHasValue && *polled) {
                    innerHasValue = (**polled).has_value();
                    if (!innerHasValue) {
                        code = (**polled).error().code();
                        stage = (**polled).error().stage();
                        interned = (**polled).error().isInterned();
                    }
                }
            } catch (...) {
                threw = true;
            }
            attempts = f135_probe::attemptCount();
        }

        CHECK_FALSE(threw);
        CHECK(attempts == 0);
        CHECK(outerHasValue);
        CHECK_FALSE(innerHasValue);
        CHECK(interned);
        CHECK(code == "host.rpa.allocation_failed");
        CHECK(stage == ErrorStage::query);
        CHECK(channel.fault() == OwnerChannel<int, Result<int>>::Fault::none);
        totalAttempts += attempts;
    }
    CHECK(totalAttempts == 0);

    // The terminal reply stays retained until acknowledgement; polling is repeatable.
    REQUIRE(channel.ack(*posted));
    auto afterAck = channel.poll(*posted);
    REQUIRE_FALSE(afterAck.has_value());
    CHECK(afterAck.error().code() == "host.rpa.context_stale");
}
