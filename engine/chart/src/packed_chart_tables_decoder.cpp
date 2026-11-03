#include "packed_chart_tables_decoder_internal.hpp"

#include <cuexis/chart/packed_chart_tables.hpp>

#include <cuexis/core/error.hpp>

#include "packed_chart_tables_units_internal.hpp"
#include "packed_gameplay_internal.hpp"
#include "packed_limits_internal.hpp"

#include <array>
#include <utility>

namespace cuexis::chart::packed::decoder_detail {

auto Decoder::section(std::string_view code) const -> const Directory* {
    for (const auto& value : directories)
        if (value.code == code && value.role == SectionRole::semantic)
            return &value;
    return nullptr;
}

auto Decoder::getString(std::uint64_t index) const -> core::Result<std::string> {
    if (index >= strings.size())
        return core::unexpected(fail("packed.strings.index", "String index is out of range"));
    return strings[static_cast<std::size_t>(index)];
}

auto Decoder::refValue(std::uint64_t index, std::uint8_t kind) const -> core::Result<std::string> {
    if (index >= refs.size() || refs[index].first != kind)
        return core::unexpected(fail("packed.references.kind", "Typed reference kind is invalid"));
    return refs[index].second;
}

} // namespace cuexis::chart::packed::decoder_detail

namespace cuexis::chart::packed {
namespace {

using decoder_detail::Decoder;

auto run(Decoder& decoder) -> core::Result<CanonicalSemanticChart> {
    if (auto step = decoder.readHeader(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readStrings(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readRefs(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readMeta(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readTiming(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readIdentity(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readArchetypes(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readEntities(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.applyStreams("TRN0", 0); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.applyStreams("REN0", 1); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.applyStreams("CAM0", 2); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readConstraints(); !step)
        return core::unexpected(std::move(step.error()));
    if (auto step = decoder.readRequirements(); !step)
        return core::unexpected(std::move(step.error()));
    return decoder.finish();
}

} // namespace

auto decodeImpl(std::span<const std::byte> bytes, PackedChartLimits limits, bool gameplay,
                gameplay_detail::DecodedStatic* staticResult)
    -> core::Result<CanonicalSemanticChart> {
    auto stats = inspectImpl(bytes, limits, gameplay);
    if (!stats)
        return core::unexpected(std::move(stats.error()));
    Decoder decoder{bytes, gameplay, staticResult};
    return run(decoder);
}

} // namespace cuexis::chart::packed
