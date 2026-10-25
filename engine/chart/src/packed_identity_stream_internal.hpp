// Included inside identity_detail by packed_semantic_identity.cpp.
// One canonical identity field traversal for counting, streaming and legacy byte export.
namespace {
struct IdentityEncoder final {
    IdentityByteSink sink;
    std::uint64_t size{};
    bool overflow{};
    void raw(std::span<const std::byte> bytes) {
        if (overflow || bytes.size() > UINT32_MAX - size) {
            overflow = true;
            return;
        }
        size += bytes.size();
        if (sink.write)
            sink.write(sink.context, bytes);
    }
    void number(std::uint32_t value, std::size_t width) {
        std::array<std::byte, 4> bytes{};
        for (std::size_t i = 0; i < width; ++i)
            bytes[i] = static_cast<std::byte>((value >> (8U * i)) & 255U);
        raw(std::span{bytes}.first(width));
    }
    void text(std::string_view value) {
        if (value.size() > UINT32_MAX) {
            overflow = true;
            return;
        }
        number(static_cast<std::uint32_t>(value.size()), 4);
        raw(std::as_bytes(std::span{value.data(), value.size()}));
    }
};

auto emitCanonicalIdentity(const CanonicalEntityIdentity& identity, IdentityEncoder& out)
    -> core::Result<void> {
    if (const auto* explicitIdentity = std::get_if<ExplicitEntityIdentity>(&identity)) {
        auto uuid = uuidBytes(explicitIdentity->objectId.value);
        if (!uuid)
            return core::unexpected(std::move(uuid.error()));
        out.number(0, 1);
        out.raw(std::as_bytes(std::span{*uuid}));
    } else {
        const auto& generated = std::get<GeneratedEntityIdentity>(identity);
        auto uuid = uuidBytes(generated.chartId.value);
        if (!uuid)
            return core::unexpected(std::move(uuid.error()));
        if (generated.path.size() > UINT32_MAX)
            return core::unexpected(fail("packed.field.wire_range", "Identity path exceeds u32"));
        out.number(1, 1);
        out.raw(std::as_bytes(std::span{*uuid}));
        out.text(generated.bindingId);
        out.text(generated.moduleId);
        out.text(generated.exportId);
        out.number(static_cast<std::uint32_t>(generated.path.size()), 4);
        for (const auto& step : generated.path) {
            out.text(step.nodeId);
            out.number(step.iterationIndexPlusOne, 4);
        }
    }
    if (out.overflow)
        return core::unexpected(fail("packed.field.wire_range", "Identity size exceeds u32"));
    return {};
}
} // namespace

auto canonicalIdentitySize(const CanonicalEntityIdentity& identity) -> core::Result<std::uint32_t> {
    IdentityEncoder count{{nullptr, nullptr}};
    auto result = emitCanonicalIdentity(identity, count);
    if (!result)
        return core::unexpected(std::move(result.error()));
    return static_cast<std::uint32_t>(count.size);
}

auto streamCanonicalIdentity(const CanonicalEntityIdentity& identity, IdentityByteSink sink)
    -> core::Result<void> {
    IdentityEncoder out{sink};
    return emitCanonicalIdentity(identity, out);
}
