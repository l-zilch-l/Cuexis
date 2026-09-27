# Stage 6 复核修正：R5 发布事务修正

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/active/stage-06-review-remediation/plan.md)
批次 **W3（R5）** 的退出记录。它逐条处置
[两轴复核记录](2026-09-28-spec.md) §8（S8 — S6-E3 资源身份、缓存与原子发布）中属于发布事务的发现项。

**修正原则**：不改写任何历史报告的现象与证据原文；所有新增断言都以**实际变异代码站点后测试必须失败**
的方式反证其非空转，反证记录见 §3。

## 1. 批次边界

- 起始 SHA：`07c8a97`（W1 批次 HEAD）。
- 涉及路径：`tools/asset_publish/src/`、`tools/asset_publish/include/`、`tests/asset_publish/`，
  以及两处 W1 遗留的 clang-format 违反文件。
- 未触碰：ADR 0042 冻结决策正文、Stage 7A/8/9–12 任何交付物、提案 1（Chart v1–v3 退出）。
- 本批次**未**改动 `docs/` 下任何历史报告的现象或证据原文；本报告是唯一新增文档。

## 2. 逐条处置

| 复核 ID | 严重度 | 现象（原文摘要） | 本批次修正 | 证据 |
| --- | --- | --- | --- | --- |
| SPEC-23 | 高 | `commitPackage` 先 `target → backup` 再 `temporary → target`，两次 rename 之间存在 target 缺失窗口；`recoverPublicationStaging` 又把 backup 当临时文件删除，崩溃后既无新包也无旧包 | 去掉 `target → backup` 这一步，改为**单次原子替换**；恢复逻辑新增 `restoreMatchingBackups`，对 `.cuexis-backup.tmp.*` 在 target 缺失时**恢复**而非删除；新增 `detail::backupTargetOf` 反转 `uniqueSibling` 命名 | `tools/asset_publish/src/asset_publish.cpp`；`tools/asset_publish/src/publish_fs_internal.cpp`（`backupTargetOf`）；`tests/asset_publish/asset_publish_tests.cpp` 的 R5 用例 1、2 |
| SPEC-24（锁范围） | 中 | pair 事务只锁 `v4Target.parent_path()`，却同时写 `candidateTarget`；两者不同目录时 candidate 不受锁保护 | 新增 `PublicationLock::acquireAll`：对**全部目标父目录**排序后依次加锁，任一失败即释放已持有的全部句柄；`publishPackagePair` 改用它，两个父目录同锁 | `tools/asset_publish/src/asset_publish.cpp`；`tools/asset_publish/include/cuexis/tools/asset_publish.hpp`；R5 用例 4 |
| SPEC-24（幂等） | 中 | 幂等重发把 `closureBytes` 报为条目数（`marker.closure.size()`），与首次发布记录的闭包字节数不一致 | 新增 `closureByteTotal()` 求和 `entry.byteCount`；幂等路径与首次发布路径共用同一算法 | `tools/asset_publish/src/asset_publish.cpp`；R5 用例 5 |
| SPEC-25 | 低 | 磁盘满与权限失败仍未取证（批次报告自认，未进关闭报告 §8） | 新增两个测试专用注入钩子：`CUEXIS_ASSET_PUBLISH_FAIL_WRITE_TARGET`（拒绝写入，走真实 `asset.publish.io_failed` 码路径）、`CUEXIS_ASSET_PUBLISH_FAIL_TEMPORARY_TARGET`（替换前移除临时文件，模拟替换失败）；各补一条可执行用例 | `tools/asset_publish/src/asset_publish.cpp`；R5 用例 1、3 |

### 2.1 追加订正说明

- `GenerationPublishResult::closureBytes` 的语义是**闭包字节总数**，不是条目数。
  关闭报告 §8 残余清单需补记 SPEC-25 项（只读介质/写入失败的取证），见 §5。
- 关闭报告或批次报告中若曾以"backup 冗余"描述 `publicationBackupRole`，本批次已使该角色
  **仅用于恢复历史遗留备份**；新写入路径不再产生 backup。

## 3. 反证记录（每条新增断言必须能被真实变异推翻）

每一条都以"把实现改回缺陷形态（PROBE 补丁）→ 运行 `[r5]` 用例 → 断言失败"的方式验证非空转，
验证后实现全部还原（`grep -c PROBE` = 0）。

