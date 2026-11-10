#pragma once

#include "../../engine/chart/src/packed_gameplay_internal.hpp"
#include "../../engine/gameplay_packed/src/semantic_stream_internal.hpp"
#include <algorithm>
#include <array>
#include <cuexis_internal/execution_diagnostics.hpp>
#include <cuexis_internal/sha256.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace pd5_capacity {
inline void jsonString(std::ostream& out, std::string_view text) {
    constexpr std::string_view hex = "0123456789abcdef";
    out << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\')
            out << '\\' << static_cast<char>(c);
        else if (c < 32)
            out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else
            out << static_cast<char>(c);
    }
    out << '"';
}

// Observation only: each success follows the real operation and its assertions.
// Catch assertion exceptions and Result failures retain the last attempted stage.
class Trace final {
  public:
    explicit Trace(bool enabled) : enabled_{enabled} {}
    void begin(std::string_view stage) {
        emit(stage, "begin");
    }
    void succeeded(std::string_view stage) {
        emit(stage, "succeeded");
    }
    template <class F> auto run(std::string_view stage, F operation) {
        emit(stage, "begin");
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
                operation();
                emit(stage, "succeeded");
            } else {
                auto result = operation();
                emit(stage, "succeeded");
                return result;
            }
        } catch (const std::exception& error) {
            emit(stage, "failed", error.what());
            throw;
        } catch (...) {
            emit(stage, "failed", "assertion or non-standard exception; see Catch log");
            throw;
        }
    }
    template <class T> auto take(cuexis::core::Result<T> result) -> T {
        if (!result) {
            if (enabled_) {
#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
                std::lock_guard outputLock(cuexis::internal::execution_diagnostics::outputMutex);
#endif
                for (const auto* error = &result.error(); error; error = error->cause()) {
                    std::cout << "PD5_C_ERROR {\"code\":";
                    jsonString(std::cout, error->code());
                    std::cout << ",\"message\":";
                    jsonString(std::cout, error->message());
                    std::cout << ",\"context\":[";
                    bool comma = false;
                    for (const auto& field : error->context()) {
                        if (comma)
                            std::cout << ',';
                        comma = true;
                        std::cout << "{\"key\":";
                        jsonString(std::cout, field.key);
                        std::cout << ",\"value\":";
                        jsonString(std::cout, field.value);
                        std::cout << '}';
                    }
                    std::cout << "]}" << std::endl;
                }
            }
            throw std::runtime_error(std::string{result.error().code()} + ": " +
                                     std::string{result.error().message()});
        }
        if constexpr (!std::is_void_v<T>)
            return std::move(*result);
    }

  private:
    void emit(std::string_view stage, std::string_view state, std::string_view message = {}) {
        if (!enabled_)
            return;
#if defined(CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY)
        std::lock_guard outputLock(cuexis::internal::execution_diagnostics::outputMutex);
#endif
        std::cout << "PD5_C_STAGE {\"stage\":";
        jsonString(std::cout, stage);
        std::cout << ",\"state\":";
        jsonString(std::cout, state);
        std::cout << ",\"message\":";
        jsonString(std::cout, message);
        std::cout << '}';
        std::cout << std::endl;
    }
    bool enabled_;
};

inline void identity(unsigned scale, unsigned requirements, std::string_view reuse) {
    const auto expected =
        cuexis::test_support::environmentVariable("CUEXIS_PD5_EXPECTED_SOURCE_SHA");
    const std::string_view compiled = CUEXIS_PD5_CAPACITY_SOURCE_SHA;
    REQUIRE(compiled.size() == 40);
    REQUIRE(expected == compiled);
    std::cout << "PD5_C_ID {\"schema\":\"pd5.stage-c.v1\",\"source_sha\":";
    jsonString(std::cout, compiled);
    std::cout << ",\"entities\":" << scale << ",\"requirements\":" << requirements
              << ",\"revision\":4,\"reuse\":";
    jsonString(std::cout, reuse);
    std::cout << ",\"pid\":" << GetCurrentProcessId() << ",\"lifetime_observer\":"
#if defined(CUEXIS_ALLOCATION_LIFETIME_OBSERVER)
              << "true"
#else
              << "false"
#endif
              << ",\"formal_measurement\":false}" << std::endl;
}

inline auto encodeR4(cuexis::test_support::SessionTestContent& fixture, Trace& trace,
                     unsigned scale, std::string_view reuse) -> std::vector<std::byte> {
    using namespace cuexis;
    namespace work = gameplay_packed::detail;
    const auto prepared = trace.take(judgement::prepareResolvedGameplay(fixture.request));
    work::HashObservation observed;
    // This is the same bounded implementation used by the public R4 Writer, with observation.
    auto bytes =
        trace.take(work::encodeBounded({fixture.chart,
                                        prepared,
                                        fixture.owners,
                                        {"normalization.one", "coordinator.policy.greedy_v1"},
                                        {},
                                        {},
                                        4},
                                       {}, {}, &observed));
    const auto stats = trace.take(chart::packed::gameplay_detail::inspect(bytes));
    const auto word = [&](std::size_t offset) {
        chart::packed::ByteReader reader{std::span{bytes}.subspan(offset, 4)};
        return trace.take(reader.readU32());
    };
    REQUIRE(word(88) == 4);
    REQUIRE(stats.entityCount == scale);
    REQUIRE(stats.requirementCount == scale);
    std::size_t section = 0;
    for (std::size_t i = 0; i < word(24); ++i)
        section = std::max(section, std::size_t(word(96 + i * 32 + 16)));
    REQUIRE(stats.packedBytes <= 16777216);
    REQUIRE(stats.decodedBytes <= 16777216);
    REQUIRE(section <= 16777216);
    REQUIRE(stats.stringCount <= 100000);
    REQUIRE(stats.referenceCount <= 100000);
    REQUIRE(stats.entityCount <= 40000);
    REQUIRE(stats.requirementCount <= 40000);
    REQUIRE(observed.budgetAccepted);
    REQUIRE(observed.shaComputations == 1);
    REQUIRE(observed.shaBytes <= 134217728);
    REQUIRE(observed.scratchHighWater <= 8388608);
    REQUIRE(observed.dynamicLiveBytes == 0);
    REQUIRE(observed.exportedBytes == 0);
    std::array<std::uint8_t, 32> digest{};
    std::transform(bytes.begin() + 32, bytes.begin() + 64, digest.begin(),
                   [](auto b) { return std::to_integer<std::uint8_t>(b); });
    const auto identity = core::detail::sha256Hex(digest);
    if (scale == 40000) {
        REQUIRE(bytes.size() == (reuse == "low" ? 6818424U : 6746664U));
        REQUIRE(stats.decodedBytes == (reuse == "low" ? 6817880U : 6746120U));
        REQUIRE(section == (reuse == "low" ? 3725942U : 3654182U));
        REQUIRE(stats.stringCount == 40048);
        REQUIRE(stats.referenceCount == 13);
        REQUIRE(observed.shaBytes == 99588531);
        REQUIRE(identity ==
                (reuse == "low"
                     ? "78c23507481084ba7aa900efd56f72065992d8ad2409f657fcf194784252038d"
                     : "e41eccde1593ca70b4b9c17696827f36b8884523d66e6c0ecb111a92ffc1cfcf"));
        const auto reference =
            test_support::environmentVariable("CUEXIS_PD5_R4_REFERENCE_DIRECTORY");
        REQUIRE_FALSE(reference.empty());
        std::ifstream in{std::filesystem::path{reference} /
                             ("40000-" + std::string{reuse} + ".r4.packed"),
                         std::ios::binary};
        REQUIRE(in);
        const std::string old{std::istreambuf_iterator<char>{in}, {}};
        REQUIRE(std::ranges::equal(std::as_bytes(std::span{old.data(), old.size()}), bytes));
    }
    const auto directory = test_support::environmentVariable("CUEXIS_PD5_R4_EVIDENCE_DIRECTORY");
    REQUIRE_FALSE(directory.empty());
    const auto path = std::filesystem::path{directory} / "input.r4.packed";
    REQUIRE_FALSE(std::filesystem::exists(path));
    std::ofstream out{path, std::ios::binary};
    REQUIRE(out);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
    std::cout << "PD5_C_PACKED {\"revision\":4,\"entities\":" << stats.entityCount
              << ",\"requirements\":" << stats.requirementCount
              << ",\"file_bytes\":" << stats.packedBytes
              << ",\"decoded_bytes\":" << stats.decodedBytes << ",\"max_section_bytes\":" << section
              << ",\"strings\":" << stats.stringCount << ",\"references\":" << stats.referenceCount
              << ",\"semantic_digest\":";
    jsonString(std::cout, identity);
    std::cout << ",\"semantic_bytes\":" << observed.shaBytes
              << ",\"scratch_peak\":" << observed.scratchHighWater
              << ",\"sha_computations\":" << observed.shaComputations
              << ",\"four_hash_limits_accepted\":true,\"ix_limit\":1000000,\"key_limit\":4096,"
                 "\"budgets_relaxed\":false}"
              << std::endl;
    return bytes;
}
} // namespace pd5_capacity
