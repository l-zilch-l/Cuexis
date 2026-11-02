#include "gameplay_capsule_operations_internal.hpp"

// Static tables, physical framing, semantic preimage and the capsule envelope.

namespace cuexis::gameplay_packed {
namespace capsule_detail {

template <typename F>
auto output(Mode mode, Model& m, chart::PackedChartLimits limits,
            const std::vector<std::string>* strings, const std::vector<Ref>* references,
            F visitRows) -> Bytes {
    Output sizing;
    sizing.mode = mode;
    sizing.sizing = true;
    sizing.limit = mode == Mode::hash ? limits.maxPackedDecodedBytes : limits.maxPackedSectionBytes;
    sizing.strings = strings;
    sizing.references = references;
    visitRows(sizing, m);
    Output out;
    out.mode = mode;
    out.limit = sizing.limit;
    out.strings = strings;
    out.references = references;
    visitRows(out, m);
    return std::move(out.bufferedWriter()).takeBytes();
}
void allRows(Output& a, Model& m) {
    globalRow(a, m);
    for (std::size_t i = 0; i < m.graph.requirements.size(); ++i) {
        requirementRow(a, m.graph.requirements[i], m.indices[i], m);
    }
    definitions(a, m);
    coordination(a, m);
}
auto staticTables(Model& m, chart::PackedChartLimits limits) -> gp::StaticTables {
    std::set<std::string> strings;
    std::set<Ref> references;
    Output collect;
    collect.mode = Mode::collect;
    collect.collected = &strings;
    collect.collectedRefs = &references;
    allRows(collect, m);
    std::vector<std::string> s{strings.begin(), strings.end()};
    std::vector<Ref> r{references.begin(), references.end()};
    auto result = checked(gp::encodeStatic(m.chart, m.owners, s, r, limits));
    counts(m, result.references.size());
    return result;
}

auto preimage(Model& m, chart::PackedChartLimits limits) -> Bytes {
    m.bind();
    auto prefix = checked(gp::staticPreimage(m.chart, m.owners, m.candidateRevision));
    bound(prefix.size(), limits.maxPackedDecodedBytes, "packed.budget.decoded_bytes");
    return output(Mode::hash, m, limits, nullptr, nullptr, [&prefix](auto& a, auto& model) {
        a.raw(prefix);
        semanticRows(a, model);
    });
}

auto hashModel(const Model& m, detail::HashOptions options, detail::HashObservation* observation)
    -> detail::SemanticHashResult {
    const detail::SemanticInput input{
        m.chart,    m.graph,  m.entities, m.owners,   m.indices,    m.patterns,
        m.measures, m.claims, m.profiles, m.timebase, m.latePolicy, m.profileId,
        m.unit,     3,        m.resolver, m.close,    m.counts,     m.deferredRelationResources};
    return checked(detail::calculateSemanticHash(input, options, observation));
}

auto framed(std::array<char, 4> code, Bytes payload, std::uint32_t rows,
            chart::PackedChartLimits limits) -> gp::Section {
    bound(16 + payload.size(), limits.maxPackedSectionBytes, "packed.budget.section_bytes");
    ByteWriter out;
    out.writeU16(1);
    out.writeU8(1);
    out.writeU8(0);
    out.writeU32(rows);
    out.writeU64(payload.size());
    out.writeBytes(payload);
    return {code, std::move(out).takeBytes(), rows};
}

void patchU32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255U);
    }
}
auto physicalTables(Model& m, gp::StaticTables tables, chart::PackedChartLimits limits)
    -> gp::StaticTables {
    const auto rows = [&](auto f) {
        return output(Mode::wire, m, limits, &tables.strings, &tables.references, f);
    };
    tables.sections.push_back(framed(
        {'G', 'P', 'H', '0'}, rows([](auto& a, auto& model) { globalRow(a, model); }), 1, limits));
    tables.sections.push_back(
        framed({'G', 'P', 'R', '0'}, rows([](auto& a, auto& model) {
                   for (std::size_t i = 0; i < model.graph.requirements.size(); ++i) {
                       requirementRow(a, model.graph.requirements[i], model.indices[i], model);
                   }
               }),
               m.counts[0], limits));
    tables.sections.push_back(framed({'G', 'P', 'D', '0'},
                                     rows([](auto& a, auto& model) { definitions(a, model); }),
                                     sum(std::span{m.counts}.subspan(1, 4)), limits));
    tables.sections.push_back(framed({'G', 'R', 'C', '0'},
                                     rows([](auto& a, auto& model) { coordination(a, model); }),
                                     sum(std::span{m.counts}.subspan(5, 5)), limits));
    std::sort(tables.sections.begin(), tables.sections.end(),
              [](const auto& l, const auto& r) { return l.code < r.code; });
    std::uint64_t decoded = 0;
    for (const auto& section : tables.sections) {
        bound(section.bytes.size(), limits.maxPackedSectionBytes, "packed.budget.section_bytes");
        decoded += section.bytes.size();
    }
    bound(static_cast<std::size_t>(decoded), limits.maxPackedDecodedBytes,
          "packed.budget.decoded_bytes");
    const auto directoryBytes = tables.sections.size() * 32;
    const auto total = 96 + directoryBytes + static_cast<std::size_t>(decoded);
    bound(total, limits.maxPackedFileBytes, "packed.budget.file_bytes");
    return tables;
}

