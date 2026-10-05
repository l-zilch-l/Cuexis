#include <cuexis/judgement/kernel_types.hpp>

namespace cuexis::judgement {
OwnedInputMappingProfile::OwnedInputMappingProfile(const InputMappingProfile& value)
    : id_(value.profileId), version_(value.profileVersion), source_(value.sourceClass.token()) {
    for (const auto& domain : value.domains) {
        domains_.emplace_back(domain.domainToken, domain.amount);
    }
}
auto OwnedInputMappingProfile::view() const -> InputMappingProfile {
    const auto source = SourceClass::fromToken(source_);
    InputMappingProfile result{id_, version_, source ? *source : SourceClass{}, {}};
    for (const auto& [token, spec] : domains_) {
        result.domains.push_back({token, spec});
    }
    return result;
}
OwnedTimebaseProfile::OwnedTimebaseProfile(const TimebaseProfile& value)
    : id_(value.profileId), unit_(value.unitToken), scale_(value.tickScale),
      origin_(value.originBeat), tempo_(value.initialTempo), tempos_(value.tempoSections),
      stops_(value.stopSections) {}
auto OwnedTimebaseProfile::view() const -> TimebaseProfile {
    return {id_, unit_, scale_, origin_, tempo_, tempos_, stops_};
}
} // namespace cuexis::judgement
