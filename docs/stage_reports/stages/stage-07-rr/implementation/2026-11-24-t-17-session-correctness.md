# T-17 会话层正确性修复

状态：evidence

<!-- cuexis-meta
id: REPORT-stage-07-rr.t-17-session-correctness
status: evidence
authority: none
date: 2026-11-24
-->


- 仓库：`E:\Cuexis-worktree`；基线 `8cfb302d`（工作树含并发 T-05 改动，见文末）。
- 日期：2026-11-24。
- 验收入口：`candidate-headless-debug`（卡指定；该预设 `CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON`、developer tools ON）。
- 默认闭包对照：`headless-debug`，用于证明 T-04 的默认源表/安装面未被本卡改动破坏；不替代卡内验收入口。

## 1. 四档结论

| 项目 | 结论 | 依据 |
| --- | --- | --- |
| RR-F509 Prepared owner 寿命守卫 | 成立 | `rr_session_prepared_owner` 通过；失效 lease 与真实会话销毁两条路径均返回既有 `host.rpa.context_stale`，解引用计数不增加 |
| RR-F508 unload 五字段清零 | 成立 | `rr_session_unload_state` 通过；前后两次字段读数、公共 owner 状态与再次 commit 均符合预期 |
| RR-F501 Presentation/GameplayOnly 有效性 | 成立 | `rr_gameplay_only_accept` 通过；两类合法候选、空/移后/错线程、内容与音频访问、表现能力独立拒绝、独立提交门禁均有断言 |
| RR-F203 临时覆盖登记前整批校验 | 成立 | `rr_override_registration_validation` 通过；类型错、值域错、合法首项+非法后项均无 token/无部分写入，失败后更新与合法 token 生命周期不变 |
| 默认/候选闭包未回退 | 成立 | 两个预设构建通过；`cuexis_rr_playback_isolation` 两个预设通过；默认与候选全套测试通过 |
| 测试注册与执行 | 成立 | 三个卡点名用例在 `candidate-headless-debug` 注册并执行，退出 0 |
| 合同页实现状态同步 | 成立 | 两篇卡点名合同更新；`check_docs.py`、`check_docs_facts.py` 退出 0 |
| shared 配置、设备运行 | 无法核实 | 卡未要求；本轮未构建 `candidate-shared-debug`，未跑设备 |

整卡在卡指定验收入口内成立。shared 导出面与真实设备行为没有本轮证据，不作为本卡通过项。

## 2. 实施内容

T-04 的实际交付把候选实现放在 `engine/playback/CMakeLists.txt` 的候选 `target_sources` 与既有 TU 的 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE` 条件段内，并未创建卡面预期的独立 `playback_session_candidate_hooks.cpp`、`playback_session_prepared_gameplay.cpp`。本卡按实际归属落地：F508 清理加在 `playback_session.cpp` 既有的候选 unload 条件段，F509 lease 与 F501 判定留在 `playback_session_prepared.cpp` 的候选段；宏关闭的默认路径没有新增候选逻辑，也没有新建空 hook 文件。

### RR-F509（`engine/playback/src/playback_session_prepared.cpp`、`playback_session_internal.hpp`）

`PlaybackSession::State` 在候选闭包内持有 `PreparedOwnerLease`（会话指针 + 分配 token）。`PreparedPlayback::State` 只持有该 lease 的 `weak_ptr`。`materializePresentation` 在候选路径解引用原会话前先 lock lease，并核对会话身份与 `ownerToken`；失败返回既有 `host.rpa.context_stale`，不再触碰裸 `owner`。公共头、类布局、移动语义、错误码与提交入口均未改。

### RR-F508（`engine/playback/src/playback_session.cpp`）

`unload()` 的候选清理由段新增 `hostFence`、`acceptedHostControl`、`confirmedHostContext`、`executionMapping`、`tailTransition` 的重置，并同步清除瞬态 `hostCommitAuthorized`。原有运行时、调度器、时钟与表现清理由顺序不变。

### RR-F501（`engine/playback/src/playback_session_prepared.cpp`、`playback_session_prepare.cpp`）

`PreparedPlayback::valid()` 现按实际内容类别判定：Runtime 存在，或候选 `gameplayContent.valid()`；仍要求对象非空、`ownerThread` 当前。`contentInfo()`、`semanticIdentity()`、`mainMusicSource()` 随 `valid()` 使用实际准备的 `PlaybackContentInfo` / 身份 / 音频 lease。依赖表现的访问仍独立检查候选资源与 Runtime；`commit()` 的 owner/session、generation、lifecycle、操作中与 host fence 检查原样保留。

### RR-F203（`engine/runtime/src/runtime_session_overrides.cpp`、`engine/world/src/property_resolver.cpp`、`engine/world/internal/cuexis/world/resolver_access.hpp`）

`RuntimeSession::acquireOverride` 保持线程、重入、会话、覆盖种类、对象与 mask 检查；对每条 write 调用内部 `world::detail::ResolverAccess::validatePropertyValue`，复用 `PropertyResolver` 捕获基线的 `parsedPropertyValue` 类型/值域规则。整批通过才构造并发布 token；任一失败在 push 前返回既有 `world.property.*` / `runtime.camera.*` / `runtime.appearance.*` 错误，无部分覆盖。该入口仅在非安装内部头，公共接口、布局与覆盖求值语义不变。

## 3. 命令、退出码与关键输出

日志均在 `out/review/stage-07-rr/T-17/`。

### 3.1 候选构建与三个卡点名用例

```text
cmake --build --preset candidate-headless-debug --target cuexis_playback_tests cuexis_runtime_tests
[exit code: 0]   （build-candidate-playback.log）
```

```text
ctest --test-dir out/build/candidate-headless-debug --no-tests=error -N \
  -R '^(rr_session_prepared_owner|rr_session_unload_state|rr_gameplay_only_accept)$'
  rr_gameplay_only_accept
  rr_session_prepared_owner
  rr_session_unload_state
