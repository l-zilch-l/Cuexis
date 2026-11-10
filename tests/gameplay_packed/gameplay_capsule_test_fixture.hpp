#pragma once

// Shared capsule byte-rewriting fixture for the capsule case units.

#include "../judgement/execution_test_fixture.hpp"
#include "capsule_fixture.hpp"
#include "gameplay_test_fixture.hpp"
#include "hash_work_internal.hpp"
#include "packed_fixture_support.hpp"
#include "packed_gameplay_internal.hpp"
#include "semantic_stream_internal.hpp"

#include <cuexis/chart/packed_chart_io.hpp>
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>
#include <cuexis/json/parse.hpp>
#include <cuexis_internal/sha256.hpp>

#include <algorithm>
#include <array>
#include <catch2/generators/catch_generators.hpp>
#include <chrono>
#include <cmath>
#include <fstream>

namespace capsule_test_fixture {

namespace {
using namespace cuexis;
using namespace judgement;
using namespace judgement::testing;
namespace capsule = gameplay_packed;
namespace packed = chart::packed;
namespace support = chart::packed::test;
using Bytes = std::vector<std::byte>;

using cuexis::gameplay_packed::testing::CapsuleFixture;

inline auto u32(const Bytes& bytes, std::size_t at) -> std::uint32_t {
    packed::ByteReader r{std::span{bytes}.subspan(at, 4)};
    const auto value = r.readU32();
    REQUIRE(value);
    return *value;
}
inline void setU32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[at + i] = static_cast<std::byte>((value >> (i * 8)) & 255);
    }
}
inline auto directory(const Bytes& bytes, std::string_view code) -> std::size_t {
    for (std::uint32_t i = 0; i < u32(bytes, 24); ++i) {
        const auto at = 96 + static_cast<std::size_t>(i) * 32;
        if (std::string_view{reinterpret_cast<const char*>(bytes.data() + at), 4} == code) {
            return at;
        }
    }
    FAIL("section missing");
}
inline void refreshHeader(Bytes& bytes) {
    setU32(bytes, 92, packed::crc32(std::span{bytes}.first(92)));
}
inline void refreshSection(Bytes& bytes, std::size_t dir) {
    setU32(bytes, dir + 24,
           packed::crc32(std::span{bytes}.subspan(u32(bytes, dir + 8), u32(bytes, dir + 12))));
    refreshHeader(bytes);
}
inline auto rewrite(Bytes bytes, bool reverse, std::string inspection = {}) -> Bytes {
    struct Section {
        Bytes directory;
        Bytes payload;
    };
    std::vector<Section> sections;
    for (std::uint32_t i = 0; i < u32(bytes, 24); ++i) {
        const auto at = 96 + static_cast<std::size_t>(i) * 32;
        const auto offset = u32(bytes, at + 8), length = u32(bytes, at + 12);
        sections.push_back({Bytes{bytes.begin() + at, bytes.begin() + at + 32},
                            Bytes{bytes.begin() + offset, bytes.begin() + offset + length}});
    }
    if (reverse) {
        std::reverse(sections.begin(), sections.end());
    }
    if (!inspection.empty()) {
        Bytes row(32), payload{std::byte{0x42}};
        for (std::size_t i = 0; i < 4; ++i) {
            row[i] = static_cast<std::byte>(inspection[i]);
        }
        row[5] = std::byte{1};
        setU32(row, 12, 1);
        setU32(row, 16, 1);
        setU32(row, 24, packed::crc32(payload));
        sections.push_back({std::move(row), std::move(payload)});
    }
    Bytes result{bytes.begin(), bytes.begin() + 96};
    const auto count = static_cast<std::uint32_t>(sections.size());
    auto offset = 96U + count * 32;
    for (auto& s : sections) {
        setU32(s.directory, 8, offset);
        offset += static_cast<std::uint32_t>(s.payload.size());
        result.insert(result.end(), s.directory.begin(), s.directory.end());
    }
    for (const auto& s : sections) {
        result.insert(result.end(), s.payload.begin(), s.payload.end());
    }
    setU32(result, 16, static_cast<std::uint32_t>(result.size()));
    setU32(result, 24, count);
    setU32(result, 28, count * 32);
    setU32(result, 76, static_cast<std::uint32_t>(result.size()) - 96 - count * 32);
    refreshHeader(result);
    return result;
}
inline auto digestText(std::span<const std::byte> bytes) -> std::string {
    const auto digest = core::detail::sha256(bytes);
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (auto b : digest) {
        result += hex[b >> 4];
        result += hex[b & 15];
    }
    return result;
}
struct Workspace final {
    std::filesystem::path path;
    Workspace() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned i = 0; i < 100; ++i) {
            auto candidate = std::filesystem::temp_directory_path() /
                             ("cuexis-capsule-" + std::to_string(stamp) + "-" + std::to_string(i));
            if (std::filesystem::create_directory(candidate)) {
                path = std::move(candidate);
                return;
            }
        }
        FAIL("cannot create an exclusive test workspace");
    }
    ~Workspace() {
        std::error_code error;
        if (!path.empty()) {
            std::filesystem::remove_all(path, error);
        }
    }
};
inline auto readBytes(const std::filesystem::path& path) -> Bytes {
    std::ifstream stream{path, std::ios::binary};
    REQUIRE(stream);
    Bytes bytes;
    char value{};
    while (stream.get(value)) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
    return bytes;
}
} // namespace

} // namespace capsule_test_fixture

using namespace capsule_test_fixture;
