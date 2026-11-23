// R1 source-table isolation: the ordinary build rejects the candidate prepare hook before any
// preparation result exists, while the candidate build reaches the candidate factory path.

#include "playback_session_prepare_internal.hpp"
#if !defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
#include "playback_candidate_hook_rejection.hpp"
#endif

#include <cuexis/playback/playback_source.hpp>
#if !defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE) && defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
// Calling the private rejection hook needs the complete candidate type. The ordinary Playback
// library no longer leaks that header; this test TU links the test-support archive explicitly,
// so the default Playback/Player closure stays free of the candidate include root.
#include <cuexis/cxc/cxc_candidate.hpp>
#endif

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>

TEST_CASE("rr_r1_playback_sources", "[playback][r1][isolation]") {
#if !defined(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    const auto missing = std::filesystem::path{"rr-r1-candidate-input-must-not-be-read"};

    const auto file = cuexis::playback::PlaybackSource::fromCxcFileEntry(missing, "entry.packed");
    REQUIRE_FALSE(file.has_value());
    CHECK(file.error().code() == "playback.candidate.disabled");

    const auto memory =
        cuexis::playback::PlaybackSource::fromCxcMemoryEntry({std::byte{0x00}}, "entry.packed");
    REQUIRE_FALSE(memory.has_value());
    CHECK(memory.error().code() == "playback.candidate.disabled");

    const auto project =
        cuexis::playback::PlaybackSource::fromFilesystemProjectEntry(missing, "entry.packed");
    REQUIRE_FALSE(project.has_value());
    CHECK(project.error().code() == "playback.candidate.disabled");

#if defined(CUEXIS_PLAYBACK_STATIC_DEFINE)
    // The private prepare hook is an implementation symbol. Static test builds can call it
    // directly; shared builds only exercise the public factory rejection above.
    cuexis::cxc::CandidateChart candidate;
    cuexis::playback::PlaybackPrepareOptions options;
    cuexis::chart::ChartLimits limits;
    cuexis::playback::prepare_detail::PrepareArtifact artifact;
    cuexis::core::Diagnostics diagnostics;
    const auto prepared = cuexis::playback::prepare_detail::prepareCandidateStage(
        candidate, false, options, false, limits, artifact, diagnostics);
    REQUIRE_FALSE(prepared.has_value());
    CHECK(prepared.error().code() == "playback.candidate.disabled");
    CHECK_FALSE(artifact.document.has_value());
    CHECK_FALSE(artifact.v4Artifact.has_value());
    CHECK_FALSE(artifact.runtimeSession);
#endif
#else
    // The candidate factories are compiled in: a missing package now fails while opening the
    // package instead of being short-circuited by the ordinary-build rejection.
    const auto missing = std::filesystem::path{"rr-r1-candidate-input-must-not-be-read"};
    const auto file = cuexis::playback::PlaybackSource::fromCxcFileEntry(missing, "entry.packed");
    REQUIRE_FALSE(file.has_value());
    CHECK(file.error().code() != "playback.candidate.disabled");
    CHECK(file.error().code() == "playback.source.cxc_invalid");
#endif
}
