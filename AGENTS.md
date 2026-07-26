# Cuexis Agent Guide

## Product direction

ADR 0027 defines Cuexis as an embeddable **Cuexis Playback SDK** with two independent applications: Cuexis Player and Cuexis Studio. RuntimeSession, World, EnTT, SDL and OpenGL are internal/optional implementation details; external hosts use PlaybackSession, ContentProvider, RuntimeFrame, FrameSnapshot and later Judgement/Replay contracts.

The current baseline includes a functioning `cuexis_playback` module (`PlaybackSession`,
`FrameSnapshot`, `RuntimeFrame`, `IContentProvider`). `PlaybackSession::update()` evaluates the
phase 1C typed Behavior path. Synchronous Filesystem/Memory/Host ContentProvider support, a
static C++20 install package, adapter-disabled presets, and add_subdirectory/find_package
external-consumer gates are implemented. Phase 1E is still in progress: the final public API,
complete component matrix, package compatibility policy, and shared-library scope are not
frozen. The phase 1C feature boundary is implemented, but the later 260722 review still records
unclosed P1/P2 findings; do not describe phase 1C as finally accepted until those findings have
closure evidence. `cuexis_judgement` remains planned for a later phase.

## Build

```powershell
# Debug (standard daily workflow)
cmake --preset debug --fresh
cmake --build --preset debug
ctest --preset debug --no-tests=error

# Release (before merging / versioning)
cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
```

**Prerequisites**: `VCPKG_ROOT` env var must point to vcpkg root. Run from a Visual Studio Developer shell (or ensure `cl.exe`, Ninja, CMake in PATH).

Note: `CUEXIS_WARNINGS_AS_ERRORS` is `OFF` in the base preset. Warnings do not fail the build by default.

**Version-change builds**: After changing `CuexisVersion.cmake` or `vcpkg.json` version, use `--fresh` + `--clean-first` to avoid stale generated headers.

### Format check

```powershell
cmake --build --preset debug --target cuexis_format_check
```

Requires `clang-format` in PATH. Uses `.clang-format` (LLVM-based, 4-space indent, 100 col limit). Files are globbed from `app/`, `engine/`, `tests/`, and `cmake/*.hpp.in`.

### Smoke test (requires GPU / window)

```powershell
.\out\build\debug\bin\cuexis_player.exe --smoke-test
```

Binary output directory: `out/build/{debug|release}/bin/`.

## Architecture constraints (verified by CTest)

These are enforced by `cuexis_architecture_tests` — any violation **fails the build**:

- **Core** (`cuexis_core`) must not include SDL, glad/GL, or platform headers.
- **Chart** must not include EnTT, SDL, glad/GL, or `cuexis/world/` headers.
- **Runtime** must not include SDL, glad/GL, `cuexis/platform_sdl/`, or `cuexis/render_opengl/` headers.
- **nlohmann JSON** types must not appear outside `engine/json_support/`.
- **OpenGL** calls and headers (`glad/GL/`) must only appear in `engine/render_opengl/`.
- **GLM** types must not appear in any public header (`engine/*/include/*.hpp`).

### Target dependency and allowlist verification (also CTest)

Every `cuexis_*` CMake target has an explicit allowed-dependency list in the root `CMakeLists.txt`. Adding a new dependency to a target without updating its allowlist fails the build. Adding a new `cuexis_*` target without registering it in `CUEXIS_ACTIVE_TARGETS` (and the allowlists) fails the build. New test targets go in the `BUILD_TESTING` conditional block.

Future constraints (not yet enforced):
- Installed Playback headers must not expose EnTT, SDL, OpenGL/GLAD, JSON DOM or implementation logging types.
- Headless Playback must configure and build without SDL/OpenGL adapters.
- Player and Studio must use PlaybackSession instead of maintaining private Runtime paths.
- `cuexis_judgement` must not depend on platform, audio, render backends or host-engine SDKs.
- Optional adapters must not become transitive dependencies of the Playback core component.