Total Tests: 3       [exit code: 0]   （registration-playback.log）
```

CTest 编号随 Catch 发现顺序漂移，验收以用例名为准。

```text
ctest --test-dir out/build/candidate-headless-debug --no-tests=error \
  -R '^(rr_session_prepared_owner|rr_session_unload_state|rr_gameplay_only_accept)$' \
  --output-on-failure
1/3 rr_session_unload_state  Passed
2/3 rr_session_prepared_owner  Passed
3/3 rr_gameplay_only_accept  Passed
100% tests passed, 0 tests failed out of 3   [exit code: 0]   （tests-playback.log）
```

拒绝码实测：F509 两条失效路径均 `host.rpa.context_stale`；F501 的 GameplayOnly 表现拒绝 `host.rpa.device_failed`，错误会话提交 `playback.prepared.wrong_session`，generation 过期提交 `playback.prepared.stale`；F203 类型错 `world.property.value_invalid`、值域错 `runtime.camera.fov_out_of_range`。

### 3.2 RR-F203 目标与既有 override 回归

```text
cmake --build --preset candidate-headless-debug --target cuexis_runtime_tests
[exit code: 0]   （build-candidate-runtime.log）

ctest --test-dir out/build/candidate-headless-debug --no-tests=error -N -R 'override|Override'
Total Tests: 18，含 Test #398: rr_override_registration_validation       [exit code: 0]
                                                                     （registration-runtime.log）

ctest --test-dir out/build/candidate-headless-debug --no-tests=error \
  -R 'override|Override' --output-on-failure
100% tests passed, 0 tests failed out of 18   [exit code: 0]   （tests-runtime-override.log）
```

`RemainingFrames override expires on the next frame`、`UntilChartTimeMs override expires at the deadline` 与 `rr_override_registration_validation` 均通过。

### 3.3 全量回归与闭包

```text
out/build/candidate-headless-debug/bin/cuexis_runtime_tests.exe --reporter compact
All tests passed (10915 assertions in 55 test cases)   [exit code: 0]  （runtime-suite-candidate.log）

out/build/candidate-headless-debug/bin/cuexis_playback_tests.exe --reporter compact
All tests passed (35174 assertions in 193 test cases)  [exit code: 0]  （playback-suite-candidate.log）

cmake --build --preset headless-debug --target cuexis_playback_tests cuexis_runtime_tests
[exit code: 0]   （build-default-closure.log）

out/build/headless-debug/bin/cuexis_playback_tests.exe --reporter compact
All tests passed (17452 assertions in 136 test cases)   [exit code: 0]  （playback-suite-default.log）

