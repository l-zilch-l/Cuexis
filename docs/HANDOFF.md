# 当前交接

状态：active

更新日期：2026-11-24

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

## 当前阶段与工作项

当前为 Stage 7-RR，状态以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准。[T-18 文档治理](stage_plans/active/stage-07-rr/tasks/T-18.md)、[T-01 基线、派单包与冷启动](stage_plans/active/stage-07-rr/tasks/T-01.md)、[T-03 合同与诊断](stage_plans/active/stage-07-rr/tasks/T-03.md)与 [T-04 构建隔离](stage_plans/active/stage-07-rr/tasks/T-04.md)已完成，波次 5 的 T-02、T-05、T-17 也已完成。T-01 把实施基线固定在 `a22e255f`；T-03 订正 8 篇合同、追加四个 `rr2_contract_*` 用例与两条 RPA 对照；T-04 拆开候选与默认闭包、Chart 基础/codec/tools 源表，交付隔离门禁、五档安装消费者与五组阴性对照；T-02 对齐四篇状态事实，T-05 交付像素几何与 GL 边界，T-17 修复会话层 F501/F508/F509/F203。

## 当前任务

T-06 第一轮只完成帧对象、GL 实验桥、安装面与 F135 场景 1/2/3/5，交付记为部分成立，见[部分交付](stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-06-frame-bridge-partial.md)。T-10 已完成九轮：值层、计分、作者 CLI、运行 records/Prepared 存储、`cuexis_judgement_compile` 骨架与 `runtime::AdvanceKernel`（编号/Stop、GP-02 四向量、单元耗尽 Fold、GP-04 多单元、最小 replay）；剩余 GP-03 故障矩阵、Playback owning DTO 与 legacy 输入切换。T-12 部分完成：timing 库、编译闭包、chart 时间适配与 beat-us 消费者成立，canonical 行、GP 轨迹与安装隔离未做。F508 已按登记册 §42 的拆分落地（§43）：内容卸载保留控制身份，Close 终态清理，`rr_session_unload_lifecycle` 六 SECTION 通过；F135 已按形状 2 实现并独立复核（§44），`Error` 112 B、零分配与持续 OOM 成立；SDK API 已升到 0.10.0。治理树引导 PR 48 已合并（`43eb0c94`），master 已并入分支并把 build 升到 26.11.24-2；推送 PR 47 与代发 owner 审批评论是下一步。波次 5 的交付报告在 [T-02](stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-02-state-facts.md)、[T-05](stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-05-frame-layout.md)与 [T-17](stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-17-session-correctness.md)。

T-01 证据在 [基线与派单报告](stage_reports/stages/stage-07-rr/readiness/2026-11-23-t-01-dispatch-baseline.md)；T-03 证据在 [合同与诊断交付](stage_reports/stages/stage-07-rr/implementation/2026-11-23-t-03-contracts-and-diagnostics.md)；T-04 证据在 [构建闭包拆分与隔离门禁](stage_reports/stages/stage-07-rr/implementation/2026-11-24-t-04-build-isolation.md)。派单输入的规范哈希取基线 blob，见 [hashes.txt](stage_reports/stages/stage-07-rr/evidence/t-01-baseline/hashes.txt)，独立 checkout 复算不需要 `out/`。

## 下一步

1. 按[唯一调度](stage_plans/active/stage-07-rr/schedule.md)继续波次 6/7 的 T-06、T-08；T-10 的 GP-03、Playback owning DTO 与 legacy 输入切换，T-12 的 canonical 行与安装隔离随后推进。F135 已完成；PR 48 已合并，master 已合入、build 已升到 26.11.24-2。接下来推送 PR 47、代发 owner 的 SDK 审批评论并重跑门禁到全绿。
2. 每卡按其任务级验收取证并回写发现处置；文档变更收尾前运行文档总入口、事实、命名与生成物检查。
3. 实际发布、SDK 晋升与旧 API 退役仍各自单独取得；设备与实体听音项留给 owner。

## 阻塞与待决

PR 48 治理树引导已合并（`43eb0c94`），PR 47 的 PR 事件现在有含治理树的可信历史基线。F135 会让 SDK 公共布局变化，按 0.10.0 处理并需要 owner 的版本审批评论（已授权代理以 owner 名义代发，审批与 head sha、UTC 日期绑定）。GL 边界门禁已由 task-25 修复，`cuexis_rr_playback_isolation` 的 Chart 源表漂移已修（`f00439c3`）。F132 的旧身份字节需要 T-10 在改动前 dump（改动前基线已采集，见[身份字节基线](stage_reports/stages/stage-07-rr/evidence/t-03/identity-baseline/identity-baseline.md)）；F135 的五类故障注入与安装消费者分别归 T-06、T-08。D-20 的正式发行判定语料在仓库内为空集，历史发行资料是否存在无法核实，缺口已具名交 T-09；两套外部包的当前实现重放已完成，见[只读重放证据](stage_reports/stages/stage-07-rr/evidence/external-corpus/replay-current/replay-current.md)。

## 验证状态

T-01：文档总入口、事实、生成物、调度与 `git diff --check` 全部退出 0；独立代理复现调度正反例，正本哈希未变；交叉核验重算 202 个派单路径的 blob 哈希，0 不一致；冷启动 T-04/T-06 成立、T-13 补注后复验成立。

