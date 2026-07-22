#include <cuexis/filesystem/secure_file.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

class TemporaryDirectory final {
  public:
    explicit TemporaryDirectory(std::string_view label) {
        static std::atomic<unsigned int> next{1};
        path_ = std::filesystem::temp_directory_path() /
                (std::string{label} + '-' + std::to_string(next.fetch_add(1)));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    auto operator=(const TemporaryDirectory&) -> TemporaryDirectory& = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

    void write(std::string_view relative, std::string_view text) const {
        const auto file = path_ / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream output{file, std::ios::binary};
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("Secure file reads bytes and size from one bounded handle", "[filesystem][security]") {
    TemporaryDirectory root{"cuexis-secure-file-root"};
    root.write("nested/data.txt", "bounded-data");
    const auto file = root.path() / "nested" / "data.txt";

    const auto read =
        cuexis::filesystem::readBoundedTextFile(file, {.root = root.path(), .maxBytes = 12});
    REQUIRE(read.has_value());
    CHECK(read->text == "bounded-data");

    const auto tooLarge =
        cuexis::filesystem::readBoundedFile(file, {.root = root.path(), .maxBytes = 11});
    REQUIRE_FALSE(tooLarge.has_value());
    CHECK(tooLarge.error().code() == "filesystem.file.too_large");
}

TEST_CASE("Secure file rejects lexical containment escapes", "[filesystem][security]") {
    TemporaryDirectory root{"cuexis-secure-file-root"};
    TemporaryDirectory outside{"cuexis-secure-file-outside"};
    outside.write("secret.txt", "secret");

    const auto lexical = cuexis::filesystem::readBoundedFile(outside.path() / "secret.txt",
                                                             {.root = root.path(), .maxBytes = 64});
    REQUIRE_FALSE(lexical.has_value());
    CHECK(lexical.error().code() == "filesystem.file.outside_root");
}

TEST_CASE("Secure file rejects physical containment escapes through symlinks",
          "[filesystem][security][symlink]") {
    TemporaryDirectory root{"cuexis-secure-file-root"};
    TemporaryDirectory outside{"cuexis-secure-file-outside"};
    outside.write("secret.txt", "secret");

    std::error_code linkError;
    std::filesystem::create_directory_symlink(outside.path(), root.path() / "redirect", linkError);
    if (linkError) {
        SKIP("Directory symlinks are unavailable in this test environment");
    }
    const auto redirected = cuexis::filesystem::readBoundedFile(
        root.path() / "redirect" / "secret.txt", {.root = root.path(), .maxBytes = 64});
    REQUIRE_FALSE(redirected.has_value());
    CHECK(redirected.error().code() == "filesystem.file.outside_root");
}
