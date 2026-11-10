# R9 gate policy impact table

Static audit of the two reference host gate scripts, closing the audit items left
open by section 4 of [R9-gate-policy-audit-decisions.md](R9-gate-policy-audit-decisions.md)
(D11, D14, D15).

Audited subject: `cmake/VerifyReferenceHost.cmake` (621 lines) and
`cmake/VerifyReferenceHostCommands.cmake` (773 lines), at the revision that
declares `cmake_minimum_required(VERSION 3.25)` at the entry point.

Policy text is quoted from the `cmake-policies(7)` reference shipped with CMake
3.28.3, extracted to `out/policies-3.28.txt` (5,676 lines, 156 policy sections).

## Why the policies matter here at all

The gate is a CMake **script-mode** process (`cmake -P`). The
`cmake_minimum_required()` in the root `CMakeLists.txt` belongs to a different
process and has no effect on it. Before this work, nothing in the script
established a policy baseline, so every policy that a 3.x CMake still defaults to
`OLD` was `OLD` for the gate — while the same script on a 4.x CMake behaved
differently, because 4.x has no `OLD` behaviour to select. That is the mechanism
behind three failures that reached the hosted Linux runners and never reproduced
on the development machine.

## The table

`Depends?` means: would flipping this policy from `NEW` to `OLD` change what the
gate does?

| Policy | Since | What it controls | Depends? | Evidence |
|---|---|---|---|---|
| **CMP0007** | 2.8 | `list()` no longer ignores empty elements | **Yes — this was one of the two defects** | `VerifyReferenceHostCommands.cmake` uses the affected read path in FIND x1, GET x20, JOIN x1, LENGTH x16, REMOVE_DUPLICATES x3, SORT x7. The trigger was a declaration ending in `\|`, whose flags field is legitimately empty. See the trace below. |
| **CMP0057** | 3.3 | `IN_LIST` is an `if()` operator | **Yes — this was the other defect** | The gate no longer contains `IN_LIST` anywhere. The independent parser test uses it deliberately, as a probe that the baseline is in effect. |
| CMP0054 | 3.1 | Quoted or bracketed `if()` arguments are not dereferenced | No | 0 candidate sites. Every `if()`/`elseif()`/`while()` in both files carries an operator keyword; there is no `if(${...})` that expands a variable into the condition itself. |
| CMP0053 | 3.1 | Variable-reference and escape-sequence evaluation corner cases | No | No `${}` form affected by the simplification is used: no `@`-to-`${}` rewriting, no escapes inside variable names or nesting. |
| CMP0064 | 3.4 | `TEST` is an `if()` operator | No | `TEST` is not used as an operator in either file. |
| CMP0121 | 3.21 | `list(GET/INSERT/SUBLIST/REMOVE_AT)` detect invalid indices | No | `INSERT`, `SUBLIST`, `REMOVE_AT` are not used. The 20 `GET` sites index either a literal or a value derived from an arity check, so no invalid index can be constructed. |
| CMP0124 | 3.21 | A `foreach()` loop variable is restored, not left at its last value | No | Analysed explicitly, see below. |
| CMP0130 | 3.24 | `while()` diagnoses condition-evaluation errors | No | Exactly one `while()` site, `:667`: `while(cuexis_seen_index LESS cuexis_seen_total)`. Two bare variable names and an operator cannot produce an evaluation error. |
| CMP0139 | 3.24 | `PATH_EQUAL` is an `if()` operator | No | `PATH_EQUAL` is not used. |

## CMP0007 trace: can any remaining input hold an empty element?

The read path is only affected when the list actually contains an empty element,
so each affected call was traced to its input. Three input shapes exist:

1. **`string(REGEX MATCHALL ...)`** — `separators` (`:98`), `case_frames`
   (`:450`), `occurrences` (`:618`). A regex match is never the empty string, so
   these lists cannot hold an empty element. Inputs to `LENGTH`/`GET` over them
   are safe under either policy.
2. **`separate_arguments(...)`** — `line_parts` (`:480`), whose length is read at
   `:482` and indexed at `:481`, `:515`, `:525`, `:533`, `:541`, `:585`, `:586`,
   `:611`. `separate_arguments` splits on unquoted whitespace and does not emit
   empty elements for runs of separators, so again the list is clean.
3. **`string(REPLACE "|" ";" ...)`** — `fields` (`:103`, `:314`). This is the one
   shape that *can* produce a trailing empty element, because a declaration is
   allowed to end in `|`. It is the original defect. It is now handled two ways:
   the shape is validated by counting separators with `MATCHALL` first (`:98` to
   `:102`), independently of any list semantics; and the flags field is read by
   anchored pattern (`:313`) rather than by `list(GET fields 2)`. The two `GET`
   calls that remain on `fields` (`:104`, `:105`) index positions 0 and 1, which
   exist under either policy.

