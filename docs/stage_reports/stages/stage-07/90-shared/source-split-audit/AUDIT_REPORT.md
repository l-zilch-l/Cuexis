# Independent audit of the source-split safety net

Repository: `D:\Cuexis-redesign`, branch `Cuexis-redesign`, HEAD `562f54d3`.
Every check below was written for this audit. Nothing under `out/` was read or executed
except writing these logs; no repository helper script was used. All temporary edits were
restored with `git checkout --`; the final tree is clean (`git status --porcelain` and
`git diff` empty).

## A. Can a forward declaration satisfy the gate?

| experiment | edit | gate exit | diagnostic |
|---|---|---|---|
| baseline | none | 0 | (silent) |
| A1 | delete `struct AmountSpec` definition | 1 | `VerifyArchitecture.cmake:196 ... Judgement S7A-2 input boundary declaration is missing: struct AmountSpec` |
| A2 | replace definition with `struct AmountSpec;` | 1 | same as A1 |
| A3 | rename unchecked `QuantizedAmount` -> `QuantizedAmountProbe` | 0 | (silent) |
| A4 | wrap definition in `#if 0` / `#endif` | 0 | (silent) |
| A5 | rename checked type `AmountSpec` -> `AmountSpecification` (longer name) | 0 | (silent) |
| A6 | delete `quantizeAmount` declaration, leave the name in a comment | 0 | (silent) |
| A7 | rename checked enum `ReferenceClosure` -> `ReferenceClosureMode` | 0 | (silent) |
| A9 | replace type definition with a comment containing `struct AmountSpec ... {` | 0 | (silent) |
| A10 | replace `class PreparedGrace` definition with `class PreparedGrace;` | 1 | `VerifyArchitecture.cmake:296 ... Judgement S7A-3 canonical graph declaration is missing: class PreparedGrace` |
| A8/final | restored tree | 0 | (silent) |

Conclusion: the gate does **not** accept a bare forward declaration for a type entry (the
0a78e56c fix works for `struct` and `class`), but it does not require a definition either.
It requires the token sequence `struct|class|enum class <Name>` followed by `{` with no
`;`, `{` or `}` in between, anywhere in the concatenated text of the partition files.
That is satisfied by a comment, by `#if 0`-disabled code, and by any longer identifier
sharing the prefix (`AmountSpecification`, `ReferenceClosureMode`). Function entries are
plain substrings, so a comment mention is enough.

## B. Does the gate check reachability?

Removed `#include "input_boundary_mapping.hpp"` from
`engine/judgement/include/cuexis/judgement/input_boundary.hpp`.

- architecture gate: **exit 0** (silent).
- consumer TU including only `<cuexis/judgement/input_boundary.hpp>` and naming
  `InputMappingProfile`: **exit 2**, `probe_mapping.cpp(6): error C2079: 'profile' uses
  undefined struct 'cuexis::judgement::InputMappingProfile'` (plus errors inside the
  umbrella because it uses `InputAction`/`ChannelRef`/`SourceClass`/`IngressSequence`
  from the removed part).
- `cmake --build --preset debug --target cuexis_judgement_tests`: **exit 2**, first error
  `engine/judgement/src/execution_profile.cpp ... input_boundary.hpp(160): error C3646`.
- restored: probes and gate exit 0.

Conclusion: the gate reads the partition headers directly and never looks at the
umbrella's include graph. It passes while the public umbrella is unbuildable. The only
other catcher is the C++ compiler (the library and the judgement tests include the
umbrella). There is no CTest or CMake check for umbrella->partition reachability and no
automated standalone-header compilation; the recorded "27 split headers compile
standalone" claim is not a gate.

## C. Did the split preserve semantics?

Method: byte-exact pre-split blobs from git (`git show <parent>:<path>`), then
(a) a comment/string-aware C++ tokenizer written for this audit (token multiset and
ordered token-stream comparison), (b) normalised-line coverage (comments, blank lines and
preprocessor lines removed; whitespace collapsed), (c) brace-matched function-body
comparison keyed by name/arity with defaults, qualifiers, `static`, `inline` and
`override` normalised away, and (d) for the header split, a clang AST declaration
inventory (`clang++ -Xclang -ast-dump`, declarations filtered by originating file).
Corroborated by compile-and-run of the existing test binaries after a full Debug build.

