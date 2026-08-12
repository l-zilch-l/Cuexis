# CFU-C3 Binary Fixtures

Committed binary evidence for the internal `cuexis_cxc` strict ZIP32 envelope and package closure.

The set includes canonical/noncanonical valid archives plus malformed ZIP64, descriptor, compression,
multi-disk, extra/comment, file-type, path, range, CRC, manifest-integrity and hidden-closure cases.
Normal tests are read-only. Regeneration requires `CUEXIS_UPDATE_CXC_FIXTURES=1` and uses the production
Writer.
