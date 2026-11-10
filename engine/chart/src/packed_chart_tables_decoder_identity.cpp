#include "packed_chart_tables_decoder_internal.hpp"

#include <cuexis/chart/uuid.hpp>
#include <cuexis/core/error.hpp>

#include "packed_chart_tables_units_internal.hpp"
#include "packed_gameplay_internal.hpp"
#include "packed_identity_internal.hpp"
#include "packed_limits_internal.hpp"
#include "packed_profile_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::chart::packed::decoder_detail {

auto Decoder::readIdentity() -> core::Result<void> {
    entities.resize(entityCount);
    const auto* idnSection = section("IDN0");
    ByteReader idn(idnSection->data);
    auto scopeCount = idn.readU32();
    if (!scopeCount)
        return core::unexpected(fail("packed.identity.invalid", "IDN0 scope count is invalid"));
    struct DecodedScope final {
        std::array<std::uint8_t, 16> chartIdBytes{};
        std::string chartId;
        std::string bindingId;
        std::string moduleId;
        std::string exportId;
    };
    // Spec 6.5: scope tables are deduplicated and ordered by their decoded tuple.
    const auto scopeLess = [](const DecodedScope& left, const DecodedScope& right) {
        if (left.chartIdBytes != right.chartIdBytes)
            return left.chartIdBytes < right.chartIdBytes;
        if (left.bindingId != right.bindingId)
            return left.bindingId < right.bindingId;
        if (left.moduleId != right.moduleId)
            return left.moduleId < right.moduleId;
        return left.exportId < right.exportId;
    };
    std::vector<DecodedScope> scopes;
    // Spec 3.2: one scope row occupies 16 chartId bytes plus three one-byte string indices at a
    // minimum, and the reservation is capped so a legal but large table grows with the rows it
    // actually contains instead of with the declared count.
    if (static_cast<std::uint64_t>(*scopeCount) > idn.remaining() / 19U)
        return core::unexpected(
            fail("packed.identity.invalid", "IDN0 scope count exceeds the section payload"));
    scopes.reserve(std::min<std::size_t>(*scopeCount, 4096U));
    for (std::uint32_t i = 0; i < *scopeCount; ++i) {
        auto uuid = idn.readBytes(16);
        auto bind = idn.readUnsignedLeb128(stringCount);
        auto module = idn.readUnsignedLeb128(stringCount);
        auto exportId = idn.readUnsignedLeb128(stringCount);
        if (!uuid || !bind || !module || !exportId)
            return core::unexpected(fail("packed.identity.invalid", "IDN0 scope is invalid"));
        auto bindValue = getString(*bind);
        auto moduleValue = getString(*module);
        auto exportValue = getString(*exportId);
        if (!bindValue || !moduleValue || !exportValue)
            return core::unexpected(fail("packed.identity.string", "IDN0 scope string is invalid"));
        auto scopeChartIdBytes = std::array<std::uint8_t, 16>{};
        for (std::size_t byteIndex = 0; byteIndex < scopeChartIdBytes.size(); ++byteIndex)
            scopeChartIdBytes[byteIndex] = std::to_integer<std::uint8_t>((*uuid)[byteIndex]);
        scopes.push_back(DecodedScope{scopeChartIdBytes, identityText(*uuid), std::move(*bindValue),
                                      std::move(*moduleValue), std::move(*exportValue)});
        if (scopes.size() > 1U && !scopeLess(scopes[scopes.size() - 2U], scopes.back()))
            return core::unexpected(
                fail("packed.identity.order",
                     "IDN0 scopes must be unique and strictly ascending by decoded tuple"));
        // Spec 6.5: every generated identity scope must use the META chartId.
        if (scopes.back().chartId != chart.chartId.value)
            return core::unexpected(
                fail("packed.identity.scope_chart", "IDN0 scope chartId must match META"));
    }
    auto pathCount = idn.readU32();
    if (!pathCount)
        return core::unexpected(fail("packed.identity.invalid", "IDN0 path count is invalid"));
    std::vector<std::vector<std::pair<std::string, bool>>> paths;
    if (static_cast<std::uint64_t>(*pathCount) > idn.remaining())
        return core::unexpected(
            fail("packed.identity.invalid", "IDN0 path count exceeds the section payload"));
    paths.reserve(std::min<std::size_t>(*pathCount, 1024U));
    for (std::uint32_t i = 0; i < *pathCount; ++i) {
        auto steps = idn.readUnsignedLeb128();
        if (!steps)
            return core::unexpected(fail("packed.identity.invalid", "IDN0 path is invalid"));
        // Spec 3.2: one path step occupies a string index and an indexed flag, so the declared
        // step count is bounded by the remaining payload before the step vector is reserved.
        if (*steps > idn.remaining() / 2U)
            return core::unexpected(
                fail("packed.identity.invalid", "IDN0 step count exceeds the section payload"));
        auto path = std::vector<std::pair<std::string, bool>>{};
        path.reserve(std::min<std::size_t>(*steps, 256U));
        for (std::uint64_t step = 0; step < *steps; ++step) {
            auto node = idn.readUnsignedLeb128(stringCount);
            auto indexed = idn.readU8();
            if (!node || !indexed || *indexed > 1U)
                return core::unexpected(fail("packed.identity.path", "IDN0 path step is invalid"));
            auto nodeValue = getString(*node);
            if (!nodeValue)
                return core::unexpected(std::move(nodeValue.error()));
            path.emplace_back(std::move(*nodeValue), *indexed == 1U);
        }
        paths.push_back(std::move(path));
        if (paths.size() > 1U && !(paths[paths.size() - 2U] < paths.back()))
            return core::unexpected(
                fail("packed.identity.order",
                     "IDN0 paths must be unique and strictly ascending by (nodeId, indexed)"));
    }
    auto identityCount = idn.readU32();
    if (!identityCount || *identityCount != entityCount)
        return core::unexpected(fail("packed.identity.invalid", "IDN0 identity count is invalid"));
    for (std::uint32_t i = 0; i < *identityCount; ++i) {
        auto tag = idn.readU8();
        if (!tag)
            return core::unexpected(std::move(tag.error()));
        if (*tag == 0U) {
            auto uuid = idn.readBytes(16);
            if (!uuid)
                return core::unexpected(std::move(uuid.error()));
            entities[i].identity = ExplicitEntityIdentity{ChartObjectId{identityText(*uuid)}};
            continue;
        }
        if (*tag != 1U)
            return core::unexpected(fail("packed.identity.tag", "Identity tag is unsupported"));
        auto scopeIndex = idn.readUnsignedLeb128(scopes.size());
        auto pathIndex = idn.readUnsignedLeb128(paths.size());
        if (!scopeIndex || !pathIndex || *scopeIndex >= scopes.size() || *pathIndex >= paths.size())
            return core::unexpected(fail("packed.identity.path", "Generated identity is invalid"));
        GeneratedEntityIdentity generated;
        generated.chartId = ChartId{scopes[*scopeIndex].chartId};
        generated.bindingId = scopes[*scopeIndex].bindingId;
        generated.moduleId = scopes[*scopeIndex].moduleId;
        generated.exportId = scopes[*scopeIndex].exportId;
        for (const auto& [nodeId, indexed] : paths[*pathIndex]) {
            if (!indexed) {
                generated.path.push_back(SemanticIdentityStep{nodeId, 0});
                continue;
            }
            auto iteration = idn.readUnsignedLeb128();
            if (!iteration || *iteration == 0U)
                return core::unexpected(
                    fail("packed.identity.path", "Generated identity iteration is invalid"));
            // Spec 6.5: the repeat index is a uint32 field, so a larger wire value is refused
            // instead of being narrowed onto a different generated identity.
            if (*iteration > std::numeric_limits<std::uint32_t>::max())
                return core::unexpected(fail("packed.identity.path",
                                             "Generated identity iteration exceeds the uint32 "
                                             "range"));
            generated.path.push_back(
                SemanticIdentityStep{nodeId, static_cast<std::uint32_t>(*iteration)});
        }
        // Spec 6.5: the last path step must be a non-indexed emit label.
        if (generated.path.empty() || generated.path.back().iterationIndexPlusOne != 0U)
            return core::unexpected(
                fail("packed.identity.generated_path",
                     "Generated identity paths must end with a non-indexed step"));
        entities[i].identity = std::move(generated);
    }
    // Spec 6.5: canonical identity bytes define the entity ordinal, so the IDN0 identity records
    // must be strictly ascending and duplicate-free.
    std::vector<std::byte> previousIdentityBytes;
    for (std::uint32_t i = 0; i < *identityCount; ++i) {
        auto identityBytes = identity_detail::canonicalIdentityBytes(entities[i].identity);
        if (!identityBytes)
            return core::unexpected(std::move(identityBytes.error()));
        if (i != 0U && !(previousIdentityBytes < *identityBytes))
            return core::unexpected(
                fail("packed.identity.order",
                     "IDN0 identities must be unique and strictly ascending by canonical bytes"));
        previousIdentityBytes = std::move(*identityBytes);
    }
    if (!idn.empty())
        return core::unexpected(fail("packed.identity.trailing", "IDN0 has trailing bytes"));
    return {};
}

} // namespace cuexis::chart::packed::decoder_detail
