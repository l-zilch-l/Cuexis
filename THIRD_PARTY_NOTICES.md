# Third-Party Notices

This file is the source-tree notice inventory for the current Cuexis stage 1D tree. Versions below are resolved by the pinned vcpkg baseline `40f3c709db80acf154ac4b17a1f83c564ebd022e`. Exact distributed transitive notices must be verified again before each release.

## Direct Dependencies

| Dependency | Version | Upstream | Purpose / owner | License | Player distribution | Alternative / exit path |
| --- | --- | --- | --- | --- | --- | --- |
| SDL3 | 3.4.12 | https://github.com/libsdl-org/SDL | Window/lifecycle/OpenGL platform integration and optional default-route audio transport / `cuexis_platform_sdl`, `cuexis_render_opengl`, `cuexis_audio_sdl` | zlib; some configurations also contain MIT or Apache-2.0 code | Shared runtime library for Player or an AudioSDL consumer | Replace behind the platform, render and audio adapter modules |
| EnTT | 3.16.0 | https://github.com/skypjack/entt | Runtime ECS registry / `cuexis_world` | MIT | Header code is compiled into engine binaries | Replace only through the World boundary; a custom ECS is not planned |
| GLM | 1.0.3 | https://github.com/g-truc/glm | Private vector, quaternion and matrix calculations behind Cuexis-owned math types / `cuexis_core`, Chart import and Runtime transforms | MIT | Header code is compiled into engine binaries | Replace behind Cuexis math conversion and transform functions |
| spdlog | 1.17.0#1 | https://github.com/gabime/spdlog | Player-private structured logging / `cuexis_player` | MIT | Linked only into the optional Player | Replace inside the Player application; Playback receives instance-owned host sinks |
| fmt | 12.2.0#1 | https://github.com/fmtlib/fmt | Transitive formatting dependency of spdlog | MIT | Linked only when the selected spdlog package requires it | Follows the Player-private spdlog dependency |
| glad | 0.1.36 | https://github.com/Dav1dde/glad | OpenGL function loader / `cuexis_render_opengl` | MIT; generated Khronos specification content is Apache-2.0 and other registry licenses | Linked into the OpenGL backend | Replace inside `cuexis_render_opengl` with another maintained loader |
| nlohmann-json | 3.12.0#2 | https://github.com/nlohmann/json | Internal JSON DOM and parsing / `cuexis_json_support` | MIT | Header code is compiled into engine binaries | Replace inside the JSON support boundary; public APIs expose only Cuexis types |
| JSON Schema Validator | 2.4.0 | https://github.com/pboettch/json-schema-validator | Structural validation of versioned JSON documents / `cuexis_json_support` | MIT | Linked runtime code | Replace behind the JSON support validation adapter |
| Catch2 | 3.15.2#1 | https://github.com/catchorg/Catch2 | Unit tests / test targets only | BSL-1.0 | Not linked into Player | Re-evaluate only through an ADR if Catch2 and small Fakes become insufficient |
| tl-expected | 1.3.1 | https://github.com/TartanLlama/expected | C++20 `cuexis::core::Result` foundation / `cuexis_core` | CC0-1.0 | Header code is compiled into engine binaries | Replace with `std::expected` after a future C++ standard migration |

## Generated Registry Inputs

The glad package is generated from Khronos OpenGL/EGL registry inputs. The installed registry files carry per-file MIT, Apache-2.0 or Khronos notices. Packaging must preserve the notices required by the generated glad sources.

The static base Playback/Content/Audio install copies the exact vcpkg copyright files for EnTT,
GLM, JSON Schema Validator, nlohmann-json and tl-expected into `share/Cuexis/licenses`. An install
that includes the optional AudioSDL component additionally copies the SDL3 copyright. This
inventory does not replace those full texts. Player packaging must additionally preserve the
SDL3, glad, spdlog and applicable fmt notices required by the artifacts it redistributes.
