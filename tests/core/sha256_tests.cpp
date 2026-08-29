#include <cuexis_internal/sha256.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace {

using cuexis::core::detail::Sha256;
using cuexis::core::detail::sha256;
using cuexis::core::detail::sha256Hex;

TEST_CASE("Core SHA-256 matches NIST known-answer vectors", "[core][sha256]") {
    CHECK(sha256Hex(sha256(std::string_view{})) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Hex(sha256(std::string_view{"abc"})) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256Hex(sha256(
              std::string_view{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"})) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    const auto digest = [](std::size_t size) {
        const std::string input(size, 'a');
        return sha256Hex(sha256(input));
    };
    CHECK(digest(55) == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(digest(56) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(digest(63) == "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34");
    CHECK(digest(64) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    CHECK(digest(65) == "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0");
}

TEST_CASE("Core SHA-256 preserves streaming results for large input", "[core][sha256]") {
    constexpr std::size_t inputSize = 1'000'000;
    constexpr std::size_t chunkSize = 4093;
    const std::array<std::byte, chunkSize> chunk = [] {
        std::array<std::byte, chunkSize> value{};
        value.fill(std::byte{0x61U});
        return value;
    }();

    Sha256 hash;
    std::size_t remaining = inputSize;
    while (remaining != 0) {
        const auto count = remaining < chunk.size() ? remaining : chunk.size();
        hash.update(std::span<const std::byte>{chunk}.first(count));
        remaining -= count;
    }

    CHECK(sha256Hex(hash.finish()) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

} // namespace
