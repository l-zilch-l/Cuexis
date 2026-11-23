// T-17 session-layer correctness cases: Prepared owner lifetime, unload state, and
// Presentation/GameplayOnly validity plus independent commit eligibility.

#include "playback_candidate_internal.hpp"
#include "playback_gameplay_fixture.hpp"

#include <cuexis/content/content_provider.hpp>
#include <cuexis/playback/playback_session.hpp>
#include <cuexis/playback/playback_source.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using cuexis::playback::GameplayPrepareIntent;
using cuexis::playback::PlaybackMode;
using cuexis::playback::PlaybackSession;
using cuexis::playback::PlaybackSource;
using cuexis::playback::PreparedPlayback;
using cuexis::playback::SessionState;

[[nodiscard]] auto capabilities() -> cuexis::playback::PresentationCapabilities {
    return cuexis::playback::PresentationCapabilities{
        .opaquePass = true,
        .transparentPass = true,
        .linearTexture = true,
        .srgbTexture = true,
        .straightAlphaBlend = true,
        .backFaceCulling = true,
        .doubleSided = true,
        .debugPass = true,
        .maxResourceBytes = 64ULL * 1024ULL * 1024ULL,
        .maxTotalDecodedBytes = 512ULL * 1024ULL * 1024ULL,
        .maxTextureDimension = 8192,
        .maxMeshVertices = 1'048'576,
        .maxMeshIndices = 3'145'728,
    };
}

[[nodiscard]] auto gameplayOnlySource(bool withAudio)
    -> std::pair<PlaybackSource, std::vector<std::byte>> {
    playback_gameplay_fixture::TestContent content;
    std::vector<cuexis::playback::PlaybackAssetDescriptor> assets;
    std::vector<std::byte> audioBytes;
    std::shared_ptr<cuexis::content::IContentProvider> provider;
    if (withAudio) {
        content.chart.mainMusic = cuexis::chart::AssetId{"audio.main"};
        content.chart.resourceClosure.resources = {
            {cuexis::chart::AssetId{"audio.main"},
             cuexis::chart::CanonicalResourceUseKind::MainMusic}};
        audioBytes.assign(64, std::byte{0x5A});
        auto created =
            cuexis::content::MemoryContentProvider::create({{"main", "music.bin", audioBytes}});
        REQUIRE(created);
        provider = std::move(*created);
        assets.push_back(cuexis::playback::PlaybackAssetDescriptor{
            .id = "audio.main",
            .type = cuexis::playback::PlaybackAssetType::Audio,
            .rootId = "main",
            .logicalSource = "music.bin",
            .dependencies = {},
        });
    } else {
        auto created = cuexis::content::MemoryContentProvider::create({});
        REQUIRE(created);
        provider = std::move(*created);
    }
    auto source = PlaybackSource::fromGameplayPacked(
        "t17", "compiled/main.packed", content.packed(), content.config, std::move(assets),
        std::move(provider), GameplayPrepareIntent::GameplayOnly);
    REQUIRE(source);
    return {std::move(*source), std::move(audioBytes)};
}

} // namespace

