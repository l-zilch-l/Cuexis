# Chart Format Foundation F0 Fixtures

This directory contains the F0 inventory only. It does not claim that CXT v2 or Packed Chart
readers/writers are implemented. Existing v4/CXT v1/CXC v1 files remain the compatibility
fixtures until the Foundation candidate path has its own generated fixtures and golden bytes.

- `manifest.json` records the frozen baseline, candidate revision/profile, v4 characterization
  anchors, positive/negative fixture inputs, and the three 40,000-entity capacity profiles.
- F2/F4/F8 may add generated inputs and golden files, but must preserve the manifest IDs and
  record the generator/compiler revision used to produce them.

