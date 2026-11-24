# T-04 构建闭包拆分与隔离门禁

状态：evidence

更新日期：2026-11-24

<!-- cuexis-meta
id: REPORT-stage-07-rr.t-04-build-isolation
status: evidence
authority: none
date: 2026-11-24
-->

## 结论

T-04 的四项验收都有可复核的运行证据，拆分没有改变默认闭包的行为。

- R1 源表隔离：**成立**。默认 `cuexis_playback` 闭包不再编译候选 hook 与 `cxc_candidate.cpp`，候选配置编译且只编译一次；`cuexis_cxc_candidate` 是独立内部静态库。四个预设的 `cuexis_rr_playback_isolation` 门禁与 `rr_r1_playback_sources`、`rr_r1_cxc_package_integrity` 全部通过。
- F302 源表与 include 根拆分：**成立**。`cuexis_chart` 只含基础 Reader/CXT1/v4 Resolver 源表与 include 根；`cuexis_chart_codec` 只含 packed wire 与 Reader；`cuexis_chart_tools` 只含 Writer、CXT2、lowering、Behavior、candidate assembler。四个预设与两个独立配置（`rr-f302-minimal`、`rr-f302-tools`）的门禁按实际配置通过，`rr_f302_chart_boundaries` 在四个预设通过。
- 链接与结构阴性：**成立（A2 在修复门禁后）**。A1 源表放回、A3 include 根加入都在门禁下点名失败；B 的两个链接 probe 分别以未解析 `lowerCandidateRuntime`、`packed::encode` 失败，链 tools 的对照通过。A2 的反对照最初因门禁 `cuexis_target_dependency_names()` 的 id 读取缺陷假通过，修复（`92539ad4`）后候选配置门禁点名 `cuexis_chart` 链 `cuexis_chart_codec`。
- 安装消费者：**成立**。默认 static、实验 static、实验 shared、minimal 等价与附加的 tools=ON 五个前缀，安装清单、导出 target、泄漏检查与两种消费者接线的配置-构建-运行全部通过。
- F216/F217/F434：分别为成立（Windows 可核实范围）、成立、成立，见第 4 节。
- 四个带阶段名的门禁脚本改名与冻结清单路径迁移：**成立**。脚本内容逐字节不变，447 个路径键只改路径，JSON 全部可解析，全仓旧名 0 命中。

## 范围与方法

本轮在四个预设与两个独立配置上构建并运行 CTest，运行工具与安装消费者，做隔离副本阴性；同时核对 CMake File API codemodel 的源表、include 根与链接依赖。未做的部分：设备运行、Release/非 Windows 构建、T-09 跨机器重放、T-08 的候选 hook 文件拆分。

## 执行记录

```text
cmake --build --preset debug|headless-debug|candidate-headless-debug|candidate-shared-debug --target <闭包目标>
ctest --test-dir out/build/<preset> --no-tests=error -R '^(cuexis_rr_playback_isolation|rr_r1_playback_sources|rr_r1_cxc_package_integrity|rr_f302_chart_boundaries|rr2_contract_packed|rr2_contract_beat|rr2_contract_entry)$'
cmake --build --preset debug --target cuexis_chart_candidate cuexis_chart_validator cuexis_chart_migrator cuexis_cxc_pack cuexis_cxc_validate cuexis_cxc_unpack
ctest --test-dir out/build/debug --no-tests=error -R '^(cuexis_candidate_cli|cuexis_chart_tool_tests|cuexis_cxc_tools)$'
cmake -DCUEXIS_BINARY_DIR=out/build/rr-f302-minimal -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF -DCUEXIS_BUILD_DEVELOPER_TOOLS=OFF -DCUEXIS_BUILD_TESTS=OFF -P cmake/VerifyPlaybackCandidateIsolation.cmake
cmake -DCUEXIS_BINARY_DIR=out/build/rr-f302-tools -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF -DCUEXIS_BUILD_DEVELOPER_TOOLS=ON -DCUEXIS_BUILD_TESTS=OFF -P cmake/VerifyPlaybackCandidateIsolation.cmake
```

构建退出码全部 0；四个预设各 7 项匹配测试全通过；工具测试 3 项全通过；两个独立配置的门禁分别按 tools OFF/ON 通过。

## 结果与证据

### 1. 四个预设的构建与门禁

| 预设 | 构建 | 门禁 | 7 项测试 |
| --- | --- | --- | --- |
| debug | 0 | 0 | 7/7 |
| headless-debug | 0 | 0 | 7/7 |
| candidate-headless-debug | 0 | 0 | 7/7 |
| candidate-shared-debug | 0 | 0 | 7/7 |

门禁在候选配置确证候选源与目标进入闭包，在默认配置确证它们不出现；shared 配置以 `-DCUEXIS_LIBRARY_TYPE=SHARED` 运行。测试记录见[四预设测试日志](../evidence/t-04/tests-four-presets.log)，独立配置门禁见[F302 门禁日志](../evidence/t-04/gate-f302.log)。

### 2. F302 独立配置与工具

- `rr-f302-minimal`：`CUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF`、`BUILD_TESTING=OFF`、`CUEXIS_BUILD_TESTS=OFF`、`CUEXIS_BUILD_DEVELOPER_TOOLS=OFF`；构建 `cuexis_playback`、`cuexis_chart`、`cuexis_judgement` 通过，codemodel 中没有 codec/tools/cxc_candidate/gameplay_packed 目标。
- `rr-f302-tools`：同上有 tools ON；六个工具可执行目标全部构建；codec/tools 目标与 include 根按消费合同出现，默认 Playback 闭包与 minimal 一致。
- debug 的六个工具目标全部构建，`cuexis_candidate_cli`、`cuexis_chart_tool_tests`、`cuexis_cxc_tools` 先 `-N` 确认注册再运行，全通过（[工具测试日志](../evidence/t-04/tools-debug-tests.log)）。

