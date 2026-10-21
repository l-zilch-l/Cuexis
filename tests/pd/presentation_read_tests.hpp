#pragma once

#include <cuexis/runtime/presentation_read.hpp>
#include <future>
#include <limits>
#include <type_traits>

TEST_CASE("PD3b initial neutral read is const and scoped without advancing state",
          "[pd][presentation-read]") {
    Fixture f{false, false};
    using Access = runtime::detail::PresentationAccess;
    const auto before = *Access::state(f.session);
    CHECK(before.stamp.version == 2); // content commit and override acquisition, no update
    CHECK(before.updateOrdinal == 0);
    CHECK_FALSE(before.frame);
    REQUIRE(std::as_const(f.session).withWorld(
        []<typename T>(T& w)
            requires std::is_const_v<T>
        {
            static_assert(std::is_const_v<std::remove_reference_t<decltype(w)>>);
            return w.withRegistry(
                []<typename R>(R& registry)
                    requires std::is_const_v<R>
                { static_assert(std::is_const_v<std::remove_reference_t<decltype(registry)>>); });
        }));
    std::optional<runtime::detail::PresentationRead> escaped;
    auto read = Access::read(
        f.session, before.stamp,
        [&](const runtime::detail::PresentationRead& view) -> core::Result<void> {
            escaped = view;
            auto child = view.readObject(0), parent = view.readObject(1);
            REQUIRE(child);
            REQUIRE(parent);
            CHECK(child->id == "child");
            CHECK(child->hasRenderable);
            CHECK(child->hasAppearance);
            CHECK(child->visible);
            CHECK(child->opacity == 1);
            CHECK(parent->id == "parent");
            CHECK_FALSE(parent->hasAppearance);
            auto camera = view.readCamera();
            REQUIRE(camera);
            CHECK(camera->selected);
            CHECK(camera->present);
            CHECK(camera->fovY == 60);
            CHECK_FALSE(view.readObject(2));
            CHECK_FALSE(f.session.update({0, 0, 0}));
            CHECK_FALSE(f.session.unload());
            CHECK_FALSE(f.session.reload(chartData()).reloaded);
            CHECK_FALSE(f.session.withWorld([](world::World&) {}));
            CHECK_FALSE(Access::read(f.session, before.stamp,
                                     [](const auto&) -> core::Result<void> { return {}; }));
            auto wrongOwner =
                std::async(std::launch::async, [&] { return view.readObject(0).has_value(); });
            CHECK_FALSE(wrongOwner.get());
            return {};
        });
    REQUIRE(read);
    REQUIRE(escaped);
    CHECK_FALSE(escaped->state());
    CHECK_FALSE(escaped->readObject(0));
    CHECK_FALSE(escaped->readCamera());
    REQUIRE(Access::read(f.session, before.stamp, [&](const auto&) -> core::Result<void> {
        CHECK_FALSE(escaped->readObject(0)); // a later window cannot reactivate an old view
        return {};
    }));
    const auto after = *Access::state(f.session);
    CHECK(after.stamp == before.stamp);
    CHECK(after.mutationEpoch == before.mutationEpoch);
    CHECK(after.updateOrdinal == before.updateOrdinal);
    pd_allocations::begin();
    auto warmed = Access::read(f.session, before.stamp, [](const auto& view) -> core::Result<void> {
        auto value = view.readObject(0);
        if (!value)
            return core::unexpected(std::move(value.error()));
        return {};
    });
    const auto allocations = pd_allocations::end();
    REQUIRE(warmed);
    CHECK(allocations == 0);
}

TEST_CASE("PD3b mutable callback publishes actual state on success error and exception",
          "[pd][presentation-read]") {
    using Access = runtime::detail::PresentationAccess;
    for (int outcome = 0; outcome < 3; ++outcome) {
        Fixture f;
        const auto before = *Access::state(f.session);
        auto changed = f.session.withWorld([&](world::World& w) -> core::Result<void> {
            auto result = w.withRegistry([&](entt::registry& registry) {
                registry.get<render::AppearanceComponent>(f.child).visible = false;
                registry.remove<render::CameraComponent>(f.parent);
            });
            if (!result)
                return result;
            if (outcome == 1)
                return core::unexpected(core::Error{"test.callback", "Entered callback failed"});
            if (outcome == 2)
                throw std::runtime_error{"Entered callback threw"};
            return {};
        });
        CHECK(changed.has_value() == (outcome == 0));
        const auto after = *Access::state(f.session);
        CHECK(after.stamp.version == before.stamp.version + 1);
        CHECK(after.updateOrdinal == before.updateOrdinal);
        CHECK(after.source == internal::presentation::Source::Mutation);
        CHECK_FALSE(after.frame);
        bool invoked{};
        CHECK_FALSE(Access::read(f.session, before.stamp, [&](const auto&) -> core::Result<void> {
            invoked = true;
            return {};
        }));
        CHECK_FALSE(invoked);
        REQUIRE(Access::read(f.session, after.stamp, [](const auto& view) -> core::Result<void> {
            CHECK_FALSE(view.readObject(0)->visible);
            CHECK(view.readCamera()->selected);
            CHECK_FALSE(view.readCamera()->present); // no repair or camera reselection
            return {};
        }));
    }
}

