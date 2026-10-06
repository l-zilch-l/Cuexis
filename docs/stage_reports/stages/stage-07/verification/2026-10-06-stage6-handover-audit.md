# Stage 6 四项交接现状复核与解决方案

状态：verification；源码与现有门禁复核完成，解决方案为实施建议，四项关闭门禁未退出

证据日期：2026-10-06

上级：[Stage 7 计划 §1.2](../../../../stage_plans/active/stage-07/plan.md#12-stage-6-交接前置) ·
[当前状态](../../../../CURRENT_STATUS.md) ·
[旧交接台账](../../../../proposals/gameplay-v2-acceptance/STAGE6_HANDOVER_LEDGER.md)

## 1. 范围与基线

本轮按 owner 要求更新过时文档、核对 Stage 6 四项遗漏并提出解决方案；不实施 S7A-7/8/9，
不变更构建、工作流、仓库保护或 SDK 版本，不接受生产预算，也不重开 Stage 6。
实际分支 stage-7，HEAD 与 origin/stage-7 均为 `4d8024e1c62d83a7a1a2859fc146107d8bcc5096`；
开工工作区 clean。日期版本 `26.10.06-1`，SDK API `0.7.0`。
本轮文档修改留在该基线之上，不将建议冒充已接受合同。

S7A-3/4 与 5/6 的受限功能验收已有带日期证据；7 是产品集成，8 是四项交接收口，9 是预算、
最终硬化与关闭。旧 AGENTS、历史交付报告和 2026-10-02 台账不能覆盖当前实现。

## 2. 四项核对

| 归属 | 已有实现 | 当前缺口 | 结论 |
| --- | --- | --- | --- |
| S7A-8.1 candidate 隔离 | 默认 OFF 开关；三种显式 Entry 工厂；OFF 分支稳定拒绝；入口表与 candidate 测试 | experimental package consumer 放行、binary/import flavor、安装元数据、Player 参数和真实 ON preset/CI 消费链 | 部分实现，需要补产品隔离与矩阵 |
| S7A-8.2 离线 assembler | author typed Reader/adapter；实际 prepare；Gameplay feature/capability/resource closure 派生；Core/Packed 相关测试 | 无完整 CLI；未打通 source→装配→编码→package→显式 Playback entry 的生产链与 closure report | 不能继续写“只有身份装配”，也不能写整项完成 |
| S7A-8.3 Reference Host | 独立安装 consumer、命令循环、七种 Verb、状态与拒绝夹具、导入表和 SDK minor 门禁 | 当前集成后的同 SHA 交接矩阵；experimental 安装消费入口随 8.1 补齐 | 不重写命令循环；命名冲突已由计划订正 |
| S7A-8.4 SDK 0.7.1 | checker 默认拒绝 SDK 变化；显式放行参数及正负例；SameMinorVersion | workflow 无条件化放行调用；无 CODEOWNERS；master 未要求 PR review；版本仍 0.7.0 | 先建立可信放行通路，再审查版本兼容性 |

### 2.1 candidate 的源码证据

- [根构建](../../../../../CMakeLists.txt) 的 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 默认 OFF；
  当前 Debug cache 也为 OFF。CMakePresets 与 workflows 无该开关的启用声明。
- [Playback target](../../../../../engine/playback/CMakeLists.txt) 只添加 PRIVATE 能力宏，仍使用
  `cuexis_configure_public_library(cuexis_playback cuexis_playback)`。
- [Package config](../../../../../cmake/CuexisConfig.cmake.in) 无 `Cuexis_ALLOW_EXPERIMENTAL`
  消费许可判断；Player 源码无 `--candidate-entry`；未发现 candidate binary flavor 命名。
- 三个安装公共工厂的声明已经存在，OFF/ON 声明保持一致是正确边界，不应删声明实现隔离。
  安装树的禁止 token 扫描是已有 default-OFF 门禁，不是 experimental flavor 实现。

### 2.2 assembler 的取代性事实与声明边界

- [author target](../../../../../tools/gameplay_assembler/CMakeLists.txt) 是静态库
  `cuexis_gameplay_author`，目录只有库源码、头和 Schema 生成器，没有 CLI main。
- [author adapter](../../../../../tools/gameplay_assembler/src/gameplay_author.cpp) 构造 typed
  `AssemblyRequest` 并调用 `prepareGameplay`，不再是旧台账所述的“只有身份装配函数”。
- [Gameplay assembler](../../../../../engine/judgement/src/gameplay_assembler.cpp) 实际调用
  `deriveCapabilityClosure`、`deriveFeatureClosure`、`deriveResourceClosure`；`validateClosures`
  将 source declared 集与 content-derived 集分别检查，缺声明稳定失败。
- source declared 集与 Ruleset/Presentation 的 `closureContributions` 有不同 owner。
  派生有效 closure 不等于替作者补声明；不得把“消除测试注入后门”解释为允许静默补齐缺声明，
  也不得把调用方写入的所有声明一概判成后门。当前 Spec 的三方身份/声明规则继续有效。
- ADR 0042 的历史 lanes4 子项须先与当前 Gameplay v2 和 Capsule revision 3 对照再消费，
  不恢复旧候选语义或另建第二条 feature 推导路径。

### 2.3 Reference Host 与可信门禁

[host_commands.hpp](../../../../../examples/reference_host/src/host_commands.hpp) 的 Verb 为
`Open, Play, Pause, Tick, Seek, Reload, Quit`。计划 §1.2 与 S7A-8.3 已采用 ADR 的
`open/play/pause/seek/reload/quit`，另有 `tick` 驱动步骤；旧台账 D-2 的“命名待决”已被取代。
本轮独立 parser gate 通过，含手工样本、检测器存活性与 41 条声明；它不能替代实际安装运行。

2026-10-06 只读访问 `GET /repos/l-zilch-l/Cuexis/branches/master/protection`：
`required_status_checks.strict=true`，要求 `Version advancement (pre-merge)`；
`required_pull_request_reviews=null`。这是当日 classic protection 响应，不推断未查询的其他 ruleset。
仓库文件中未发现 CODEOWNERS，因此不能声称已具备代码所有者必审保护。
[version-gate workflow](../../../../../.github/workflows/version-gate.yml) 仍从可信 base 提取 checker
与 workflow 后执行，未传 `--allow-sdk-api-change`；不能靠候选提交修改自己来放行自身。

## 3. 建议解决方案与验收条件

下列为拟实施步骤；未接受前不改变 ADR/Spec 的决定。普通内部拆分沿既有授权处理；
若发现公共 ABI 或旧边界冲突，先给反例并修对应合同。

### 3.1 S7A-8.1：保留显式入口，补齐 experimental 安装隔离

1. 沿用当前构建开关；ON 构建生成明确 experimental 元数据及区别于 production 的
   Playback binary/import 名称；两种安装使用分离前缀，CMake consumer target 名保持既有 API。
   对真实链接依赖逐项检查同名库误加载风险，不只改压缩包名或随意扩张公共 target 集。
2. `CuexisConfig.cmake` 在导入 targets/查找依赖前拒绝未许可的 experimental consumer，
   只有显式 `Cuexis_ALLOW_EXPERIMENTAL=ON` 可继续。production 仍不可加载 candidate，
   设置 consumer 许可也不能将 OFF 库变成 ON 库；PUBLIC 类布局/声明不随开关变化。
3. Player 增加显式 `--candidate-entry <path>`，仅与明确的 project/CXC locator 组合；
   dispatch 到既有 Entry 工厂，按 metadata 选择，禁止裸 Packed、后缀猜测、第一项回退与失败重试。
   启动诊断标明 flavor；旧入口、旧偏好和 v4 默认路径不变。
4. 增加独立 candidate preset，至少一条 CI 真正启用、注册并运行 ON 入口用例；
   分别运行 OFF/ON、static/shared、安装 consumer 和 Player/Headless 的正负矩阵。

退出证据：OFF 拒绝且不读 candidate 输入；ON 只有显式入口成功；consumer 未许可、错误 entry、
缺 metadata、裸 Packed、错 flavor/混用前缀均稳定失败；clean staging 的真实加载位置可核对。
安装 token 扫描需区分 OFF/ON 预期，不能直接全局禁用扫描。

### 3.2 S7A-8.2：复用现有 assembler，加离线 CLI 和完整 artifact 链

1. 使用 tools 中的薄 CLI 调用现有 typed Reader/author adapter/Gameplay assembler 与 chart
   编码能力；Playback 不依赖 tools，不在 prepare 或帧路径执行 CXT v2 展开。
2. 保留 authored declarations 与 derived closure 的区别；Ruleset/Presentation contributions
   从绑定的真实消费者取得。未知、缺失、重复或伪造声明按现行合同拒绝，不替作者修复。
3. 生成 owning artifact、两种 entry 所需 metadata 及 closure report，再 encode/package。
   report 分开列 declared/derived、资源身份、requirements、capability、revision、诊断和预算状态；
   不接受 S7A-9 尚未测量的数值。复用既有原子发布设施，全部验证后发布，失败保留旧输出。
4. 用真实 CLI 产物，经 Core/Graph 与 Packed 路径恢复同一 prepared graph/identity，
   交给 S7A-7 的同一个 Judgement/Playback 消费点；禁止另造 Player 专用推导器。

退出证据：file/memory、数组置换、affine/CXT 来源等价；缺声明、非法引用、冲突资源和预算负例；
完整 CLI→encode→package→entry→prepare 的 golden；失败不覆盖旧包或发布部分 report。
库单测作为回归保留，但不代替 CLI 与 clean-staged consumer。

### 3.3 S7A-8.3：对已有宿主做交接回归

沿用现有七动词实现及命令夹具，补当前安装包下的 v4 production 与显式 experimental entry 消费。
复跑状态、pause/tick、seek/reload/discontinuity、错误保持旧 active、导入表与版本拒绝门禁；
将同 SHA 的本地和 hosted run 逐行关联。无须新增 load/stop 别名或重写命令循环。
若需增加 experimental source 选择参数，只扩展入口适配，仍通过公开 Playback API 运行。

退出证据：安装树外独立构建/运行、静态与 shared matching-toolchain、headless 路径和旧状态保持；
parser gate 与实际命令执行分别记录，不复制 R9 旧 run 充当本轮证据。

### 3.4 S7A-8.4：分两步建立可信放行，再发布 additive SDK 版本

1. 先完成可信门禁变更：明确实际 owner，CODEOWNERS 覆盖 checker/workflow/版本合同与公共头，
   配置 required owner review、旧 approval 失效和 required check。只提交 CODEOWNERS 文件
   不等于启用保护；仓库外配置须由具备权限者实施并另列证据。
2. 推荐采用受保护的显式授权记录，绑定完整 candidate/base SHA、SDK from/to、审批主体和有效 UTC
   日期；由可信 base 的 checker/workflow 验证后才传放行参数。禁止普通 PR label、候选自声明或
   永久无条件 `--allow-sdk-api-change`。缺授权、SHA/base/目标版本不符、过期或批准后改代码均拒绝。
   merge_group 要绑定实际队列 SHA；post-merge audit 必须验证批准候选与实际 merge 的关联，
   不能直接把 PR head 的许可用于任意 merge SHA。具体授权存储/审查机制先落版本合同。
3. 先将门禁与保护变更变成可信 master 基线，再让 SDK 变更分支基于该基线；
   第一步与第二步不能依赖同一候选提交自举。合并/外部保护配置不由本轮文档工作代办。
4. 审核 S7A-7 拟安装公共合同的新增名字、签名、layout、枚举与虚表。满足 ADR additive
   条件才落实 0.7.1；若出现 breaking 反例，重开版本决定，不用实验 flavor 掩盖。
   `update_version.py` 只同步日期构建身份与 manifest，不能将运行它误写为自动更新 SDK API。

退出证据：默认 SDK 变化失败，一次合法授权成功，授权重用/篡改/过期/基线变化/未审门禁变更失败；
0.7.0 consumer 源码重建兼容、fresh/clean build、安装元数据和同 SHA hosted 一致。
shared 不承诺二进制替换。owner 接受不升版本只能通过明确 ADR/版本规范例外，不能默认为豁免。

## 4. 顺序与其他旧残余

建议先闭合 8.1 安装隔离和 8.2 离线 artifact 链，并与 7 的产品消费者接线共同验证；
8.3 重用宿主完成安装回归。8.4 的保护/可信门禁可提前准备，SDK 实际版本变更等待公共合同
兼容性核对。最终产品预算与完整最终矩阵仍归 9，不能因四项局部通过提前关闭阶段。

Stage 6 复核报告仍登记 SPEC-01/11/12/14/25/30 等记录、测试、介质和平台残余；
本轮没有发现它们在 7A/8 计划中的逐编号核销映射。这些不是“四项”全部，也不能在本次文档
更新中自动关掉。建议在实施前分别登记明确消费者、退出证据与处置批次，已有 owner 接受的
保留项与新补证据分开处理，具体来源见
[旧复核交付报告 §5](../../../reviews/stage-06-review-2026-09/2026-09-28-delivery-report.md)。
本次没有重跑这些残余对应的全部实现测试，故不声称它们仍是新的运行时缺陷。

## 5. 本轮验证与证据限制

- `python -B tools/check_version_gate_tests.py`：20 项，2 项 skip，其余通过；
  证明 checker 现有功能，不证明可信 SDK 放行接线。
- `cmake -P cmake/VerifyReferenceHostCommandParser.cmake`：通过，41 条声明；非完整安装运行。
- 当前 SHA Version Gate run `37463533191` success；其余 Linux/MSVC/MinGW 在查询时仍运行。
  本轮不等待 CI，不以 Version Gate 成功替代三平台矩阵或 SDK API 许可。
- `python -B tools/check_docs.py`：378 Markdown、20 JSON/CXT 通过；状态合同 4 项、目标合同 2 项
  全通过；`git diff --check` 通过。源码、依赖与生成头未改，不重跑全量 C++。
- 未构建 experimental flavor、未运行新 CLI、未升 SDK，也未修改远端保护；以上方案不构成完成证据。
