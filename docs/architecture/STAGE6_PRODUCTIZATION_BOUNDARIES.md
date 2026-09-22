# Stage 6 Productization Boundaries

Status: S6-A2 dependency and installation contract. `cuexis_presentation_renderer` now exists
as an internal static library. `cuexis_player_support` and `cuexis_media_import` remain planned.

## 1. Direct Dependency Graph

Arrows mean direct build dependencies. Existing lower-level dependencies are abbreviated; every new
target must be registered in the root CMake allowlists before implementation.

~~~text
cuexis_playback
  -> cuexis_runtime / cuexis_render / existing internal dependencies

cuexis_presentation_renderer (new internal static library)
  -> cuexis_playback / cuexis_render / cuexis_core

cuexis_render_opengl
  -> cuexis_presentation_renderer / cuexis_platform_sdl / existing shader cache

cuexis_player_support (new internal application library)
  -> cuexis_playback / cuexis_presentation_renderer / backend-neutral cuexis_audio

Player assembly and adapters
  -> cuexis_platform_sdl / cuexis_render_opengl / cuexis_audio_sdl

cuexis_media_import (new default-OFF internal tool library)
  -> fixed private decoder adapters / cuexis_core / portable output writers
cuexis_media_importer
  -> cuexis_media_import
~~~

The graph is intentionally one-way. cuexis_render and cuexis_runtime do not depend on
cuexis_presentation_renderer; the renderer layer consumes prepared Playback values and does not
create a second Runtime scene. cuexis_playback does not depend on SDL, OpenGL, the media importer,
Player support or tools. A forward declaration, header-only trick or private Runtime path cannot
hide a cycle.

## 2. Renderer Ownership

cuexis_presentation_renderer owns the backend-neutral IPresentationRenderer, move-only prepared
presentation candidate, common draw ordering/pass construction, transaction token and normalized
frame submission. It consumes existing PreparedPlayback/FrameSnapshot values and immutable portable
resources. cuexis_render_opengl owns GPU upload, OpenGL drawing, pixel probes and native diagnostics.
A test renderer consumes the same neutral commands and does not implement a second sort.

Prepare may have at most one outstanding renderer candidate. Candidate activation/discard is an
owner-thread, no-allocation, no-failure exchange after the token/generation guard has passed.
Submit and present are separate operations; a valid frame is submitted/presented at most once.
Zero-size surfaces suspend submission. Resize does not rebuild Playback content. Renderer close
releases GPU/context resources but does not close the application-owned window.

The Player control loop and command layer include Playback, the renderer contract and backend-neutral
audio only. SDL, OpenGL and concrete factories occur only in the application assembly/adapter files.

## 3. Media And Tool Isolation

cuexis_media_import is a build-time/offline tool boundary. It owns the fixed PNG/JPEG/MP3/Ogg
Vorbis/FLAC adapters, hard budgets, canonical Texture2D/WAV writers, provenance, cache validation
and generation/atomic publication. It does not enter the Playback or Player link closure and is not
an installed SDK component. Runtime direct decode is not a fallback.

The candidate source extension is a typed shared contract between ProjectConfig/CXC validation and
Playback source selection. It is not a Player-private JSON channel and does not make the CXC package
API public.

## 4. Installation And Staging

~~~text
production install
  Cuexis::Core / Cuexis::Playback / existing explicitly requested SDK components
  no candidate entry capability, no renderer target, no media importer target

experimental candidate install
  same public declaration surface plus candidate implementation flavor
  separate prefix, package flavor metadata and binary/import-library identity
  requires Cuexis_ALLOW_EXPERIMENTAL=ON

application staging
  Player support + renderer + SDL/OpenGL/audio adapters + optional media tools
  never exported as Cuexis SDK components

reference-host staging (future C4)
  clean find_package consumer of the installed public Playback boundary
  no source-tree engine include, no Player private library, no third-party engine SDK
~~~

Production/experimental, static/shared and Debug/Release staging directories are independent.
The runtime directory does not rely on source-tree assets or developer PATH. Installed package
metadata exposes display version, SDK API version, enabled flavor/components and required license
files. A candidate package cannot be accepted by a production consumer accidentally.

## 5. Architecture Verification State

| Boundary | Current state | Required verification |
| --- | --- | --- |
| Existing Playback isolation | Implemented and covered by A1 baseline | Preserve in C1/F1 |
| New renderer direction | D2 local exit: OpenGL implements the interface and Player submits through it | MinGW rerun of the member-order fix is still open |
| Player support separation | C2 library loads config and AudioSDL can open an enumerated device; neither is installed | C2 exit still open |
| Media importer isolation | Contract only; target does not exist | E1/E2 target and package tests |
| Production/experimental staging | Contract only; no package flavor gate yet | B1/C4 clean staging |
| Reference Host | Contract only; example not yet present | C4 external consumer/interactive host |

This document records the dependency contract. The presentation renderer target and the OpenGL
dependency edge now exist. Player support exists and is not installed. Media import does not
exist yet.
