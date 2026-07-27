#include "fake_audio_transport.hpp"

#include <cuexis/audio/audio_clip.hpp>
#include <cuexis/audio/audio_transport.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {

[[nodiscard]] auto registerClip(cuexis::audio::AudioClipStore& store, float sample)
    -> cuexis::audio::AudioClipHandle {
    auto clip = cuexis::audio::AudioClip::create(48000, 1, std::vector<float>(96000, sample));
    REQUIRE(clip.has_value());
    auto handle = store.registerClip(std::move(*clip));
    REQUIRE(handle.has_value());
    return *handle;
}

} // namespace

TEST_CASE("Fake transport freezes and resumes the source clock across underrun episodes",
          "[audio][fake][recovery]") {
    cuexis::audio::AudioClipStore store;
    const auto handle = registerClip(store, 0.25F);
    cuexis::test_support::FakeAudioTransport transport{store};
    REQUIRE(transport.load(handle).has_value());
    REQUIRE(transport.play().has_value());
    REQUIRE(transport.advance(250.0).has_value());
    const auto beforeUnderrun = transport.snapshot();

    transport.beginUnderrun();
    transport.beginUnderrun();
    REQUIRE(transport.advance(500.0).has_value());
    CHECK(transport.snapshot().source.positionMs ==
          Catch::Approx(beforeUnderrun.source.positionMs));
    CHECK(transport.snapshot().source.discontinuityId == beforeUnderrun.source.discontinuityId);
    CHECK(transport.metrics().underrunCount == 1);

    transport.recoverUnderrun();
    REQUIRE(transport.advance(125.0).has_value());
    CHECK(transport.snapshot().source.positionMs == Catch::Approx(375.0));
    transport.beginUnderrun();
    CHECK(transport.metrics().underrunCount == 2);
}

TEST_CASE("Fake transport freezes once when an injected device failure enters Error",
          "[audio][fake][recovery]") {
    cuexis::audio::AudioClipStore store;
    const auto handle = registerClip(store, 0.25F);
    cuexis::test_support::FakeAudioTransport transport{store};
    REQUIRE(transport.load(handle).has_value());
    REQUIRE(transport.play().has_value());
    REQUIRE(transport.advance(200.0).has_value());
    transport.failNextService();

    REQUIRE_FALSE(transport.service().has_value());
    const auto failed = transport.snapshot();
    CHECK(failed.source.state == cuexis::audio::PlaybackState::Error);
    REQUIRE_FALSE(transport.service().has_value());
    CHECK(transport.snapshot().source.positionMs == Catch::Approx(failed.source.positionMs));
    CHECK(transport.snapshot().source.discontinuityId == failed.source.discontinuityId);
}

TEST_CASE("Fake replacement preparation failure preserves the active clip and clock",
          "[audio][fake][replacement]") {
    cuexis::audio::AudioClipStore store;
    const auto active = registerClip(store, 0.25F);
    const auto replacement = registerClip(store, 0.5F);
    cuexis::test_support::FakeAudioTransport transport{store};
    REQUIRE(transport.load(active).has_value());
    REQUIRE(transport.play().has_value());
    REQUIRE(transport.advance(300.0).has_value());
    const auto before = transport.snapshot();

    REQUIRE_FALSE(transport.prepareReplacement({}, before.source.positionMs).has_value());
    CHECK(transport.snapshot().source.positionMs == Catch::Approx(before.source.positionMs));
    CHECK(transport.snapshot().source.state == before.source.state);
    CHECK(transport.snapshot().source.discontinuityId == before.source.discontinuityId);

    REQUIRE(transport.prepareReplacement(replacement, before.source.positionMs).has_value());
    transport.failNextReplacementActivation();
    REQUIRE_FALSE(transport.activateReplacement().has_value());
    CHECK(transport.snapshot().source.state == cuexis::audio::PlaybackState::Error);
    CHECK(transport.snapshot().source.positionMs == Catch::Approx(before.source.positionMs));
}
