// Negative installed-SDK probe. The default prefix neither installs this header nor exposes the
// candidate Gameplay surface, so compiling this unit against that prefix must fail at the include.
#include <cuexis/playback/experimental/gameplay.hpp>

namespace {
[[maybe_unused]] constexpr int experimentalHeaderNegativeProbe = 1;
}
