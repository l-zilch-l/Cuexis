# Chart Format Foundation F7: CXC Candidate Entry

日期：2026-09-07  
状态：implemented candidate validator; hosted and capacity closure pending

## Scope

F7 uses the existing CXC v1 ZIP32 container and keeps the three-field archive entry contract
(`path`, `byteCount`, `sha256`) unchanged. Chart v5 metadata is carried only by the registered
`extensions["cuexis.chart-entry.v1"]` object. This avoids teaching the v1 manifest reader to
accept unknown entry fields and preserves v4/CXT v1 compatibility.

The Foundation mapping is:

- `sourcePath` and `sourceSemanticIdentity` are optional authoring provenance;
- compiled/playback entries use `kind=chart`, `encoding=packed-chart`;
- `playback=true` requires a registered compiler profile, expanded entity/requirement counts,
  compiled semantic identity, and an artifact identity equal to the exact archive entry SHA-256;
- Foundation revision 1 accepts only `candidate.static-tap-lanes4-v1` and a 16 MiB Packed entry;
- at least one playback entry is mandatory when the candidate extension is present.

`cxc_validate` now calls the typed extension validator. It inspects the candidate bytes and Packed
statistics only; it never reads CXT v2, freezes parameters, or runs an expander. `cxc_pack` remains
the existing Source Project packer and does not implicitly compile CXT v2. A future explicit
compile/prepare command must produce Packed bytes before CXC packaging.

## Evidence

- Positive mapping: `tests/fixtures/chart_format_foundation/cxc_candidate_extension.valid.json`.
- Negative mapping: `tests/fixtures/chart_format_foundation/cxc_candidate_extension.invalid.json`.
- Typed implementation: `tools/cxc_common/include/cuexis/tools/cxc_candidate.hpp` and
  `tools/cxc_common/src/cxc_candidate.cpp`.
- CXC tooling wiring: `tools/cxc_common/src/cxc_tool_common.cpp`.

The validator rejects missing archive entries, unsupported encoding/kind, missing or malformed
identities, artifact hash mismatches, unsupported compiler profiles, count overflow, missing
playback entries, Packed bytes above 16 MiB, invalid Packed bytes, and metadata/entity-count
mismatches. Rejections occur before a successful tool result is emitted.

## Compatibility and handoff

No v4 reader, CXT v1 reader, CXC container version, or default Playback route is changed. Source
entries remain inspection/reproduction material and are never a v5 Playback fallback. F4-F6 must
provide the final Packed header semantic identity bridge and package-facing playback path before
F7 can claim full artifact-to-semantic verification.

## Known gaps

The 40,000-entity capacity fixtures and raw section/decoded/prepare measurements belong to F8.
Hosted MSVC, MinGW, and Linux verification, owner acceptance, and Stage 6 handoff belong to F9.
