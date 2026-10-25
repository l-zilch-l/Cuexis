#include "../../tools/cxc/cxc_common/src/cxc_unpack_commit_internal.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#include "cxc_test_support.hpp"

#include <Windows.h>
#endif

TEST_CASE("CXC unpack directory commit retries only bounded native sharing failures",
          "[cxc][tools]") {
    namespace fs = std::filesystem;
    const fs::path source{"staging"};
    const fs::path target{"output"};
    std::error_code error;
    int attempts = 0;
    int waits = 0;
    int failureCount = 1;
    auto failure = std::error_code{32, std::system_category()};
    const auto rename = [&](const fs::path& from, const fs::path& to, std::error_code& result) {
        REQUIRE(from == source);
        REQUIRE(to == target);
        ++attempts;
        result = attempts <= failureCount ? failure : std::error_code{};
    };
    const auto wait = [&] { ++waits; };

#if defined(_WIN32)
    SECTION("sharing access and lock failures recover after the reader releases") {
        for (const int code : {5, 32, 33}) {
            failure = {code, std::system_category()};
            attempts = 0;
            waits = 0;
            failureCount = 2;
            cuexis::tools::detail::renameUnpackDirectoryWithRetry(source, target, error, rename,
                                                                  wait);
            CHECK_FALSE(error);
            CHECK(attempts == 3);
            CHECK(waits == 2);
        }
    }
    SECTION("persistent sharing failure stops after eight atomic attempts") {
        failureCount = 100;
        cuexis::tools::detail::renameUnpackDirectoryWithRetry(source, target, error, rename, wait);
        CHECK(error == failure);
        CHECK(attempts == 8);
        CHECK(waits == 7);
    }
    SECTION("unrelated native errors fail immediately") {
        failure = {3, std::system_category()};
        cuexis::tools::detail::renameUnpackDirectoryWithRetry(source, target, error, rename, wait);
        CHECK(error == failure);
        CHECK(attempts == 1);
        CHECK(waits == 0);
    }
    SECTION("generic permission errors are not mistaken for native sharing errors") {
        failure = {5, std::generic_category()};
        cuexis::tools::detail::renameUnpackDirectoryWithRetry(source, target, error, rename, wait);
        CHECK(error == failure);
        CHECK(attempts == 1);
        CHECK(waits == 0);
    }
#else
    SECTION("non-Windows errors are not retried") {
        cuexis::tools::detail::renameUnpackDirectoryWithRetry(source, target, error, rename, wait);
        CHECK(error == failure);
        CHECK(attempts == 1);
        CHECK(waits == 0);
    }
#endif
    SECTION("successful rename does not wait") {
        failureCount = 0;
        cuexis::tools::detail::renameUnpackDirectoryWithRetry(source, target, error, rename, wait);
        CHECK_FALSE(error);
        CHECK(attempts == 1);
        CHECK(waits == 0);
    }
}

#if defined(_WIN32)
TEST_CASE("CXC unpack directory commit handles an actual Windows reader deterministically",
          "[cxc][tools][windows]") {
    namespace fs = std::filesystem;
    namespace support = cuexis::cxc::test;
    const auto root = fs::temp_directory_path() /
                      ("cuexis-cxc-unpack-reader-" + std::to_string(GetCurrentProcessId()));
    struct Cleanup final {
        fs::path root;
        HANDLE reader = INVALID_HANDLE_VALUE;
        ~Cleanup() {
            if (reader != INVALID_HANDLE_VALUE) {
                CloseHandle(reader);
            }
            std::error_code ignored;
            fs::remove_all(root, ignored);
        }
    } cleanup{root};
    const auto source = root / "staging";
    const auto target = root / "output";
    fs::create_directories(source);
    const auto payload = support::bytesFromText("verified staged bytes");
    support::writeBytes(source / "payload", payload);
    cleanup.reader =
        CreateFileW(source.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    REQUIRE(cleanup.reader != INVALID_HANDLE_VALUE);
    int attempts = 0;
    int waits = 0;
    bool release = false;
    SECTION("reader released between attempts commits the complete directory") {
        release = true;
    }
    SECTION("persistent reader preserves the staging bytes and fails boundedly") {}
    std::error_code error;
    cuexis::tools::detail::renameUnpackDirectoryWithRetry(
        source, target, error,
        [&](const auto& from, const auto& to, auto& result) {
            ++attempts;
            fs::rename(from, to, result);
            if (attempts == 1) {
                CHECK(result == std::error_code{ERROR_SHARING_VIOLATION, std::system_category()});
            }
        },
        [&] {
            ++waits;
            if (release && cleanup.reader != INVALID_HANDLE_VALUE) {
                REQUIRE(CloseHandle(cleanup.reader));
                cleanup.reader = INVALID_HANDLE_VALUE;
            }
        });
    CHECK(attempts == (release ? 2 : 8));
    CHECK(waits == (release ? 1 : 7));
    CHECK(static_cast<bool>(error) == !release);
    CHECK(fs::exists(source) == !release);
    CHECK(fs::exists(target) == release);
    CHECK(support::readBytes((release ? target : source) / "payload") == payload);
}
#endif
