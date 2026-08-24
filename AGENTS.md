#Cuexis Agent Guide

##Product direction

    ADR 0027 defines Cuexis as an embeddable** Cuexis Playback SDK** with two independent
        applications : Cuexis Player and Cuexis Studio.RuntimeSession,
    World, EnTT, SDL and OpenGL are internal / optional implementation details;
external hosts use PlaybackSession, ContentProvider, RuntimeFrame,
    FrameSnapshot and later Judgement /
            Replay contracts
                .

            The current baseline includes a functioning `cuexis_playback` module(`PlaybackSession`,
`FrameSnapshot`, `RuntimeFrame`, `IContentProvider`)
                .SDK API `0.6.0` supports static and matching
        - toolchain C++ shared packages,
    versioned public libraries, clean staged consumers, compatibility rejection gates,
    PlaybackSource, FrameDigest v1 - v3,
    and Portable Presentation v1.Filesystem / Memory / Host ContentProvider support,
    ChartClock / HostClock / CuexisAudio, RuntimeTimeline, Prepared Playback, `cuexis_audio`,
    and optional `cuexis_audio_sdl` are active.Stage 1C review findings R01 - R21 are closed.Stage
                                                                              2 delivered Chart v3,
    TimingMap, Behavior / Step Event, migration,
    and FrameDigest v2.Stage 3 delivered portable resources,
    candidate / active presentation transactions,
    Validation Sink, the OpenGL adapter, Player rendering, external package consumers,
    and cross - platform closure.Stable C ABI work remains in Stage 12;
`cuexis_judgement` remains planned for Stage 11.

The active implementation stage is **Stage Chart Format Update**, positioned between Stage 3 and
Stage 4;
do not call it Stage 3.5. ADR 0038 defines `.cxc` as a strict ZIP32 Stored exchange
package containing existing Project/Asset Index formats, `cuexis.chart` v4 data, CXT JSON, and
required resources. The CXT v1, ChartParameter, and Template Binding subdecision was accepted on
August 10, 2026: `.cxt` is UTF-8 JSON for one declarative local-time animation template, and host
parameters are frozen before prepare lowering. ADR 0038 was accepted on August 11, 2026. CFU-C1
Schema, fixtures, typed source Readers, and the internal manifest target are complete. CFU-C2
canonical Writers, parameter resolution/identity, CXT import, deterministic lowering, resource
closure, capability derivation, and aggregate budget gates are complete. CFU-C3 delivered the
internal `cuexis_cxc` strict ZIP32 Stored
Reader/Writer, manifest and project closure validation, owning file/memory packages,
package-backed Asset ContentProvider, separate Chart/CXT project-document table, exact package
identity, binary fixtures, and static/shared package leakage gates. CFU-C4 delivered developer
pack/validate/unpack tools. CFU-E0 froze the public API and version direction; CFU-E1 unified
PlaybackSource state, added owning typed project-document and CXC file/memory factories, and moved
the preview SDK to `0.6.0`. CFU-E2 added prepare options, Chart v4 resolution and format capability
preflight, with static v4 projected into the existing Runtime and nonempty animation rejected.
**CFU-C0/C1/C2/C3/C4 complete; CFU-D owner-accepted and closed after “no external assets”;
CFU-E owner-accepted and closed; CFU-F closed after final-SHA hosted verification; CFU-G is
active. G0 status calibration, G1 exit audit, and G2 Stage 4 typed handoff are complete. G3 local
candidate gates passed on August 19, 2026, but candidate publication and same-SHA hosted
Linux/MSVC/MinGW evidence are blocked; G3 is not complete. G4 offline closure readiness was
completed on August 20, 2026 without waiving G3, creating a completion report, or unblocking
Stage 4.**
This checkpoint must not be described as complete CXC, a public package API, complete v4 animation
Playback, or CFU-G complete.
Stage 4 must consume
typed data produced by the format stage and must not parse JSON/CXC/CXT in `engine/animation/`.
Runtime scripts and per-frame script callbacks are deferred indefinitely. No scheduled stage,
Chart/CXT/CXC field, extension, capability, bytecode, ABI, or Playback execution hook is reserved
for them. Offline authoring generators are a separate future tooling discussion and are never
implicitly executed by pack, prepare, or Playback.

## Build

```powershell
#Debug(standard daily workflow)
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error

#Release(before merging / versioning)
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
```

**Prerequisites**: `VCPKG_ROOT` env var must point to vcpkg root. Run from a Visual Studio Developer shell (or ensure `cl.exe`, Ninja, CMake in PATH).

Note: `CUEXIS_WARNINGS_AS_ERRORS` is `OFF` in the base preset. Warnings do not fail the build by default.

