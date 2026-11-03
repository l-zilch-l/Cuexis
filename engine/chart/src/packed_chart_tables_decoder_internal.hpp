#pragma once

// Shared state of the packed table decoder. The reader consumes one artifact in a fixed order and
// each stage materializes its own tables, so the state is named here and the stages live in
// separate translation units.

#include <cuexis/chart/packed_chart_tables.hpp>

#include <cuexis/core/error.hpp>

#include "packed_chart_tables_units_internal.hpp"
#include "packed_gameplay_internal.hpp"
#include "packed_limits_internal.hpp"
#include "packed_profile_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::chart::packed::decoder_detail {

struct Directory final {
    std::string code;
    std::span<const std::byte> data;
    std::uint32_t records{};
    SectionRole role{SectionRole::semantic};
};

struct DecodedArch final {
    std::uint64_t mask{};
    std::optional<CanonicalTransform> transform;
    std::optional<CanonicalRenderable> renderable;
    std::optional<CanonicalCamera> camera;
};

struct Decoder final {
    std::span<const std::byte> bytes;
    bool gameplay{};
    gameplay_detail::DecodedStatic* staticResult{};

    std::vector<Directory> directories;
    std::span<const std::byte> semanticHash;
    std::uint32_t entityCount{};
    std::uint32_t requirementCount{};
    std::uint32_t stringCount{};
    std::uint32_t refCount{};
    std::uint32_t revision{};

    std::vector<std::string> strings;
    std::vector<std::pair<std::uint8_t, std::string>> refs;
    CanonicalSemanticChart chart;
    std::vector<CanonicalEntity> entities;
    std::vector<std::uint32_t> parents;
    std::vector<std::uint32_t> archetypes;
    std::vector<DecodedArch> arches;
    std::vector<std::uint32_t> lanes;

    [[nodiscard]] auto section(std::string_view code) const -> const Directory*;
    [[nodiscard]] auto getString(std::uint64_t index) const -> core::Result<std::string>;
    [[nodiscard]] auto refValue(std::uint64_t index, std::uint8_t kind) const
        -> core::Result<std::string>;

    [[nodiscard]] auto readHeader() -> core::Result<void>;
    [[nodiscard]] auto readStrings() -> core::Result<void>;
    [[nodiscard]] auto readRefs() -> core::Result<void>;
    [[nodiscard]] auto readMeta() -> core::Result<void>;
    [[nodiscard]] auto readTiming() -> core::Result<void>;
    [[nodiscard]] auto readIdentity() -> core::Result<void>;
    [[nodiscard]] auto readArchetypes() -> core::Result<void>;
    [[nodiscard]] auto readEntities() -> core::Result<void>;
    [[nodiscard]] auto applyStreams(std::string_view code, int kind) -> core::Result<void>;
    [[nodiscard]] auto readConstraints() -> core::Result<void>;
    [[nodiscard]] auto readRequirements() -> core::Result<void>;
    [[nodiscard]] auto finish() -> core::Result<CanonicalSemanticChart>;
};

} // namespace cuexis::chart::packed::decoder_detail