### 3. 安装消费者

五个前缀（默认 static、实验 static、实验 shared、minimal、tools=ON）的安装退出码 0；默认与 minimal 前缀含 `Cuexis::InternalChart`/`Cuexis::InternalJudgement`，无 codec/tools/cxc_candidate 的 export、头与库；实验 static 含 `Cuexis::InternalChartCodec`/`Cuexis::InternalChartTools`；shared 只导出公共库。两种消费者接线（Playback-only 与 Playback+Content+Audio）在五档共 10 组配置-构建-运行全部退出 0，运行输出包含 `Cuexis Playback-only CFU-F2 consumer passed identity=…` 与 `Cuexis external consumer passed digest=…`。安装文件与 `.cmake` 中构建树绝对路径扫描 0 命中。详见[安装消费者证据](../evidence/t-04/t-04-install.md)。

### 4. F216/F217/F434

- RR-F216：**成立（Windows 可核实范围）**。三个全新目录 `--fresh` 配置：默认不传库类型与显式 STATIC 均退出 0，生成 `CUEXIS_LIBRARY_TYPE=STATIC`；显式 SHARED 退出 1，首条错误 `PD allocation observer requires Windows STATIC`。非 Windows 分支需要非 Windows 主机，未在本机执行。
- RR-F217：**成立**。`out/build/debug` 是 GL ON、shader tools OFF；`cuexis_shader_tests` 构建成功，`-N` 注册 6 个 cache 用例，实际运行 6/6 通过；codemodel 显示该目标只编译 `tests/shader/shader_cache_tests.cpp`，链接边只有 `cuexis_shader_cache` 与 `Catch2::Catch2WithMain`，没有 shader compiler 源、`cuexis_shader` 或 shaderc。本机没有 tools ON + tests ON + GL ON 的现成配置，compiler/cache 对照未做。
- RR-F434：**成立**。已有 Playback 摘要用例注册 2 个、运行 2/2 通过；版本 0 与 4 均以 `playback.frame_digest.version_unsupported` 拒绝，公共默认 v3 的摘要值为 `9292624206614054870`，历史 v1/v2 为 `7174372098521454160`、`7850652359432829177`，均未变化。

完整命令与日志见[三项验收证据](../evidence/t-04/t-04-rr-cases.md)。

### 5. 阴性

| 项 | 变异 | 结果 | 判定 |
| --- | --- | --- | --- |
| A1 | 基础 Chart 源表加回 `cxt_v2_loader.cpp` | 配置 0；门禁 1，点名该 TU 为 `cuexis_chart` 的意外源 | 成立 |
| A2 | 给 `cuexis_chart` 加 codec 链接 | 修复前：候选配置门禁 0（假通过）；修复后候选配置门禁 1，点名 `cuexis_chart` 链 `cuexis_chart_codec`（548 行）；默认配置首错仍是 Playback 闭包命中（453 行，早于 548 行） | 成立（修复后） |
| A3 | 基础 PUBLIC include 加 `engine/chart/tools_include` | 配置 0；门禁 1，点名该 include 根 | 成立 |
| B1 | 只链 `cuexis_chart`，调用 `lowerCandidateRuntime` | LNK2019/LNK1120，未解析符号 | 成立 |
| B2 | 只链 `cuexis_chart_codec`，调用 `packed::encode` | LNK2019/LNK1120，未解析符号 | 成立 |
| B3 | 链 `cuexis_chart_tools` 的对照 | 两次均退出 0 | 成立 |

A2 暴露的缺陷是门禁帮助函数把 File API `dependencies` 数组元素当字符串读取，未取 `id` 成员；修复提交 `92539ad4`。证据见[阴性证据](../evidence/t-04/t-04-negatives.md)与[修复后复跑日志](../evidence/t-04/gate-fixed-recheck.log)。

### 6. 门禁脚本改名与清单迁移

四个脚本改名后内容哈希不变：`VerifyClipCompilePerformance`（`61c95459…`）、`VerifyRulesetPreparePerformance`（`1001d729…`）、`VerifyContentDeterminism`（`7614cd84…`）、`VerifyContentIOPerformance`（`e79ad485…`）。13 份冻结清单中的 447 个路径键只替换路径，JSON 全部可解析，旧名全仓 0 命中。

## 限制与未做

- 全部运行是 Windows Debug；Release、非 Windows 与跨机器配置未覆盖。
- 安装消费者的 `find_package` 接线使用与仓库脚本相同的工程与参数，但由本卡脚本直接指向已安装前缀；`VerifyGameplayInstalledConsumer` 只执行前缀侧断言，因为两个候选构建目录没有 `cuexis_playback_tests` 可执行文件与 `s7a78-public-fixture/main.packed`。
- F216 的非 Windows 拒绝分支、F217 的 tools ON + tests ON + GL ON 对照未在本机执行。
- 未做设备运行与实体输入；T-09 的跨机器重放不在本卡。
- 候选 hook 文件拆分按 owner 裁定移交 T-08，T-04 保持两个 hook 文件现状。
- 门禁脚本依赖 CMake File API codemodel；无该回复时按设计失败，本卡未提供降级路径。

## 对登记册与合同的影响

F302、F216、F217、F434 的处置写入 findings 第 34 节；T-04 卡进入完成状态；`docs/guides/BUILDING.md` 的目标块已按生成的 `cuexis-targets.txt` 同步（77 个目标）。合同文档不需要改：拆分没有改变公共头、安装合同或 wire 格式，内部库只新增导出别名。
