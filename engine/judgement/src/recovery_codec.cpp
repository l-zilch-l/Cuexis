#include "diagnostic_policy.hpp"
#include "recovery_wire.hpp"
#include <cuexis/judgement/recovery_codec.hpp>

#include <algorithm>
#include <array>

namespace cuexis::judgement {
namespace {
using namespace detail::wire;
constexpr std::array<std::byte, 8> magic{std::byte{'C'}, std::byte{'X'}, std::byte{'G'},
                                         std::byte{'R'}, std::byte{'E'}, std::byte{'C'},
                                         std::byte{'0'}, std::byte{'1'}};
auto failure(std::string_view code) -> core::Error {
    return core::Error{std::string{code}, "recovery wire validation failed"}
        .withContext("category", "invalid_relation")
        .withContext("severity", "error")
        .withContext("faulted", "false");
}
auto beginEnvelope(std::uint32_t revision, std::uint8_t kind) -> Writer {
    Writer out;
    out.revision = revision;
    out.bytes.insert(out.bytes.end(), magic.begin(), magic.end());
    out(revision, kind, std::uint64_t{0}, std::uint64_t{0});
    return out;
}
void patchInteger(Writer& out, std::size_t offset, std::uint64_t value) noexcept {
    if (out.measuring())
        return;
    for (unsigned i = 0; i < 8; ++i)
        out.bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 255U);
}
auto beginSection(Writer& out, std::uint8_t id) -> std::size_t {
    out(id);
    const auto lengthAt = out.size();
    out(std::uint64_t{0});
    return lengthAt;
}
void endSection(Writer& out, std::size_t lengthAt) noexcept {
    patchInteger(out, lengthAt, static_cast<std::uint64_t>(out.size() - lengthAt - 8));
}
auto endEnvelope(Writer out) -> std::vector<std::byte> {
    const auto payload = std::span<const std::byte>{out.bytes}.subspan(29);
    patchInteger(out, 13, static_cast<std::uint64_t>(payload.size()));
    patchInteger(out, 21, digest(payload));
    return std::move(out.bytes);
}
struct Envelope final {
    std::span<const std::byte> payload;
    std::uint64_t hash;
};
auto unwrap(std::span<const std::byte> bytes, std::uint8_t expectedKind,
            const KernelProjection& initial, std::optional<CodecBudget> budget) -> Envelope {
    if (budget && !budget->testOnly) {
        throw Failure{"codec.budget_unaccepted"};
    }
    if (budget && bytes.size() > budget->maxBytes) {
        throw Failure{"codec.budget_exceeded"};
    }
    if (bytes.size() < 29) {
        throw Failure{"codec.truncated"};
    }
    if (!std::equal(magic.begin(), magic.end(), bytes.begin())) {
        throw Failure{"codec.magic_invalid"};
    }
    Reader reader{bytes.subspan(8), 0, 0, budget, initial};
    std::uint32_t version;
    std::uint8_t kind;
    std::uint64_t length, hash;
    reader(version, kind, length, hash);
    if (version != (initial.rpa ? 3U : 2U)) {
        throw Failure{"codec.version_unsupported"};
    }
    if (kind != expectedKind) {
        throw Failure{"codec.tag_invalid"};
    }
    if (length != bytes.size() - 29) {
        throw Failure{"codec.length_invalid"};
    }
    return {bytes.subspan(29), hash};
}
auto readSection(Reader& r, std::uint8_t expected) -> Reader {
    std::uint8_t id;
    std::uint64_t length;
    r(id, length);
    if (id != expected) {
        throw Failure{"codec.section_invalid"};
    }
    if (length > r.bytes.size() - r.pos) {
        throw Failure{"codec.truncated"};
    }
    auto data = r.bytes.subspan(r.pos, static_cast<std::size_t>(length));
    r.pos += static_cast<std::size_t>(length);
    return Reader{data, 0, r.elements, r.budget, r.initial, r.revision};
}
void finish(const Reader& r) {
    if (r.pos != r.bytes.size()) {
        throw Failure{"codec.trailing_bytes"};
    }
}
struct Header final {
    std::vector<std::byte> identity;
    std::string factRevision;
    std::uint32_t stateSchema;
    std::uint64_t events, facts;
    std::string eventCodecId;
};
void writeHeader(Writer& h, const KernelProjection& p, std::uint64_t events) {
    const auto lengthAt = beginSection(h, 1);
    h(p.judgementIdentity.canonicalBytes().bytes(), p.effectiveFactSemanticRevision,
      std::uint32_t{p.rpa ? 2U : 1U}, events, static_cast<std::uint64_t>(p.facts.size()),
      std::string{p.rpa ? "canonical.discrete.rpa.le.v1" : "canonical.discrete.le.v1"});
    endSection(h, lengthAt);
}
auto readHeader(Reader& r) -> Header {
    auto section = readSection(r, 1);
    Header h;
    section(h.identity, h.factRevision, h.stateSchema, h.events, h.facts, h.eventCodecId);
    finish(section);
    r.elements = section.elements;
    if (h.stateSchema != (r.revision == 3 ? 2U : 1U) ||
        h.eventCodecId !=
            (r.revision == 3 ? "canonical.discrete.rpa.le.v1" : "canonical.discrete.le.v1")) {
        throw Failure{"codec.version_unsupported"};
    }
    return h;
}
void checkHeader(const Header& h, const KernelProjection& actual, const RecoveryInputs& inputs) {
    static_cast<void>(inputs);
    if (h.identity != actual.judgementIdentity.canonicalBytes().bytes() ||
        h.factRevision != actual.effectiveFactSemanticRevision) {
        throw Failure{"codec.identity_mismatch"};
    }
}
auto initial(const RecoveryInputs& inputs)
    -> core::Result<std::unique_ptr<detail::ExecutionKernel>> {
    return detail::ExecutionKernel::prepare(inputs.configuration, inputs.prepared);
}
} // namespace
auto encodeReplay(const ReplayArchive& archive) -> core::Result<std::vector<std::byte>> try {
    auto base = detail::ExecutionKernel::initialProjection(archive);
    if (!base) {
        return core::unexpected(std::move(base.error()));
    }
    if ((*base)->projection.judgementIdentity != archive.data().identity) {
        throw Failure{"codec.identity_mismatch"};
    }
    const auto* p = &(*base)->projection;
    for (const auto& r : archive.data().records) {
        if (r.result) {
            if (r.result->faultDiagnostic &&
                detail::containsLegacyDiagnostic(*r.result->faultDiagnostic))
                throw Failure{"codec.record_invalid"};
            p = r.result.get();
        }
    }
    const auto payload = [&](Writer& writer) {
        writeHeader(writer, *p, archive.data().eventCount);
        const auto bodyAt = beginSection(writer, 2);
        writer(archive.data().records);
        endSection(writer, bodyAt);
    };
    Writer measure{true};
    measure.revision = p->rpa ? 3U : 2U;
    payload(measure);
    auto out = beginEnvelope(measure.revision, 1);
    if (measure.size() > out.bytes.max_size() - out.size())
        throw Failure{"codec.length_invalid"};
    out.bytes.reserve(out.size() + measure.size());
    payload(out);
    return endEnvelope(std::move(out));
} catch (const detail::wire::Failure& e) {
    return core::unexpected(failure(e.what()));
} catch (const std::exception&) {
    return core::unexpected(failure("codec.allocation_failed"));
}
auto decodeReplay(std::span<const std::byte> bytes, const RecoveryInputs& inputs,
                  std::optional<CodecBudget> budget) -> core::Result<ReplayArchive> try {
    auto base = initial(inputs);
    if (!base) {
        return core::unexpected(base.error());
    }
    const auto p = (*base)->query()->projection;
    const auto env = unwrap(bytes, 1, p, budget);
    Reader r{env.payload, 0, 0, budget, p, p.rpa ? 3U : 2U};
    const auto h = readHeader(r);
    auto body = readSection(r, 2);
    ReplayData data{{}, h.events, p.judgementIdentity};
    body(data.records);
    finish(body);
    finish(r);
    if (budget && data.records.size() > budget->maxRecords) {
        throw Failure{"codec.budget_exceeded"};
    }
    if (digest(env.payload) != env.hash) {
        throw Failure{"codec.digest_mismatch"};
    }
    checkHeader(h, p, inputs);
    std::uint64_t facts = 0;
    for (const auto& record : data.records) {
        if (record.result) {
            facts = static_cast<std::uint64_t>(record.result->facts.size());
        }
    }
    if (facts != h.facts) {
        throw Failure{"codec.count_mismatch"};
    }
    for (const auto& record : data.records)
        if (record.result && record.result->faultDiagnostic &&
            detail::containsLegacyDiagnostic(*record.result->faultDiagnostic))
            throw Failure{"codec.record_invalid"};
    std::uint64_t events = 0;
    std::optional<Tick> horizon;
    for (std::size_t i = 0; i < data.records.size(); ++i) {
        const auto& record = data.records[i];
        if (record.control) {
            if (!p.rpa || record.controlDrain || record.horizon || !record.batch.empty() ||
                !record.admission.empty() || !record.result)
                throw Failure{"codec.record_invalid"};
            const auto result = (*base)->controlRpa(*record.control);
            if (!result && record.result->state != KernelSessionState::Faulted)
                throw Failure{"codec.record_invalid"};
        } else if (record.horizon) {
            if (!record.result || !record.batch.empty() || !record.admission.empty() ||
                (horizon && *record.horizon < *horizon) ||
                (record.result->state == KernelSessionState::Faulted &&
                 i + 1 != data.records.size())) {
                throw Failure{"codec.record_invalid"};
            }
            horizon = record.horizon;
            const auto advanced = (*base)->advance(*horizon, record.controlDrain);
            if (!advanced && record.result->state != KernelSessionState::Faulted) {
                throw Failure{"codec.record_invalid"};
            }
        } else {
            if (record.result || record.batch.empty() ||
                (record.rejection ? !record.admission.empty()
                                  : record.batch.size() != record.admission.size()) ||
                record.batch.size() > UINT64_MAX - events ||
                !std::is_sorted(record.batch.begin(), record.batch.end(),
                                [](const auto& a, const auto& b) { return a.key < b.key; })) {
                throw Failure{"codec.record_invalid"};
            }
            const auto admitted = (*base)->submitCanonical(record.batch);
            if (record.rejection
                    ? admitted.has_value() || !sameRecoveryError(admitted.error(), record.rejection)
                    : !admitted || *admitted != record.admission) {
                throw Failure{"codec.record_invalid"};
            }
            events += static_cast<std::uint64_t>(record.batch.size());
        }
    }
    if (events != h.events) {
        throw Failure{"codec.count_mismatch"};
    }
    auto archive = ReplayArchive{std::make_shared<const ReplayData>(std::move(data)),
                                 std::make_shared<const RecoveryInputs>(inputs)};
    // Structural Reader validation does not claim execution validity. Evaluation verifies
    // accepted admission, full results and injected-fault equivalence separately.
    return archive;
} catch (const detail::wire::Failure& e) {
    return core::unexpected(failure(e.what()));
} catch (const std::exception&) {
    return core::unexpected(failure("codec.allocation_failed"));
}
auto encodeSnapshot(const SnapshotPayload& value) -> core::Result<std::vector<std::byte>> try {
    if (value.state().kernel.faultDiagnostic &&
        detail::containsLegacyDiagnostic(*value.state().kernel.faultDiagnostic))
        throw Failure{"codec.record_invalid"};
    const auto payload = [&](Writer& writer) {
        writeHeader(writer, value.state().kernel,
                    static_cast<std::uint64_t>(value.state().ingress.accepted.size()));
        const auto bodyAt = beginSection(writer, 2);
        writer(value.state());
        endSection(writer, bodyAt);
    };
    Writer measure{true};
    measure.revision = value.state().kernel.rpa ? 3U : 2U;
    payload(measure);
    auto out = beginEnvelope(measure.revision, 2);
    if (measure.size() > out.bytes.max_size() - out.size())
        throw Failure{"codec.length_invalid"};
    out.bytes.reserve(out.size() + measure.size());
    payload(out);
    return endEnvelope(std::move(out));
} catch (const detail::wire::Failure& e) {
    return core::unexpected(failure(e.what()));
} catch (const std::exception&) {
    return core::unexpected(failure("codec.allocation_failed"));
}
auto decodeSnapshot(std::span<const std::byte> bytes, const RecoveryInputs& inputs,
                    std::optional<CodecBudget> budget) -> core::Result<SnapshotPayload> try {
    auto base = initial(inputs);
    if (!base) {
        return core::unexpected(base.error());
    }
    const auto p = (*base)->query()->projection;
    const auto env = unwrap(bytes, 2, p, budget);
    Reader r{env.payload, 0, 0, budget, p, p.rpa ? 3U : 2U};
    const auto h = readHeader(r);
    auto body = readSection(r, 2);
    auto dto = body.seed<SnapshotDTO>();
    body(dto);
    finish(body);
    finish(r);
    if (digest(env.payload) != env.hash) {
        throw Failure{"codec.digest_mismatch"};
    }
    checkHeader(h, p, inputs);
    if (dto.ingress.accepted.size() != h.events || dto.kernel.facts.size() != h.facts) {
        throw Failure{"codec.count_mismatch"};
    }
    if (dto.kernel.faultDiagnostic && detail::containsLegacyDiagnostic(*dto.kernel.faultDiagnostic))
        throw Failure{"codec.record_invalid"};
    auto stored = std::make_shared<const detail::SnapshotStorage>(
        std::move(dto), std::make_shared<const RecoveryInputs>(inputs));
    auto valid = detail::ExecutionKernel::restore(*stored, inputs);
    if (!valid) {
        return core::unexpected(valid.error());
    }
    return SnapshotPayload{std::move(stored)};
} catch (const detail::wire::Failure& e) {
    return core::unexpected(failure(e.what()));
} catch (const std::exception&) {
    return core::unexpected(failure("codec.allocation_failed"));
}
} // namespace cuexis::judgement
