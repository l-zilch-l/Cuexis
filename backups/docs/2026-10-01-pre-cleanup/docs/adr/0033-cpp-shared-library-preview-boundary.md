# ADR 0033: C++ Shared Library Preview Boundary

Status: Accepted

Date: 2026-07-28

Implementation update: 2026-07-29. SDK API `0.3.0`, the static/shared package topology, versioned
shared filenames, compatibility checks, symbol/import gates, and Windows/MSVC acceptance matrix are
implemented. Linux GCC/Clang shared build, install, consumer, ELF export, and clean-staging jobs have
passed together with Windows MinGW and MSVC acceptance.

## Context

Phase 1E already provides an installable C++20 static package and isolated
`add_subdirectory`/`find_package` consumers. The remaining package work includes deciding whether
shared libraries are part of the Playback Core preview or are deferred until the stable C ABI.
Deferring every shared-library build until phase 12 would leave DLL ownership, visibility, runtime
deployment, and private dependency problems untested while the C++ API is still able to change.

At the same time, the current preview API uses C++ standard-library types and
`cuexis::core::Result`, which is implemented with `tl::expected`. It cannot promise a portable or
long-lived binary ABI. Phase 11 Judgement/Replay contracts also do not exist yet. Supporting a C++
shared package in phase 1E must therefore remain distinct from freezing the stable C ABI in phase
12.

## Decision

### Delivery level

Phase 1E will officially support an installable C++20 shared-library preview in addition to the
existing static package. The shared package is a supported build, install, deploy, and external
consumer configuration. It is not a stable ABI and does not support replacing Cuexis binaries
without rebuilding the consumer.

The supported contract is:

```text
same Cuexis SDK API minor and package build
+ compatible compiler family/toolset, C++ standard library, architecture, and runtime linkage
+ matching Debug/Release configuration where the platform requires it
-> supported C++ shared preview
```

Package metadata and documentation must state these constraints. `SameMinorVersion` remains the
CMake source/package compatibility rule; it must not be interpreted as a stable binary-compatibility
promise.

### Public binary topology

The shared package exposes the following binary components:

```text
Cuexis Core       support runtime for Result, Error, Diagnostics, and other public foundations
Cuexis Content    ContentProvider implementations; depends on Core
Cuexis Audio      backend-independent audio contracts; depends on Core
Cuexis Playback   host facade; depends on Core, Content, and Audio
Cuexis AudioSDL   optional adapter; depends on Audio and SDL3
```

Installed CMake target names remain:

```text
Cuexis::Core
Cuexis::Content
Cuexis::Audio
Cuexis::Playback
Cuexis::AudioSDL
```

`Cuexis::Core` is a support component and is normally reached transitively from the public product
components. It is a queryable package component for hosts that directly use Result, Error, or
Diagnostics, but it is not a separate application framework.

World, Runtime, Assets, Chart, Behavior, Gameplay, Render, JSON support, and filesystem mechanics
remain implementation modules. In a shared build they are position-independent internal libraries
linked into the owning public binary, primarily Cuexis Playback or Cuexis Content. They are not
separate supported DLLs, package components, or binary compatibility boundaries. Optional SDL and
OpenGL platform/render adapters remain outside the Playback Core shared package.

A static package may install internal archives and CMake implementation targets when they are needed
to complete the public targets' link closure. Their names, headers, and direct use are not part of the
SDK contract. Starting with the `0.3.0` package boundary, the installed public header allowlist is
limited to Core, Content, Audio, Playback, optional AudioSDL, and generated version/export headers.
Internal module headers are not installed merely because their archives are packaged.

### Playback facade boundary

A phase 1E shared consumer must not construct or pass `AssetDatabase`, `ResourceManager`,
`RuntimeSession`, `World`, or other internal module objects through the Playback API. Before the
shared package is declared supported, the existing public `PlaybackSession` constructors that accept
`assets::AssetDatabase` must be replaced by Cuexis-owned Playback source/session configuration and
ContentProvider contracts. Exact type names may change during implementation, but this ownership
boundary is fixed.

The public shared headers may depend on Cuexis Core, Content, Audio, and Playback headers. They must
not require consumers to include internal Assets, Runtime, World, JSON support, SDL, or OpenGL
headers.

### Host extension points

Phase 1E does not make arbitrary C++ inheritance a shared-library plugin ABI. The supported host
content path is a Cuexis-created `HostContentProvider` that contains the documented synchronous host
callback. Filesystem and Memory providers are also created and destroyed by Cuexis factories. Direct
host subclassing of `IContentProvider` is not part of the installed shared package support matrix,
even if the preview header still permits it during migration.

Likewise, host-driven playback time crosses the shared boundary as `SourceClockSample`/`HostClock`.
Direct host subclassing of `IAudioClock` or `IAudioTransport` is not a phase 1E shared contract.
`Cuexis::AudioSDL` exports its Cuexis-owned concrete transport; future third-party audio backends
require an explicit adapter decision rather than inheriting an undocumented vtable boundary.

These restrictions let Cuexis catch callback exceptions, enforce reentry and owner-thread rules, and
keep destruction in the module that created the object. They do not change static or repository-
internal test doubles, but external shared consumers must use the supported wrappers and value
contracts.

### Build selection and package identity

Cuexis will use one validated cache string to select linkage:

```text
CUEXIS_LIBRARY_TYPE=STATIC|SHARED
```

`STATIC` remains the default until the shared matrix passes final phase 1E acceptance. A build tree
and install prefix contain exactly one Cuexis linkage type. Building or installing both variants into
the same prefix is not supported in phase 1E. Cuexis does not delegate this decision to the global
`BUILD_SHARED_LIBS`, because doing so would also affect unrelated dependencies and parent projects.

