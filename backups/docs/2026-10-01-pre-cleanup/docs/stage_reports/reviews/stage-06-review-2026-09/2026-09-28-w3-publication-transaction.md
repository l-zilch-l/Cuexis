# Stage 6 复核修正：R5 发布事务修正

状态：completed
快照日期：2026-09-28
更新日期：2026-09-28

本报告是 [Stage 6 复核修正计划](../../../stage_plans/reviews/stage-06-review-remediation/plan.md)
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

## 7. 跨批次修复：W1 用例的检出深度依赖（hosted 阻断项）

W1 提交 `07c8a97` 在 PR #30 的 hosted 检查中**未通过**，是本批次必须先行修掉的阻断项。

- **现象**：`cuexis_contract_version_gate` 在 `GCC Coverage`（job `108703347653`，`633/634`）与
  `GCC Adapter Coverage`（job `108703347683`，`679/680`）中以
  `fatal: ambiguous argument 'HEAD^': unknown revision or path not in the working tree.` 失败。
- **根因**：W1 新增的 `test_compare_refs_rejects_invalid_missing_and_non_ancestor_refs`
  调用 `git rev-parse HEAD^` 读取**环境仓库**的父提交。hosted 的 `actions/checkout` 取
  `fetch-depth: 1` 浅检出，父提交不存在；本机是全历史检出，所以本机 15 tests 全绿而 hosted 失败。
  两个失败 job 的失败测试**是同一个**，根因相同，不是两个独立问题。
- **修复**：该用例改为在 `tempfile.TemporaryDirectory` 中 `git init` 并造两个提交，
  自身构造 base/candidate 关系，或用例不读环境历史。变更后不再引用 `HEAD^`。
- **复现与验证**：
  1. 复现失败条件：`git clone --depth 1` 得单提交仓库，`git rev-parse HEAD^` 确认报同样的 fatal。
  2. 在该浅克隆中先跑**旧**用例文件 → `FAILED (errors=1)`，与 hosted 一致。
  3. 覆盖为**新**用例文件后重跑 → `Ran 15 tests ... OK`。
  4. 本机全历史检出：`Ran 15 tests in 24.464s ... OK`。
- **订正说明**：该修复同时是本批次对 W1 结论的**诚实订正**——
  W1 §4 的"本机通过"不蕴含 hosted 通过；已在 W1 报告 §5 追加订正说明指向本节。

## 复核更正（独立审计，2026-09-28）

本次复核独立重跑了 R5 的测试并用 git 历史核对前置版本。以下为**已确认的实质性声明**与**必须更正的表述**。

**核对前提**：审计期间工作区 HEAD 由 `0c8f837` 前进到 `b59d899`；`git status --porcelain` 除既有未跟踪文件（`.buildenv.ps1`、`.workbuddy/`）外无改动，无跟踪文件被修改。R5 的代码与用例自批次提交后未变：`git log 3ee5717..HEAD -- tools/asset_publish/ tests/asset_publish/` 仅列出 `3f9551b`，且该提交只改 `publish_fs_internal.cpp` 的 `_MSC_VER` 选择（MSVC 下与 `_WIN32` 等价）。因此下述核对结论对批次代码成立。

### 1. 已确认的实质性声明

- **单次原子替换**：`tools/asset_publish/src/asset_publish.cpp:1048` 的
  `auto committed = detail::replaceAtomically(temporary, target);` 是 `commitPackage` 中唯一触碰
  target 的调用。前置版本（`git show 07c8a97:tools/asset_publish/src/asset_publish.cpp`）确有两次
  rename：blob `:951` `auto moved = detail::replaceAtomically(target, backup);` 与 blob `:958`
  `auto committed = detail::replaceAtomically(temporary, target);`；两者均被 `e907606` 删除。
  `git grep -n replaceAtomically -- tools/asset_publish/` 的其余站点（`:667`、`:842`、`:907`、`:1075`）
  没有任何一处把在用包移开。target 缺失窗口在代码层确已关闭。
- **恢复还原 backup 而非删除**：`asset_publish.cpp:649-679` 新增 `restoreMatchingBackups`
  （target 缺失时 `:667 auto moved = detail::replaceAtomically(candidate, restored);`，仅在 target 已存在时
  `:672-676` 删除备份）；`detail::backupTargetOf` 见 `publish_fs_internal.cpp:261-280`。前置版本 blob `:598`
  在 `removeMatchingTemporaries` 中匹配 `publicationBackupRole`，即把备份当临时文件删除。
- **pair 锁覆盖两个目标父目录**：`asset_publish.cpp:1146-1148` 调用
  `PublicationLock::acquireAll({v4 父目录锁, candidate 父目录锁})`，实现见 `:569-587`（排序、去重、任一失败即由
  析构释放已持有句柄）。
