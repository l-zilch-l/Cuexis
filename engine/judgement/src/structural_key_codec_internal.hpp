#pragma once

#include <cuexis/judgement/gameplay_graph.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::judgement::detail {

// Shared E fields. Physical archives retain their own index/varint representation.
template <typename Archive, typename Step> void emissionStepFields(Archive& out, Step& step) {
    out.fields(step.nodeId, step.repeatIndex);
}
template <typename Archive, typename Identity>
void emissionIdentityFields(Archive& out, Identity& identity) {
    out.fields(identity.chartEntryId, identity.invocationId, identity.moduleId, identity.exportId,
               identity.emissionPath, identity.requirementLocalId);
}

// The adapter owns output, count-range errors and text validation. Framing owns no allocation.
template <typename Adapter> class StructuralKeyEncoder final {
  public:
    explicit StructuralKeyEncoder(Adapter& adapter) : adapter_{adapter} {}
    void number(std::uint64_t value, unsigned width) {
        std::array<std::byte, 8> bytes{};
        for (unsigned i = 0; i < width; ++i)
            bytes[i] = static_cast<std::byte>((value >> (8U * i)) & 255U);
        adapter_.write(std::span{bytes}.first(width));
    }
    void tag(std::uint8_t value) {
        number(value, 1);
    }
    void count(std::size_t value) {
        number(adapter_.checkedCount(value), 4);
    }
    void text(std::string_view value) {
        adapter_.validateText(value);
        count(value.size());
        adapter_.write(std::as_bytes(std::span{value.data(), value.size()}));
    }
    void value(const std::string& textValue) {
        text(textValue);
    }
    void value(const std::uint64_t& numberValue) {
        number(numberValue, 8);
    }
    void value(const EmissionPathStep& step) {
        emissionStepFields(*this, step);
    }
    void value(const std::vector<EmissionPathStep>& path) {
        count(path.size());
        for (const auto& step : path)
            value(step);
    }
    template <typename... T> void fields(const T&... values) {
        (value(values), ...);
    }

  private:
    Adapter& adapter_;
};

template <typename Encoder>
void structuralProjectionFields(Encoder& out, std::uint8_t subdomain,
                                std::initializer_list<std::string_view> tokens,
                                const RequirementIdentity* identity) {
    out.tag(subdomain);
    for (auto token : tokens)
        out.text(token);
    if (identity)
        emissionIdentityFields(out, *identity);
}
template <typename Encoder>
void claimKeyFields(Encoder& out, std::string_view explicitNamespace,
                    const RequirementIdentity& identity) {
    structuralProjectionFields(out, 0, {explicitNamespace}, &identity);
}

inline constexpr std::string_view kStructuralKeyPrefix = "cxgp2:";

// Saturate at limit+1 without overflow. Callers use limits below UINT64_MAX.
inline auto structuralProjectionKeySize(std::uint64_t binaryBytes, std::uint64_t limit)
    -> std::uint64_t {
    if (limit < kStructuralKeyPrefix.size() ||
        binaryBytes > (limit - kStructuralKeyPrefix.size()) / 2)
        return limit + 1;
    return kStructuralKeyPrefix.size() + 2 * binaryBytes;
}

// The overflow callback must reject an unrepresentable string using the caller's error policy.
template <typename Overflow>
auto formatStructuralProjectionKey(std::span<const std::byte> bytes, Overflow overflow)
    -> std::string {
    const auto limit = std::string{}.max_size();
    const auto size = structuralProjectionKeySize(bytes.size(), limit);
    if (size > limit)
        overflow();
    constexpr char digits[] = "0123456789abcdef";
    std::string key{kStructuralKeyPrefix};
    key.reserve(static_cast<std::size_t>(size));
    for (auto byte : bytes) {
        const auto value = std::to_integer<unsigned char>(byte);
        key.push_back(digits[value >> 4U]);
        key.push_back(digits[value & 15U]);
    }
    return key;
}

} // namespace cuexis::judgement::detail
