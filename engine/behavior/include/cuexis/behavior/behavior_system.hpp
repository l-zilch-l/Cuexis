#pragma once

#include <cuexis/behavior/behavior_program.hpp>
#include <cuexis/core/result.hpp>

namespace cuexis::behavior {

class BehaviorSystem final {
  public:
    //  Legacy millisecond-only overload.
    //
    //  It forwards a sample whose Beat is exactly 0: keyframe tracks keep reading `chartTimeMs`,
    //  while Event and Step tracks are evaluated at Beat 0. That is the historical behaviour and it
    //  is preserved unchanged for content whose Event / Step declarations are anchored at Beat 0 or
    //  that carries neither track kind. It is not a way to suppress a Beat: any content with an
    //  Event or Step position after Beat 0 must be evaluated with the full `BehaviorSample` built
    //  from that content's own TimingMap (chartTimeMs, Beat, Stop state and Stop progress), which
    //  the Playback path already does.
    [[nodiscard]] static auto evaluate(const BehaviorProgram& program, double chartTimeMs,
                                       world::PropertyWriteBuffer& writes) -> core::Result<void>;

    //  Full sample overload. Event and Step tracks are evaluated at `sample.beat`; keyframe tracks
    //  keep using `sample.chartTimeMs`. `inStop` and `stopProgress` are carried for the caller's
    //  freeze semantics; the sampler itself only reads the Beat value.
    [[nodiscard]] static auto evaluate(const BehaviorProgram& program, const BehaviorSample& sample,
                                       world::PropertyWriteBuffer& writes) -> core::Result<void>;
};

} // namespace cuexis::behavior