- **五条 R5 用例存在、已注册、全绿**：
  `out\build\debug-media-tools\bin\cuexis_asset_publish_tests.exe "[r5]"` →
  `All tests passed (85 assertions in 5 test cases)`；
  `out\build\debug-media-tools\bin\cuexis_asset_publish_tests.exe` →
  `All tests passed (332 assertions in 23 test cases)`（与 §4 记录一致）；
  `--list-tests "[r5]"` → `5 matching test cases`；`TEST_CASE` 计数由 `07c8a97` 的 19 增至 24（+5，其中 1 条为
  锁持有子进程用例，默认运行中被隐藏）。

### 2. `closureBytes` 只修了一半（**必须按此表述**）

`asset_publish.cpp:768-770` 的幂等路径已改用 `closureByteTotal(marker.closure)`，但**第二条幂等返回路径仍返回 `0`**：
`asset_publish.cpp:849`

```
                return GenerationPublishResult{generationPath, marker.identity, 0,
                                               marker.closure.size(), marker.provenance.size()};
```

该路径是"替换失败但目标上已存在同一 generation"的竞态返回，`e907606` 未触及。在该行修正前，SPEC-24（幂等）
的表述应为**部分修正**，并点名 `:849`；§2 中"幂等路径与首次发布路径共用同一算法"一句不成立。

### 3. 前置值不是条目数，而是 `0`

§2 写"幂等重发把 `closureBytes` 报为条目数（`marker.closure.size()`）"，与实际前置代码不符：
`git show 07c8a97:tools/asset_publish/src/asset_publish.cpp` 在该路径上读作
`GenerationPublishResult{generationPath, marker.identity, 0, marker.closure.size(), ...}`，
第三字段为 `0`；本批次处置的复核记录亦如此写：`docs/stage_reports/reviews/stage-06-review-2026-09/2026-09-28-spec.md:376`
"在重复发布时把 `closureBytes` 置 0"。因此 §2 的现象描述应更正为"置 0"，§3 第 4 行的变异（改为
`marker.closure.size()`）不是"改回缺陷形态"，而是一个同样可被断言捕获的替代错误值。

### 4. R5 用例 1 未被报告所述的变异推翻

用例 1（`tests/asset_publish/asset_publish_tests.cpp:797`）注入 `CUEXIS_ASSET_PUBLISH_FAIL_TEMPORARY_TARGET`
移除临时文件，使替换**优雅失败**。前置版本的 `commitPackage` 在第二次 rename 失败后有一条回滚分支
（blob `:961-962` `if (!backup.empty()) { auto restored = detail::replaceAtomically(backup, target);`），
因此把控制流改回双 rename（保留同一注入钩子）时 target 会被还原、`CHECK(fs::exists(target))` 通过，用例仍然**通过**。
该用例只能捕获"移开 target 且**没有**回滚分支"的形态，不能区分本批次修正前后的实现。§3 第 1 行的变异记录不得
被读作"该窗口已被用例证明关闭"；窗口关闭的依据是单次 rename 的结构（见 §1），而非此用例。

### 5. 用例 3 的断言偏弱

用例 3（`:859`）的写失败钩子在 `asset_publish.cpp:994-1002` 提前返回，位于 `detail::writeFileExclusive`
（`:1003`）之前，因此临时文件从未被创建，`:894 CHECK( leftovers == 0U )` 恒真；真正的写失败清理分支
`asset_publish.cpp:1004-1007` 从未被执行。它仍然可被变异推翻（禁用钩子后发布成功，`:881 REQUIRE_FALSE` 失败），
但 §2 所称"走真实 `asset.publish.io_failed` 码路径"只对错误码常量成立，不对失败的写系统调用成立（§5 已自认）。

### 6. §3 行号引用更正

用例文件自 `3ee5717` 起未再改动（`git diff --stat 3ee5717 HEAD -- tests/asset_publish/` 为空），故下列为确实的引用错误：

| §3 行 | 报告引用 | 实际行 | 实际内容 |
| --- | --- | --- | --- |
| 1 | `:825 CHECK(fs::exists(target))` | `:827` | `:825` 是 `CHECK(errorCode(failed.error()) == "asset.publish.replace_failed");` |
| 2 | `:853 CHECK(fs::exists(target))` | `:854` | `:853` 是 `requireOk(removed);` |
| 3 | `:923 REQUIRE_FALSE(blocked.has_value())` | `:922` | `:923` 是 `CHECK(errorCode(blocked.error()) == "asset.publish.busy");` |
| 4 | `:955` | `:955` | 正确 |
| 5 | `:881` | `:881` | 正确 |

