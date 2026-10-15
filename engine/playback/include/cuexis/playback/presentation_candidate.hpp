#pragma once

// CPU-owning candidate data; no Prepared pointer or device object crosses owners.
#include <cuexis/playback/playback_types.hpp>
#include <cuexis/playback/presentation.hpp>
namespace cuexis::playback {
CUEXIS_ABI_WARNING_PUSH
struct HostContext final {
    std::uint64_t sessionScope{}, clockSegment{}, runEpoch{}, controlSequence{};
    std::uint64_t resourceGeneration{}, rendererGeneration{};
    PreparedSemanticIdentity contentIdentity{};
    friend auto operator==(const HostContext&, const HostContext&) -> bool = default;
};
struct AudioCandidateInput final {
    std::string assetId;
    double timingOffsetMs{};
    std::uint64_t contentRevision{};
    std::vector<std::byte> encoded;
};
struct PresentationCandidateInput final {
    HostContext context;
    std::uint64_t mutationGeneration{}, candidateId{}, rendererInstance{};
    PresentationCandidateToken token;
    PresentationCapabilities capabilities;
    PresentationResourceManifest manifest;
    EffectivePresentationSettings settings;
    std::vector<PortableResourcePtr> resources;
    std::optional<AudioCandidateInput> audio;
    std::uint64_t playbackOwnerToken{};
};
CUEXIS_ABI_WARNING_POP
} // namespace cuexis::playback
