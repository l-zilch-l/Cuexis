// F135: interned core::Error storage. The allocation probe replaces the global allocation
// functions for this executable, so each case can assert the exact number of allocations made by
// construction, copying and optional<Result> error-state delivery.

#include "../support/f135_allocation_probe.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/result.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

using cuexis::core::Error;
using cuexis::core::ErrorStage;

auto internedError() -> Error {
    return Error::interned(cuexis::core::intern("host.rpa.allocation_failed"),
                           cuexis::core::intern("Reserved allocation failure"), ErrorStage::query);
}

} // namespace

TEST_CASE("F135 interned Error keeps its fixed code, message and stage", "[core][error][f135]") {
    const auto error = internedError();

    REQUIRE(error.isInterned());
    REQUIRE(error.code() == "host.rpa.allocation_failed");
    REQUIRE(error.message() == "Reserved allocation failure");
    REQUIRE(error.stage() == ErrorStage::query);
    REQUIRE(error.context().empty());
    REQUIRE(error.cause() == nullptr);
    REQUIRE(noexcept(Error::interned(cuexis::core::intern("a"), cuexis::core::intern("b"))));
    REQUIRE(std::is_nothrow_move_constructible_v<Error>);
    REQUIRE(std::is_nothrow_move_assignable_v<Error>);
    REQUIRE(std::is_copy_constructible_v<Error>);
}

TEST_CASE("F135 interned Error construction and copy allocate nothing", "[core][error][f135]") {
    std::uint64_t attempts = 0;
    std::size_t contextSize = 0;
    std::string_view copiedCode;
    std::string_view copiedMessage;
    ErrorStage copiedStage = ErrorStage::none;
    bool copiedInterned = false;
    bool copiedHasCause = true;

    {
        // The owned placeholder is built before counting: Debug STL may allocate string/container
        // proxies for an owning string, which is irrelevant to the interned copy path.
        Error assigned{"placeholder", "placeholder"};
        const f135_probe::CountScope scope;
        const auto original = internedError();
        Error copy = original;
        assigned = copy;
        std::optional<Error> optionalCopy{assigned};
        contextSize = optionalCopy->context().size();
        copiedCode = optionalCopy->code();
        copiedMessage = optionalCopy->message();
        copiedStage = optionalCopy->stage();
        copiedInterned = optionalCopy->isInterned();
        copiedHasCause = optionalCopy->cause() != nullptr;
        attempts = f135_probe::attemptCount();
    }

    CHECK(attempts == 0);
    CHECK(copiedInterned);
    CHECK(copiedCode == "host.rpa.allocation_failed");
    CHECK(copiedMessage == "Reserved allocation failure");
    CHECK(copiedStage == ErrorStage::query);
    CHECK(contextSize == 0);
    CHECK_FALSE(copiedHasCause);
}

TEST_CASE("F135 optional Result error-state copy allocates nothing", "[core][error][f135]") {
    using Result = cuexis::core::Result<int>;
    std::optional<Result> source{std::in_place, cuexis::core::unexpected(internedError())};

    std::uint64_t attempts = 0;
    std::string_view code;
    ErrorStage stage = ErrorStage::none;
    bool interned = false;
    {
        const f135_probe::CountScope scope;
        std::optional<Result> copy;
        copy.emplace(*source);
        auto returned = [&] {
            std::optional<Result> local;
            local.emplace(*source);
            return local;
        }();
        code = returned->error().code();
        stage = returned->error().stage();
        interned = returned->error().isInterned();
        attempts = f135_probe::attemptCount();
    }

    CHECK(attempts == 0);
    REQUIRE_FALSE(source->has_value());
    CHECK(source->error().code() == "host.rpa.allocation_failed");
    CHECK(code == "host.rpa.allocation_failed");
    CHECK(stage == ErrorStage::query);
    CHECK(interned);
}

TEST_CASE("F135 interned Error move and copy stay usable under sustained OOM",
          "[core][error][f135]") {
    static_assert(std::is_nothrow_move_constructible_v<Error>);
    static_assert(std::is_nothrow_move_assignable_v<Error>);

    using Result = cuexis::core::Result<int>;
    auto original = internedError();
    std::optional<Result> source{std::in_place, cuexis::core::unexpected(original)};

    std::uint64_t attempts = 0;
    bool threw = false;
    std::string_view code;
    ErrorStage stage = ErrorStage::none;
    bool copyHasValue = true;
    {
        const f135_probe::FailScope scope;
        try {
            Error moved = std::move(original);
            code = moved.code();
            stage = moved.stage();
            std::optional<Result> copied = source;
            copyHasValue = copied->has_value();
        } catch (...) {
            threw = true;
        }
        attempts = f135_probe::attemptCount();
    }

    CHECK_FALSE(threw);
    CHECK(attempts == 0);
    CHECK(code == "host.rpa.allocation_failed");
    CHECK(stage == ErrorStage::query);
    CHECK_FALSE(copyHasValue);
}

TEST_CASE("F135 allocation probe fails every allocation and counts attempts",
          "[core][error][f135]") {
    std::uint64_t attempts = 0;
    bool threw = false;
    {
        const f135_probe::FailScope scope;
        try {
            void* value = ::operator new(8);
            ::operator delete(value);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        attempts = f135_probe::attemptCount();
    }
    CHECK(threw);
    CHECK(attempts == 1);
}

TEST_CASE("F135 owning Error construction remains compatible and may allocate",
          "[core][error][f135]") {
    Error error{"core.asset.load_failed", "The owning message survives a copy"};
    REQUIRE_FALSE(error.isInterned());
    REQUIRE(error.stage() == ErrorStage::none);
    REQUIRE(error.code() == "core.asset.load_failed");
    REQUIRE(error.message() == "The owning message survives a copy");

    error.withContext("path", "chart.cuexis");
    error.withCause(Error{"core.file.read_failed", "Read failed"});
    REQUIRE(error.context().size() == 1);
    REQUIRE(error.context()[0].key == "path");
    REQUIRE(error.cause() != nullptr);
    REQUIRE(error.cause()->code() == "core.file.read_failed");

    std::uint64_t attempts = 0;
    bool copiedInterned = true;
    std::optional<Error> copy;
    {
        const f135_probe::CountScope scope;
        copy.emplace(error);
        copiedInterned = copy->isInterned();
        attempts = f135_probe::attemptCount();
    }
    REQUIRE(copy);
    CHECK(attempts > 0);
    CHECK_FALSE(copiedInterned);
    CHECK(copy->code() == "core.asset.load_failed");
    CHECK(copy->context().size() == 1);
    CHECK(copy->cause() != nullptr);
}
