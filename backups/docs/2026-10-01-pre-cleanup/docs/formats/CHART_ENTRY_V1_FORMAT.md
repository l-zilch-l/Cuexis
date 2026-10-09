# Cuexis Chart Entry Extension v1

Status: Stage 6 candidate contract recorded by S6-A2. The entry factories and candidate
Playback implementation are not present yet.

This document owns the field and selection contract for the registered extension
cuexis.chart-entry.v1. The JSON shape is defined by
[cuexis.chart-entry.v1.schema.json](../../schemas/cuexis.chart-entry.v1.schema.json).
The extension is used in both ProjectConfig v1 and the CXC manifest extensions object. It
does not change the existing CXC entry record, ProjectConfig entry.chart, or the v4 default
source factories.

## 1. Purpose And Boundary

The extension maps a portable relative path to a typed chart entry. It is metadata, not a
second chart format and not a public representation of the Packed semantic model.

The Stage 6 candidate path is deliberately narrow:

- Packed flags must be 1 and candidateRevision must be 1.
- The registered feature is cuexis.gameplay.candidate.lanes4, version 1.
- The requirement domain is candidate.lanes4 and the action is press.
- Each supported requirement has exactly one lane constraint in the inclusive range 0 through 3.
- Only tap or point requirements are admitted.
- Effects are empty. Hold, Release, Slide, Flick, multi-lane and unregistered sections fail.
- BEH0, BHD0, ANM0 and FXS0 are rejected even when their payload is empty.
- Runtime does not parse CXT v2. CXT expansion and A16 feature/resource derivation are offline
  typed assembler work.

This contract is candidate-only. It does not enable a default Writer, a formal Chart v5
capability, a stable C ABI, or a new SDK version.

## 2. Extension Shape

The value stored under extensions[cuexis.chart-entry.v1] is an object with one member:

| Field | Type | Rule |
| --- | --- | --- |
| entries | array | One or more entry records, sorted by path bytes |

Each record has the following fields. Fields marked candidate are required when playback is
true; source records may omit them.

| Field | Type | Rule |
| --- | --- | --- |
| path | portable path | Required. Project-relative or archive-relative entry path |
| kind | string | Required. Stage 6 value is chart |
| encoding | string | source-json, source-cxt, or packed-chart |
| playback | boolean | Required. Only true records can be selected by a candidate factory |
| sourcePath | portable path | Optional provenance path; not a runtime input |
| sourceSemanticIdentity | lowercase SHA-256 | Optional source/build identity observation |
| compiledSemanticIdentity | lowercase SHA-256 | Required for playback candidate records |
| artifactIdentity | lowercase SHA-256 | Required for playback candidate records; hash of exact bytes |
| compilerProfile | string | Required for playback; Stage 6 value is candidate.static-tap-lanes4-v1 |
| expandedEntityCount | unsigned integer | Required for playback; maximum 40,000 |
| expandedRequirementCount | unsigned integer | Required for playback; maximum 40,000 |

The entry schema rejects unknown fields. Packed flags, candidateRevision, section registration,
CRC, decoded byte counts and eventCount are checked from the selected Packed bytes; they are not
duplicated in this extension. A metadata count never replaces a decoded count comparison.

The existing base entry record remains only path, byteCount and sha256. Extension fields must not
be added to that record. The CXC manifest continues to contain exactly the files required by its
existing package closure plus the declared extension entries.

## 3. Path And Selection Rules

Paths are UTF-8 JSON strings containing portable ASCII segments separated by forward slashes.
They are non-empty, relative, and contain no empty segment, dot segment, dot-dot segment,
backslash, colon, NUL, control byte, trailing dot or trailing space. Windows reserved device
names and case-folding conflicts are rejected. A regular file and one of its descendants are
also a path conflict.

Project paths are relative to the validated project root. CXC paths are relative to the archive
root. The selected path must be an exact path match after validation; no extension, suffix,
content magic or case-folding probe is allowed.

The caller must provide one explicit entry path. A package may contain more than one valid
playback record, but no factory may choose the first record or a sorted record. The requested path
must resolve to one and only one extension record, and that record must have playback=true. An
exact duplicate or case-fold duplicate is rejected before selection.

The legacy ProjectConfig entry.chart continues to point to the v4 chart document. Existing
fromFilesystemProject, fromCxcFile, fromCxcMemory and fromChartText keep their v1-v4 behavior.
They do not probe this extension after a failure and do not silently upgrade to a candidate.
A bare Packed file is not a Stage 6 user entry.

The complete project or package contract is validated before the selected candidate is published.
The selected semantic entry is decoded at most once; the owned decoded value, owned source bytes,
provider and resource closure are passed to prepare. Playback does not reparse JSON, re-decode
Packed bytes, or depend on a tools target.

## 4. Candidate Validation Order

The order below is observable through the first stable diagnostic and is part of the contract:

