#pragma once

// Shared canonical-JSON helpers for the internal media tool records (provenance, cache).
//
// Both records are auditable sidecars rather than runtime formats, so they are canonical JSON with
// a fixed key order. The reader is deliberately strict: unknown keys, duplicate keys, malformed
// escapes, non-canonical numbers and trailing content are refused instead of defaulted, because a
// silently repaired cache or provenance record is worse than a refused one.

#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cuexis::media_import::detail {

// Appends a JSON string literal, escaping only what JSON requires. Bytes above 0x1F pass through so
// UTF-8 locators stay readable.
void appendJsonString(std::string& out, std::string_view value);

// One parsed field: either a decoded string or an unsigned integer written without a sign,
// exponent or leading zero.
struct JsonField final {
    std::string value;
    bool number{false};
};

class JsonObjectReader final {
  public:
    // `errorCode` becomes the code of every diagnostic this reader produces, so each caller keeps
    // its own error taxonomy.
    [[nodiscard]] static auto parse(std::string_view text, std::string errorCode)
        -> core::Result<JsonObjectReader>;

    [[nodiscard]] auto takeString(std::string_view key) -> core::Result<std::string>;
    [[nodiscard]] auto takeNumber(std::string_view key) -> core::Result<std::uint64_t>;
    // A missing key yields the fallback; a present key must still be a well-formed string.
    [[nodiscard]] auto takeOptionalString(std::string_view key, std::string fallback)
        -> core::Result<std::string>;

    [[nodiscard]] auto remaining() const noexcept -> std::size_t;
    [[nodiscard]] auto firstRemainingKey() const noexcept -> std::string_view;

  private:
    [[nodiscard]] auto find(std::string_view key) const noexcept -> const JsonField*;
    // Removes and returns the field, so taking every known key proves the document had no others.
    [[nodiscard]] auto take(std::string_view key) noexcept -> std::optional<JsonField>;

    std::string errorCode_;
    std::vector<std::pair<std::string, JsonField>> fields_;
};

[[nodiscard]] auto isLowerHex(std::string_view value, std::size_t length) noexcept -> bool;
[[nodiscard]] auto isPortableAssetId(std::string_view value, std::size_t maxBytes) noexcept -> bool;
[[nodiscard]] auto isPrintableAscii(std::string_view value) noexcept -> bool;

} // namespace cuexis::media_import::detail
