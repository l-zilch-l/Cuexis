# F1 collector parse regression fixture

`f1-scale-0.log` is the unmodified native output from the failed tool-only F1
scale attempt on October 23, 2026. The collector assumed one
`PD_FOLLOW_CONTENT` record per object count; the executable emitted an identity
pair around each read block group. The fixed parser accepts that observed
shape and continues to reject missing, reordered or changed identities.

Original source (historical evidence; read-only):

```text
D:\Cuexis-pd5-40k-admission\out\pd5-etw-f1-tools-20261023-3e5837f-r2\f1-20261023T122949Z-8ca88299bbd24d30a8c40ba75ec191b5\f1-scale\f1-native\f1-scale-0.log
```

- Size: 6549 bytes.
- SHA256: `458df31dcacf96ad900715204f304fcb6883978232c6ac773d9d98255a86e459`.
- Copied byte-for-byte into the main worktree on October 26, 2026; no original
  raw log, sealed package, registration or result was changed.
- This failed attempt is historical tooling evidence, not a successful F1
  acceptance run, formal measurement or performance acceptance.

`tests/pd/pd5_etw_collect_f1_parse_tests.py` prefers this fixed fixture and checks
its bytes, hash, 2 process starts, 24 rows and 8 content identities offline. The
old repository-relative `out/` location is only a fallback; missing both copies
fails the regression instead of skipping it. No role or native workload runs.