## Adding or changing dependencies

When adding/removing a dependency, update **all** of:

1. `vcpkg.json` — dependency entry
2. (if baseline changes) `vcpkg-configuration.json`
3. `docs/DEPENDENCY_POLICY.md` — dependency record
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
- Use `cuexis::core::Result<T, E>` (aliased to `tl::expected`) for recoverable errors — **never** ignore a `Result` without explicit discard/log.
- Exceptions must not cross module public boundaries. Destructors and real-time paths (audio/render) must not throw.
- Third-party types must not leak into Cuexis public API (exceptions: `tl::expected` in `core`).
- No raw `new`/`delete` across module boundaries. Use `unique_ptr`, `shared_ptr` only when truly shared, raw ptrs for non-owning observers.

## Testing

- Framework: **Catch2 v3** (`Catch2::Catch2WithMain`). Tests auto-discovered via `catch_discover_tests()`.
- Run a single test executable: `ctest --preset debug -R cuexis_core_tests`
- Unit tests must not depend on GPU or window. Smoke tests (`cuexis_player`) do require GPU.
- Architecture and player-failure tests use CMake script mode (no compilation).
- Test source layout mirrors `engine/`: `tests/core/`, `tests/chart/`, etc.

## Versioning

- Single source of truth: `cmake/CuexisVersion.cmake` (year/month/day/hour/build).
- `vcpkg.json` `version-string` must match the canonical version from CuexisVersion.cmake **exactly** (mismatch = fatal configure error).
- `CUEXIS_SDK_API_VERSION` is independent from the date-based build identity and controls the
  installed CMake package compatibility version; stable C ABI versioning starts in phase 12.
- Generated header: `${CMAKE_BINARY_DIR}/generated/cuexis/version.hpp` — never committed.
- Format: `yy.mm.dd.hh-v[-suffix]` (UTC-based).

## Repo layout

```
engine/      -> libraries (core, json_support, project, platform, world, assets, chart,
                behavior, gameplay, render, debug, runtime, render_opengl, playback)
app/player/  -> the CLI player executable (cuexis_player)
app/studio/  -> exists on disk, not yet wired into CMake
tests/       -> Catch2 tests, mirrors engine/ structure
schemas/     -> JSON schemas for chart/project formats
tools/       -> asset_importer, chart_validator
cmake/       -> custom cmake modules (warnings, arch verification, version)
docs/        -> project docs (BUILDING.md, CODE_POLICY.md, etc.)
```

`engine/animation/`, `engine/audio/`, `engine/audio_sdl/`, and `engine/particles/` contain stub CMakeLists.txt but are not wired into the build. `sdk/` and `adapters/` directories do not exist yet (planned).

## Key reference docs

- `docs/BUILDING.md` — full build instructions and prerequisites
- `docs/CODE_POLICY.md` — coding, error handling, threading, and ownership conventions
- `docs/DEPENDENCY_POLICY.md` — dependency selection, recording, and licensing
- `docs/VERSIONING.md` — version format and update process
- `docs/adr/0027-playback-sdk-product-boundary.md` — accepted SDK product and host boundary
- `docs/adr/0030-playback-preview-api-version-and-result.md` — preview Result, package-version,
  and FrameSnapshot lifetime contract
- `docs/stage_plans/stage_1b_implementation_plan.md` — completed phase 1B resource lifecycle plan
- `docs/stage_plans/cuexis_sdk_transition_plan.md` — phase 0-12 SDK migration route
- `docs/stage_plans/stage_1c_implementation_plan.md` — PlaybackSession, RuntimeFrame and headless Playback loop
- `docs/stage_plans/stage_1d_implementation_plan.md` — HostClock/CuexisAudio dual-mode and audio adapter
- `docs/stage_plans/stage_1e_implementation_plan.md` — packaging and external-consumer gate
