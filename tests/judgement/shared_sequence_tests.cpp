#include <catch2/catch_test_macros.hpp>
#include <cuexis/judgement/shared_sequence.hpp>

#include <stdexcept>
#include <type_traits>

using cuexis::judgement::detail::SharedSequence;
namespace {
struct Payload {
    static inline unsigned copies{};
    static inline bool fail{};
    int value{};
    explicit Payload(int v) : value(v) {}
    Payload(const Payload& other) : value(other.value) {
        if (fail)
            throw std::bad_alloc{};
        ++copies;
    }
    Payload(Payload&&) noexcept = default;
    auto operator=(const Payload&) -> Payload& = default;
    auto operator=(Payload&&) noexcept -> Payload& = default;
    friend auto operator==(const Payload&, const Payload&) -> bool = default;
};
} // namespace
TEST_CASE("Shared sequence preserves old owners and copies only a changed page",
          "[judgement][redesign][ownership]") {
    SharedSequence<Payload, 4> source;
    for (int i = 0; i < 11; ++i)
        source.emplace_back(i);
    Payload::copies = 0;
    auto branch = source;
    CHECK(Payload::copies == 0);
    CHECK(&branch[5] == &source[5]);
    branch.update(5, [](auto& p) { p.value = 50; });
    CHECK(Payload::copies == 5);
    CHECK(source[5].value == 5);
    CHECK(branch[5].value == 50);
    CHECK(&branch[1] == &source[1]);
    CHECK(&branch[9] == &source[9]);
    CHECK_FALSE(branch == source);
    branch.update(5, [](auto& p) { p.value = 5; });
    CHECK(branch == source);
    auto independent = SharedSequence<Payload, 4>{};
    for (int i = 0; i < 11; ++i)
        independent.emplace_back(i);
    CHECK(independent == source);
    auto retained = source;
    source.clear();
    CHECK(retained[10].value == 10);
    retained.push_back(retained[0]);
    CHECK(retained.size() == 12);
    CHECK(branch.size() == 11);
    CHECK(retained.back().value == 0);
    static_assert(std::is_same_v<decltype(branch[0]), const Payload&>);
    static_assert(std::random_access_iterator<decltype(branch.begin())>);
}
TEST_CASE("Shared sequence callback can capture a copy or throw without leaking writes",
          "[judgement][redesign][ownership][fault]") {
    SharedSequence<int, 4> current{1, 2};
    SharedSequence<int, 4> captured;
    current.update(0, [&](int& value) {
        captured = current;
        value = 9;
    });
    CHECK(captured[0] == 1);
    CHECK(current[0] == 9);
    CHECK_THROWS_AS(current.update(0,
                                   [](int& value) {
                                       value = 12;
                                       throw std::runtime_error{"callback failed"};
                                   }),
                    std::runtime_error);
    CHECK(current[0] == 9);
    CHECK(captured[0] == 1);
}
TEST_CASE("Shared sequence failed detachment leaves published pages intact",
          "[judgement][redesign][fault]") {
    SharedSequence<Payload, 4> source;
    for (int i = 0; i < 5; ++i)
        source.emplace_back(i);
    auto branch = source;
    Payload::fail = true;
    CHECK_THROWS_AS(branch.update(1, [](auto& p) { p.value = 99; }), std::bad_alloc);
    Payload::fail = false;
    CHECK(branch == source);
    CHECK(source[1].value == 1);
    CHECK(&branch[1] == &source[1]);
    branch.update(1, [](auto& p) { p.value = 99; });
    CHECK(source[1].value == 1);
    CHECK(branch[1].value == 99);
}

TEST_CASE("Shared sequence scalar writes preserve captures and failed detachment",
          "[judgement][redesign][ownership][fault]") {
    SharedSequence<Payload, 4> source;
    for (int i = 0; i < 5; ++i)
        source.emplace_back(i);
    auto branch = source;
    Payload::copies = 0;
    branch.assignScalar(1, &Payload::value, 99);
    CHECK(Payload::copies == 4);
    CHECK(source[1].value == 1);
    CHECK(branch[1].value == 99);
    CHECK(&branch[4] == &source[4]);

    // No row copy is needed after this page is exclusive, even if row copies throw.
    Payload::fail = true;
    CHECK_NOTHROW(branch.assignScalar(2, &Payload::value, 88));
    const auto captured = branch.storageOwner();
    REQUIRE(captured);
    CHECK_THROWS_AS(branch.assignScalar(1, &Payload::value, 77), std::bad_alloc);
    Payload::fail = false;
    CHECK(branch[1].value == 99);
    CHECK(branch[2].value == 88);
    CHECK(source[2].value == 2);
    CHECK_THROWS_AS(branch.assignScalar(5, &Payload::value, 77), std::out_of_range);
    const auto retained = branch;
    branch.assignScalar(1, &Payload::value, 77);
    CHECK(retained[1].value == 99);
    CHECK(branch[1].value == 77);
}