out/build/headless-debug/bin/cuexis_runtime_tests.exe --reporter compact
All tests passed (10915 assertions in 55 test cases)   [exit code: 0]  （runtime-suite-default.log）
```

```text
ctest --test-dir out/build/headless-debug --no-tests=error \
  -R '^cuexis_rr_playback_isolation$' --output-on-failure
100% tests passed, 0 tests failed out of 1   [exit code: 0]   （isolation-default.log）

ctest --test-dir out/build/candidate-headless-debug --no-tests=error \
  -R '^cuexis_rr_playback_isolation$' --output-on-failure
100% tests passed, 0 tests failed out of 1   [exit code: 0]   （isolation-candidate.log）
```

### 3.4 文档门禁

```text
python -B tools/check_docs.py
Documentation checks passed: 798 Markdown files and 20 candidate JSON/CXT files validated.
[exit code: 0]   （docs-check.log）

python -B tools/verification/check_docs_facts.py
version-drift findings: 0
documentation facts are current (SDK 0.9.1, build 26.11.23-1)
[exit code: 0]   （docs-facts.log）
```

## 4. 改动文件

- `engine/playback/src/playback_session_internal.hpp`、`playback_session.cpp`、`playback_session_prepare.cpp`、`playback_session_prepared.cpp`、`playback_candidate_internal.hpp`
- `engine/runtime/src/runtime_session_overrides.cpp`
- `engine/world/src/property_resolver.cpp`、`engine/world/internal/cuexis/world/resolver_access.hpp`
- `tests/playback/rr_t17_session_tests.cpp`（新增）、`tests/playback/CMakeLists.txt`（候选块注册）、`tests/runtime/animation_runtime_tests.cpp`
- `docs/api/playback-session.md`、`docs/architecture/RUNTIME_SESSION.md`

`tests/playback/CMakeLists.txt` 同一文件含并发 T-05 的 `frame_layout_tests.cpp`/include 行；本卡只加 `rr_t17_session_tests.cpp` 一行。完整 `git diff` 见 `diff.patch`，逐文件 SHA-256 见 `files.txt`。

## 5. 合同同步与链接

- F501 小节标题改为“已接受、已实现”，正文记录实现入口与证据；保留旧锚点 `<a id="preparedplayback-有效性修订f501已接受未实现">`，因此 `findings.md`、T-17 卡与 `plan.md` 的既有链接继续可跳转。
- F203 小节标题改为“已接受、已实现”，正文记录复用既有属性校验的入口与原子登记语义；同样保留旧锚点 `临时覆盖登记修订f203已接受未实现`。
- 两篇合同未改字段、值域、错误码或公共接口承诺。

## 6. 未做项与原因

- 未 `git add`/`commit`：按派单约束。
- 未改 `docs/CURRENT_STATUS.md`、`docs/HANDOFF.md`、`plan.md`、`findings.md`、`schedule.md` 或 T-17 卡：主线回写。
- 未构建 `candidate-shared-debug`、未跑设备：卡未要求，且本轮无 shared/设备验收入口；shared 行为不能由本证据推断。
- 未新增 CLI 选项或工具：本卡无可执行工具产出；板面描述中的“内容提交门禁/CXC/CXP 打包校验”与本卡文本不符，实施以 `docs/stage_plans/active/stage-07-rr/tasks/T-17.md` 为准。
- 未新建卡面预期的 `playback_session_candidate_hooks.cpp`/`playback_session_prepared_gameplay.cpp`：T-04 的实际闭包没有这些文件，强行新建会改变已验收的 R1 源表；本卡在既有候选条件段完成清理与守卫。
- 工作树含并发 T-05 的未提交改动（`tests/playback/frame_layout*`、`engine/playback/src/frame_layout*`、`engine/render_opengl/**`、根/测试 CMake 等）。本报告只对本卡列出的文件负责；主线合入时应以合并后的树重跑 3.1–3.3 命令。

## 7. 证据文件

`build-candidate-playback.log`、`registration-playback.log`、`tests-playback.log`、`build-candidate-runtime.log`、`registration-runtime.log`、`tests-runtime-override.log`、`runtime-suite-candidate.log`、`playback-suite-candidate.log`、`build-default-closure.log`、`playback-suite-default.log`、`runtime-suite-default.log`、`isolation-default.log`、`isolation-candidate.log`、`docs-check.log`、`docs-facts.log`、`diff.patch`、`files.txt`。
