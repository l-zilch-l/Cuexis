# Third-Party Notices

This file is the source-tree notice inventory for Cuexis stage 1A. Versions below are resolved by the pinned vcpkg baseline `8e8dfb4ba483886936ded5ca201b500b8d8b0096`. Exact distributed transitive notices must be verified again before each release.

## Direct Dependencies

| Dependency | Version | Upstream | Purpose / owner | License | Player distribution | Alternative / exit path |
| --- | --- | --- | --- | --- | --- | --- |
| SDL3 | 3.4.12 | https://github.com/libsdl-org/SDL | Window, lifecycle events and OpenGL platform integration / `cuexis_platform_sdl`, `cuexis_render_opengl` | zlib; some configurations also contain MIT or Apache-2.0 code | Shared runtime library | Replace behind the platform and backend modules |
| EnTT | 3.16.0 | https://github.com/skypjack/entt | Runtime ECS registry / `cuexis_world` | MIT | Header code is compiled into engine binaries | Replace only through the World boundary; a custom ECS is not planned |
| GLM | 1.0.3 | https://github.com/g-truc/glm | Private vector, quaternion and matrix calculations behind Cuexis-owned math types / `cuexis_core`, Chart import and Runtime transforms | MIT | Header code is compiled into engine binaries | Replace behind Cuexis math conversion and transform functions |
| spdlog | 1.17.0#1 | https://github.com/gabime/spdlog | Structured logging implementation / `cuexis_core` | MIT | Linked runtime code | Replace behind the Cuexis logging API |
| fmt | 12.2.0 | https://github.com/fmtlib/fmt | Formatting used by spdlog and Core / `cuexis_core` | MIT | Linked runtime code | Migrate internal formatting to a compatible standard-library path when practical |
| glad | 0.1.36 | https://github.com/Dav1dde/glad | OpenGL function loader / `cuexis_render_opengl` | MIT; generated Khronos specification content is Apache-2.0 and other registry licenses | Linked into the OpenGL backend | Replace inside `cuexis_render_opengl` with another maintained loader |
| nlohmann-json | 3.12.0#2 | https://github.com/nlohmann/json | Internal JSON DOM and parsing / `cuexis_json_support` | MIT | Header code is compiled into engine binaries | Replace inside the JSON support boundary; public APIs expose only Cuexis types |
| JSON Schema Validator | 2.4.0 | https://github.com/pboettch/json-schema-validator | Structural validation of versioned JSON documents / `cuexis_json_support` | MIT | Linked runtime code | Replace behind the JSON support validation adapter |
| Catch2 | 3.15.2 | https://github.com/catchorg/Catch2 | Unit tests / test targets only | BSL-1.0 | Not linked into Player | Re-evaluate only through an ADR if Catch2 and small Fakes become insufficient |
| tl-expected | 1.3.1 | https://github.com/TartanLlama/expected | C++20 `cuexis::core::Result` foundation / `cuexis_core` | CC0-1.0 | Header code is compiled into engine binaries | Replace with `std::expected` after a future C++ standard migration |

## Generated Registry Inputs

The glad package is generated from Khronos OpenGL/EGL registry inputs. The installed registry files carry per-file MIT, Apache-2.0 or Khronos notices. Packaging must preserve the notices required by the generated glad sources.

This inventory does not replace the full license texts installed by vcpkg. Release packaging must include every notice required by the actual linked and redistributed artifacts; the local vcpkg cache is not the legal record.
