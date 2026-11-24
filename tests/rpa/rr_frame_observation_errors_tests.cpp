// RR-F135 observation bridge error classification and fault injection. The test exercises the
// exact production catch paths: allocation faults are injected at their real allocation points,
// capacity exhaustion is reached through the measured configure budget, and non-fatal observation
// loss must not stop the owner. The sustained-OOM case uses the F135 allocation probe so the
// delivery path itself must copy its Error with zero allocations.

#include "../support/f135_allocation_probe.hpp"
#include "rpa_b4_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

using cuexis::playback::detail::FaultSlot;
using cuexis::playback::detail::FeedbackId;
using cuexis::playback::detail::FeedbackObservation;
using cuexis::playback::detail::FrameEnvelope;
using cuexis::playback::detail::frameObservationInjectedFaultAttempts;
using cuexis::playback::detail::FrameObservationLedger;
using cuexis::playback::detail::PresentReceipt;
using cuexis::playback::detail::PresentStatus;

class ThrowingRenderer final {
  public:
    enum class Mode {
        none,
        submitBadAlloc,
        submitRuntimeError,
        presentBadAlloc,
        presentRuntimeError
    };

    explicit ThrowingRenderer(Mode mode) noexcept : mode_(mode) {}

    [[nodiscard]] auto identity() const noexcept
        -> cuexis::presentation_renderer::RendererIdentity {
        return {7, 1};
    }
    [[nodiscard]] auto submit(const cuexis::playback::FrameSnapshot&)
        -> cuexis::core::Result<cuexis::presentation_renderer::DrawSummary> {
        ++submitCalls;
        if (mode_ == Mode::submitBadAlloc) {
            throw std::bad_alloc{};
        }
        if (mode_ == Mode::submitRuntimeError) {
            throw std::runtime_error{"renderer submit rejected"};
        }
        return cuexis::presentation_renderer::DrawSummary{};
    }
    [[nodiscard]] auto present() -> cuexis::core::Result<void> {
        ++presentCalls;
        if (mode_ == Mode::presentBadAlloc) {
            throw std::bad_alloc{};
        }
        if (mode_ == Mode::presentRuntimeError) {
            throw std::runtime_error{"renderer present rejected"};
        }
        return {};
    }

    int submitCalls{};
    int presentCalls{};

  private:
    Mode mode_{};
};

auto matchingFrame() -> FrameEnvelope {
    FrameEnvelope frame;
    frame.context = rpa_b4_fixture::context();
    frame.context.rendererGeneration = 1;
    frame.frameId = 1;
    frame.projectionScope = 1;
    return frame;
}

} // namespace

