# PD-1 Judgement Publication Bridge

日期：2026-10-18。状态：PD-1 内部实施及本地适用门禁完成。

## 1. 范围与源码绑定

本批只实施 [PD 计划](../../../../../stage_plans/active/stage-07/04-pd/plan.md)
PD-1 和[设计细则 §2](../../../../../stage_plans/active/stage-07/04-pd/design.md#2-judgement-发布视图pd-1)。
owner 在本次对话明确要求先提交当前状态留档，再开始 PD-1；Desktop 接手文档用于理解背景，
不据其中的后续路线启动 PD-2 至 PD-5。

实施前归档提交为 `324d874f56eb466f1c8533550e80794bf106a02b`（`stage-7`，
`wip(pd): archive PD-0 closure and pre-PD-1 workspace`）。本批作为后续独立提交，可整体回退到该点。
没有 push、PR、合并或发行。

本地证据根目录：`out/pd1-verification-20261018/`。18 项实现、测试和门禁输入的 SHA-256
映射及聚合指纹记录于 `implementation-build-binding.json`，输入副本在 `implementation-inputs.zip`。
指纹为 `f853e637fe1cc15bdab06befa80d620e285601f3decd7b3c78e3cef53b491ff2`；
不含本报告及状态、索引文字。证据目录被 Git 忽略，属于本机验证材料，本报告保存结果与重现条件。
PD-0 的正式注册、失败样本、冻结输入和二进制目录均未覆盖或重跑。

## 2. 实现与边界

- Judgement 维护非安装的 `engine/judgement/internal/cuexis/judgement/publication.hpp`。
  `PublicationAccess::readSince` 显式检查期望 runScope 和 exclusive cursor；只读窗口提供
  run identity、epoch、sealed/publishable cursor、frontier、kernel/Fold work tick、control cursor、
  fault stage/diagnostic 与 `mayProject`。`fact(ordinal)` 返回带 tag 的记录，完整 requirement identity
  借用同一个 KernelProjection 中的数据；接口不返回 Fact 容器。
- 读取只适用于当前 owner 串行操作。每次 accessor 检查 owner、epoch、mutation/reentry 和会话寿命；
  submit、canonical submit、advance、control、configure/prepare、reset、restore、Seek 等可写入口统一失效
  旧读取。视图只弱持有微小寿命 token，不保活历史 KernelProjection；短期引用不得越过该操作。
- 有 Fold 时仅发布到 `Fold.factCursor`，合法 no-Fold 路径采用 sealed prefix。Kernel/Fold/Control fault
  可读取实际前缀但 `mayProject=false`，Playback 在任何 Runtime 更新前拒绝激活。空区间仍携带控制元数据，
  不阻止同 cursor 下的 T、到期效果更新。
- Playback 的 advance、presentation sampling、restore 和 Seek 使用该桥，投影不再先构造复制全量历史的
  GameplayResult。首次发布及显式恢复建立新 scope；普通 scope 不匹配不隐式归零。
  只跳过正式允许的 stray/consumeEmpty，未知必需 tag/phase/outcome 拒绝，失败不安装候选 cursor。
  现有绑定扫描、聚合、Runtime 更新、FrameEnvelope 同步路径仍保留，索引和事务重构留给后续批次。
- 公开 queryGameplay、ReplayEvaluation 和协调操作的既有 owning 结果语义保留。
  `classifyPublicDiagnostic` 委托 Judgement 原有限码表，Playback 不再 include
  `judgement/src/diagnostic_policy.hpp`。既有诊断穷举测试直接引入该表作为期望源；生产路径不复制码表。
- CMake 只增加 private include。Judgement 单元测试仍只链接 Judgement、Core 和 Catch2；不增加
  Runtime/World/Renderer 反向依赖。安装头只增加不透明 friend 声明并撤下原私有投影方法，未增加公共操作、
  数据字段或版本；SDK 仍为 `0.8.0` preview，日期构建仍为 `26.10.15-1`。

维护成本主要是一个统一的 mutation guard 与一次短期读取协议，没有新增 owner、线程、历史结果缓存或
第二个 Fact 发布源。未来新增 Judgement 可写入口须接入同一 guard；跨 owner 读取仍需另立 owning 合同。

## 3. 验证结果

四个独立目录均 fresh configure，构建全部默认目标，PD 编译插桩 OFF。使用 MSVC 18.9.2、Ninja、
`VCPKG_ROOT=D:/vcpkg`。构建进程内同时设置 ConsoleCP/ConsoleOutputCP 为 65001、`VSLANG=2052`，
退出恢复 codepage，沿用 PD-0 已验证的依赖记录修正。没有使用旧正式测量构建目录。

| 配置/检查 | 结果及限制 |
| --- | --- |
| `pd1-debug` / headless-debug / candidate ON | 完整构建通过，使用 base preset 的 warnings-as-errors OFF |
| `pd1-shared-debug` / headless-shared-debug / candidate ON | 完整构建通过，warnings-as-errors OFF |
| `pd1-release` / headless-release / candidate ON | 完整构建通过，保留 Release 原定 warnings-as-errors ON |
| `pd1-default-off` / headless-debug / candidate OFF | 完整构建通过，warnings-as-errors OFF |
| Judgement 新发布桥单元测试 | 7 项、323 条断言通过；同源引用、无 Fold、非零 cursor/receipt、owner/scope/reentry、mutation/销毁失效、三类 fault、空区间 control cursor |
| PD Playback 故障集成测试 | 1 项、28 条断言通过；真实 Fold 溢出形成 sealed > publishable，sampling 不激活来源，完整 query/Replay 等价 |
| static Debug 普通回归 | 选中 1,048 项：初次 1,045 通过、2 项旧诊断结构门禁失败、1 项跳过；修正门禁后两项定向复验通过，原失败日志保留，未将整轮改写为一次全绿 |
| static Release 普通回归 | 选中 1,048 项：1,047 通过、0 失败、1 项符号链接条件测试跳过 |
| candidate OFF 普通回归 | 选中 941 项：940 通过、0 失败、1 项符号链接条件测试跳过 |
| shared 定向回归 | 14 项通过，含新测试、诊断精确字段、独立 source-scan/Fold golden、FactBinding、恢复及 GameplayOnly |
| Release 定向回归 | 相同 14 项通过 |
| static 包门禁 | find_package、find_package_playback、gameplay_installed_consumer，3/3 通过 |
| shared 包门禁 | 上述三项及 shared_export_surface、shared_consumer_imports、shared_playback_consumer_imports，6/6 通过 |
| 文档、格式、版本和 diff | `check_docs.py`、改动 C++ 的 clang-format 检查、`update_version.py --check`、`git diff --check` 通过 |

普通回归以 `ctest --test-dir <build> --output-on-failure --no-tests=error -j 2` 运行，排除
`cuexis_external_consumer|cuexis_gameplay_installed_consumer|cuexis_reference_host_staging|cuexis_.*performance|cuexis_.*consumer_imports`；
上述包门禁另跑。此为有明确排除项的 headless 回归，不是全部 CTest、Player/GPU 或跨平台矩阵。
性能名义的 CTest 未执行，既有 PD-0 功能表征测试仍随普通回归执行。

实际 Ninja 依赖也已核验：Debug session/publication/Playback session/发布桥单测/集成测试分别记录
58、55、146、98、112 个依赖，全部存在且不晚于对象文件；前三个生产对象和发布桥单测含新 internal 头。
集成测试只消费公开 facade，无需直接 include 桥。四套目录的选定对象、依赖状态、cache 和测试程序散列
见 `implementation-build-binding.json`，原始依赖与 Ninja 日志在各 `pd1-*-deps.txt`、`pd1-*-ninja-log.txt`。

## 4. 失败处置及审查

初次 Debug static/shared 额外启用 `/WX` 时，既有 `tests/rpa/rpa_b5_audio_tests.cpp:22` 的
`FAIL` 后 return 触发 MSVC C4702。保留 `pd1-*-werror-failure.log`，没有修改无关测试来消除警告；
Debug 按仓库 base preset 原值重新构建，Release 在 `/WX` 下通过。

生产桥移除旧间接 include 后，诊断穷举测试无法看到私有码表；修复为测试直接 include。
新测试还分别因恢复后无历史 archive、binding 名未匹配判定引用而失败，已按既有合同修正 fixture。
相应失败日志独立保留于证据目录，没有放宽生产语义。

普通 Debug 回归的两项诊断门禁原先硬编码 Playback 直接调用私有函数。现改为同时验证桥消费者、
桥直接委托原有限策略、禁止旧 src include，保留原有码表成员、历史散列等校验。
增加两项真实断链 mutation 自测，确认缺失消费者或策略委托会经 CTest 返回非零；
最终 `diagnostic-gates.log` 为 2/2 通过。运行时诊断的 code/message、ordered context、sourceCode
和 cause 仍由原有精确比较测试核验。

依 `code-quality` 技能做了只读独立审查，未发现阻断缺陷；末轮门禁修订也经审查。
未知必需枚举的运行时拒绝目前有显式分支和代码检查，但本批未另造损坏 KernelProjection 注入接口。
这不是已经执行的故障注入证据。

## 5. 当前结论与后续

PD-1 内部实施及上述本地适用门禁完成。PD-2、PD-3-a/b/c、PD-4、PD-5 未启动；下一批是 PD-2 的
prepare 绑定索引与 Projection Adapter，须继续使用现有 golden，并保留 Runtime 失败后不安装候选的边界。

本批未测性能增益，不接受生产预算、尾延迟或长期稳态；帧账本真实回收仍归 PD-4。
没有本批 SHA 的 Linux/MinGW hosted 矩阵、真实设备、听音同步、Player/GPU 验收或 owner 最终接受证据。
Stage 7-PD、Stage 7A、RPA 和整体 Stage 7 均不因此关闭。
