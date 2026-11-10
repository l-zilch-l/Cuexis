#pragma once

// The packed chart writer builds the string and reference dictionaries, then the sections that
// use them. The sections unit calls the dictionary helpers, so they are named here.

#include <cuexis/chart/packed_chart_tables.hpp>

#include <cuexis/core/error.hpp>

#include "packed_gameplay_internal.hpp"
#include "packed_identity_internal.hpp"
#include "packed_limits_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::chart::packed {

using Section = gameplay_detail::Section;

enum class SectionRole : std::uint8_t { semantic, inspection };

struct Dictionaries final {
    std::vector<std::string> strings;
    std::map<std::string, std::uint32_t> stringIndex;
    std::map<std::pair<std::uint8_t, std::string>, std::uint32_t> refIndex;
};

struct EntityOrder final {
    std::vector<const CanonicalEntity*> entities;
    std::map<std::vector<std::byte>, std::uint32_t, identity_detail::ByteKeyLess> ordinals;
    std::map<const CanonicalEntity*, std::uint64_t> masks;
};

struct Archetype final {
    std::uint64_t mask{};
    std::optional<CanonicalTransform> transform;
    std::optional<CanonicalRenderable> renderable;
    std::optional<CanonicalCamera> camera;
};

struct IdentityScope final {
    std::array<std::uint8_t, 16> chartId{};
    std::string bindingId;
    std::string moduleId;
    std::string exportId;
};
struct IdentityPath final {
    std::vector<std::pair<std::string, bool>> steps;
};

[[nodiscard]] auto inspectImpl(std::span<const std::byte> bytes, PackedChartLimits limits,
                               bool gameplay) -> core::Result<PackedChartStatistics>;

[[nodiscard]] auto decodeImpl(std::span<const std::byte> bytes, PackedChartLimits limits,
                              bool gameplay, gameplay_detail::DecodedStatic* staticResult)
    -> core::Result<CanonicalSemanticChart>;

[[nodiscard]] auto fail(std::string code, std::string message) -> core::Error;

[[nodiscard]] auto narrowU32(std::uint64_t value) -> core::Result<std::uint32_t>;

[[nodiscard]] auto classifySection(std::string_view name, std::uint8_t flags, bool gameplay = false)
    -> core::Result<SectionRole>;

[[nodiscard]] auto identityText(std::span<const std::byte> bytes) -> std::string;

[[nodiscard]] auto writeUuid(ByteWriter& writer, std::string_view value) -> core::Result<void>;

void writeF32(ByteWriter& writer, float value);

void writeF64(ByteWriter& writer, double value);

[[nodiscard]] auto readF32(ByteReader& reader) -> core::Result<float>;

[[nodiscard]] auto readF64(ByteReader& reader) -> core::Result<double>;

[[nodiscard]] auto validUtf8(std::string_view text) -> bool;

[[nodiscard]] auto identityKey(const CanonicalEntityIdentity& identity)
    -> core::Result<std::vector<std::byte>>;

void collectStrings(Dictionaries& dict, const CanonicalSemanticChart& chart);

[[nodiscard]] auto stringRef(const Dictionaries& dict, const std::string& value)
    -> core::Result<std::uint32_t>;

[[nodiscard]] auto typedRef(const Dictionaries& dict, std::uint8_t kind, const std::string& value)
    -> core::Result<std::uint32_t>;

void writeTransform(ByteWriter& writer, const CanonicalTransform& value);

[[nodiscard]] auto writeRenderable(ByteWriter& writer, const CanonicalRenderable& value,
                                   const Dictionaries& dict) -> core::Result<void>;

[[nodiscard]] auto writeStringSection(const Dictionaries& dict) -> core::Result<Section>;

[[nodiscard]] auto writeReferenceSection(const Dictionaries& dict) -> core::Result<Section>;

[[nodiscard]] auto writeMetaSection(const CanonicalSemanticChart& chart, const Dictionaries& dict)
    -> core::Result<Section>;

[[nodiscard]] auto writeTimeSection(const ChartTiming& timing) -> core::Result<Section>;

[[nodiscard]] auto orderEntities(const CanonicalSemanticChart& chart) -> core::Result<EntityOrder>;

[[nodiscard]] auto scopeLess(const IdentityScope& left, const IdentityScope& right) -> bool;

[[nodiscard]] auto writeIdentitySection(const EntityOrder& order, const CanonicalSemanticChart&,
                                        const Dictionaries& dict) -> core::Result<Section>;

[[nodiscard]] auto makeArchetypes(const EntityOrder& order,
                                  std::map<std::uint64_t, std::uint32_t>& indices)
    -> std::vector<Archetype>;

[[nodiscard]] auto writeArchetypeSection(const std::vector<Archetype>& arches,
                                         const Dictionaries& dict) -> core::Result<Section>;

[[nodiscard]] auto writeEntitySection(const EntityOrder& order,
                                      const std::map<std::uint64_t, std::uint32_t>& arches)
    -> core::Result<Section>;

[[nodiscard]] auto writeComponentStream(const EntityOrder& order,
                                        const std::vector<Archetype>& arches,
                                        const std::map<std::uint64_t, std::uint32_t>& archIndices,
                                        const Dictionaries& dict, int kind)
    -> core::Result<Section>;

[[nodiscard]] auto writeConstraintAndRequirementSections(const EntityOrder& order,
                                                         const Dictionaries& dict)
    -> core::Result<std::pair<Section, Section>>;

} // namespace cuexis::chart::packed
