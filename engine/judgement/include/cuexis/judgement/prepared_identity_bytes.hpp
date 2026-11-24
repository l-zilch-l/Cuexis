#pragma once

//  Judgement runtime - prepared judgement identity bytes.
//
//  The identity is a byte concatenation of four pieces: the prepared prefix (engine and ruleset
//  declarations), the chart projection bytes produced by the compile side, the prepared session
//  component, and the execution session component. The runtime side owns the concatenation and
//  never reads an author type; the compile side owns the two byte projections.
//
//  `PreparedIdentityBytes` is a runtime value, distinct from the legacy
//  `cuexis::judgement::PreparedIdentity`; the two are compared by their canonical bytes.

#include <cuexis/core/result.hpp>
#include <cuexis/judgement/prepared_gameplay.hpp>
#include <cuexis/judgement/prepared_identity.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cuexis::judgement::runtime {

class PreparedIdentityBytes final {
  public:
    [[nodiscard]] auto bytes() const noexcept -> const std::vector<std::byte>& {
        return bytes_;
    }
    [[nodiscard]] auto empty() const noexcept -> bool {
        return bytes_.empty();
    }
    //  Stable hexadecimal rendering of the bytes, for diagnostics and evidence.
    [[nodiscard]] auto toHex() const -> std::string;

    friend auto operator==(const PreparedIdentityBytes&, const PreparedIdentityBytes&) noexcept
        -> bool = default;

  private:
    explicit PreparedIdentityBytes(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}

    friend auto composePreparedIdentity(const PreparedGameplay&,
                                        const PreparedIdentityDeclarations&,
                                        std::span<const std::byte>)
        -> core::Result<PreparedIdentityBytes>;

    std::vector<std::byte> bytes_;
};

//  Composes the prepared identity from the owning prepared records, the declarations and the
//  already-encoded execution-session bytes. Fails with
//  `judgement.identity.chart_projection_missing` when the prepared handle carries no chart
//  projection; the compile bridge always supplies one.
[[nodiscard]] auto composePreparedIdentity(const PreparedGameplay& prepared,
                                           const PreparedIdentityDeclarations& declarations,
                                           std::span<const std::byte> executionSession)
    -> core::Result<PreparedIdentityBytes>;

} // namespace cuexis::judgement::runtime