TEST_CASE("rr_session_prepared_owner", "[playback][t17][candidate][prepared-owner]") {
    using Access = cuexis::playback::detail::CandidateMetadataAccess;

    // Empty and moved-from objects are invalid; accessors follow that answer.
    {
        PreparedPlayback empty;
        CHECK_FALSE(empty.valid());
        CHECK(empty.contentInfo() == nullptr);
        CHECK_FALSE(empty.semanticIdentity().has_value());
        CHECK_FALSE(empty.mainMusicSource().has_value());
    }

    // A live session owns the lease: materialization dereferences the owner exactly once.
    {
        auto session = std::make_unique<PlaybackSession>();
        auto prepared =
            session->prepareLoad(playback_gameplay_fixture::presentation(), PlaybackMode::ChartClock);
        REQUIRE(prepared);
        REQUIRE(prepared->valid());
        CHECK(Access::preparedOwnerDereferences(*prepared) == 0);
        auto candidate = prepared->materializePresentation(capabilities(), {}, 1, 1, 1);
        REQUIRE(candidate);
        CHECK(Access::preparedOwnerDereferences(*prepared) == 1);

        // Controlled pre-invalid internal state: expired lease must fail before the owner is
        // dereferenced.
        Access::expirePreparedOwnerLease(*prepared);
        const auto stale = prepared->materializePresentation(capabilities(), {}, 1, 1, 1);
        REQUIRE_FALSE(stale);
        CHECK(stale.error().code() == "host.rpa.context_stale");
        CHECK(Access::preparedOwnerDereferences(*prepared) == 1);
        REQUIRE(session->unload());
    }

    // Real session destruction: the weak lease expires; materialize reports the existing checked
    // error without dereferencing the dangling owner.
    {
        auto session = std::make_unique<PlaybackSession>();
        auto prepared =
            session->prepareLoad(playback_gameplay_fixture::presentation(), PlaybackMode::ChartClock);
        REQUIRE(prepared);
        REQUIRE(prepared->valid());
        session.reset();
        const auto dereferences = Access::preparedOwnerDereferences(*prepared);
        const auto stale = prepared->materializePresentation(capabilities(), {}, 1, 1, 1);
        REQUIRE_FALSE(stale);
        CHECK(stale.error().code() == "host.rpa.context_stale");
        CHECK(Access::preparedOwnerDereferences(*prepared) == dereferences);
        // Releasing the Prepared after the session is the reverse-order path and must be safe.
        *prepared = PreparedPlayback{};
    }

    // Reverse order still works: Prepared destroyed before its session.
    {
        auto session = std::make_unique<PlaybackSession>();
        auto prepared =
            session->prepareLoad(playback_gameplay_fixture::presentation(), PlaybackMode::ChartClock);
        REQUIRE(prepared);
        *prepared = PreparedPlayback{};
        REQUIRE(session->unload());
    }

    // Moved-from and off-thread objects are invalid; the moved-to object stays valid.
    {
        PlaybackSession session;
        auto prepared =
            session.prepareLoad(playback_gameplay_fixture::presentation(), PlaybackMode::ChartClock);
        REQUIRE(prepared);
        PreparedPlayback moved = std::move(*prepared);
        CHECK_FALSE(prepared->valid());
        CHECK(moved.valid());
        bool offThreadInvalid = false;
        std::thread worker{[&] { offThreadInvalid = !moved.valid(); }};
        worker.join();
        CHECK(offThreadInvalid);
        CHECK(moved.valid());
        CHECK(moved.contentInfo() != nullptr);
        REQUIRE(session.commit(std::move(moved)));
    }
}

TEST_CASE("rr_session_unload_state", "[playback][t17][candidate][unload]") {
    using Access = cuexis::playback::detail::CandidateMetadataAccess;

    PlaybackSession session;
    REQUIRE(session.load(playback_gameplay_fixture::presentation(), PlaybackMode::ChartClock));
    REQUIRE(session.update({.chartTimeMs = 10.0}));

    // Seed the candidate fields that unload must clear. hostCommitAuthorized models the
    // transient permission that lets the fence-owning owner unload.
    auto slots = Access::candidateSlots(session);
    slots.hostFence.emplace();
    slots.acceptedHostControl.emplace();
    slots.confirmedHostContext.emplace();
    slots.executionMapping.emplace();
    slots.tailTransition.emplace();
    slots.hostCommitAuthorized = true;
    const auto before = Access::candidateState(session);
    CHECK(before.hostFence);
    CHECK(before.acceptedHostControl);
    CHECK(before.confirmedHostContext);
    CHECK(before.executionMapping);
    CHECK(before.tailTransition);

    const auto ownersBefore = session.executionOwnerState();
    REQUIRE(ownersBefore);
    CHECK(ownersBefore->fence.has_value());
    CHECK(ownersBefore->acceptedControl.has_value());

    REQUIRE(session.unload());
    const auto after = Access::candidateState(session);
    CHECK_FALSE(after.hostFence);
    CHECK_FALSE(after.acceptedHostControl);
    CHECK_FALSE(after.confirmedHostContext);
    CHECK_FALSE(after.executionMapping);
    CHECK_FALSE(after.tailTransition);

    const auto ownersAfter = session.executionOwnerState();
    REQUIRE(ownersAfter);
    CHECK_FALSE(ownersAfter->fence.has_value());
    CHECK_FALSE(ownersAfter->acceptedControl.has_value());

    // A fresh commit in the same session must not observe the previous round's control state.
    auto prepared =
        session.prepareLoad(playback_gameplay_fixture::presentation(), PlaybackMode::ChartClock);
    REQUIRE(prepared);
    CHECK(prepared->valid());
    REQUIRE(session.commit(std::move(*prepared)));
    REQUIRE(session.state());
    CHECK(*session.state() == SessionState::Ready);
    CHECK_FALSE(Access::candidateState(session).hostFence);
}

