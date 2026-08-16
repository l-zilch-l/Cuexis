#include <cuexis/playback/frame_digest.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using cuexis::playback::FrameDigest;
using cuexis::playback::PlaybackContentInfo;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PlaybackSource;
using cuexis::playback::PreparedSemanticIdentity;
using cuexis::playback::RuntimeFrame;

constexpr std::array<RuntimeFrame, 4> sampleFrames{
    RuntimeFrame{.chartTimeMs = 0.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 0},
    RuntimeFrame{.chartTimeMs = 625.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 1},
    RuntimeFrame{.chartTimeMs = 250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 2},
    RuntimeFrame{.chartTimeMs = 1250.0, .simulationDeltaTimeMs = 0.0, .timeDiscontinuityId = 3},
};

constexpr std::uint64_t expectedStopDigest = 11596562486377158370ULL;
constexpr std::string_view expectedSemanticIdentity =
    "6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5";

struct Observation final {
    PreparedSemanticIdentity identity;
    std::array<std::uint64_t, sampleFrames.size()> digests{};
};

[[nodiscard]] auto sourceRoot() -> std::filesystem::path {
    return std::filesystem::path{CUEXIS_SOURCE_DIR};
}

[[nodiscard]] auto fixtureRoot() -> std::filesystem::path {
    return sourceRoot() / "tests" / "fixtures" / "chart_format_update";
}

[[nodiscard]] auto referenceProject() -> std::filesystem::path {
    return fixtureRoot() / "cfu_f_reference_project";
}

[[nodiscard]] auto referencePackage() -> std::filesystem::path {
    return fixtureRoot() / "golden" / "cfu_f_v4_reference.cxc";
}

[[nodiscard]] auto animatedPackage() -> std::filesystem::path {
    return fixtureRoot() / "golden" / "cxc_v1_v4_cxt.cxc";
}

[[nodiscard]] auto fail(std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

[[nodiscard]] auto fail(std::string_view operation, const cuexis::core::Error& error) -> int {
    std::cerr << operation << " failed: " << error.code() << ": " << error.message() << '\n';
    return 1;
}

[[nodiscard]] auto readBytes(const std::filesystem::path& path)
    -> std::optional<std::vector<std::byte>> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        std::cerr << "Could not open fixture: " << path << '\n';
        return std::nullopt;
    }
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes;
    bytes.reserve(raw.size());
    for (const unsigned char value : raw) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

[[nodiscard]] auto identityHex(const PreparedSemanticIdentity& identity) -> std::string {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : identity.sha256) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}

[[nodiscard]] auto contextValue(const cuexis::core::Diagnostic& diagnostic, std::string_view key)
    -> std::string_view {
    const auto found =
        std::ranges::find(diagnostic.context(), key, &cuexis::core::DiagnosticContext::key);
    return found == diagnostic.context().end() ? std::string_view{} : found->value;
}

[[nodiscard]] auto diagnosticSignature(const cuexis::core::Diagnostics& diagnostics)
    -> std::string {
    std::ostringstream output;
    bool first = true;
    for (const auto& diagnostic : diagnostics.items()) {
        if (!first) {
            output << '|';
        }
        first = false;
        output << diagnostic.code() << '@' << diagnostic.fieldPath();
        const auto capability = contextValue(diagnostic, "capability");
        if (!capability.empty()) {
            output << '#' << capability;
        }
    }
    return output.str();
}

[[nodiscard]] auto diagnosticFingerprint(const cuexis::core::Diagnostics& diagnostics)
    -> std::string {
    std::ostringstream output;
    output << diagnostics.size() << ':' << diagnostics.hasErrors() << ':'
           << diagnostics.hasWarnings() << ':' << diagnostics.limitReached();
    for (const auto& diagnostic : diagnostics.items()) {
        output << '|' << static_cast<int>(diagnostic.severity()) << ':' << diagnostic.code() << ':'
               << diagnostic.message() << ':' << diagnostic.fieldPath();
        for (const auto& context : diagnostic.context()) {
            output << ':' << context.key << '=' << context.value;
        }
    }
    return output.str();
}

[[nodiscard]] auto sameContent(const PlaybackContentInfo& left, const PlaybackContentInfo& right)
    -> bool {
    return left.chartId == right.chartId && left.chartFormatVersion == right.chartFormatVersion &&
           left.timingOffsetMs == right.timingOffsetMs && left.mode == right.mode &&
           left.mainMusicAssetId == right.mainMusicAssetId;
}

