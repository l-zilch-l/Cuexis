#include <algorithm>
#include <bit>
#include <cuexis/playback/realtime_clock.hpp>
#include <numeric>
#include <stdexcept>

namespace cuexis::playback {
namespace {
using Rational = RealtimeRational;
auto invalid(std::string_view reason) -> core::Error {
    return core::Error{"host.rpa.clock_invalid", std::string{reason}};
}
auto magnitude(std::int64_t x) -> std::uint64_t {
    return x < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(x) : static_cast<std::uint64_t>(x);
}
auto checkedAdd(std::int64_t a, std::int64_t b) -> std::int64_t {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
        throw std::overflow_error{"clock sum"};
    return a + b;
}
auto checkedMultiply(std::int64_t a, std::int64_t b) -> std::int64_t {
    const bool negative = (a < 0) != (b < 0);
    const auto limit = static_cast<std::uint64_t>(INT64_MAX) + (negative ? 1U : 0U);
    const auto x = magnitude(a), y = magnitude(b);
    if (y && x > limit / y)
        throw std::overflow_error{"clock product"};
    const auto product = x * y;
    if (negative && product == (std::uint64_t{1} << 63))
        return INT64_MIN;
    return negative ? -static_cast<std::int64_t>(product) : static_cast<std::int64_t>(product);
}
auto rational(Rational x) -> Rational {
    if (x.denominator <= 0)
        throw std::invalid_argument{"clock denominator"};
    const auto d = std::gcd(magnitude(x.numerator), static_cast<std::uint64_t>(x.denominator));
    return {x.numerator / static_cast<std::int64_t>(d),
            x.denominator / static_cast<std::int64_t>(d)};
}
auto add(Rational a, Rational b) -> Rational {
    a = rational(a);
    b = rational(b);
    const auto d = std::gcd(a.denominator, b.denominator);
    return rational({checkedAdd(checkedMultiply(a.numerator, b.denominator / d),
                                checkedMultiply(b.numerator, a.denominator / d)),
                     checkedMultiply(a.denominator, b.denominator / d)});
}
auto negative(Rational x) -> Rational {
    x = rational(x);
    if (x.numerator == INT64_MIN)
        throw std::overflow_error{"clock negate"};
    return {-x.numerator, x.denominator};
}
auto multiply(Rational a, Rational b) -> Rational {
    a = rational(a);
    b = rational(b);
    const auto g = std::gcd(magnitude(a.numerator), static_cast<std::uint64_t>(b.denominator));
    const auto h = std::gcd(magnitude(b.numerator), static_cast<std::uint64_t>(a.denominator));
    return rational({checkedMultiply(a.numerator / static_cast<std::int64_t>(g),
                                     b.numerator / static_cast<std::int64_t>(h)),
                     checkedMultiply(a.denominator / static_cast<std::int64_t>(h),
                                     b.denominator / static_cast<std::int64_t>(g))});
}
auto rounded(Rational x) -> std::int64_t {
    x = rational(x);
    const auto q = x.numerator / x.denominator;
    const auto remainder = magnitude(x.numerator % x.denominator);
    const auto denominator = static_cast<std::uint64_t>(x.denominator);
    const bool increment =
        remainder > denominator - remainder || (remainder == denominator - remainder && q % 2 != 0);
    return increment ? checkedAdd(q, x.numerator < 0 ? -1 : 1) : q;
}
auto elapsed(std::int64_t now, std::int64_t sample) -> std::int64_t {
    if (now < sample || (sample < 0 && now > INT64_MAX + sample))
        throw std::overflow_error{"clock elapsed"};
    return now - sample;
}
auto position(const ClockFramePosition& p) -> Rational {
    if (p.frame < 0 || p.rate <= 0)
        throw std::invalid_argument{"clock frame domain"};
    return multiply({p.frame, p.rate}, {1000000, 1});
}
} // namespace
struct RealtimeClock::Impl final {
    Impl(SchedulingProfile p, ClockMappingConfiguration c, std::string d, std::uint64_t s)
        : profile(std::move(p)), config(c), domain(std::move(d)), segment(s) {}
    SchedulingProfile profile;
    ClockMappingConfiguration config;
    std::string domain;
    std::uint64_t segment;
    bool permitted{}, tail{}, tailPaused{}, requiresNewSegment{};
    std::optional<ClockObservation> last;
    std::optional<std::int64_t> lastH, lastNow;
    Rational tailSource{};
    std::int64_t tailAnchor{}, tailError{}, terminalH{};
    std::optional<TailProfile> tailProfile;
    auto ticks(Rational presented, std::uint64_t sequence, std::int64_t error) const -> ClockTicks {
        const auto session = add(presented, negative(config.timingOffsetUs));
        return {rounded(add(session, negative(config.inputCompensationUs))),
                rounded(session),
                rounded(add(session, config.displayLeadUs)),
                segment,
                sequence,
                error};
    }
    auto validate(const ClockObservation& observation, std::int64_t now, ClockPlaybackState state,
                  bool frozen = false) const -> Rational {
        if (observation.monotonicDomain != domain || observation.segment != segment ||
            !observation.presented || observation.quality == ClockQuality::Unknown ||
            observation.errorBoundUs < 0 || observation.errorBoundUs > *profile.maximumErrorUs ||
            observation.underrun || observation.deviceChanged || observation.rateChanged ||
            observation.playback != state || now > observation.validUntilMonotonicUs ||
            observation.validUntilMonotonicUs < observation.sampleMonotonicUs ||
            elapsed(observation.validUntilMonotonicUs, observation.sampleMonotonicUs) >
                *profile.observationValidityUs ||
            (lastNow && now < *lastNow))
            throw std::invalid_argument{"clock observation invalid"};
        if (observation.source)
            static_cast<void>(position(*observation.source));
        if (observation.deviceMixed)
            static_cast<void>(position(*observation.deviceMixed));
        if (last && (observation.publicationSequence < last->publicationSequence ||
                     (observation.publicationSequence == last->publicationSequence &&
                      observation != *last)))
            throw std::invalid_argument{"clock publication conflict"};
        const auto age = elapsed(now, observation.sampleMonotonicUs);
        // At EOF the adapter's final presented position is frozen; no extrapolation past EOF.
        auto result =
            frozen ? position(*observation.presented)
                   : add(position(*observation.presented), multiply({age, 1}, config.fixedRate));
        if (observation.maximumPresented) {
            const auto maximum = position(*observation.maximumPresented);
            if (observation.maximumPresented->rate != observation.presented->rate ||
                observation.presented->frame > observation.maximumPresented->frame)
                throw std::invalid_argument{"clock source endpoint invalid"};
            if (add(result, negative(maximum)).numerator > 0)
                result = maximum;
        }
        return result;
    }
};
RealtimeClock::RealtimeClock(std::unique_ptr<Impl> p) noexcept : impl_(std::move(p)) {}
RealtimeClock::~RealtimeClock() noexcept = default;
RealtimeClock::RealtimeClock(RealtimeClock&&) noexcept = default;
auto RealtimeClock::operator=(RealtimeClock&&) noexcept -> RealtimeClock& = default;
auto RealtimeClock::calibrationIdentity(ClockMappingConfiguration config)
    -> core::Result<std::string> try {
    const auto timing = rational(config.timingOffsetUs);
    const auto input = rational(config.inputCompensationUs);
    const auto rate = rational(config.fixedRate);
    static_cast<void>(rational(config.displayLeadUs));
    if (rate.numerator <= 0)
        return core::unexpected(invalid("clock configuration invalid"));
    const auto encode = [](Rational value) {
        return std::to_string(value.numerator) + "/" + std::to_string(value.denominator);
    };
    return "gameplay.clock-mapping.v1;A=" + encode(timing) + ";I=" + encode(input) +
           ";rate=" + encode(rate);
} catch (...) {
    return core::unexpected(invalid("clock calibration identity failed"));
}
auto RealtimeClock::seekPosition(ClockMappingConfiguration config, std::int64_t horizon)
    -> core::Result<SeekClockPosition> try {
    if (rational(config.fixedRate).numerator <= 0)
        return core::unexpected(invalid("seek mapping rate invalid"));
    const auto timing = rational(config.timingOffsetUs);
    const auto source = add({horizon, 1}, timing);
    if (source.numerator < 0)
        return core::unexpected(invalid("seek source position is negative"));
    const auto session = add(source, negative(timing));
    return SeekClockPosition{source, rounded(add(session, negative(config.inputCompensationUs))),
                             horizon, rounded(add(session, config.displayLeadUs))};
} catch (...) {
    return core::unexpected(invalid("seek mapping arithmetic rejected"));
}
auto RealtimeClock::create(SchedulingProfile profile, ClockMappingConfiguration config,
                           std::string domain, std::uint64_t segment)
    -> core::Result<RealtimeClock> try {
    if (!profile.observationValidityUs || *profile.observationValidityUs < 0 ||
        !profile.maximumErrorUs || *profile.maximumErrorUs < 0 ||
        !profile.retainedMessageCapacity || !*profile.retainedMessageCapacity ||
        (!profile.testOnly && profile.measurementIdentity.empty())) {
        return core::unexpected(core::Error{"host.rpa.profile_incomplete",
                                            "Explicit measured scheduling parameters required"});
    }
    if (domain.empty() || segment == 0 || rational(config.fixedRate).numerator <= 0)
        return core::unexpected(invalid("clock configuration invalid"));
    static_cast<void>(rational(config.timingOffsetUs));
    static_cast<void>(rational(config.inputCompensationUs));
    static_cast<void>(rational(config.displayLeadUs));
    return RealtimeClock{
        std::make_unique<Impl>(std::move(profile), config, std::move(domain), segment)};
} catch (...) {
    return core::unexpected(invalid("clock configuration arithmetic failed"));
}
auto RealtimeClock::sample(const ClockObservation& observation, std::int64_t now)
    -> core::Result<ClockTicks> try {
    if (impl_->requiresNewSegment || impl_->tail)
        throw std::invalid_argument{"tail requires explicit sampleTail"};
    auto result = impl_->ticks(impl_->validate(observation, now, ClockPlaybackState::Playing),
                               observation.publicationSequence, observation.errorBoundUs);
    if (impl_->lastH && result.horizon < *impl_->lastH)
        throw std::invalid_argument{"clock horizon rewind"};
    impl_->last = observation;
    impl_->lastH = result.horizon;
    impl_->lastNow = now;
    impl_->permitted = true;
    return result;
} catch (const std::exception& exception) {
    impl_->permitted = false;
    impl_->requiresNewSegment = true;
    return core::unexpected(
        invalid(std::string{"clock observation rejected: "} + exception.what()));
} catch (...) {
    impl_->permitted = false;
    impl_->requiresNewSegment = true;
    return core::unexpected(invalid("clock observation rejected"));
}
auto RealtimeClock::observePaused(const ClockObservation& observation, std::int64_t now)
    -> core::Result<ClockTicks> try {
    impl_->permitted = false;
    if (impl_->requiresNewSegment || impl_->tail)
        throw std::invalid_argument{"capture requires a coherent body segment"};
    auto result = impl_->ticks(impl_->validate(observation, now, ClockPlaybackState::Paused, true),
                               observation.publicationSequence, observation.errorBoundUs);
    impl_->last = observation;
    impl_->lastNow = now;
    return result;
} catch (...) {
    impl_->permitted = false;
    impl_->requiresNewSegment = true;
    return core::unexpected(invalid("paused capture observation rejected"));
}
auto RealtimeClock::startSegment(std::uint64_t segment, const ClockObservation& observation,
                                 std::int64_t now) -> core::Result<ClockTicks> try {
    if (segment <= impl_->segment || observation.segment != segment)
        return core::unexpected(invalid("new segment required"));
    auto candidate = *impl_;
    candidate.segment = segment;
    candidate.last.reset();
    candidate.lastNow.reset();
    candidate.tail = false;
    const auto result =
        candidate.ticks(candidate.validate(observation, now, ClockPlaybackState::Playing),
                        observation.publicationSequence, observation.errorBoundUs);
    // A new clock segment may rebase to a paused logical position. It grants no kernel rewind.
    candidate.last = observation;
    candidate.lastH = result.horizon;
    candidate.lastNow = now;
    candidate.permitted = true;
    candidate.requiresNewSegment = false;
    *impl_ = std::move(candidate);
    return result;
} catch (...) {
    impl_->permitted = false;
    impl_->requiresNewSegment = true;
    return core::unexpected(invalid("new segment rejected"));
}
auto RealtimeClock::startTail(const ClockObservation& observation, std::int64_t now,
                              std::uint64_t segment, TailProfile profile, std::int64_t lastTimer,
                              std::int64_t close) -> core::Result<TailClockTransition> try {
    if (impl_->requiresNewSegment || impl_->tail || segment <= impl_->segment ||
        profile.maximumAnchorErrorUs < 0 ||
        profile.maximumAnchorErrorUs < observation.errorBoundUs ||
        rational(profile.fixedRate).numerator <= 0 || close < 0 ||
        (!profile.testOnly && profile.measurementIdentity.empty()) ||
        (!impl_->profile.testOnly && profile.testOnly))
        throw std::invalid_argument{"tail profile invalid"};
    const auto source = impl_->validate(observation, now, ClockPlaybackState::EndOfStream, true);
    const auto anchor =
        impl_->ticks(source, observation.publicationSequence, observation.errorBoundUs);
    if (impl_->lastH && anchor.horizon < *impl_->lastH)
        throw std::invalid_argument{"EOF anchor rewinds time"};
    const auto terminal = checkedAdd(std::max(lastTimer, profile.inputDeadline), close);
    if (terminal < anchor.horizon)
        throw std::invalid_argument{"tail terminal precedes anchor"};
    auto candidate = *impl_;
    candidate.segment = segment;
    candidate.tail = true;
    candidate.tailPaused = false;
    candidate.tailSource = source;
    candidate.tailAnchor = now;
    candidate.tailError = observation.errorBoundUs;
    candidate.terminalH = terminal;
    candidate.tailProfile = profile;
    candidate.last = observation;
    candidate.lastNow = now;
    candidate.permitted = true;
    candidate.requiresNewSegment = false;
    *impl_ = std::move(candidate);
    return TailClockTransition{observation,           segment,   now,  terminal,
                               profile.inputDeadline, lastTimer, close};
} catch (...) {
    impl_->permitted = false;
    impl_->requiresNewSegment = true;
    return core::unexpected(invalid("tail transition rejected"));
}
auto RealtimeClock::sampleTail(std::int64_t now) -> core::Result<ClockTicks> try {
    if (!impl_->tail || impl_->tailPaused || !impl_->permitted ||
        (impl_->lastNow && now < *impl_->lastNow))
        throw std::invalid_argument{"tail unavailable"};
    const auto presented = add(impl_->tailSource, multiply({elapsed(now, impl_->tailAnchor), 1},
                                                           impl_->tailProfile->fixedRate));
    auto result = impl_->ticks(presented, impl_->last ? impl_->last->publicationSequence : 0,
                               impl_->tailError);
    // Owner polling may pass the last obligation between samples. Freeze at the explicit
    // terminal clock position so the final horizon remains reachable at any fixed rate.
    if (result.horizon > impl_->terminalH)
        result = impl_->ticks(add({impl_->terminalH, 1}, impl_->config.timingOffsetUs),
                              impl_->last ? impl_->last->publicationSequence : 0, impl_->tailError);
    impl_->lastH = result.horizon;
    impl_->lastNow = now;
    return result;
} catch (...) {
    impl_->permitted = false;
    impl_->requiresNewSegment = true;
    return core::unexpected(invalid("tail sample rejected"));
}
auto RealtimeClock::observeEof(const ClockObservation& observation, std::int64_t now)
    -> core::Result<ClockTicks> try {
    const auto source = impl_->validate(observation, now, ClockPlaybackState::EndOfStream, true);
    auto result = impl_->ticks(source, observation.publicationSequence, observation.errorBoundUs);
    if (impl_->lastH && result.horizon < *impl_->lastH)
        return core::unexpected(invalid("EOF anchor rewinds time"));
    impl_->permitted = false;
    return result;
} catch (...) {
    return core::unexpected(invalid("EOF observation rejected"));
}

auto RealtimeClock::pausedTailTicks() const -> core::Result<ClockTicks> try {
    if (!impl_->tail || !impl_->tailPaused)
        return core::unexpected(invalid("Tail is not paused"));
    return impl_->ticks(impl_->tailSource, impl_->last ? impl_->last->publicationSequence : 0,
                        impl_->tailError);
} catch (...) {
    return core::unexpected(invalid("Paused tail query failed"));
}

auto RealtimeClock::pauseTail(std::int64_t now) -> core::Result<void> try {
    auto result = sampleTail(now);
    if (!result)
        return core::unexpected(result.error());
    impl_->tailSource = result->horizon >= impl_->terminalH
                            ? add({impl_->terminalH, 1}, impl_->config.timingOffsetUs)
                            : add(impl_->tailSource, multiply({elapsed(now, impl_->tailAnchor), 1},
                                                              impl_->tailProfile->fixedRate));
    impl_->tailAnchor = now;
    impl_->tailPaused = true;
    impl_->permitted = false;
    return {};
} catch (...) {
    impl_->permitted = false;
    impl_->requiresNewSegment = true;
    return core::unexpected(invalid("tail pause failed"));
}
auto RealtimeClock::adoptTailPause(std::int64_t anchor) -> core::Result<void> try {
    if (!impl_->tail || impl_->tailPaused || anchor < impl_->tailAnchor)
        return core::unexpected(invalid("Tail pause action rejected"));
    auto source = add(impl_->tailSource, multiply({elapsed(anchor, impl_->tailAnchor), 1},
                                                  impl_->tailProfile->fixedRate));
    if (impl_->ticks(source, 0, impl_->tailError).horizon >= impl_->terminalH)
        source = add({impl_->terminalH, 1}, impl_->config.timingOffsetUs);
    impl_->tailSource = source;
    impl_->tailAnchor = anchor;
    impl_->tailPaused = true;
    impl_->permitted = false;
    return {};
} catch (...) {
    return core::unexpected(invalid("Tail pause action failed"));
}

auto RealtimeClock::resumeTail(std::int64_t now, std::uint64_t segment,
                               std::optional<std::int64_t> minimumTerminal) -> core::Result<void> {
    if (!impl_->tail || !impl_->tailPaused || segment <= impl_->segment || now < impl_->tailAnchor)
        return core::unexpected(invalid("tail resume requires new segment and monotonic anchor"));
    impl_->tailAnchor = now;
    if (minimumTerminal)
        impl_->terminalH = std::max(impl_->terminalH, *minimumTerminal);
    impl_->segment = segment;
    impl_->tailPaused = false;
    impl_->requiresNewSegment = false;
    impl_->lastNow = now;
    impl_->permitted = true;
    return {};
}
auto RealtimeClock::permitsAdvance() const noexcept -> bool {
    return impl_ && impl_->permitted;
}
} // namespace cuexis::playback