The installed package exposes `Cuexis_LIBRARY_TYPE` and validates that it is `STATIC` or `SHARED`.
Consumers continue to link the same namespaced targets and must not hard-code library filenames.
Shared filenames and ELF/Mach-O identities include the SDK API major/minor; Debug artifacts use a
platform-appropriate distinct postfix. This avoids silently mixing incompatible preview minors or
build configurations without implying a stable ABI.

The phase 1E supported shared platform matrix is Windows x64 with MSVC and Linux x64 with the CI
GCC/Clang toolchains. A shared package records its compiler/toolset, C++ standard, architecture,
configuration model, and runtime linkage in package metadata. Configure fails on a detectable
mismatch. Windows shared packages and consumers use the dynamic MSVC runtime (`/MD` for Release and
`/MDd` for Debug); `/MT` mixing is not supported. MinGW, macOS, other architectures, and cross-
compiler consumption remain experimental until each receives equivalent build, deploy, and runtime
consumer evidence.

### Visibility and ownership

Each public binary owns a dedicated export macro and installed pure-ASCII export header:

```text
CUEXIS_CORE_API
CUEXIS_CONTENT_API
CUEXIS_AUDIO_API
CUEXIS_PLAYBACK_API
CUEXIS_AUDIO_SDL_API
```

Shared builds use hidden visibility by default and export only annotated public functions and types.
`CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS` is not permitted. Internal modules do not receive public export
macros. Static builds define the corresponding static-use macros so the same installed headers work
for both linkage types.

Objects created by a Cuexis binary are destroyed through an exported Cuexis destructor or operation.
No raw owning allocation crosses the boundary. Public value types that contain standard-library
storage are allowed only under the documented matching-toolchain/runtime requirement. Exceptions
must not cross the boundary; host callback failures and allocation/validation failures become stable
`Result` errors. Existing owner-thread, reentry, Snapshot ownership, and Prepared Playback token rules
apply unchanged to shared builds.

### Dependency and deployment closure

A shared package config finds only dependencies required by its public headers or explicitly requested
components. In particular:

```text
Playback/Content/Audio shared consumer
  may require the documented tl-expected header dependency
  must not require EnTT, GLM, JSON/schema-validator, SDL3, glad, or spdlog development packages

AudioSDL shared consumer
  may find SDL3 only when AudioSDL is explicitly requested
```

Private runtime libraries may still be part of the deployed binary closure. The install/deployment
gate must copy them, record their licenses, and run the consumer from a clean staging directory that
does not borrow DLLs or shared objects from the Cuexis build tree. Base Playback must not import or
initialize SDL or OpenGL.

Static package dependency behavior remains valid independently: its complete link closure may require
private implementation packages. Static and shared package configs must not accidentally use the
other linkage's dependency rules.

### Version transition

The previous static-only preview used SDK API `0.2.0`. The implementation of this ADR uses SDK API
`0.3.0`, because it changes the public Playback construction boundary and adds linkage/package
semantics that consumers must explicitly accept.

Later incompatible C++ preview changes increase the SDK API minor and require consumers to rebuild.
Date-based display versions, content format versions, and the future stable C ABI version remain
independent.

## Required verification

Phase 1E cannot complete until the following evidence exists for both static and shared linkage:

```text
Debug and Release build/install/consumer gates on Windows/MSVC
headless-only and Player-full configurations
add_subdirectory and install/find_package consumers
base Playback/Content/Audio and explicitly requested AudioSDL components
consumer execution from a clean staged install/deployment directory
public header, exported-symbol, imported-library, and license scans
Snapshot, Provider callback, error, Session destruction, and multi-Session lifetime tests
Player/internal/static-consumer/shared-consumer RuntimeFrame and FrameSnapshot hash parity
Linux GCC/Clang shared build and consumer coverage in CI
package metadata mismatch failures for architecture, toolset/runtime, and unsupported linkage
```

The export-symbol denylist includes RuntimeSession, World, EnTT, backend implementation types, and
unannotated internal APIs. The imported-library gate rejects SDL/OpenGL dependencies from the base
Playback closure.

## Non-goals

Phase 1E shared support does not provide:

```text
a stable C ABI or long-term C++ binary compatibility
drop-in replacement of a Cuexis shared library without rebuilding the host
mixing Cuexis package versions, toolchains, standard libraries, runtimes, architectures, or configs
runtime plugin discovery, arbitrary LoadLibrary/dlopen unload, or extension plugin ABI
Unity, Unreal, C#, or other language bindings
simultaneous static and shared artifacts in one phase 1E install prefix
```

Phase 6 will use the shared preview evidence to stabilize C++ usage and upgrade policy. Phase 11 adds
the required Judgement/Replay lifecycle. Phase 12 still owns the opaque-handle C ABI, allocator and
buffer ABI, long-term binary compatibility policy, language wrappers, and formal Playback SDK v1.

## Rejected alternatives

### Defer every shared build to phase 12

Rejected. It postpones deployment, visibility, dependency, and ownership evidence until after the C++
surface is much harder to change.

### Build every internal module as a DLL

Rejected. It turns World, Runtime, Assets, EnTT-facing code, and other implementation modules into
accidental compatibility boundaries and creates an unnecessarily fragmented runtime deployment.

### Treat the C++ shared preview as the stable ABI

Rejected. Standard-library types, `tl::expected`, incomplete Judgement/Replay contracts, and limited
host evidence make that guarantee indefensible.

### Use `BUILD_SHARED_LIBS` without a Cuexis linkage contract

Rejected. It leaks Cuexis policy into dependencies and parent projects, permits ambiguous mixed build
trees, and does not provide package metadata or validation.
