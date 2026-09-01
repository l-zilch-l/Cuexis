#include <cuexis/content/content_provider.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

auto bytes(std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const unsigned char character : text) {
        result.push_back(static_cast<std::byte>(character));
    }
    return result;
}

} // namespace

TEST_CASE("MemoryContentProvider returns bounded owned bytes and revision", "[content]") {
    auto provider = cuexis::content::MemoryContentProvider::create({{
        .rootId = "base",
        .source = "mesh/note.bin",
        .bytes = bytes("mesh"),
        .revision = 7,
    }});
    REQUIRE(provider.has_value());

    auto blob = (*provider)->readBlob({.rootId = "base", .source = "mesh/note.bin", .maxBytes = 4});
    REQUIRE(blob.has_value());
    CHECK(blob->bytes == bytes("mesh"));
    CHECK(blob->revision == 7);

    const auto tooSmall =
        (*provider)->readBlob({.rootId = "base", .source = "mesh/note.bin", .maxBytes = 3});
    REQUIRE_FALSE(tooSmall.has_value());
    CHECK(tooSmall.error().code() == "content.provider.too_large");
}

TEST_CASE("HostContentProvider contains callback failures exceptions and reentry", "[content]") {
    auto throwing = cuexis::content::HostContentProvider::create(
        [](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            throw std::runtime_error{"host failed"};
        });
    REQUIRE(throwing.has_value());
    const auto exception = (*throwing)->readBlob({.rootId = "base", .source = "a.bin"});
    REQUIRE_FALSE(exception.has_value());
    CHECK(exception.error().code() == "content.host.callback_exception");

    std::shared_ptr<cuexis::content::HostContentProvider> provider;
    auto created = cuexis::content::HostContentProvider::create(
        [&](const cuexis::content::ContentRequest& request)
            -> cuexis::core::Result<cuexis::content::ContentBlob> {
            return provider->readBlob(request);
        });
    REQUIRE(created.has_value());
    provider = *created;
    const auto reentrant = provider->readBlob({.rootId = "base", .source = "a.bin"});
    REQUIRE_FALSE(reentrant.has_value());
    CHECK(reentrant.error().code() == "content.host.callback_failed");
    REQUIRE(reentrant.error().cause() != nullptr);
    CHECK(reentrant.error().cause()->code() == "content.host.reentrant");

    auto unknown = cuexis::content::HostContentProvider::create(
        [](const cuexis::content::ContentRequest&)
            -> cuexis::core::Result<cuexis::content::ContentBlob> { throw 7; });
    REQUIRE(unknown.has_value());
    const auto unknownException = (*unknown)->readBlob({.rootId = "base", .source = "a.bin"});
    REQUIRE_FALSE(unknownException.has_value());
    CHECK(unknownException.error().code() == "content.host.callback_exception");
}