TEST_CASE("rr_gameplay_only_accept", "[playback][t17][candidate][gameplay-only]") {
    // Presentation prepared content stays valid, readable and committable in the same session.
    {
        PlaybackSession session;
        auto prepared =
            session.prepareLoad(playback_gameplay_fixture::presentation(), PlaybackMode::ChartClock);
        REQUIRE(prepared);
        CHECK(prepared->valid());
        REQUIRE(prepared->contentInfo() != nullptr);
        CHECK(prepared->contentInfo()->chartId == "019b0000-0000-7abc-8def-000000000001");
        CHECK(prepared->semanticIdentity().has_value());
        CHECK(prepared->presentationManifest() != nullptr);
        CHECK_FALSE(prepared->mainMusicSource().has_value());
        REQUIRE(session.commit(std::move(*prepared)));
        CHECK(*session.state() == SessionState::Ready);
    }

    // GameplayOnly content is valid without a Runtime or World and answers content/identity
    // accessors from the actual prepared content.
    {
        auto [source, unusedBytes] = gameplayOnlySource(false);
        static_cast<void>(unusedBytes);
        PlaybackSession session{{.version = 1, .ids = {}}};
        auto prepared = session.prepareLoad(std::move(source), PlaybackMode::ChartClock);
        REQUIRE(prepared);
        CHECK(prepared->valid());
        REQUIRE(prepared->contentInfo() != nullptr);
        CHECK(prepared->contentInfo()->chartId == "019b0000-0000-7abc-8def-000000000001");
        CHECK(prepared->semanticIdentity().has_value());
        CHECK_FALSE(prepared->mainMusicSource().has_value());
        // Presentation ability is checked separately from content validity.
        CHECK(prepared->validatePresentation(capabilities(), {}).diagnostics.hasErrors());
        const auto noPresentation = prepared->materializePresentation(capabilities(), {}, 1, 1, 1);
        REQUIRE_FALSE(noPresentation);
        CHECK(noPresentation.error().code() == "host.rpa.device_failed");
        REQUIRE(session.commit(std::move(*prepared)));
        CHECK(*session.state() == SessionState::Ready);
        CHECK_FALSE(session.update({.chartTimeMs = 1.0}));
        CHECK_FALSE(session.extractFrame({640, 480}));
        REQUIRE(session.advanceGameplay({20}, {20}, {20, 0, 0}));
        CHECK_FALSE(session.gameplayAdvanceReceipt()->runtimeUpdated);
    }

    // A valid audio resource is available; missing audio is not a content-invalid result.
    {
        auto [source, audioBytes] = gameplayOnlySource(true);
        PlaybackSession session{{.version = 1, .ids = {}}};
        auto prepared = session.prepareLoad(std::move(source), PlaybackMode::HostClock);
        REQUIRE(prepared);
        CHECK(prepared->valid());
        REQUIRE(prepared->contentInfo() != nullptr);
        REQUIRE(prepared->contentInfo()->mainMusicAssetId.has_value());
        CHECK(*prepared->contentInfo()->mainMusicAssetId == "audio.main");
        auto music = prepared->mainMusicSource();
        REQUIRE(music.has_value());
        CHECK(music->assetId == "audio.main");
        CHECK(music->bytes.size() == audioBytes.size());
        REQUIRE(session.commit(std::move(*prepared)));
        CHECK(*session.state() == SessionState::Ready);
    }

    // Commit eligibility is checked independently from content validity.
    {
        auto [source, unusedBytes] = gameplayOnlySource(false);
        static_cast<void>(unusedBytes);
        PlaybackSession owner;
        auto prepared = owner.prepareLoad(std::move(source), PlaybackMode::ChartClock);
        REQUIRE(prepared);
        REQUIRE(prepared->valid());
        PlaybackSession other;
        const auto wrongSession = other.commit(std::move(*prepared));
        REQUIRE_FALSE(wrongSession);
        CHECK(wrongSession.error().code() == "playback.prepared.wrong_session");
        CHECK(prepared->valid());
        CHECK(*other.state() == SessionState::Empty);
        REQUIRE(owner.commit(std::move(*prepared)));
        CHECK(*owner.state() == SessionState::Ready);
    }

    // A still-valid prepared object can be rejected because its session generation moved on.
    {
        PlaybackSession session;
        REQUIRE(session.load(playback_gameplay_fixture::presentation(), PlaybackMode::ChartClock));
        const auto chartId = session.contentInfo();
        REQUIRE(chartId);
        auto prepared = session.prepareReload(playback_gameplay_fixture::presentation(),
                                              {.chartTimeMs = 1.0,
                                               .simulationDeltaTimeMs = 0.0,
                                               .timeDiscontinuityId = 0},
                                              cuexis::playback::ReloadPolicy::KeepChartTime);
        REQUIRE(prepared);
        REQUIRE(prepared->valid());
        REQUIRE(session.update({.chartTimeMs = 2.0}));
        const auto stale = session.commit(std::move(*prepared));
        REQUIRE_FALSE(stale);
        CHECK(stale.error().code() == "playback.prepared.stale");
        CHECK(prepared->valid());
        const auto unchanged = session.contentInfo();
        REQUIRE(unchanged);
        CHECK(unchanged->chartId == chartId->chartId);
        REQUIRE(session.update({.chartTimeMs = 3.0}));
    }
}