TEST_CASE("PD read refreshes component storage and presence for each window",
          "[pd][presentation-read]") {
    Fixture f{false, false};
    using Access = runtime::detail::PresentationAccess;
    REQUIRE(f.session.withWorld([&](world::World& w) {
        return w.withRegistry([&](entt::registry& registry) {
            registry.remove<world::WorldTransformComponent>(f.child);
            registry.remove<render::AppearanceComponent>(f.child);
            registry.remove<render::RenderableComponent>(f.child);
        });
    }));
    REQUIRE(Access::read(f.session, Access::state(f.session)->stamp,
                         [](const auto& view) -> core::Result<void> {
                             const auto child = view.readObject(0);
                             REQUIRE(child);
                             CHECK_FALSE(child->hasTransform);
                             CHECK_FALSE(child->hasAppearance);
                             CHECK_FALSE(child->hasRenderable);
                             return {};
                         }));
    REQUIRE(f.session.withWorld([&](world::World& w) {
        return w.withRegistry([&](entt::registry& registry) {
            core::Mat4 matrix;
            matrix.values[12] = 7;
            registry.emplace<world::WorldTransformComponent>(f.child, matrix);
            auto& appearance = registry.emplace<render::AppearanceComponent>(f.child);
            appearance.opacity = 0.25;
        });
    }));
    REQUIRE(Access::read(f.session, Access::state(f.session)->stamp,
                         [](const auto& view) -> core::Result<void> {
                             const auto child = view.readObject(0);
                             REQUIRE(child);
                             CHECK(child->hasTransform);
                             CHECK(child->matrix.values[12] == 7);
                             CHECK(child->hasAppearance);
                             CHECK(child->opacity == 0.25);
                             CHECK_FALSE(child->hasRenderable);
                             return {};
                         }));
}

TEST_CASE("PD3b revision covers Runtime writers and rejects old content and failed updates",
          "[pd][presentation-read]") {
    using Access = runtime::detail::PresentationAccess;
    assets::ResourceManager resources{database()};
    runtime::RuntimeSession session{resources};
    auto prepared = session.prepare(chartData());
    REQUIRE(prepared.hasValue());
    REQUIRE(session.commit(std::move(*prepared.prepared)));
    auto current = *Access::state(session);
    CHECK(current.stamp.version == 1);
    CHECK(current.stamp.layout == 1);
    CHECK(current.source == internal::presentation::Source::Prepared);
    CHECK_FALSE(current.frame);
    REQUIRE(session.update({0, 0, 0}));
    auto updated = *Access::state(session);
    CHECK(updated.stamp.version == current.stamp.version + 1);
    CHECK(updated.updateOrdinal == 1);
    REQUIRE(updated.frame);
    REQUIRE_FALSE(session.update({std::numeric_limits<double>::max(), 0, 0}));
    CHECK(Access::state(session)->stamp == updated.stamp);
    const std::array writes{
        runtime::PropertyOverrideWrite{{"child"}, world::PropertyId::RenderVisible, false}};
    auto token =
        session.acquireOverride(world::OverrideKind::Host, "host", 0,
                                world::propertyBit(world::PropertyId::RenderVisible), {}, writes);
    REQUIRE(token);
    CHECK(Access::state(session)->stamp.version == updated.stamp.version + 1);
    CHECK(Access::state(session)->updateOrdinal == 1);
    REQUIRE(session.releaseOverride(*token));
    const auto released = *Access::state(session);
    CHECK(released.stamp.version == updated.stamp.version + 2);
    CHECK_FALSE(session.releaseOverride(*token));
    CHECK(Access::state(session)->stamp == released.stamp);
    REQUIRE(session.applyBaseProperty({"child"}, world::PropertyId::MaterialOpacity, 0.25));
    CHECK(Access::state(session)->stamp.version == released.stamp.version + 1);
    CHECK(Access::state(session)->updateOrdinal == 1);
    REQUIRE(
        session.reload(chartData(), {250, 0, 1}, runtime::ReloadPolicy::KeepChartTime).reloaded);
    current = *Access::state(session);
    CHECK(current.source == internal::presentation::Source::Restore);
    CHECK(current.stamp.layout == 2);
    REQUIRE(current.frame);
    CHECK(current.frame->chartTimeMs == 250);
    bool invoked{};
    auto wrongLayout = current.stamp;
    --wrongLayout.layout;
    CHECK_FALSE(Access::read(session, wrongLayout, [&](const auto&) -> core::Result<void> {
        invoked = true;
        return {};
    }));
    Fixture other;
    CHECK_FALSE(Access::read(other.session, current.stamp, [&](const auto&) -> core::Result<void> {
        invoked = true;
        return {};
    }));
    CHECK_FALSE(invoked);
    REQUIRE(session.unload());
    CHECK_FALSE(Access::state(session));
}

TEST_CASE("PD3b revision reservation rejects exhaustion before writing without wrapping",
          "[pd][presentation-read]") {
    using namespace internal::presentation;
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (int counter = 0; counter < 4; ++counter) {
        RevisionClock clock;
        if (counter == 0)
            clock.version = maximum;
        if (counter == 1)
            clock.layout = maximum;
        if (counter == 2)
            clock.epoch = maximum;
        if (counter == 3)
            clock.updates = maximum;
        CHECK_FALSE(Mutation::reserve(clock, true, true));
        CHECK(clock.version == (counter == 0 ? maximum : 0));
        CHECK(clock.layout == (counter == 1 ? maximum : 0));
        CHECK(clock.epoch == (counter == 2 ? maximum : 0));
        CHECK(clock.updates == (counter == 3 ? maximum : 0));
    }
    RevisionClock clock{maximum - 1, maximum - 1, maximum - 1, maximum - 1};
    auto reserved = Mutation::reserve(clock, true, true);
    REQUIRE(reserved);
    reserved->publish();
    CHECK(clock.version == maximum);
    CHECK(clock.layout == maximum);
    CHECK(clock.epoch == maximum);
    CHECK(clock.updates == maximum);
    CHECK_FALSE(Mutation::reserve(clock));
}