T-03：四个预设上 `rr2_contract_packed`、`rr2_contract_beat`、`rr2_contract_entry`、`rr2_contract_input` 与两条 RPA B3 对照均退出 0；本轮修正了 input 用例里一处错误的 `wasForwarded` 断言。F135 故障注入与安装消费者未执行，不能按通过计。

T-04：四个预设（`debug`、`headless-debug`、`candidate-headless-debug`、`candidate-shared-debug`）构建退出 0，各 7 项测试全通过；`rr-f302-minimal` 与 `rr-f302-tools` 两个独立配置门禁通过；五档安装前缀与两种消费者接线 10 组配置-构建-运行退出 0；A1/A3/B 阴性成立，A2 在修复门禁依赖 id 读取（`92539ad4`）后成立；RR-F216/F217/F434 分别成立（F216 的非 Windows 分支未在本机核实）。

波次 5：T-02 的四项文档门禁与负例注入退出 0，RR-F105 部分成立，`[packed]` 69 例全过（含修复 T-04b 遗留的 SPEC-02 源单元列表）；T-05 的 `rr_frame_layout` 在 `debug` 4/4、`headless-debug` 3/3 通过，真实 GL 上下文覆盖借用 FBO 与状态恢复，`framebuffer_incomplete` 本机不可构造；T-17 的四例与既有 override 集在 `candidate-headless-debug` 通过，`headless-debug` 默认闭包测试通过、隔离门禁未回退。合并树上重新跑过：candidate 预设 `[packed]` 69/69、T-17 两例与 override 集全过；`debug` 的 `rr_frame_layout` 4/4、`[packed]` 69/69、隔离门禁与 T-03/T-04 七项测试全过。

T-06：帧对象、能力检查、GL 实验桥（窗口路径只编译）、安装面、共享导出与 F135 场景 1/2/3/5 通过；场景 4 与宿主/Player 帧链未闭环。B4/R0 等 7 个 worker 用例在 F508 清零回退后全部恢复，`cuexis_architecture_tests` 因渲染测试直接调用 GL 变红，修复在 task-25。

F508 第二轮：`rr_session_unload_lifecycle` 六 SECTION 通过，`-L rpa` 186/186、`-L execution-host` 71/71、候选 playback 195/35794、默认闭包 137/17556；`cuexis_architecture_tests` 与 `cuexis_format_check` 退出 0，隔离门禁在 `candidate-debug`、`debug` 退出 0。PR 47 的编译修复在本地复现：GCC 全量构建、MSVC Release `cuexis_judgement_compile`、`candidate-debug` 的 `cuexis_chart_tools` 均退出 0；远端重跑待推送。

F135：lead 在 `candidate-debug` 重跑 F135 七例通过；独立复核重做尺寸、零分配、OOM、负编译与基线重跑，12 项成立，MSVC Release `Error` 112、`optional<Error>` 120、`FaultSlot` 56，`audit_layout.py` 全量重跑的 sizes 键与提交 JSON 一致；shared、Release 全量与 Clang 项目内 ctest 未做。

CMake target 事实已可检查：`debug` 预设 fresh configure 生成的 `cuexis-targets.txt` 已同步到 `BUILDING.md` 目标块，`check_docs.py` 的 target contract 项退出 0。产品行为、设备与 hosted 证据随各责任任务取得。

## 接手沿用的产品决定

- RR 完整实现并实验验证 Gameplay v3；Stage 7.5 完整验收后 Stage 8 以 Chart v5＋Gameplay v3 为正式主线，旧内容按原规则、时间和身份运行。
- v5 保持 CXT2、Clip2、Behavior Event2 和整数 render.alpha 的原设计，拒绝旧作者动画语法。
- 设备等待的计时、超时判断和处置归 Player，SDK 不实现该机制；取消/清理及终态须真实发生。
- F501 的内容有效性与独立提交门禁、F135 的分配失败/容量/观测缺失分类、F203 的整批登记验证按已接受合同执行。
- 全面结构清理及逐组 API 过渡沿[登记册裁定](stage_plans/active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)，实际过渡发布、替代稳定和消费者证据满足后才能裁定旧入口退出。
- 生产源、安装头与构建系统文件名不得带阶段/任务编号，测试与工具可以；遗留路径按 `docs/governance_rules.json` 逐路径豁免。

## 最近改动

本轮已提交：`a22e255f` 治理落盘、`3b8bf41e` T-01 派单与证据、`569253d8` T-13 复验、`03334ecd` 命名硬规则与检查器、`6c77a888` T-01 证据修正、`5dc55387` T-04 拆分与改名迁移、`3943fb13` 门禁依赖遍历修复、`0bb2d927` D-20 双侧重放证据、`92539ad4` 门禁帮助函数依赖 id 修复、`8cfb302d` T-04 交付、`9c25a88f` 日期构建推进、`b07e20dc` T-05 几何与 GL、`5f1ef718` T-17 会话修复、`52f0d760` T-02 与 SPEC-02 修复、`95de188d` 波次 5 交付与状态回写、`b2ad4cf8` T-06 帧对象与 GL 桥、`8383672c` F508 清零回退、`07eb42ea` T-10 第九轮、`77157702` T-12、`b2769f55` F508/F135 裁定、`c85cfa61` F508 生命周期拆分、`80c121b4` GCC/MSVC/Clang 编译修复、`f00439c3` 隔离源表对齐。以上后三项与 F135 相关文档更新尚未推送。T-06 的部分交付报告与状态回写随后提交。