[[nodiscard]] auto observe(PlaybackSource&& source, std::string_view sourceName)
    -> std::optional<Observation> {
    PlaybackSession session;
    auto prepared = session.prepareLoad(std::move(source), PlaybackMode::ChartClock);
    if (!prepared) {
        static_cast<void>(fail(std::string{sourceName} + " prepare", prepared.error()));
        return std::nullopt;
    }
    const auto candidateIdentity = prepared->semanticIdentity();
    if (!candidateIdentity) {
        static_cast<void>(fail(std::string{sourceName} + " candidate identity is missing"));
        return std::nullopt;
    }
    const auto* manifest = prepared->presentationManifest();
    if (manifest == nullptr || manifest->entries.size() != 4U) {
        static_cast<void>(fail(std::string{sourceName} + " manifest is incomplete"));
        return std::nullopt;
    }
    for (const auto& entry : manifest->entries) {
        auto resource = prepared->acquirePresentationResource(entry.reference);
        if (!resource || !*resource || (*resource)->reference != entry.reference) {
            static_cast<void>(fail(std::string{sourceName} + " resource acquisition failed"));
            return std::nullopt;
        }
    }
    auto committed = session.commit(std::move(*prepared));
    if (!committed) {
        static_cast<void>(fail(std::string{sourceName} + " commit", committed.error()));
        return std::nullopt;
    }
    auto activeIdentity = session.semanticIdentity();
    if (!activeIdentity || *activeIdentity != *candidateIdentity) {
        static_cast<void>(fail(std::string{sourceName} + " active identity mismatch"));
        return std::nullopt;
    }

    Observation observation{.identity = *activeIdentity};
    for (std::size_t index = 0; index < sampleFrames.size(); ++index) {
        auto updated = session.update(sampleFrames[index]);
        if (!updated) {
            static_cast<void>(fail(std::string{sourceName} + " update", updated.error()));
            return std::nullopt;
        }
        auto snapshot = session.extractFrame({.width = 1280, .height = 720});
        if (!snapshot || snapshot->objects.size() != 2U) {
            static_cast<void>(fail(std::string{sourceName} + " frame extraction failed"));
            return std::nullopt;
        }
        auto digest = cuexis::playback::computeFrameDigest(sampleFrames[index], *snapshot);
        if (!digest || digest->algorithmVersion != 3U) {
            static_cast<void>(fail(std::string{sourceName} + " FrameDigest failed"));
            return std::nullopt;
        }
        observation.digests[index] = digest->value;
    }
    return observation;
}

[[nodiscard]] auto frameDigest(PlaybackSession& session, const RuntimeFrame& frame)
    -> std::optional<FrameDigest> {
    auto snapshot = session.extractFrame({.width = 1280, .height = 720});
    if (!snapshot) {
        static_cast<void>(fail("failure-path frame extraction", snapshot.error()));
        return std::nullopt;
    }
    auto digest = cuexis::playback::computeFrameDigest(frame, *snapshot);
    if (!digest) {
        static_cast<void>(fail("failure-path FrameDigest", digest.error()));
        return std::nullopt;
    }
    return *digest;
}