**Version-change builds**: Run `python -B tools/update_version.py yy.mm.dd-v` to update the
canonical version and vcpkg manifest together. Use `python -B tools/update_version.py --check`
for read-only validation. After a version change, use `--fresh` + `--clean-first` to avoid stale
generated headers.

### Format check

```powershell
cmake --build --preset debug --target cuexis_format_check
```

Requires `clang-format` in PATH. Uses `.clang-format` (LLVM-based, 4-space indent, 100 col limit). Files are globbed from `app/`, `engine/`, `tests/`, and `cmake/*.hpp.in`.

### Documentation check

For documentation-only changes, run:

```powershell
python -B tools/check_docs.py
git diff --check
```

`check_docs.py` validates repository-relative Markdown links, one H1 per document, reachability
from `docs/README.md`, required directory indexes, complete stage indexing, the Stage Chart Format
Update name, the runtime-script deferral boundary, Stage 4-12 plan structure, and candidate
JSON/CXT consistency. Documentation-only changes do not require a C++ build or CTest unless they
also modify generated/build inputs or make claims that require new implementation evidence.

### Smoke test (requires GPU / window)

```powershell
.\out\build\debug\bin\cuexis_player.exe --smoke-test
```

Binary output directory: `out/build/{debug|release}/bin/`.

### Running the player as a background process

A synchronous shell call cannot observe a long-running or interactive player. Start it
detached, poll the redirected logs, then stop it. The process survives between separate
shell invocations, so the polling step can be a later command:

```powershell
$p = Start-Process -FilePath .\out\build\debug\bin\cuexis_player.exe `
     -ArgumentList '--smoke-test' -PassThru -NoNewWindow `
     -RedirectStandardOutput run.log -RedirectStandardError run.err
# ... later, in the same or a different shell call ...
Get-Content run.log -Tail 40
if (-not $p.HasExited) { Stop-Process -Id $p.Id }
```

`-RedirectStandardOutput` and `-RedirectStandardError` must point at *different* files;
PowerShell fails if both target the same path. Always stop the process when finished so no
orphaned `cuexis_player.exe` holds the GPU context or the log files.

### Static analysis, sanitizers and coverage

`CMakePresets.json` defines `headless-sanitize`, `headless-clang-tidy` and
`headless-coverage`. **All three are Linux/Clang/GCC-only** and are exercised by
`.github/workflows/linux-quality.yml`, not locally on MSVC:

| Preset | Requires | Status on Windows/MSVC |
|---|---|---|
| `headless-sanitize` | Clang or GCC | Fatal configure error |
| `headless-coverage` | Clang or GCC + `gcovr` | Fatal configure error |
| `headless-clang-tidy` | `clang-tidy` | Configures, but the build fails |

`CMakeLists.txt` hard-stops sanitize/coverage with `requires Clang or GCC`, and both use
GNU-style flags (`-fsanitize=address,undefined`, `--coverage`) that MSVC does not accept.
`clang-cl` does not rescue them either: `-fno-omit-frame-pointer` is rejected as an unknown
argument under `/WX`, and ASan conflicts with the debug runtime (`-MDd`).

`headless-clang-tidy` configures cleanly on MSVC but **fails to build**: `/EHsc` is applied
through `cuexis_enable_warnings` as a `PRIVATE` option that clang-tidy never receives, so
every `try`/`throw` becomes `cannot use 'try' with exceptions disabled`. Do not treat that
output as a real finding.

For local static analysis on Windows, drive the tools directly against
`compile_commands.json` (emitted by every preset) instead of using these presets:

```powershell
clang-tidy -p out\build\debug engine\playback\src\playback_session.cpp
clangd --check=engine\playback\src\playback_session.cpp --compile-commands-dir=out\build\debug
```

`clangd --check` runs a full parse plus index of one file and reports diagnostics, which
catches far more than a grep-level review. Both inherit the MSVC triple, include paths and
`/std:c++20` from `compile_commands.json`.

## Architecture constraints (verified by CTest)

These are enforced by `cuexis_architecture_tests` — any violation **fails the build**:

- **Core** (`cuexis_core`) must not include SDL, glad/GL, or platform headers.
- **Chart** must not include EnTT, SDL, glad/GL, World, Audio, or AudioSDL headers.
- **Audio** must not include SDL, AudioSDL, or platform SDL headers.
- **Runtime** must not include SDL, glad/GL, Audio, AudioSDL, platform SDL, or OpenGL adapter headers.
- **Playback** must not include SDL, AudioSDL, platform SDL, or OpenGL adapter headers.
- **nlohmann JSON** types must not appear outside `engine/json_support/`.
- **OpenGL** calls and headers (`glad/GL/`) must only appear in `engine/render_opengl/`.
- **GLM** types must not appear in any public header (`engine/*/include/*.hpp`).
- Installed Playback headers must not expose EnTT, SDL, OpenGL/GLAD, JSON DOM, implementation
  logging, RuntimeSession, or World types.

