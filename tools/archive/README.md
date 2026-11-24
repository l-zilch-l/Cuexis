# tools/archive

状态：historical

更新日期：2026-11-17

本目录存放已停用、但为历史证据保留的工具。规则见 [tools/README.md](../README.md)：
归档保留原相对路径，不改写证据报告；删除需要 owner 明确同意。

## 路径对照

| 原路径 | 归档路径 | 原因 |
| --- | --- | --- |
| `tools/research/gameplay_fold_spike/` | `tools/archive/research/gameplay_fold_spike/` | 2026-10-01 fold spike；无 CMake target、无代码 import，设计链已被 ADR 0043 → 0044 取代 |
| `tools/verification/playback_performance/compare_read_candidates.py` | `tools/archive/verification/playback_performance/compare_read_candidates.py` | C2 读候选对照；无测试、无 docs 引用，候选已关闭 |

## 注意

- 2026-10-08 起的证据 manifest（如 `docs/stage_reports/stages/stage-07/01-a/s7a-7-8/2026-10-08-s7a-7-8-evidence-manifest.json`）
  按**当时路径**记录 research 文件的 SHA-256；本目录的移动不回溯改写这些 manifest，复核时以 manifest 记录的
  原路径为准。
- `tools/research/**` 曾被发现逻辑覆盖在 `CUEXIS_FORMAT_FILES` 的递归 glob 下；归档后仍会被格式门禁扫描，
  保持格式即可，不要为归档文件缩小门禁范围。