### C1. `engine/chart/src/packed_chart_tables_decoder*`
Old: 964 lines, 890 normalised lines, 8872 tokens, one `decodeImpl` function.
New: 6 files, 998 normalised lines, 10322 tokens, 12 stage methods plus helpers.
- 818/890 old normalised lines present verbatim; the other 72 are all the mechanical
  translation lines: `auto x = header.readU32()` -> `auto xField = ...` plus
  `this->x = *xField` (validated once, then stored as a plain member), `*entityCount`
  -> `entityCount`, `result` -> `step` in the stage driver, lambda `[&]` -> member
  function, `std::vector<T> v(n)` -> `v.resize(n)` / `v.assign(n, 0)`, `return chart`
  -> `return std::move(chart)`, `semanticHash->data()` -> `semanticHash.data()`.
- Token multiset deficit 50/8872 (0.56%): 28 `*` (the Result-deref removals), 9 `result`
  (renamed `step`), 3 `&` (lambda captures), and the member identifiers rewritten by the
  same translation. No operator, literal, condition or callee disappeared.
- Stage alignment 12/12, in the same order as the old function (header, strings, refs,
  meta, timing, identity, archetypes, entities, TRN0/REN0/CAM0, constraints,
  requirements, finish). Every new-only line is scaffolding/assignment listed above.
- Tests: `cuexis_chart_tests` 227 cases / 42435 assertions pass; `cuexis_gameplay_packed_tests`
  51 cases / 5641 assertions pass.

### C2. `engine/gameplay_packed/src/gameplay_graph_reader*`
Old: 1037 normalised lines, 7764 tokens, class bodies inline.
New: umbrella + two sink headers; 1055 normalised lines, 8055 tokens.
- 40/40 extracted function bodies are **token-identical** (same tokens, same order),
  including all 21 out-of-line `GraphSink::` definitions.
- 0 token deficit. 29/1037 old lines absent, every one a declaration line changed by the
  out-of-line move (`static auto names(...)` -> `auto GraphSink::names(...)`, `override`
  dropped from the definition, one condition and two argument lists reflowed).
- Tests: `cuexis_gameplay_packed_tests` 51 cases / 5641 assertions pass.

### C3. `tests/presentation/validation_sink*`
Old: 1402 normalised lines, 12609 tokens, one `.cpp`.
New: 2 `.cpp` + 4 headers, 1423 normalised lines, 12723 tokens.
- 64/64 function bodies **token-identical**.
- 0 token deficit. 34/1402 old lines absent, all declaration lines moved to headers
  (`inline` added) or the anonymous `namespace {` line.
- The type/function inventory has no missing entry; the three apparent "members missing"
  (`addError`, `assignReference`, `hashCommand`) were false negatives of the first
  extractor (they are `inline` definitions in the new headers, verified by grep).
- The split file contains no `TEST_CASE`; `validation_sink_tests.cpp` was not touched by
  the split commit, so the 9 test cases / 261 assertions are unchanged by construction.
- Tests: `cuexis_presentation_validation_tests` 9 cases / 261 assertions pass;
  `cuexis_playback_tests` 131 cases / 17188 assertions pass.

### C4. `engine/judgement/include/cuexis/judgement/gameplay_graph*`
Old: 632 normalised lines, 3977 tokens, one header.
New: umbrella + 4 partition headers, 4297 tokens.
- 0 token deficit; **632/632** old normalised lines present (the extra tokens are the
  added includes/namespace wrappers).
- clang AST declaration inventory filtered by originating file: **444 old declarations,
  444 new declarations, empty set difference** (records, enums, methods, fields,
  functions with signatures).
- Tests: `cuexis_judgement_tests` 236 cases / 56139 assertions pass.

No line, declaration, function body or test disappeared, and no condition/callee/literal
changed meaning in the four splits. One non-semantic technical note: the moved
`GraphSink` member definitions are no longer implicitly inline, but the fragment already
contained non-inline free functions and is included by exactly one TU, so the practical
multi-TU property did not change.

## D. Do the automated gates run and pass?

| command | exit |
|---|---|
| `python -B tools/verification/check_gameplay_diagnostic_formalization.py --self-test` | 0 |
| `python -B tools/check_docs.py` | 0 (`652 Markdown files and 20 candidate JSON/CXT files validated`) |
| `ctest --preset debug -R architecture --output-on-failure` | 0 (1/1 passed) |
| direct `cmake -P cmake/VerifyArchitecture.cmake` after restore | 0 |

