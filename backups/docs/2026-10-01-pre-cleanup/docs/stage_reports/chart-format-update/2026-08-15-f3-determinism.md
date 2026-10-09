# Stage Chart Format Update CFU-F3 Determinism

状态：completed locally

快照日期：2026-08-15

实施基线：`codex/cfu-f` on commit `819064556e7f6eb0fe32a075cbcf7b13e141d8a4`
plus the uncommitted CFU-F1/CFU-F2/CFU-F3 surface closed by this report

权威计划：[Stage Chart Format Update 实施计划](../../stage_plans/completed/chart-format-update/plan.md)

## 1. 结论

CFU-F3 已完成本地实现与 MSVC 验证。新增 `cuexis_cfu_f3_determinism` CTest 使用生产
`cuexis_cxc_pack`、`cuexis_chart_migrator --target 4` 和 public Playback headless consumer，把
canonical CXC bytes、migration bytes/report、Prepared semantic identity、FrameDigest v3 与 capability
diagnostics 顺序合成为一个 LF-only committed fingerprint。

测试目录保留生成的 CXC、迁移 chart/report、工具 stdout/stderr、actual/expected fingerprint 和
`evidence.txt`。evidence 记录完整 40-hex implementation SHA；本地未显式注入时从 Git worktree
解析，hosted workflow 必须显式使用 `${{ github.sha }}`。

该本地关闭不是 hosted 跨平台 parity 成功、最终实现 SHA 证据、完整 CFU-F、公共 CXC package
API、完整 v4 动画 Playback、CFU-G 或 Stage 4。CFU-F4 为下一批次。

## 2. Deterministic Fingerprint

committed `tests/fixtures/chart_format_update/golden/cfu_f3_determinism.txt` 固定：

```text
format=cuexis.cfu-f3.determinism
version=1
cxc_sha256=1cb2fcbf7a852a4db2ed9119359c68ff1cc4a06d1d41d18018fff89cf737d723
cxc_bytes=6905
semantic_identity=6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5
stop_frame_digest_v3=11596562486377158370
migration_chart_sha256=1ca9f60feee215fdc4eca1f7cafbbea8704976eca18ed40e51333b6b2e7a5385
migration_report_sha256=81df14e422603ae411ea8a70f5de89fb49ae2a34b804af71146a8be3075824e0
diagnostic_signature=playback.capability.unsupported@$/animationClips#cuexis.animation.clip.v1|playback.capability.unsupported@$/objects#cuexis.animation.layers.v1
```

fingerprint 文件本身 SHA-256 为：

```text
9f7f98fc3bedf81588e62011fd3a49587014b3390aae3906404498e7b60176a0
```

生成文件与 committed fingerprint 都显式使用 LF，不依赖宿主文本换行。CXC 和 migrated chart
先 byte-compare golden，再进入 aggregate fingerprint；migration report 由 CLI binary-mode 写入并按
bytes 哈希。headless consumer 输出的 identity、Stop digest 和 diagnostics signature 由 CMake 门禁
解析，缺少任一观察值即失败。

## 3. SHA-bound Hosted Evidence

新增 cache input `CUEXIS_IMPLEMENTATION_SHA`。F3 test 要求完整 40 位十六进制 SHA，并把它写入：

```text
out/build/<preset>/cfu-f3/evidence.txt
```

Linux Quality、Windows MSVC 与 Windows MinGW workflow 均在 configure 时显式传入
`${{ github.sha }}`。以下 developer-tool 配置会在全量 CTest 后独立重跑 F3，并上传包含 preset 与
GitHub SHA 的 artifact：

```text
GCC Release
Clang Shared Debug
MSVC Debug / Release
MinGW Debug / Release
```

artifact 上传使用 `always()`，因此失败运行仍可保留已生成的 package、migration 与 fingerprint
差异。当前报告没有 hosted run URL 或跨编译器成功结果；这些必须在最终实现 SHA 上取得，不能由
本地 MSVC 结果替代。

## 4. Local Verification

工具链：Visual Studio 2026 Community 18 / MSVC 19.51.36248.0，CMake 4.3.3，Windows x64。

```text
Debug fresh configure + clean-first full build                 passed (200 build steps)
ctest --preset debug --no-tests=error                           369/369 passed
Release fresh configure + clean-first full build               passed (200 build steps)
Release full CTest initial run                                  368/369 passed
cuexis_cxc_tools immediate isolated retry                       1/1 passed
Release full CTest final rerun                                  369/369 passed
shared-debug CFU-F3 deterministic gate                          1/1 passed
headless-release + developer tools CFU-F3 gate                  1/1 passed
LF-only fingerprint/evidence check                              passed (0 CR bytes)
cuexis_format_check                                             passed
tools/check_docs.py                                             129 Markdown / 20 JSON-CXT passed
tools/update_version.py --check                                 passed (26.08.01-1)
git diff --check                                                passed
```

Release 初次失败来自既有 `cuexis_cxc_tools` 的一次
`cxc.unpack.output_commit_failed`；相同二进制和工作树立即独立复跑通过。F3 deterministic gate 在
Debug、Release、shared Debug 与 adapter-disabled headless Release 四个配置均通过，四者 fingerprint
SHA-256 相同。随后完整 Release CTest 复跑通过 `369/369`；本报告仍保留首次瞬时失败记录。

## 5. Handoff

CFU-F4 下一步覆盖 archive/parser/offset/closure sanitizer、Playback/CXC/Chart clang-tidy、CXC/Chart v4
coverage 缺口、warmed-up update/extract allocation，以及 limits/overflow/diagnostic truncation 和性能
证据。CFU-F 整体继续保持打开，直到 F4 完成且最终实现 SHA 的 hosted Linux/MSVC/MinGW parity
全部成功。
