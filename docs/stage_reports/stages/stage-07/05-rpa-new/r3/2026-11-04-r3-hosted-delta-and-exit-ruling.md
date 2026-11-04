# R3 hosted 口径裁定与批次退出（2026-11-04）

状态：owner ruling；R3 关闭记录

矩阵冻结点：`d70df15a463cb1fe4329bffb8f0c99bcac676c15`

hosted 验证点：`de73aa2c474fe142ef3d85764346fcd2773afce5`

返回 [New RPA 报告索引](../README.md)。当前门账与阶段状态只以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准，实施登记见
[R3 实施登记 §12](../../../../../stage_plans/active/stage-07/05-rpa-new/r3/plan.md)。

## 0. 决定

owner 于 2026-11-04 作出两项决定：

1. **hosted 口径**：接受“同源矩阵绑定 `d70df15a`、hosted 三平台绑定其后代 `de73aa2c`”作为 R3 的
   hosted 验证口径。两点之间只允许 §2 列出的变更类别；越界即失效，需重新冻结。
2. **批次退出**：R3 关闭。关闭含义仅为“R3 批次本地实施与验证完成、门账收口、hosted 口径已裁定”，
   不扩展到 R4、设备验证、生产接受或 Stage 7 关闭。

## 1. 事实

### 1.1 矩阵（本地，绑定冻结点）

[最终同源矩阵证据](2026-11-04-rpa-acceptance-matrix-final-evidence/index.md)：11/11 行 `local_pass`，
13,635 个 CTest 用例、0 失败、120 个候选 `[measurement]` 事件，逐行与清单级 `source_unchanged=true`；
`d70df15a` 的 5,052 个受检文件内容清单为 `f8ac9c2a…`，独立复算 `status=pass`。

### 1.2 冻结点到分支头的七个提交

| 提交 | 摘要 | 类别 |
| --- | --- | --- |
| `a866a59b` | 记录最终矩阵证据 | 文档 |
| `d5b894c1` | 版本推进到 26.11.04-1 | 版本字符串 |
| `7ac8769c` | asset publish 补 `<algorithm>` | include-only（离线工具） |
| `285c4c86` | animator 三元表达式改为显式 optional | 等价重构 |
| `3ea03380` | 测试探针补 nothrow delete | test-only |
| `9ef8eb9f` | 五个翻译单元补 `<cmath>` | include-only |
| `de73aa2c` | cxc unpack 接受 EBUSY 重试 | 离线工具语义 |

合计 28 个文件，逐项分类见 §2。

### 1.3 hosted（`de73aa2c`）

| 平台 | push | pull_request |
| --- | --- | --- |
| Linux Quality | `41838690080` success | `41838719915` success |
| Windows MSVC | `41838690136` success | `41838719809` success |
| Windows MinGW | `41838689826` success | `41838720171` success |
| Version Gate | — | `41838720070` success（SDK 0.7.0 → 0.9.1，批准评论 `6631952678`） |

### 1.4 为什么不在冻结点上直接跑 hosted

`d70df15a` 不含 §2 中的四项平台修复。逐项核对：

- `tools/common/asset_publish/src/asset_publish.cpp` 无 `#include <algorithm>`；
- `app/player/src/player_control_transaction.cpp` 无 `#include <cmath>`；
- `tests/pd/transaction_allocations.hpp` 无 nothrow delete；
- `tools/cxc/cxc_common/src/cxc_unpack_commit_internal.hpp` 无 EBUSY 分支。

这四处缺失在 hosted 上分别触发 MinGW 编译失败（`'sort'` / `'llround'` / `'isfinite'` 不是 `std` 成员）
与 Clang ASan `alloc-dealloc-mismatch`。因此该冻结点无法通过 hosted；在那个 SHA 上补跑只会得到一个
已知失败的结果，不构成验证。这也是 §1.2 的修复必须先于 hosted 的原因。

## 2. delta 分类

| 类别 | 文件 | 是否改变矩阵覆盖路径的可观测行为 |
| --- | --- | --- |
| include-only | `host_protocol_ledger.cpp`、`player_control_transaction.cpp`、`gameplay_capsule.cpp`、`gameplay_capsule_read.cpp`、`playback_session_gameplay.cpp` | 否 |
| include-only（离线工具） | `asset_publish.cpp` | 否 |
| 等价重构 | `chart_v4_loader_animators.cpp` | 否；语义等价，prepare/load 行仍全绿 |
| 离线工具语义 | `cxc_unpack_commit_internal.hpp`、`cxc_unpack_commit_tests.cpp` | 否；不在矩阵热路径 |
| test-only | `transaction_allocations.hpp`、`validation_sink_canonical.hpp`、`validation_sink_model.hpp`、`validation_sink_frame.cpp` | 否 |
| 版本与文档 | `cmake/CuexisVersion.cmake`、`vcpkg.json`、13 个 `docs/` 文件 | 否 |

计数：28 = 6 include-only + 1 等价重构 + 2 离线工具 + 4 test-only + 2 版本 + 13 文档。

## 3. 裁定条款

1. 豁免只在 §2 的类别内成立。任何改变矩阵覆盖路径可观测行为（计时、内存、语义）的提交不在豁免范围，
   必须重新冻结矩阵后才能继续。
2. hosted 验证点必须是 delta 为零的提交。§1.2 之后若再有提交，本条裁定不自动延伸。
3. 矩阵证据对 `d70df15a` 继续有效；`source-content-sha256.json` 与 `identity-closure.json` 不重写、
   不回填，delta 由本文登记。
4. 本裁定不修改 R3b 可信恢复边界、R3c 决定、128 MiB 判据取消与 40K @ 240 FPS 裁定。

## 4. 边界：不构成先例

- 适用范围仅限 PR #32 的 `a866a59b..de73aa2c`。
- 任何 R3 证据重算必须在新冻结点上执行，不能复用本文的分类结论。
- 本裁定不适用于 R4 及以后阶段；那些阶段仍要求同一 SHA 的 hosted 验证，除非另有 owner 裁定。
- 不构成设备验证、生产接受或 Stage 7 关闭。

## 5. 引用

- 矩阵证据：[2026-11-04 最终同源本地验收矩阵](2026-11-04-rpa-acceptance-matrix-final-evidence/index.md)
- 门账与 R3c 决定：[2026-11-04 最终本地验证与 R3c 决定](2026-11-04-r3-final-validation-and-r3c-decision.md)
- 四项门裁定：[2026-11-02 R3 owner 四项门裁定](2026-11-02-r3-owner-gate-rulings.md)
- 重复性口径：[2026-11-03 R3 重复性口径裁定](2026-11-03-r3-repeatability-scope-ruling.md)