### Target dependency and allowlist verification (also CTest)

Every `cuexis_*` CMake target has an explicit allowed-dependency list in the root `CMakeLists.txt`. Adding a new dependency to a target without updating its allowlist fails the build. Adding a new `cuexis_*` target without registering it in `CUEXIS_ACTIVE_TARGETS` (and the allowlists) fails the build. New test targets go in the `BUILD_TESTING` conditional block.

Headless presets and external-consumer gates verify that Playback configures and builds without
SDL/OpenGL adapters and that optional adapters are not transitive Playback dependencies. Player
already uses PlaybackSession instead of a private Runtime path.

Future constraints:
- Studio must use PlaybackSession instead of maintaining a private Runtime path.
- `cuexis_judgement` must not depend on platform, audio, render backends or host-engine SDKs.

## Adding or changing dependencies

When adding/removing a dependency, update **all** of:

1. `vcpkg.json` — dependency entry
2. (if baseline changes) `vcpkg-configuration.json`
3. `docs/guides/DEPENDENCY_POLICY.md` — dependency record
4. `THIRD_PARTY_NOTICES.md` — license/NOTICE tracking

## C++ conventions

| Category | Style |
|---|---|
| Types, classes, enums | PascalCase |
| Functions, variables | camelCase |
| Namespaces | `cuexis::module` |
| Macros | `CUEXIS_UPPER_CASE` |
| Filenames | `snake_case.hpp` / `snake_case.cpp` |
| Test files | `<subject>_tests.cpp` |

- Public headers in `include/cuexis/<module>/`, impl in `src/`.
- **Installed public headers must be pure ASCII — no CJK comments, no `—`, `；`, `（）` or other non-ASCII punctuation.** This applies to every `engine/*/include/cuexis/**/*.hpp` that ships in the install tree. Write header comments in English.
- Use `cuexis::core::Result<T, E>` (aliased to `tl::expected`) for recoverable errors — **never** ignore a `Result` without explicit discard/log.
- Exceptions must not cross module public boundaries. Destructors and real-time paths (audio/render) must not throw.
- Third-party types must not leak into Cuexis public API (exceptions: `tl::expected` in `core`).
- No raw `new`/`delete` across module boundaries. Use `unique_ptr`, `shared_ptr` only when truly shared, raw ptrs for non-owning observers.

**Why the ASCII rule matters**: the project compiles its own targets with `/utf-8` (`cmake/CuexisWarnings.cmake`), but that flag is `PRIVATE` and is *not* exported to consumers. An external consumer on a non-UTF-8 system codepage (e.g. `ACP=936`, Simplified Chinese Windows) parses non-ASCII comment bytes as the local encoding, which swallows the line ending and produces `error C2059`/`C2065` inside the header, plus cascading STL errors. This is exactly what broke `cuexis_external_consumer_find_package`. English-only ASCII comments in installed headers keep the SDK independent of any compiler encoding flag. Comments in `src/` and `tests/` are not affected by this rule.

## Testing

- Framework: **Catch2 v3** (`Catch2::Catch2WithMain`). Tests auto-discovered via `catch_discover_tests()`.
- Run a single test executable: `ctest --preset debug -R cuexis_core_tests`
- Unit tests must not depend on GPU or window. Smoke tests (`cuexis_player`) do require GPU.
- Architecture and player-failure tests use CMake script mode (no compilation).
- Test source layout mirrors `engine/`: `tests/core/`, `tests/chart/`, etc.

## Versioning

- Version components live in `cmake/CuexisVersion.cmake` (year/month/day/build);
update them and
  `vcpkg.json` together through `tools / update_version.py`.- `vcpkg.json` `version -
    string` must match the canonical version from CuexisVersion.cmake* * exactly *
        *(mismatch = fatal configure error).- `CUEXIS_SDK_API_VERSION` is independent from the date
    - based build identity and controls the installed CMake package compatibility version; stable C ABI versioning starts in Stage 12.
- Generated header: `${CMAKE_BINARY_DIR}/generated/cuexis/version.hpp` — never committed.
- Format: `yy.mm.dd-v[-suffix]` (UTC-based). The public legacy `cuexis::version::hour` remains `0`
  for SDK 0.5.x source compatibility and is not part of the build identity.

