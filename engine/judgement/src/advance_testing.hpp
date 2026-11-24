#pragma once

//  Test-only fault injection for the runtime advance kernel.
//
//  This header is internal: it lives next to the kernel implementation, is not installed, and is
//  compiled into the ordinary library so the contract test can drive the seal/Fold failure points
//  through the real transaction. Nothing in production sets these controls; an empty optional is
//  the production state. The injected failures are real exceptions, so the kernel classifies them
//  through its production catch path instead of a pre-labelled fault.

#include <cuexis/judgement/advance_kernel.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace cuexis::judgement::runtime::detail {

enum class AdvanceFaultPoint : std::uint8_t {
    none = 0,
    //  Throw std::runtime_error after the tick draft and before any slot/frontier publication.
    beforeSeal = 1,
    //  Throw std::runtime_error after the sealed prefix is published and before the Fold runs.
    afterSealFold = 2,
    //  Fail the ingress reservation before any sequence is numbered.
    beforeNumbering = 3,
    //  Invoke `atSeal` inside the transaction to prove the reentrancy guard.
    reentrantSeal = 4,
    //  Throw std::bad_alloc after the tick draft and before any slot/frontier publication.
    allocationBeforeSeal = 5,
    //  Throw std::overflow_error (a checked-arithmetic failure) before any slot/frontier
    //  publication.
    arithmeticBeforeSeal = 6,
};

struct AdvanceTestControls final {
    AdvanceFaultPoint point{AdvanceFaultPoint::none};
    //  The 1-based ordinal of the event-bearing tick seal that receives the fault. Eventless seals
    //  do not count, so `1` is the first settlement-bearing tick of the call.
    std::size_t sealOrdinal{1};
    std::function<void()> atSeal;
};

class AdvanceTestAccess final {
  public:
    static void inject(AdvanceKernel& kernel, AdvanceTestControls controls) {
        if (controls.point == AdvanceFaultPoint::none) {
            kernel.testFaultPoint_.reset();
        } else {
            kernel.testFaultPoint_ = static_cast<std::uint8_t>(controls.point);
        }
        kernel.testSealOrdinal_ = controls.sealOrdinal;
        kernel.testAtSeal_ = std::move(controls.atSeal);
    }

    static void clear(AdvanceKernel& kernel) noexcept {
        kernel.testFaultPoint_.reset();
        kernel.testAtSeal_ = {};
    }
};

} // namespace cuexis::judgement::runtime::detail
