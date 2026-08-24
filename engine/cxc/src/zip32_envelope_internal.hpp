#pragma once

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_package.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace cuexis::cxc::detail {

struct Zip32Entry final {
    CxcArchiveEntry metadata;
    std::size_t localHeaderOffset{};
    std::size_t dataOffset{};
};

struct Zip32EnvelopeResult final {
    std::vector<Zip32Entry> entries;
    core::Diagnostics diagnostics;

    [[nodiscard]] auto hasValue() const noexcept -> bool {
        return !entries.empty() && !diagnostics.hasErrors();
    }
};

[[nodiscard]] auto validateZip32Envelope(std::span<const std::byte> bytes,
                                         const CxcPackageLimits& limits) -> Zip32EnvelopeResult;
[[nodiscard]] auto
writeCanonicalZip32(std::span<const std::pair<std::string, std::vector<std::byte>>> entries,
                    const CxcPackageLimits& limits) -> core::Result<std::vector<std::byte>>;
[[nodiscard]] auto verifyWithMinizip(std::span<const std::byte> bytes,
                                     std::span<const Zip32Entry> expected) -> core::Result<void>;

} // namespace cuexis::cxc::detail