### What the formalization gate validates
It reads a fixed mapping JSON, `diagnostic_policy.hpp`, `source_codes.hpp`,
`execution_kernel*.cpp`, `execution_profile.cpp`, `gameplay_internal.hpp`,
`publication.cpp`, the diagnostic fixture manifest, and scans
`engine/app/tools/tests/cmake/schemas` for `.cpp/.hpp/.cmake/.json`. It asserts exact
finite membership (no missing/extra/duplicate) of three code arrays against the mapping,
per-symbol constant mappings, that live private codes are registered, that dynamic reason
suffixes have a finite policy, that the Playback projection uses and the publication
bridge delegates to `classifyPublicDiagnostic`, that no active `judgement.s7a[234].` /
`judgement.provisional.` spelling survives (comments stripped, strings preserved), that
the two negative-fixture exceptions occur exactly once, and that historical fixture
hashes are unchanged. The `--self-test` copies the tree, runs the check, then applies six
mutations and proves each makes CTest fail through the CMake wrapper.

### Can it pass while checking nothing?
Not literally: the core files are read unconditionally (a missing one raises and exits
non-zero), and emptying the mapping JSON alone makes it fail with eleven failure lines
(`legacyDiagnosticCodes...`, `categoryProjectedDiagnosticCodes...`, `live private
diagnostic collision...`, and eight per-unit `dynamic reason has no finite policy` lines).
But several checks are existence-dependent and silently disappear:
- moving `app/`, `schemas/`, `tools/` and `cmake/` aside: gate still passes (the
  directory scan is skipped for missing directories);
- deleting `schemas/cuexis.gameplay-diagnostics.v2.codes.json`: gate still passes (the
  schema is only scanned if present);
- moving every `execution_kernel*.cpp` aside: gate still passes; the finite-policy loop
  then checks only `execution_profile.cpp` because the glob silently shrinks.
The `--self-test` does not cover any of these vacuity cases.

## Real gaps found

1. **The type checks accept text, not definitions.** A comment containing
   `struct AmountSpec ... {` (A9), an `#if 0`-disabled definition (A4), and a longer
   identifier (`AmountSpecification`, A5; `ReferenceClosureMode`, A7) all leave the gate
   green. The regex `Name[^;{}]*{` has no identifier boundary and does not skip comments
   or preprocessor-disabled text.
2. **The function checks are plain substrings.** Deleting the `quantizeAmount`
   declaration while its name survives in a comment keeps the gate green (A6). It never
   checks that a declaration exists.
3. **No reachability check.** Removing the umbrella's include of a partition header
   leaves the gate green (B) while the umbrella and its consumers fail to compile. The
   only catcher is the compiler; there is no CTest/CMake header self-containment or
   umbrella-include check, and the "27 headers compile standalone" claim is not automated.
4. **The partition is one concatenated string.** A declaration in any listed file (or a
   match spanning a file boundary) satisfies every entry; the gate cannot enforce that
   the header it names owns the declaration.
5. **The formalization gate's coverage can be silently disabled** by removing/renaming
   inputs: missing scan directories, missing codes schema, or a shrunken
   `execution_kernel*.cpp` glob all pass. It is a real check when the inputs exist, but
   the self-test does not prove those dimensions cannot vanish.

## What I could not verify

- The recorded "514 pre-split namespace-scope declarations all present" figure: I did not
  reproduce the implementer's counting method. My independent counts are 444 AST
  declarations for the graph header, 40/40 function bodies for the reader, 64/64 for the
  validation sink, and 12/12 decoder stages.
- The full Release build and the full 1211-test CTest matrix (the recorded claim). I ran a
  full Debug build (exit 0), the architecture CTest, and the five affected test binaries
  (all pass), not the Release matrix.
- Cross-platform (Linux/MinGW) and external/shared-package consumers.
- Differential fuzzing of old vs new decoder on malformed inputs. I reviewed the
  translation line by line and ran the existing chart/gameplay test corpora; a
  byte-for-byte differential harness was not built.
- The other split commits (timebase, pattern_dfa, input_boundary, assembler, etc.) were
  not re-derived; only the four chosen splits were.

## Logs

All under `out/audit2/` (gate runs `gate_*.log`, CTest/build `D3_*.log`, `build_*.log`,
test runs `C_*.log`, comparison reports `c/report_*.txt`, `c/bodies_*.json`,
`c/inventory_graph.json`, `c/new_only_decoder.txt`, vacuity runs `D1_vacuity_*.log`).
