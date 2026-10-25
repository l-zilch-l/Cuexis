// Metered ordering/cache preparation. All semantic fields are emitted by shared writers.
namespace {
struct StaticStreamFailure final {
    core::Error error;
};
template <typename T> auto staticValue(core::Result<T> value) -> T {
    if (!value)
        throw StaticStreamFailure{std::move(value.error())};
    return std::move(*value);
}
void staticValue(core::Result<void> value) {
    if (!value)
        throw StaticStreamFailure{std::move(value.error())};
}
struct IdentityCursor final {
    std::span<std::byte> bytes;
    std::size_t position{};
};
void copyIdentityBytes(void* state, std::span<const std::byte> bytes) {
    auto& cursor = *static_cast<IdentityCursor*>(state);
    if (bytes.size() > cursor.bytes.size() - cursor.position)
        throw StaticStreamFailure{fail("packed.identity.size", "Identity byte count disagrees")};
    std::copy(bytes.begin(), bytes.end(), cursor.bytes.begin() + cursor.position);
    cursor.position += bytes.size();
}
void identityInto(const CanonicalEntityIdentity& identity, std::span<std::byte> destination) {
    IdentityCursor cursor{destination};
    staticValue(identity_detail::streamCanonicalIdentity(identity, {&cursor, copyIdentityBytes}));
    if (cursor.position != destination.size())
        throw StaticStreamFailure{fail("packed.identity.size", "Identity byte count disagrees")};
}
auto keyLess(std::span<const std::byte> left, std::span<const std::byte> right) -> bool {
    return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end());
}
struct PrefixEntity final {
    std::span<const std::byte> key;
    const CanonicalEntity* entity;
    std::uint32_t parent{};
    std::uint8_t flags{};
};
static_assert(sizeof(PrefixEntity) <= 32);
} // namespace

auto gameplay_detail::streamStaticPreimage(const CanonicalSemanticChart& chart,
                                           std::span<const CanonicalEntityIdentity> gameplayOwners,
                                           SemanticByteSink sink, std::uint32_t candidateRevision)
    -> core::Result<void> try {
    Preimage out{{}, &sink};
    staticValue(writeStaticHeader(out, chart, true, candidateRevision, sink.scratch));
    std::uint64_t keyBytes = 0;
    for (const auto& entity : chart.entities) {
        const auto size = staticValue(identity_detail::canonicalIdentitySize(entity.identity));
        if (size > UINT32_MAX - keyBytes)
            return core::unexpected(fail("packed.field.wire_range", "Identity table exceeds u32"));
        keyBytes += size;
        if (const auto* generated = std::get_if<GeneratedEntityIdentity>(&entity.identity)) {
            if (generated->chartId.value != chart.chartId.value)
                return core::unexpected(
                    fail("packed.identity.scope_chart", "Generated identity uses another chart"));
            if (generated->path.empty() || generated->path.back().iterationIndexPlusOne != 0)
                return core::unexpected(
                    fail("packed.identity.generated_path", "Invalid generated identity path"));
        }
    }
    std::pmr::vector<std::byte> keys{sink.scratch};
    keys.resize(static_cast<std::size_t>(keyBytes));
    std::pmr::vector<PrefixEntity> entities{sink.scratch};
    entities.reserve(chart.entities.size());
    std::size_t offset = 0;
    for (const auto& entity : chart.entities) {
        const auto size = staticValue(identity_detail::canonicalIdentitySize(entity.identity));
        const auto key = std::span{keys}.subspan(offset, size);
        identityInto(entity.identity, key);
        entities.push_back({key, &entity, static_cast<std::uint32_t>(chart.entities.size()), 0});
        offset += size;
    }
    std::sort(entities.begin(), entities.end(),
              [](const auto& l, const auto& r) { return keyLess(l.key, r.key); });
    for (std::size_t i = 1; i < entities.size(); ++i)
        if (!keyLess(entities[i - 1].key, entities[i].key))
            return core::unexpected(
                fail("packed.identity.duplicate_identity", "Duplicate canonical entity identity"));
    {
        std::pmr::vector<std::byte> lookup{sink.scratch};
        const auto find = [&](const CanonicalEntityIdentity& identity) {
            lookup.resize(staticValue(identity_detail::canonicalIdentitySize(identity)));
            identityInto(identity, lookup);
            const auto found = std::lower_bound(
                entities.begin(), entities.end(), std::span<const std::byte>{lookup},
                [](const auto& row, auto key) { return keyLess(row.key, key); });
            return found == entities.end() || keyLess(lookup, found->key)
                       ? entities.size()
                       : static_cast<std::size_t>(found - entities.begin());
        };
        for (const auto& owner : gameplayOwners) {
            const auto i = find(owner);
            if (i == entities.size())
                return core::unexpected(
                    fail("packed.requirements.component", "Missing Gameplay owner entity"));
            if (entities[i].flags & 1)
                return core::unexpected(
                    fail("packed.requirements.owner_duplicate", "Duplicate Gameplay owner entity"));
            entities[i].flags |= 1;
        }
        for (auto& row : entities)
            if (row.entity->parent) {
                const auto i = find(*row.entity->parent);
                if (i == entities.size())
                    return core::unexpected(
                        fail("packed.identity.parent_missing", "Entity parent is missing"));
                row.parent = static_cast<std::uint32_t>(i);
            }
    }
    for (std::size_t i = 0; i < entities.size(); ++i) {
        auto current = i;
        while (current < entities.size() && !(entities[current].flags & 4)) {
            if (entities[current].flags & 2)
                return core::unexpected(
                    fail("packed.identity.parent_cycle", "Entity parent cycle"));
            entities[current].flags |= 2;
            current = entities[current].parent;
        }
        current = i;
        while (current < entities.size() && (entities[current].flags & 2)) {
            entities[current].flags =
                static_cast<std::uint8_t>((entities[current].flags & ~2U) | 4U);
            current = entities[current].parent;
        }
    }
    writeU32(out, static_cast<std::uint32_t>(entities.size()));
    for (const auto& row : entities) {
        const auto& entity = *row.entity;
        const auto parent =
            row.parent < entities.size() ? entities[row.parent].key : std::span<const std::byte>{};
        staticValue(writeEntity(out, entity, row.key, parent, true, (row.flags & 1) != 0));
    }
    writeStaticDefinitionTail(out);
    return {};
} catch (StaticStreamFailure& failure) {
    return core::unexpected(std::move(failure.error));
}