TEST_CASE("rr_frame_observation_errors", "[rpa][observation][f135]") {
    SECTION("actual allocation failure reports allocation_failed and keeps the failing stage") {
        auto ledger = FrameObservationLedger::create(rpa_b4_fixture::context(), 1, 2, 2);
        REQUIRE(ledger);

        cuexis::playback::detail::armFrameObservationAllocationFault(FaultSlot::Stage::configure);
        auto configured = ledger->configure(FrameObservationLedger::Limits{});
        REQUIRE_FALSE(configured.has_value());
        CHECK(configured.error().code() == "host.rpa.allocation_failed");
        CHECK(configured.error().isInterned());
        CHECK(configured.error().stage() == FaultSlot::Stage::configure);
        CHECK(ledger->fault().reason == FaultSlot::Reason::allocation);
        CHECK(ledger->fault().stage == FaultSlot::Stage::configure);

        // The same class of failure on a real copy allocation reports the same stable code.
        FeedbackId id{1, 1, 0, 0, "target", "binding"};
        REQUIRE(ledger->feedback(id, 0, 10).has_value());
        cuexis::playback::detail::armFrameObservationAllocationFault(FaultSlot::Stage::query);
        auto queried = ledger->query(id);
        REQUIRE_FALSE(queried.has_value());
        CHECK(queried.error().code() == "host.rpa.allocation_failed");
        CHECK(queried.error().code() != "host.rpa.capacity_exceeded");
        CHECK(queried.error().isInterned());
        CHECK(queried.error().stage() == FaultSlot::Stage::query);
        CHECK(ledger->fault().reason == FaultSlot::Reason::allocation);
        CHECK(ledger->fault().stage == FaultSlot::Stage::query);

        cuexis::playback::detail::armFrameObservationAllocationFault(FaultSlot::Stage::create);
        auto created = FrameObservationLedger::create(rpa_b4_fixture::context(), 1, 2, 2);
        REQUIRE_FALSE(created.has_value());
        CHECK(created.error().code() == "host.rpa.allocation_failed");
        CHECK(created.error().isInterned());
        CHECK(created.error().stage() == FaultSlot::Stage::create);
    }

    SECTION("measured configuration budget exhaustion keeps capacity_exceeded") {
        auto ledger = FrameObservationLedger::create(rpa_b4_fixture::context(), 1, 2, 2);
        REQUIRE(ledger);
        FrameObservationLedger::Limits limits;
        limits.bytes = 1;
        auto configured = ledger->configure(limits);
        REQUIRE_FALSE(configured.has_value());
        CHECK(configured.error().code() == "host.rpa.capacity_exceeded");
        CHECK(configured.error().code() != "host.rpa.allocation_failed");
        CHECK(configured.error().stage() == FaultSlot::Stage::configure);
        CHECK(ledger->fault().reason == FaultSlot::Reason::capacity);
        CHECK(ledger->fault().stage == FaultSlot::Stage::configure);
    }

    SECTION("non-fatal observation loss leaves Unknown evidence and does not stop the owner") {
        auto ledger = FrameObservationLedger::create(rpa_b4_fixture::context(), 1, 2, 2);
        REQUIRE(ledger);
        const auto before = ledger->unknownCount();
        cuexis::playback::detail::armFrameObservationAllocationFault(FaultSlot::Stage::feedback);
        FeedbackId lost{1, 1, 0, 0, "target", "lost"};
        auto recorded = ledger->feedback(lost, 0, 10);
        REQUIRE(recorded.has_value());
        CHECK(ledger->unknownCount() > before);
        CHECK(ledger->historyUnavailable());

        // The owner keeps accepting legitimate work; loss is recorded, not converted to success.
        FeedbackId next{1, 2, 0, 0, "target", "next"};
        REQUIRE(ledger->feedback(next, 1, 20).has_value());
        auto context = rpa_b4_fixture::context();
        context.controlSequence = 2;
        REQUIRE(ledger->activate(context, 2).has_value());
    }

    SECTION("submit and present failures are not reported as success") {
        auto frame = matchingFrame();
        ThrowingRenderer badSubmit(ThrowingRenderer::Mode::submitBadAlloc);
        auto failedSubmit = cuexis::playback::detail::presentOwnedFrame(
            frame, frame.context, 1, 7, badSubmit, [] { return 1; }, std::nullopt);
        REQUIRE(failedSubmit.has_value());
        CHECK(failedSubmit->status == PresentStatus::failed);
        REQUIRE(failedSubmit->cause);
        CHECK(failedSubmit->cause->code() == "host.rpa.allocation_failed");
        CHECK(failedSubmit->cause->isInterned());
        CHECK(badSubmit.submitCalls == 1);
        CHECK(badSubmit.presentCalls == 0);

        ThrowingRenderer otherSubmit(ThrowingRenderer::Mode::submitRuntimeError);
        auto otherFailed = cuexis::playback::detail::presentOwnedFrame(
            frame, frame.context, 1, 7, otherSubmit, [] { return 2; }, std::nullopt);
        REQUIRE(otherFailed.has_value());
        CHECK(otherFailed->status == PresentStatus::failed);
        REQUIRE(otherFailed->cause);
        CHECK(otherFailed->cause->code() == "host.rpa.device_failed");
        CHECK(otherFailed->cause->code() != "host.rpa.allocation_failed");
        CHECK(otherSubmit.presentCalls == 0);

        ThrowingRenderer badPresent(ThrowingRenderer::Mode::presentBadAlloc);
        auto failedPresent = cuexis::playback::detail::presentOwnedFrame(
            frame, frame.context, 1, 7, badPresent, [] { return 3; }, std::nullopt);
        REQUIRE(failedPresent.has_value());
        CHECK(failedPresent->status == PresentStatus::failed);
        REQUIRE(failedPresent->cause);
        CHECK(failedPresent->cause->code() == "host.rpa.allocation_failed");
        CHECK(failedPresent->cause->isInterned());
        CHECK(badPresent.submitCalls == 1);
        CHECK(badPresent.presentCalls == 1);

        ThrowingRenderer otherPresent(ThrowingRenderer::Mode::presentRuntimeError);
        auto presentFailed = cuexis::playback::detail::presentOwnedFrame(
            frame, frame.context, 1, 7, otherPresent, [] { return 4; }, std::nullopt);
        REQUIRE(presentFailed.has_value());
        CHECK(presentFailed->status == PresentStatus::failed);
        REQUIRE(presentFailed->cause);
        CHECK(presentFailed->cause->code() == "host.rpa.device_failed");
        CHECK(otherPresent.presentCalls == 1);
    }

    SECTION("sustained OOM: three deliveries keep code and stage with zero allocations") {
        auto ledger = FrameObservationLedger::create(rpa_b4_fixture::context(), 1, 2, 2);
        REQUIRE(ledger);
        FeedbackId id{1, 1, 0, 0, "target", "binding"};
        REQUIRE(ledger->feedback(id, 0, 10).has_value());
        const auto attemptsBefore = frameObservationInjectedFaultAttempts();
        const auto unknownBefore = ledger->unknownCount();
        const auto retainedBefore = ledger->retainedAttempts();

        struct Delivery final {
            std::string_view code;
            FaultSlot::Stage stage{};
            bool interned{};
            bool threw{};
            std::uint64_t allocations{};
        };
        std::array<Delivery, 3> deliveries{};

        for (std::size_t attempt = 0; attempt < deliveries.size(); ++attempt) {
            cuexis::playback::detail::armFrameObservationAllocationFault(FaultSlot::Stage::query);
            Delivery& observed = deliveries[attempt];
            {
                const f135_probe::FailScope scope;
                try {
                    auto queried = ledger->query(id);
                    std::optional<cuexis::core::Result<FeedbackObservation>> delivered;
                    delivered.emplace(std::move(queried));
                    auto copy = delivered;
                    observed.code = copy->error().code();
                    observed.stage = copy->error().stage();
                    observed.interned = copy->error().isInterned();
                } catch (...) {
                    observed.threw = true;
                }
                observed.allocations = f135_probe::attemptCount();
            }
            CHECK_FALSE(observed.threw);
            CHECK(observed.allocations == 0);
            // The preallocated slot names the same failure after every delivery attempt.
            CHECK(ledger->fault().reason == FaultSlot::Reason::allocation);
            CHECK(ledger->fault().stage == FaultSlot::Stage::query);
        }

        for (const auto& delivery : deliveries) {
            CHECK(delivery.code == "host.rpa.allocation_failed");
            CHECK(delivery.stage == FaultSlot::Stage::query);
            CHECK(delivery.interned);
        }
        CHECK(deliveries[0].code == deliveries[1].code);
        CHECK(deliveries[1].code == deliveries[2].code);
        CHECK(deliveries[0].stage == deliveries[2].stage);
        CHECK(frameObservationInjectedFaultAttempts() == attemptsBefore + deliveries.size());
        CHECK(ledger->unknownCount() == unknownBefore);
        CHECK(ledger->retainedAttempts() == retainedBefore);
    }
}