[[nodiscard]] auto verifyFailureAtomicity(std::string& observedSignature) -> bool {
    auto source = PlaybackSource::fromCxcFile(referencePackage());
    if (!source) {
        static_cast<void>(fail("reference CXC source", source.error()));
        return false;
    }
    PlaybackSession session;
    auto loaded = session.load(std::move(*source), PlaybackMode::ChartClock);
    if (!loaded) {
        static_cast<void>(fail("reference CXC load", loaded.error()));
        return false;
    }
    const auto activeFrame = sampleFrames[1];
    auto updated = session.update(activeFrame);
    if (!updated) {
        static_cast<void>(fail("reference CXC update", updated.error()));
        return false;
    }
    const auto identityBefore = session.semanticIdentity();
    const auto contentBefore = session.contentInfo();
    const auto diagnosticsBefore = session.diagnostics();
    const auto digestBefore = frameDigest(session, activeFrame);
    if (!identityBefore || !contentBefore || !diagnosticsBefore || !digestBefore) {
        static_cast<void>(fail("reference CXC active state is incomplete"));
        return false;
    }

    const auto animatedBytes = readBytes(animatedPackage());
    if (!animatedBytes) {
        return false;
    }
    auto animated = PlaybackSource::fromCxcMemory(*animatedBytes);
    if (!animated) {
        static_cast<void>(fail("animated CXC source", animated.error()));
        return false;
    }
    const auto rejected = session.reload(std::move(*animated), activeFrame,
                                         cuexis::playback::ReloadPolicy::KeepChartTime);
    if (rejected || rejected.error().code() != "playback.capability.preflight_failed") {
        static_cast<void>(fail("animated CXC did not fail capability preflight"));
        return false;
    }
    const auto operationDiagnostics = session.lastOperationDiagnostics();
    if (!operationDiagnostics || operationDiagnostics->empty()) {
        static_cast<void>(fail("animated CXC diagnostics are missing"));
        return false;
    }
    const auto signature = diagnosticSignature(*operationDiagnostics);
    constexpr std::string_view expectedSignature =
        "playback.capability.unsupported@$/animationClips#cuexis.animation.clip.v1|"
        "playback.capability.unsupported@$/objects#cuexis.animation.layers.v1";
    if (signature != expectedSignature) {
        std::cerr << "Unexpected animated CXC diagnostic signature: " << signature << '\n';
        return false;
    }
    observedSignature = signature;

    const auto identityAfterCapability = session.semanticIdentity();
    const auto contentAfterCapability = session.contentInfo();
    const auto diagnosticsAfterCapability = session.diagnostics();
    const auto digestAfterCapability = frameDigest(session, activeFrame);
    if (!identityAfterCapability || *identityAfterCapability != *identityBefore ||
        !contentAfterCapability || !sameContent(*contentAfterCapability, *contentBefore) ||
        !diagnosticsAfterCapability ||
        diagnosticFingerprint(*diagnosticsAfterCapability) !=
            diagnosticFingerprint(*diagnosticsBefore) ||
        !digestAfterCapability ||
        digestAfterCapability->algorithmVersion != digestBefore->algorithmVersion ||
        digestAfterCapability->value != digestBefore->value) {
        static_cast<void>(fail("capability failure replaced active Playback state"));
        return false;
    }

    auto invalidTargetSource = PlaybackSource::fromCxcFile(referencePackage());
    if (!invalidTargetSource) {
        static_cast<void>(fail("invalid-target CXC source", invalidTargetSource.error()));
        return false;
    }
    const auto invalidTarget =
        session.reload(std::move(*invalidTargetSource),
                       {.chartTimeMs = std::numeric_limits<double>::quiet_NaN(),
                        .simulationDeltaTimeMs = 0.0,
                        .timeDiscontinuityId = activeFrame.timeDiscontinuityId + 1U},
                       cuexis::playback::ReloadPolicy::KeepChartTime);
    if (invalidTarget || invalidTarget.error().code() != "playback.session.reload_sample_failed") {
        static_cast<void>(fail("invalid reload target did not fail deterministically"));
        return false;
    }
    const auto targetDiagnostics = session.lastOperationDiagnostics();
    if (!targetDiagnostics || targetDiagnostics->size() != 1U ||
        targetDiagnostics->items().front().code() != "runtime.frame.chart_time_non_finite") {
        static_cast<void>(fail("invalid reload target diagnostic mismatch"));
        return false;
    }
    const auto identityAfterTarget = session.semanticIdentity();
    const auto contentAfterTarget = session.contentInfo();
    const auto diagnosticsAfterTarget = session.diagnostics();
    const auto digestAfterTarget = frameDigest(session, activeFrame);
    if (!identityAfterTarget || *identityAfterTarget != *identityBefore || !contentAfterTarget ||
        !sameContent(*contentAfterTarget, *contentBefore) || !diagnosticsAfterTarget ||
        diagnosticFingerprint(*diagnosticsAfterTarget) !=
            diagnosticFingerprint(*diagnosticsBefore) ||
        !digestAfterTarget ||
        digestAfterTarget->algorithmVersion != digestBefore->algorithmVersion ||
        digestAfterTarget->value != digestBefore->value) {
        static_cast<void>(fail("invalid target failure replaced active Playback state"));
        return false;
    }
    return true;
}

} // namespace

int main() {
    const auto packageBytes = readBytes(referencePackage());
    if (!packageBytes) {
        return 1;
    }
    auto filesystemSource = PlaybackSource::fromFilesystemProject(referenceProject());
    auto fileSource = PlaybackSource::fromCxcFile(referencePackage());
    auto memorySource = PlaybackSource::fromCxcMemory(*packageBytes);
    if (!filesystemSource) {
        return fail("filesystem reference source", filesystemSource.error());
    }
    if (!fileSource) {
        return fail("file reference source", fileSource.error());
    }
    if (!memorySource) {
        return fail("memory reference source", memorySource.error());
    }

    const auto filesystem = observe(std::move(*filesystemSource), "filesystem");
    const auto file = observe(std::move(*fileSource), "CXC file");
    const auto memory = observe(std::move(*memorySource), "CXC memory");
    if (!filesystem || !file || !memory) {
        return 1;
    }
    if (filesystem->identity != file->identity || filesystem->identity != memory->identity ||
        filesystem->digests != file->digests || filesystem->digests != memory->digests) {
        return fail("Reference sources produced different semantic observations");
    }
    if (file->digests[1] != expectedStopDigest) {
        std::cerr << "Unexpected stop FrameDigest: " << file->digests[1] << '\n';
        return 1;
    }
    if (identityHex(file->identity) != expectedSemanticIdentity) {
        std::cerr << "Unexpected semantic identity: " << identityHex(file->identity) << '\n';
        return 1;
    }
    std::string diagnosticOrder;
    if (!verifyFailureAtomicity(diagnosticOrder)) {
        return 1;
    }

    std::cout << "CFU-F1 headless reference passed identity=" << identityHex(file->identity)
              << " stop_digest=" << file->digests[1] << " diagnostics=" << diagnosticOrder << '\n';
    return 0;
}
