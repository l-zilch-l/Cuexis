#pragma once

#include <cuexis/core/diagnostic.hpp>
#include <cuexis/cxc/cxc_package.hpp>
#include <cuexis/cxc/cxc_writer.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::cxc::test {

struct ZipEntryLocation final {
    std::string path;
    std::size_t localHeaderOffset{};
    std::size_t dataOffset{};
    std::size_t byteCount{};
    std::size_t centralHeaderOffset{};
};

struct ZipLayout final {
    std::vector<ZipEntryLocation> entries;
    std::size_t centralOffset{};
    std::size_t centralSize{};
    std::size_t eocdOffset{};
};

[[nodiscard]] auto sourceRoot() -> std::filesystem::path;
[[nodiscard]] auto binaryFixtureRoot() -> std::filesystem::path;
[[nodiscard]] auto goldenFixtureRoot() -> std::filesystem::path;
[[nodiscard]] auto readText(const std::filesystem::path& path) -> std::string;
[[nodiscard]] auto readBytes(const std::filesystem::path& path) -> std::vector<std::byte>;
void writeBytes(const std::filesystem::path& path, std::span<const std::byte> bytes);

[[nodiscard]] auto bytesFromText(std::string_view text) -> std::vector<std::byte>;
[[nodiscard]] auto textFromBytes(std::span<const std::byte> bytes) -> std::string;
[[nodiscard]] auto textEntry(std::string path, std::string text) -> CxcWriteEntry;
[[nodiscard]] auto binaryEntry(std::string path, std::string_view bytes) -> CxcWriteEntry;

[[nodiscard]] auto makeV1Request() -> CxcWriteRequest;
[[nodiscard]] auto makeV2Request() -> CxcWriteRequest;
[[nodiscard]] auto makeV3Request() -> CxcWriteRequest;
[[nodiscard]] auto makeV4CxtRequest() -> CxcWriteRequest;
[[nodiscard]] auto staticV4Chart() -> std::string;

[[nodiscard]] auto writePackage(CxcWriteRequest request, const CxcPackageLimits& limits = {})
    -> std::vector<std::byte>;
[[nodiscard]] auto writeUncheckedPackage(std::vector<CxcWriteEntry> entries,
                                         const CxcPackageLimits& limits = {})
    -> std::vector<std::byte>;
[[nodiscard]] auto writeRawZip(std::vector<CxcWriteEntry> entries,
                               const CxcPackageLimits& limits = {}) -> std::vector<std::byte>;
[[nodiscard]] auto parseZip(std::span<const std::byte> bytes) -> ZipLayout;
[[nodiscard]] auto findEntry(const ZipLayout& layout, std::string_view path)
    -> const ZipEntryLocation&;
[[nodiscard]] auto readU16(std::span<const std::byte> bytes, std::size_t offset) -> std::uint16_t;
[[nodiscard]] auto readU32(std::span<const std::byte> bytes, std::size_t offset) -> std::uint32_t;
void writeU16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value);
void writeU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value);
[[nodiscard]] auto crc32(std::span<const std::byte> bytes) noexcept -> std::uint32_t;
void refreshEntryCrc(std::vector<std::byte>& bytes, std::string_view path);
void replaceEntryData(std::vector<std::byte>& bytes, std::string_view path,
                      std::span<const std::byte> replacement);
void replaceEntryText(std::vector<std::byte>& bytes, std::string_view path, std::string_view from,
                      std::string_view to);
void replaceEntryPath(std::vector<std::byte>& bytes, std::string_view oldPath,
                      std::string_view newPath);

[[nodiscard]] auto hasDiagnostic(const core::Diagnostics& diagnostics, std::string_view code)
    -> bool;
[[nodiscard]] auto diagnosticsText(const core::Diagnostics& diagnostics) -> std::string;

} // namespace cuexis::cxc::test
