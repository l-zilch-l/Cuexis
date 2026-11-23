// RR-F135 observation bridge error classification and fault injection. The test exercises the
// exact production catch paths: allocation faults are injected at their real allocation points,
// capacity exhaustion is reached through the measured configure budget, and non-fatal observation
// loss must not stop the owner.

#include "rpa_b4_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {

using cuexis::playback::detail::FaultSlot;
using cuexis::playback::detail::FeedbackId;
using cuexis::playback::detail::FrameEnvelope;
using cuexis::playback::detail::FrameObservationLedger;
using cuexis::playback::detail::PresentReceipt;
using cuexis::playback::detail::PresentStatus;

auto invalidReceipt() -> PresentReceipt {
    PresentReceipt receipt;
    receipt.context = rpa_b4_fixture::context();
    receipt.frameId = 1;
    receipt.projectionScope = 1;
    receipt.status = PresentStatus::succeeded;
    return receipt;
}

#if defined(_MSC_VER) && defined(_DEBUG)
std::atomic<bool> allocationHookEnabled{};
std::atomic<bool> allocationHookFails{};
std::atomic<std::uint64_t> allocationHookAttempts{};

int __cdecl allocationHook(int allocType, void*, std::size_t, int, long, const unsigned char*,
                           int) {
    if (!allocationHookEnabled.load(std::memory_order_relaxed)) {
        return 1;
    }
    if (allocType == _HOOK_ALLOC) {
        allocationHookAttempts.fetch_add(1, std::memory_order_relaxed);
        if (allocationHookFails.load(std::memory_order_relaxed)) {
            return 0;
        }
    }
    return 1;
}

struct AllocationHookScope final {
    AllocationHookScope() {
        _CrtSetAllocHook(&allocationHook);
    }
    ~AllocationHookScope() {
        allocationHookEnabled.store(false, std::memory_order_relaxed);
        allocationHookFails.store(false, std::memory_order_relaxed);
        _CrtSetAllocHook(nullptr);
    }
    AllocationHookScope(const AllocationHookScope&) = delete;
    auto operator=(const AllocationHookScope&) -> AllocationHookScope& = delete;
};

auto countedAllocations(const auto& operation) -> std::uint64_t {
    allocationHookAttempts.store(0, std::memory_order_relaxed);
    allocationHookEnabled.store(true, std::memory_order_relaxed);
    operation();
    allocationHookEnabled.store(false, std::memory_order_relaxed);
    return allocationHookAttempts.load(std::memory_order_relaxed);
}
#endif

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
        CHECK(ledger->fault().reason == FaultSlot::Reason::allocation);
        CHECK(ledger->fault().stage == FaultSlot::Stage::query);

        cuexis::playback::detail::armFrameObservationAllocationFault(FaultSlot::Stage::create);
        auto created = FrameObservationLedger::create(rpa_b4_fixture::context(), 1, 2, 2);
        REQUIRE_FALSE(created.has_value());
        CHECK(created.error().code() == "host.rpa.allocation_failed");
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

#if defined(_MSC_VER) && defined(_DEBUG)
    SECTION("sustained OOM: the fault slot survives but the owning error copy allocates") {
        AllocationHookScope hookScope;
        auto ledger = FrameObservationLedger::create(rpa_b4_fixture::context(), 1, 2, 2);
        REQUIRE(ledger);
        FeedbackId id{1, 1, 0, 0, "target", "binding"};
        REQUIRE(ledger->feedback(id, 0, 10).has_value());

        // The prebuilt owning error copy is not allocation-free; measure it instead of assuming.
        const auto copyAttempts = countedAllocations([&] {
            cuexis::core::Error copy = ledger->allocationFailureTemplateForTest();
            (void)copy;
        });
        CHECK(copyAttempts > 0);

        // Reading the fixed fault slot never allocates.
        FaultSlot::Reason observedReason = FaultSlot::Reason::allocation;
        FaultSlot::Stage observedStage = FaultSlot::Stage::none;
        const auto faultReadAttempts = countedAllocations([&] {
            observedReason = ledger->fault().reason;
            observedStage = ledger->fault().stage;
        });
        CHECK(observedReason == FaultSlot::Reason::none);
        CHECK(observedStage == FaultSlot::Stage::none);
        CHECK(faultReadAttempts == 0);

        // Sustained OOM: every allocation fails. The production delivery path cannot return an
        // owning error without allocating, so it throws instead of reporting allocation_failed.
        allocationHookFails.store(true, std::memory_order_relaxed);
        allocationHookAttempts.store(0, std::memory_order_relaxed);
        allocationHookEnabled.store(true, std::memory_order_relaxed);
        cuexis::playback::detail::armFrameObservationAllocationFault(FaultSlot::Stage::query);
        bool threw = false;
        bool returnedError = false;
        try {
            auto queried = ledger->query(id);
            returnedError = !queried.has_value();
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        allocationHookEnabled.store(false, std::memory_order_relaxed);
        allocationHookFails.store(false, std::memory_order_relaxed);
        const auto deliveryAttempts = allocationHookAttempts.load(std::memory_order_relaxed);

        INFO("delivery allocations attempted under OOM: " << deliveryAttempts);
        INFO("threw=" << threw);
        CHECK(deliveryAttempts > 0);
        CHECK(threw);
        CHECK_FALSE(returnedError);
        // The fixed, preallocated slot is still the authoritative evidence after the failed
        // delivery attempt; only the owning Error copy could not be produced.
        CHECK(ledger->fault().reason == FaultSlot::Reason::allocation);
        CHECK(ledger->fault().stage == FaultSlot::Stage::query);
    }
#else
    SECTION("sustained OOM delivery is not measurable on this platform") {
        WARN("MSVC Debug CRT allocation hook is unavailable; F135 sustained-OOM evidence missing");
    }
#endif
}
