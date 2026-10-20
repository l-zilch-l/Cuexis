#pragma once

// Private, owner-thread adapter. Prepared definitions never contain session state.
#include <cuexis/core/diagnostic.hpp>
#include <cuexis/judgement/publication.hpp>
#include <cuexis/playback/gameplay_candidate.hpp>

#include <functional>
#include <map>
#include <tuple>

namespace cuexis::playback::detail {
using ProjectionSourceKey =
    std::tuple<std::string, std::string, std::string, std::string, std::string,
               std::vector<std::pair<std::string, std::uint64_t>>, GameplayPhase, GameplayOutcome,
               GameplayTimingClass>;

struct PreparedProjection final {
    struct Row final {
        GameplayFactBinding binding;
        std::size_t member, declarationOrder;
        std::optional<std::size_t> group, targetLayout;
    };
    struct Group final {
        std::vector<std::string> names;
        std::vector<std::size_t> members;
    };
    std::vector<Row> rows;
    // Ordered full keys also compare emission paths; no hash or traversal-order identity.
    std::map<ProjectionSourceKey, std::vector<std::size_t>> sources;
    std::map<std::string, std::size_t, std::less<>> members, targets;
    std::vector<Group> groups;
    std::vector<std::optional<std::size_t>> memberGroups;
};

struct ProjectionEffect final {
    judgement::FactId factId;
    std::uint64_t ordinal;
    std::size_t row;
    bool published{false};
};
struct ProjectionState final {
    std::shared_ptr<const std::uint8_t> scope;
    std::uint64_t sourceScope{}, cursor{};
    // Active includes members waiting for their group. Expiry never erases seen members.
    std::vector<ProjectionEffect> active;
    using FutureKey = std::tuple<std::int64_t, std::uint64_t, std::size_t>;
    std::map<FutureKey, ProjectionEffect> future;
    std::vector<bool> seenMembers, rejectedGroups;
    std::vector<std::size_t> matchedGroupMembers;
    std::optional<GameplayPresentationTick> lastTick;
};
struct ProjectionCandidate final {
    ProjectionState state;
    std::map<std::string, bool, std::less<>> writes;
    core::Diagnostics diagnostics;
    std::size_t matchedRows{};
};

class PresentationProjectionAdapter final {
  public:
    // Binding support for both currently compiled kernel profiles: Tap/Head/Body/Tail,
    // Hit/Miss, Any/Early/Exact/Late, Any/All/GroupCommit. New profiles opt in here.
    static constexpr std::pair<std::string_view, std::string_view> capabilities[]{
        {"cuexis.gameplay.t4-k4.v1", "1"}, {"cuexis.gameplay.rpa.v1", "1"}};
    static auto prepare(std::span<const GameplayFactBinding>,
                        std::map<std::string, std::size_t, std::less<>> targets)
        -> core::Result<PreparedProjection>;
    using ReadFact = std::function<core::Result<judgement::detail::PublicationFact>(std::uint64_t)>;
    // Must recheck the current closure, even for a prepared target index.
    using HasTarget = std::function<core::Result<bool>(std::size_t, std::string_view)>;
    static auto project(const PreparedProjection&, const ProjectionState&,
                        const judgement::detail::PublicationMetadata&, GameplayPresentationTick,
                        const ReadFact&, const HasTarget&) -> core::Result<ProjectionCandidate>;
};
} // namespace cuexis::playback::detail
