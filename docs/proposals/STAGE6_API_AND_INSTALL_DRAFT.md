# Stage 6 Candidate API And Install Draft

Status: S6-A2 declaration draft. It is an accepted-boundary implementation input, not an installed
SDK API. No public header or CMake package was changed by A2.

## 1. Candidate Source Declarations

The three names below are the only new candidate factories. The declaration is identical when
CUEXIS_ENABLE_CHART_V5_CANDIDATE is OFF or ON; the switch changes implementation capability and
installation flavor, not class layout, overload resolution or default behavior.

~~~cpp
namespace cuexis::playback {

class PlaybackSource final {
  public:
    [[nodiscard]] static auto fromFilesystemProjectEntry(
        const std::filesystem::path& locator,
        std::string entryPath) -> core::Result<PlaybackSource>;

    [[nodiscard]] static auto fromCxcFileEntry(
        const std::filesystem::path& locator,
        std::string entryPath) -> core::Result<PlaybackSource>;

    [[nodiscard]] static auto fromCxcMemoryEntry(
        std::vector<std::byte> packageBytes,
        std::string entryPath) -> core::Result<PlaybackSource>;
};

} // namespace cuexis::playback
~~~

The snippet is a future additive declaration. It does not expose CXC, Packed, CanonicalSemanticChart,
Requirement, CXT AST, JSON DOM, SDL, OpenGL, EnTT or a private Runtime type. entryPath is owning
portable-path text; memory factory bytes are owning and consumed. File locators identify the existing
project/package source, while the provider, validated entry bytes and typed candidate own their
required lifetime after the factory returns.

The methods return core::Result and never let exceptions cross the public boundary. A malformed path,
missing entry, non-unique playback declaration, unsupported profile/revision, identity mismatch,
resource failure or partial-publication attempt returns a stable diagnostic and no source value.
The OFF implementation returns playback.candidate.disabled without probing or parsing the input.
The ON implementation still requires the explicit entry path and never uses suffix, magic or a failed
legacy load to select a candidate.

Existing fromChartText, fromFilesystemProject, fromCxcFile, fromCxcMemory, typed project factories
and their parameter lists retain their v1-v4 semantics in both builds. There is no new overload with
an implicit path and no automatic retry or upgrade. --candidate-entry <path> is an application option
paired with --project or --cxc; non-experimental Player rejects it and the option is not saved as a
preference. --chart remains the existing text entry.

An implementation that delivers these methods changes the public source surface additively and must
follow the approved Stage 6 0.7.1 version gate. A2 does not change CUEXIS_SDK_API_VERSION,
vcpkg.json, generated headers or package metadata.

## 2. Source State And Parse-Once Contract

The internal source state has an explicit tagged payload for legacy project documents versus a
validated candidate. Candidate state owns the entry bytes, provider lifetime, decoded typed chart,
identity map and resource closure. Prepare receives that immutable result; it does not parse source
JSON, expand CXT v2, decode Packed a second time or query a global registry.

Project and CXC extension records share the same typed entry description and path rules. Project
selection reuses project-root containment and asset-root rules; CXC selection reuses archive closure
and exact entry identity. Unselected package entries still receive the existing package validation.
The selected semantic entry is decoded at most once.

Typed lowering keeps generated tuple, parent, each complete Requirement tuple, constraints/effects,
feature derivation and transitive resource closure. A16 is produced by the offline typed assembler;
Player is not allowed to repair a missing feature or supplement requirements.

## 3. Experimental Package Boundary

CUEXIS_ENABLE_CHART_V5_CANDIDATE defaults OFF. An ON build/install is marked experimental and
requires Cuexis_ALLOW_EXPERIMENTAL=ON in the consumer before its config file succeeds. Production
and experimental prefixes cannot be mixed. Static/shared, Debug/Release and candidate/production
staging are separate; binary/import-library names and package flavor metadata distinguish candidate
from production.

The supported SDK install remains the public Playback/Core boundary. The following are internal or
application artifacts and are not installed as public SDK components by this stage:

- cuexis_presentation_renderer;
- cuexis_player_support;
- cuexis_media_import and cuexis_media_importer;
- the Cuexis Reference Host and its test-only helpers.

The Reference Host is a future independent executable that consumes a clean staged find_package
installation. It owns its command loop, Memory ContentProvider, HostClock and frame/resource
consumption; it cannot be replaced by a compile-only consumer or a Player private include path.

## 4. API And Consumer Gates

| Gate | Required behavior | A2 state | Owner |
| --- | --- | --- | --- |
| Declaration parity | OFF/ON public declarations and layout are identical | Draft only | C1/C4 |
| Legacy behavior | Existing factories preserve v1-v4 default semantics | Existing baseline | C1/F1 |
| Candidate opt-in | Explicit factory and explicit path required | Contract recorded | C1 |
| Package opt-in | Experimental consumer opt-in is required | Contract recorded | B1/C4 |
| Installed consumer | Clean staging uses public headers and package targets only | Not implemented | C4/F1 |
| Reference Host | Named host uses installed public Playback API | Not implemented | C4 |
| SDK version | 0.7.1 only after additive implementation gates | Current 0.7.0 | B1 |

No row in this draft is an implementation or owner-acceptance claim.