### 7. §3 第 1 行的失败签名不可能成立

若 target 真的缺失，用例会先在 `:828 CHECK(readBytes(target) == previousBytes)` 处经由 `readBytes` 内部的
`REQUIRE(stream.good())`（`:68`）中止，早于 `:829 loadPackage(target)`（其内部断言在 `:198`）。因此"`loadPackage`
以嵌套断言异常终止"的记录与该用例的实际控制流不符。

### 8. §4 CTest 编号更正

编号今天不可复现。`ctest --preset debug-media-tools -N` 的实际输出为：

```
  Test #777: R5 a replacement failure never leaves the target missing
  Test #783: R5 recovery restores a backup instead of deleting it
  Test #784: R5 a write failure leaves the previous package and no temporary file
  Test #789: R5 an idempotent republish reports the real closure size
  Test #793: R5 the pair lock covers both target parent directories
Total Tests: 795
```

注册本身为真：`out\build\debug-media-tools\tests\asset_publish\cuexis_asset_publish_tests-b12d07c_tests.cmake`
含 23 条 `add_test`。差异属后续提交引起的编号漂移，但 §4 所记 `#764`/`#769`/`#772`/`#773`/`#777` 应更正为上述实际值。
另外 §4 把 `ctest --preset debug-media-tools --no-tests=error` 的结果指向 §4.1，而 §4.1 并未记录任何结果。

### 9. 新登记的残余（本报告此前未声明，不属"表述不符"）

- pair 事务在 `asset_publish.cpp:1166`（v4 提交）与 `:1172`（candidate 提交）之间崩溃，会留下"新 v4 + 旧 candidate"
  的混合对；`asset_publish.hpp:134-135` 只承诺替换**失败**时回滚，未承诺崩溃一致性。
- `rollbackPackage` 的临时文件角色为 `"rollback"`（`asset_publish.cpp:1069`），既不被
  `removeMatchingTemporaries`（`cuexis-publish`）也不被 `restoreMatchingBackups`（`cuexis-backup`）匹配，
  回滚中途崩溃会永久泄漏 `.rollback.tmp.*` 文件。

### 10. 未核实项

- §3 的五条变异实验本身**未核实**（未重新执行）：执行变异需要修改源码，本次审计为只读，不允许改动任何文件。
  第 2、3、4、5 条的"可被变异推翻"由代码路径推理得出；第 1 条按 §4 的推理判定为不可复现。
- §4 的构建类门禁**未核实**：`cmake --preset`、`cmake --build`（含 `cuexis_format_check`）被本次审计的硬性限制
  禁止执行（其他代理共用同一构建树）。
- 只读门禁的复跑结果与 §4 记录存在后续提交造成的漂移，非本批次证据失效：
  `python -B tools/check_docs.py` → `Documentation checks passed: 274 Markdown files and 20 candidate JSON/CXT files validated.`（§4 记 268）；
  `python -B tools/update_version.py --check` → `Cuexis version is consistent: 26.09.28-1`（§4 记 `26.09.27-2`）；
  `git diff --check` → 无输出，退出码 0（与 §4 一致）。

**结论**：§2 的原子替换与恢复两项修正、§2 的锁范围修正均已在代码中落实并经真实用例复跑确认；幂等 `closureBytes`
为部分修正（`:849` 仍返回 0）；§3 的变异记录与行号、§4 的 CTest 编号需按本节更正；用例 1 与用例 3 的证明力弱于
报告表述。

### 后续（同日，`:849` 已修复）

上节记录的"幂等 `closureBytes` 为部分修正（`:849` 仍返回 0）"**已不再成立**：
`asset_publish.cpp:849` 现返回 `closureByteTotal(marker.closure)`，与该函数另一条幂等路径
（`:768-770`）一致。修复理由是该处**违反本模块自身的契约**——即"即使本次未写入，
也要报告真实的闭包字节总数"，这一点由既有用例直接钉住
（`tests/asset_publish/asset_publish_tests.cpp:955` 断言 `second->closureBytes == bytes.size()`，
`:954` 断言两次 `closureBytes` 相等）。

**诚实性登记**：`:842` 的 `replaceAtomically` 失败分支**没有故障注入钩子**
（现有钩子 `CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_PUBLISH` 在 `:828` 提前返回，
`CUEXIS_ASSET_PUBLISH_FAIL_REPLACE_TARGET` 只作用于 `:1023` 的 `commitPackage` 路径），
因此这处修复**没有本次新增的测试覆盖**；它的正确性来自与被测路径 `:768-770` 的对称性，
而不是来自一次独立复跑。此分支若要被测试覆盖，需要新增一个能令该次 `replaceAtomically`
失败的注入点——属**后续工作**，本报告不作"已验证"之声明。