auto envelope(Model& m, gp::StaticTables tables, chart::PackedChartLimits limits,
              detail::HashOptions hashOptions, detail::HashObservation* observation) -> Bytes {
    tables = physicalTables(m, std::move(tables), limits);
    std::size_t decoded = 0;
    for (const auto& section : tables.sections)
        decoded += section.bytes.size();
    const auto directoryBytes = tables.sections.size() * 32;
    const auto total = 96 + directoryBytes + decoded;
    const auto digest = m.candidateRevision == 4 ? hashModel(m, hashOptions, observation).digest
                                                 : core::detail::sha256(preimage(m, limits));
    ByteWriter out;
    constexpr std::array magic{std::byte{'C'}, std::byte{'X'}, std::byte{'P'}, std::byte{'K'},
                               std::byte{'5'}, std::byte{0},   std::byte{0},   std::byte{0}};
    out.writeBytes(magic);
    out.writeU16(1);
    out.writeU16(96);
    out.writeU32(1);
    out.writeU32(u32(total));
    out.writeU32(96);
    out.writeU32(u32(tables.sections.size()));
    out.writeU32(u32(directoryBytes));
    for (auto b : digest) {
        out.writeU8(b);
    }
    out.writeU32(u32(m.entities.size()));
    out.writeU32(m.counts[0]);
    out.writeU32(0);
    out.writeU32(u32(static_cast<std::size_t>(decoded)));
    out.writeU32(u32(tables.strings.size()));
    out.writeU32(u32(tables.references.size()));
    out.writeU32(m.candidateRevision);
    out.writeU32(0);
    auto offset = 96 + directoryBytes;
    for (const auto& s : tables.sections) {
        out.writeBytes(std::as_bytes(std::span{s.code}));
        out.writeU8(0);
        out.writeU8(0);
        out.writeU16(0);
        out.writeU32(u32(offset));
        out.writeU32(u32(s.bytes.size()));
        out.writeU32(u32(s.bytes.size()));
        out.writeU32(s.records);
        out.writeU32(crc32(s.bytes));
        out.writeU32(0);
        offset += s.bytes.size();
    }
    for (const auto& s : tables.sections) {
        out.writeBytes(s.bytes);
    }
    auto bytes = std::move(out).takeBytes();
    patchU32(bytes, 92, crc32(std::span<const std::byte>{bytes}.first(92)));
    return bytes;
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed
