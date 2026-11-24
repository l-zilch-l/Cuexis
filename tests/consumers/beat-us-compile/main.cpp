//  Independent source compile consumer.
//
//  It is configured as its own CMake project and links only the compile side plus the shared
//  timing map. It sees the shared owning-product header and the author compiler header, so a
//  compile-side change that leaks a Chart, Reader, Packed or Playback dependency into those headers
//  breaks this consumer instead of staying hidden inside the tree.

#include <cuexis/judgement/beat_us_compile.hpp>
#include <cuexis/judgement/compiled_gameplay_rows.hpp>
#include <cuexis/timing/timing_map.hpp>

#include <cstdint>
#include <iostream>
#include <type_traits>

int main() {
    cuexis::judgement::BeatGameplayProduct product;
    static_assert(std::is_same_v<std::decay_t<decltype(product.rows)>,
                                 cuexis::judgement::CanonicalGameplayRows>);
    static_assert(std::is_same_v<std::decay_t<decltype(product.requirements)>,
                                 std::vector<cuexis::judgement::runtime::PreparedRequirement>>);

    //  Referencing the entry point forces the compile-side symbol into the link closure without
    //  requiring a full author fixture.
    using Compile = cuexis::core::Result<cuexis::judgement::BeatGameplayProduct> (*)(
        const cuexis::judgement::BeatGameplaySourceView&, const cuexis::timing::TimingMap&,
        const cuexis::judgement::GameplayCompileContext&);
    const Compile compile = &cuexis::judgement::compileBeatGameplay;
    if (compile == nullptr) {
        return 1;
    }
    if (!product.rows.gpr0.empty() || !product.beatKeys.empty() || !product.requirements.empty()) {
        return 2;
    }
    std::cout << "beat-us compile consumer: rows/beatKeys/requirements empty, entry linked\n";
    return 0;
}
