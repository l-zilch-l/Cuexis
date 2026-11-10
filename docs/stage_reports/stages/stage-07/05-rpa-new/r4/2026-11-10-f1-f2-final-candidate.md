# R4-6 F1 本地矩阵与 F2 hosted 验证（最终候选）

状态：evidence；F1/F2 已在同一 SHA 通过，不含接受裁定

日期：2026-11-10

候选：`2c4e1274b02415c288cc756dcd7dfef5c59a0f01`（`26.11.09-1`，tree `ec24db81`）

返回 [R4 门账](2026-11-08-r4-gate-status.md)与 [New RPA 报告索引](../README.md)。原始字节位置见\n[F1/F2 证据包](2026-11-10-f1-f2-final-candidate-evidence/index.md)。

## 1. F1：本地 11 行矩阵

`tools/verification/run_rpa_acceptance_matrix.py` 未修改，逐行独立构建，全部 `local_pass`：

| 行 | 结果 | 用时（分钟） |
| --- | --- | ---: |
| `msvc-on-debug` | local_pass | 14.1 |
| `msvc-on-release` | local_pass | 21.5 |
| `msvc-on-shared-debug` | local_pass | 14.0 |
| `msvc-off-debug` | local_pass | 10.6 |
| `msvc-off-headless-debug` | local_pass | 11.1 |
| `msvc-off-release` | local_pass | 15.1 |
| `msvc-off-headless-release` | local_pass | 15.5 |
| `msvc-off-shared-debug` | local_pass | 11.5 |
| `mingw-on-debug` | local_pass | 18.2 |
| `mingw-on-shared-debug` | local_pass | 17.9 |
| `mingw-on-release` | local_pass | 17.8 |

没有行被跳过、过滤或替换；每次尝试（含失败）都留在 `out/evidence/r4/f1rows*` 里。

### 1.1 两次环境性失败（与源码无关）

1. `msvc-on-release` 首次在链接阶段失败：`The process cannot access the file because it is being used by another process`。
   同 SHA 重跑即通过，源码未变。
2. `msvc-off-headless-debug`、`msvc-off-headless-release`、`msvc-off-shared-debug` 首次在同一处失败：
   `cuexis_msvc_dependency_tracking` 的 canary 配置报
   `fatal error C1083: 无法打开编译器生成的文件: ""`。原因是 canary 的 scratch 路径在
   `D:\Cuexis-worktree\out\build\r4\...` 之下超过 260 字符；把同一 canary 源放到短路径
   （`D:\xtest`）配置成功，确认是 MAX_PATH 而不是源码缺陷。这三行因此改在同一 commit 的
   短路径 worktree（`D:\w1`）中执行，证据已拷回 `f1rows-short/`。

## 2. F2：同 SHA hosted

`2c4e1274` 的 push 与 pull_request 两种事件下：

| workflow | 结果 |
| --- | --- |
| Linux Quality | success |
| Windows MSVC | success |
| Windows MinGW | success |

## 3. Version Gate

`Version Gate` 在 `2c4e1274` 上 success。批准记录是 PR #32 上一条未编辑的 owner 评论
（`6745276741`，`utc_date` 2026-11-09），`from` `0.7.0` → `to` `0.9.1`，与
`--trusted-utc-date 2026-11-09` 一致；候选版本为当天日期且 `build = 1`。

## 4. 边界

- F1 是本机 MSVC/MinGW 的本地矩阵，不等于 F3 生产参数校准。
- F2 是同一 SHA 的 hosted 结果，不覆盖 O2 实体操作、P7 听音或设备级证据。
- 本报告不构成 R4 接受，也不改变任何 O1 数值。
