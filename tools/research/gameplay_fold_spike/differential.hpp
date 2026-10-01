// Research spike: differential test. The runtime implements the standard entries by hand, so
// nothing yet proves the compiled Pattern means the same thing. This checks the compiled DFA
// against a direct predicate written from the entry's stated rule, over sampled traces.
// Not part of the product build. See README.md in this directory.
#pragma once

#include <cstdint>
#include <vector>

namespace spike::diff {

// Per-sample alphabet, matching main.cpp's pattern section.
enum Symbol : int { Press = 0, Release = 1, HeldSymbol = 2, NotHeld = 3, Idle = 4 };
inline constexpr int kAlphabet = 5;

enum class Entry : std::uint8_t { Tap, HoldBody, Bomb, Roll };

struct Spec {
    Entry entry = Entry::Tap;
    int windowSteps = 0; // Tap / Bomb / Roll: length of the judged window in samples
    int graceSteps = 0;  // HoldBody: largest permitted run of not-held samples
    int repeatCount = 0; // Roll: number of presses required
};

struct Mismatch {
    std::vector<int> trace;
    bool compiled = false;
    bool reference = false;
};

struct Report {
    int cases = 0;
    std::vector<Mismatch> mismatches;
    bool ok() const {
        return mismatches.empty();
    }
};

// Enumerates traces for the entry and compares the compiled DFA with the reference predicate.
Report check(const Spec& spec, int maxSteps);

} // namespace spike::diff