`list(REMOVE_DUPLICATES)` (`:290`, `:685`, `:688`) and `list(SORT)` (`:143` to
`:146`, `:736` to `:738`) operate on lists built from `MATCHALL`, `GET` of the
above, or `set()` literals, and are used for set comparison and reporting rather
than for anything whose count is asserted.

**Conclusion.** With the baseline declared, no call in either script depends on
`CMP0007` being `OLD`, and the one input that genuinely carried an empty element is
now validated without relying on list semantics at all. `CMP0007` is recorded as a
policy the gate *would* have depended on, not one it depends on now.

This is also why removing the baseline from the **current** scripts does not
reintroduce a failure: the three defects were each worked around before this
audit. The baseline removes the condition that produced them; it is not what makes
today's code pass.

## CMP0124 analysis

`CMP0124` is the one policy in the table that could plausibly bite a script full of
`foreach` loops, so it was checked rather than dismissed. A scope-aware scan
(`out/cmp0124-audit.py`) looked for every read of a `foreach` loop variable after
its `endforeach()`, excluding reads inside another loop declaring the same name and
inside nested `function`/`macro` bodies. It reported 11 candidate reads; all 11 are
false positives on inspection:

* six are text inside comments (`library`, `installed`, `candidate`, `index`,
  `source`),
* three are words inside diagnostic string literals (`name` in "lower-case name",
  `failure` in "failure(s)"),
* one is a **function parameter list** (`function(cuexis_command_record_failure
  case_id message)`), which is a different scope entirely.

Every loop variable in both files is read only within its own loop, and the
variables read after a loop (`shown_count`, `failure_count`, `expected_count`) are
assigned by `set()` and `math(EXPR)`, not by a loop header. `CMP0124` therefore has
no behavioural effect on either script.

## Decisive experiment: the baseline on the original script

Recorded to D14's standard, because it is the only direct evidence that the
baseline is the fix rather than a precaution.

| | |
|---|---|
| **Subject** | `cmake/VerifyReferenceHostCommands.cmake` as of `2eed5c9` (before any of the three targeted fixes) |
| **Tool** | `out/tools/cmake-3.28.3-windows-x86_64/bin/cmake.exe`, `cmake version 3.28.3`, official Kitware Windows binary |
| **Control** | this machine's `cmake version 4.3.3`, where the policies are `NEW` unconditionally |
| **Invocation** | the exact command line CTest registers for `cuexis_reference_host_staging`, run with `-P cmake/VerifyReferenceHost.cmake` |
| **Input range** | the 41 shipped command cases, unmodified |

* **Without** the baseline: fails at `VerifyReferenceHostCommands.cmake:108` with
  `Malformed case declaration 'c01-absolute-anchor|cmd|': expected
  <id>|<kind>|<flags>` — the same file, line and message as the hosted Linux
  failure.
* **With only** `cmake_minimum_required(VERSION 3.25)` added and nothing else
  changed: `expected 41, completed 41, passed 41`.

**Falsifier.** If the baseline were not the operative difference, adding that one
line could not have changed the outcome. It did.

**Uncovered scope.** This shows the baseline suffices for the three observed
defects on 3.28.3. It does not show that the gate is correct on every 3.x version
below 3.28.3, and it does not exercise `VerifyPlayerDistribution.cmake`, which
still has no policy baseline (D13, a separate follow-up).

## CMP0054 scope note

The audit reports 0 candidate sites for `CMP0054`, which is a stronger statement
than "no `if("${var}")` found". The check was implemented as: every
`if()`/`elseif()`/`while()` argument list that contains a quoted argument must also
contain a recognised operator keyword. Sites with a quoted argument and no operator
are the ones where `OLD` and `NEW` can diverge. Both files report zero. The two
files were also grepped for a variable expanded directly into the condition
(`if(...${`), the form that produced an earlier, unrelated runner failure; every
match has an operator immediately after the parenthesis (`NOT DEFINED`,
`NOT EXISTS`, `MATCHES`, `STREQUAL`).

## Reproducing this audit

```
python -B out/policy-audit.py       # the table's evidence: list() use, CMP0054 sites, construct census
python -B out/cmp0124-audit.py      # the CMP0124 post-loop read scan
cmake -P cmake/VerifyReferenceHostCommandParser.cmake                        # 4.3.3: passes
out/tools/cmake-3.28.3-windows-x86_64/bin/cmake.exe -P cmake/VerifyReferenceHostCommandParser.cmake   # 3.28.3: passes
```

The two Python auditors live in `out/`, which is not tracked, so they are working
tools rather than committed evidence. The findings they produced are recorded
above with the line numbers needed to re-derive them by hand.
