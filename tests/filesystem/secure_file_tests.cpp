#include <cuexis/filesystem/secure_file.hpp>

#include "secure_file_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <Windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

[[nodiscard]] auto processId() noexcept -> std::uint64_t {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(_getpid());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

class TemporaryDirectory final {
  public:
    explicit TemporaryDirectory(std::string_view label) {
        static std::atomic<unsigned int> next{1};
        path_ = std::filesystem::temp_directory_path() /
                (std::string{label} + '-' + std::to_string(processId()) + '-' +
                 std::to_string(next.fetch_add(1)));
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

#if defined(_WIN32)

namespace {

struct RenameRootAttempt final {
    std::filesystem::path root;
    std::filesystem::path moved;
    DWORD error{ERROR_SUCCESS};
    bool invoked{};
    bool succeeded{};
};

void attemptRootRename(void* opaque) noexcept {
    auto& attempt = *static_cast<RenameRootAttempt*>(opaque);
    attempt.invoked = true;
    attempt.succeeded = MoveFileExW(attempt.root.c_str(), attempt.moved.c_str(), 0) != FALSE;
    attempt.error = attempt.succeeded ? ERROR_SUCCESS : GetLastError();
}

struct ReplaceFileAttempt final {
    std::filesystem::path replacement;
    std::filesystem::path target;
    DWORD error{ERROR_SUCCESS};
    bool invoked{};
    bool succeeded{};
};

void attemptFileReplacement(void* opaque) noexcept {
    auto& attempt = *static_cast<ReplaceFileAttempt*>(opaque);
    attempt.invoked = true;
    attempt.succeeded = MoveFileExW(attempt.replacement.c_str(), attempt.target.c_str(),
                                    MOVEFILE_REPLACE_EXISTING) != FALSE;
    attempt.error = attempt.succeeded ? ERROR_SUCCESS : GetLastError();
}

[[nodiscard]] auto contentsText(const cuexis::filesystem::FileContents& contents) -> std::string {
    return {reinterpret_cast<const char*>(contents.bytes.data()), contents.bytes.size()};
}

} // namespace

TEST_CASE("Secure file pins its trusted Windows root against replacement",
          "[filesystem][security][windows]") {
    TemporaryDirectory root{"cuexis-secure-file-pinned-root"};
    root.write("data.txt", "trusted");
    RenameRootAttempt attempt{.root = root.path(), .moved = root.path().wstring() + L"-moved"};
    const cuexis::filesystem::detail::ReadFileTestHooks hooks{
        .afterRootValidated = attemptRootRename,
        .context = &attempt,
    };

    const auto read = cuexis::filesystem::detail::readBoundedFileWithHooks(
        root.path() / "data.txt", {.root = root.path(), .maxBytes = 64}, hooks);
    if (attempt.succeeded) {
        std::error_code ignored;
        std::filesystem::rename(attempt.moved, attempt.root, ignored);
    }

    REQUIRE(attempt.invoked);
    CHECK_FALSE(attempt.succeeded);
    CHECK((attempt.error == ERROR_SHARING_VIOLATION || attempt.error == ERROR_ACCESS_DENIED));
    REQUIRE(read.has_value());
    CHECK(contentsText(*read) == "trusted");
}

TEST_CASE("Secure file pins an opened Windows file against same-size replacement",
          "[filesystem][security][windows]") {
    TemporaryDirectory root{"cuexis-secure-file-pinned-file"};
    root.write("target.txt", "trusted!");
    root.write("replacement.txt", "attacker");
    ReplaceFileAttempt attempt{.replacement = root.path() / "replacement.txt",
                               .target = root.path() / "target.txt"};
    const cuexis::filesystem::detail::ReadFileTestHooks hooks{
        .afterFileOpened = attemptFileReplacement,
        .context = &attempt,
    };

    const auto read = cuexis::filesystem::detail::readBoundedFileWithHooks(
        attempt.target, {.root = root.path(), .maxBytes = 64}, hooks);

    REQUIRE(attempt.invoked);
    CHECK_FALSE(attempt.succeeded);
    CHECK((attempt.error == ERROR_SHARING_VIOLATION || attempt.error == ERROR_ACCESS_DENIED));
    REQUIRE(read.has_value());
    CHECK(contentsText(*read) == "trusted!");
}

#endif

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