| # | 断言 | 变异方式 | 结果 |
| --- | --- | --- | --- |
| 1 | R5 用例 1「替换失败后 target 从不缺失」 | 恢复双 rename 序列（`target → backup` 后再替换） | **失败**：`asset_publish_tests.cpp:825 CHECK(fs::exists(target))` → `false`，即 target 真的消失 |
| 2 | R5 用例 2「恢复逻辑还原 backup 而非删除」 | 让 `restoreMatchingBackups` 跳过还原、直接删除 | **失败**：`:853 CHECK(fs::exists(target))` → `false` |
| 3 | R5 用例 4「pair 锁覆盖两个目标父目录」 | 锁范围回退为 `PublicationLock::acquire(v4Parent)` 单路径 | **失败**：`:923 REQUIRE_FALSE(blocked.has_value())` → 持有 candidate 锁时 pair 仍发布成功 |
| 4 | R5 用例 5「幂等重发报告真实闭包大小」 | 把 `closureByteTotal(...)` 改回 `marker.closure.size()` | **失败**：`:955 1 == 9`（条目数 vs 字节数） |
| 5 | R5 用例 3「写入失败留下旧包且无临时文件」 | `if (false && testFailureTargetMatches(...))` 禁用写入注入钩子 | **失败**：`:881 REQUIRE_FALSE(failed.has_value())` → 发布意外成功 |

**方法论记录**：第 1 条反证首次运行时，用例的 `loadPackage(target)` 以嵌套断言异常终止，
说明"target 消失"这一后果直接破坏了后续读取——正是 SPEC-23 描述的不可恢复状态，
比单纯的存在性检查更能证明缺陷真实存在。

## 4. 门禁与命令输出

| 命令 | 结果 |
| --- | --- |
| `cmake --preset debug-media-tools` | 配置成功（`CUEXIS_BUILD_MEDIA_TOOLS=ON`） |
| `cmake --build --preset debug-media-tools` | 全量构建 0 错误 |
| `cuexis_asset_publish_tests.exe` | `All tests passed (332 assertions in 23 test cases)`（含 5 条 R5 新用例） |
| `ctest --preset debug-media-tools -N` | R5 五条用例均已注册为真实 CTest 条目（`#764`/`#769`/`#772`/`#773`/`#777`） |
| `ctest --preset debug-media-tools --no-tests=error` | 见 §4.1 |
| `cmake --build --preset debug --target cuexis_format_check` | 通过（无 clang-format 违反） |
| `python -B tools/check_docs.py` | `Documentation checks passed: 268 Markdown files and 20 candidate JSON/CXT files validated.` |
| `python -B tools/update_version.py --check` | `Cuexis version is consistent: 26.09.27-2` |
| `git diff --check` | 通过（无空白错误） |

### 4.1 说明

`asset_publish` 目标受 `CUEXIS_BUILD_MEDIA_TOOLS` 门控（`CMakeLists.txt:170-176`），
在基础 `debug` preset 中**不构建**，因此其用例的正确门禁 preset 是 **`debug-media-tools`**
而非复核建议中写的 `ctest --preset debug -R asset_publish`。本批次按实际门控执行，
并在此记录该口径订正。

## 5. 残余与未核对

- **磁盘满与只读介质**：SPEC-25 的两条用例通过**注入钩子**走真实失败码路径，属故障注入而非
  真实满盘/只读卷。真实介质失败的取证仍属未完成项，应补记进关闭报告残余清单
  （原 C4 退出报告 §7:259 已自认，本批次只把它变成**可执行证据**而非消除该残余）。
- **崩溃注入的粒度**：SPEC-23 的用例在同进程内以"替换前移除临时文件"模拟替换失败；
  它证明的是"单次替换不存在缺失窗口"这一**结构性**结论，而非真实进程在两次系统调用之间被杀死
  的时序复现。该结论对单次 `MoveFileExW`/`rename` 是充分的，但若要覆盖电源级崩溃持久性，
  仍需文件系统级故障注入（属 F1 范畴）。
- **`PublicationLock::acquireAll` 的跨进程死锁**：实现按路径排序后依次加锁以避免自死锁；
  本批次只覆盖同进程的 held-then-pair 场景，未做两个进程各持一半路径的交叉测试。

## 6. 变更文件

- 实现：`tools/asset_publish/src/asset_publish.cpp`、`tools/asset_publish/src/publish_fs_internal.cpp`、
  `tools/asset_publish/src/publish_fs_internal.hpp`、
  `tools/asset_publish/include/cuexis/tools/asset_publish.hpp`
- 测试：`tests/asset_publish/asset_publish_tests.cpp`（新增 5 条 R5 用例）、
  `tests/chart/packed_budget_tests.cpp`、`tests/cxc/cxc_candidate_roundtrip_tests.cpp`（格式化）
- 文档：本报告