## Repo layout

```
engine/      -> libraries (core, json_support, project, platform, world, assets, chart,
                behavior, gameplay, audio, audio_sdl, render, debug, runtime,
                render_opengl, playback)
app/player/  -> the CLI player executable (cuexis_player)
app/studio/  -> exists on disk, not yet wired into CMake
tests/       -> Catch2 tests, mirrors engine/ structure
schemas/     -> JSON schemas for chart/project formats
tools/       -> asset_importer, chart_validator, chart_migrator, version updater, documentation checker
cmake/       -> custom cmake modules (warnings, arch verification, version)
docs/        -> indexed architecture, formats, plans, reports, proposals, examples and archive
```

`engine/animation/` and `engine/particles/` contain stub CMakeLists.txt but are not wired into the
build. `engine/audio/` and `engine/audio_sdl/` are active Stage 1D modules. `sdk/` and `adapters/`
directories do not exist yet (planned).

## Key reference docs

- `docs/README.md` — primary documentation index and authority map
- `docs/CURRENT_STATUS.md` — sole current-stage and implementation-status summary
- `docs/PROJECT_GUIDE.md` — compact product, architecture, workflow, and completion guide
- `docs/ROADMAP.md` — current stage sequence; detailed work remains in stage plans
- `docs/DOCUMENTATION_POLICY.md` — document roles, status vocabulary, archive and link rules
- `docs/architecture/README.md` — architecture navigation
- `docs/formats/README.md` — production/candidate format authority matrix
- `docs/guides/README.md` — build, coding, dependency and version guide index
- `docs/adr/README.md` — complete ADR index
- `docs/stage_plans/README.md` — Stage 0-12 and Stage Chart Format Update plan index
- `docs/stage_reports/README.md` — completion, review, and inventory evidence index
- `docs/proposals/README.md` — candidate and deferred design index
- `docs/examples/README.md` — candidate/validation example index
- `docs/archive/README.md` — superseded and historical source index
- `docs/guides/BUILDING.md` — full build instructions and prerequisites
- `docs/guides/CODE_POLICY.md` — coding, error handling, threading, and ownership conventions
- `docs/guides/DEPENDENCY_POLICY.md` — dependency selection, recording, and licensing
- `docs/guides/VERSIONING.md` — version format and update process
- `docs/adr/0027-playback-sdk-product-boundary.md` — accepted SDK product and host boundary
- `docs/adr/0030-playback-preview-api-version-and-result.md` — preview Result, package-version,
  and FrameSnapshot lifetime contract
- `docs/adr/0033-cpp-shared-library-preview-boundary.md` — Stage 1E C++ shared preview topology,
  linkage, dependency, deployment, and compatibility boundary
- `docs/stage_plans/stage_chart_format_update_implementation_plan.md` — active CXC/Chart format
  decision, migration, and acceptance gates
- `docs/stage_plans/stage_4_implementation_plan.md` through
  `docs/stage_plans/stage_12_implementation_plan.md` — independent future/deferred plans with
  goals, prerequisites, scope, acceptance criteria, and archived sources
- `docs/adr/0038-cxc-v1-and-chart-v4-boundary.md` — accepted CXC v1, Chart v4, CXT and prepare boundary
- `docs/formats/CXC_FORMAT.md` — accepted CXC v1 container, manifest, closure, and package contract
- `docs/formats/CHART_V4_FORMAT.md` — accepted Chart v4 fields, imports, animation, binding, and lowering contract
- `docs/formats/CXT_FORMAT.md` — accepted CXT v1 JSON template file contract

## Documentation maintenance

- Treat `docs/CURRENT_STATUS.md` as the only current-status summary. Do not infer current work from
  a historical report's next-step section.
- ADRs own decisions, specs own fields/semantics, plans own future scope and gates, and reports own
  dated evidence. Avoid copying a complete contract or test matrix into another role.
- Every Markdown file under `docs/` must be reachable from `docs/README.md` through an index.
- New top-level documentation areas require a `README.md` index and a link from the main index.
- When moving a document, preserve an old-path compatibility entry for at least one reorganization
  cycle and archive the full historical text when it remains useful.
- Stage 0 and Stage 1A have completion reports but no separate current plan files;
their original planning text is preserved in the archived PROJECT_GUIDE snapshot.- Stage 4 -
    12 plans are future / deferred contracts,
    not implementation claims.Each must retain a stage goal, prerequisites or recovery conditions,
    scope, acceptance criteria, exclusions,
    and archived source references.-
        Candidate examples remain review inputs until the relevant ADR and production Schema /
            Reader / Writer gates close.Do not move them into production fixtures prematurely.