1. Validate the extension object, entry array, path syntax, ordering and duplicate conflicts.
2. Validate the explicit path and require one matching playback record.
3. Validate the base package or project closure, regular file presence, byte count and exact hash.
4. Read the Packed fixed header and apply the 16 MiB file, section and decoded-byte limits.
5. Validate magic, wire version, header size, flags, candidateRevision, directory ranges and CRCs.
6. Validate registered sections, required sections, UTF-8, canonical ordering and all count limits.
7. Decode through packed::decode, then validate the candidate profile and typed requirements.
8. Recompute semantic identity and compare the extension compiledSemanticIdentity and Packed header.
9. Verify resource closure and keep the generated identity, parent relation, requirements,
   constraints and effects in the immutable typed candidate.
10. Publish no partial result on failure; only the later Playback transaction may prepare or commit it.

The minimum stable diagnostic set is:

| Code | Meaning |
| --- | --- |
| cxc.chart_entry.extension_missing | The registered extension is absent |
| cxc.chart_entry.extension_invalid | Extension shape or unknown field is invalid |
| cxc.chart_entry.path_invalid | A path is not portable or escapes its root |
| cxc.chart_entry.duplicate_path | Exact or file/descendant path conflict |
| cxc.chart_entry.casefold_conflict | Case-folded path conflict |
| cxc.chart_entry.entry_missing | The explicit path has no extension record or file |
| cxc.chart_entry.playback_not_declared | The selected record is not playback=true |
| cxc.candidate.profile_unsupported | The profile or requirement subset is outside Stage 6 |
| cxc.candidate.revision_unsupported | Flags or candidateRevision is unsupported |
| cxc.candidate.budget_exceeded | A file, section, decoded or count limit is exceeded |
| cxc.candidate.packed_invalid | Packed structure, CRC, section or typed decode failed |
| cxc.candidate.artifact_identity_mismatch | Exact entry bytes do not match artifactIdentity |
| cxc.candidate.compiled_identity_mismatch | Recomputed semantic identity does not match metadata |
| cxc.candidate.resource_closure_invalid | A required resource is missing, duplicated or mismatched |
| cxc.candidate.partial_publish_forbidden | A failure attempted to publish a partial candidate |

Profile rejection precedes semantic identity mismatch. A structurally valid but unsupported
artifact must not be made to look like a hash failure.

## 5. Typed Lowering And Identity Preservation

The offline typed assembler supplies chart metadata, expanded entities, parent relations,
requirements and resource references. It derives the lanes4 feature and closure from validated
typed requirements; Player cannot add a feature after decode. The resulting candidate keeps:

- the complete explicit or generated identity tuple;
- the parent identity, with cycle and missing-parent rejection;
- every Requirement tuple, including localId, Beat/interval, kind, domain, action,
  constraints and effects;
- the declared and derived resource closure; and
- the mapping from generated identity to its full typed identity for snapshot and host override use.

Generated execution IDs use the separate domain
cuexis.entity.execution-id.v5.candidate.1 followed by a NUL byte and the complete Packed
identity bytes defined by Packed Chart section 6.5. The result is v5g1: followed by all 64
lowercase SHA-256 hex characters. Ordinals, display names, addresses, std::hash and truncated
hashes are not identity inputs.

Prepared semantic identity uses the separate domain
cuexis.prepared-semantic.v5.candidate.1 followed by a NUL byte. It contains the compiled
semantic identity and the sorted actual resource identities, but not paths, archive layout,
device selection, window settings, clock mode or gain. Execution configuration and the composite
session identity are defined in the Stage 6 configuration contract.

The existing v4 identity and FrameDigest v1-v3 algorithms remain unchanged. Candidate identity
does not rename or rewrite old v4 golden values.

## 6. Build And User Entry Boundary

The candidate build switch is CUEXIS_ENABLE_CHART_V5_CANDIDATE and its default is OFF. An ON
installation is marked experimental, uses a separate staging prefix and flavor metadata, and
requires Cuexis_ALLOW_EXPERIMENTAL=ON from the consumer. Production and experimental trees are
not mixed.

The candidate API has three new names only:

- fromFilesystemProjectEntry
- fromCxcFileEntry
- fromCxcMemoryEntry

Both OFF and ON builds expose the same declarations and class layout. OFF returns a stable
candidate-disabled error when these factories are called. ON still requires the explicit entry
path. The public API carries only locators, an owning entry path, owning CXC bytes and Result;
it does not expose Packed, CXC, CanonicalSemanticChart, Requirement or CXT AST types.

## 7. Support Matrix And Evidence State

| Capability | A2 contract | Current implementation state | Verification owner |
| --- | --- | --- | --- |
| Project and CXC extension shape | Recorded and schema-linked | Local production bridge exists; C1 exit is open | C1 |
| Explicit candidate factory names | Recorded in API draft | Public declarations exist; experimental install gate remains C4 | C1/C4 |
| v4 legacy factory behavior | Preserved by scope | Local MSVC playback tests still pass | C1/F1 |
| R5 candidate profile | Repeated here as a hard boundary | Playback uses the production bridge; hosted exit is open | C1 |
| Typed lowering and A16 | Field retention and ownership recorded | Local lowering and prepare path exist; C1 exit is open | C1 |
| Experimental install separation | Consumer and staging contract recorded | Version/package gate not implemented | B1/C4 |
| Official v5/default Writer | Explicitly excluded | Not supported | Stage 8 |

This file and the linked schema are A2 contract artifacts. They do not certify a Playback
implementation or a release.
