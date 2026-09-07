# Chart Format Foundation Fixtures

`manifest.json` records the F0 baseline, candidate revision/profile, v4 characterization
anchors, and the three 40,000-entity capacity profiles. Existing v4/CXT v1/CXC v1 files remain
the compatibility fixtures.

F2 candidate inputs:

- `valid/pattern_stair.cxt` — four-lane stair; default `groups=4` expands to 16 entities.
- `valid/pattern_stair_reordered.cxt` — same semantics with reordered arrays.

Packed goldens and 40k/16 MiB capacity artifacts remain F4/F8 outputs.

The CXC candidate mapping is carried by the registered
`extensions["cuexis.chart-entry.v1"]` object. `sourcePath` is optional; a playback entry
must point at an already compiled `packed-chart` artifact and must not trigger CXT parsing.
The mapping examples are kept in `cxc_candidate_extension.valid.json` and
`cxc_candidate_extension.invalid.json`; the latter is a rejection fixture for a missing
playback entry and unsupported encoding.
