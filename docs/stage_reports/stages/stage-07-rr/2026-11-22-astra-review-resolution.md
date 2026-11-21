# Stage 7-RR：Astra 执行者复审九项设计修订

状态：evidence

更新日期：2026-11-22

<!-- cuexis-meta
id: EVIDENCE-STAGE-07-RR-2026-11-22-ASTRA-REVIEW-RESOLUTION
status: evidence
authority: none
date: 2026-11-22
-->

## 结论

**成立（限九项候选设计修订）。** [首次复审](2026-11-22-astra-executor-review.md)登记的九项均已修订并完成双向交叉复验。B 对 A 成立；A 对 B 首次部分成立，随后确认 T-08 的默认导出差分已与 T-06 对齐，唯一剩余 P2 关闭。逐项落点和实施时应观察的结果见[结果与证据](#结果与证据)。生产未实施。

## 范围与方法

两名 Astra 先独立接卡审查；主代理分别核对阶段发现并向原代理追问，两份终稿完成后去重。owner 随后授权修正文档。A 处理 AX-03/04/05/06 和分阶段导出合同，B 处理 AX-01/02/07/08/09；两人交叉读取对方落盘的合同和卡，对照现有代码、schema、CMake，检查责任卡及正反例。下表是设计验收入口，产品结果留实施时取证。

## 执行记录

- 基线：首次审查起点为 `9623efa321fe488e9c6b8549f7059283020c1861`，206 项输入的核对见[首次复审执行记录](2026-11-22-astra-executor-review.md#执行记录)。本次文档修订前快照在 `out/review/stage-07-rr/astra-resolution-20261122/before/`。
- 局部自检：B 修订时 `python -B tools/check_docs.py` 通过（766 篇 Markdown、20 项 candidate），`python -B tools/verification/check_docs_facts.py` 通过（版本漂移 0）。主代理转交的 A 自检：文档 766/20，16 卡调度和 17 项调度检查器测试通过。两组结果均早于本轮整合定版。
- B 复验 A：对照 RPA 作者夹具及根依赖守卫、旧 profile §8 和输入实现、Phase 声明及旧准备关系检查、生产命令和 owner 通道。AX-03/04/05/06 及 AX-01 的 R1、R5、RR-4 分阶段设计成立。
- A 首次复验 B 部分成立：帧接口、生命周期、安装与纯值数据边界，以及 AX-02/07/08/09 成立；T-08 默认使用 `headless-shared-debug` 对比 T-06 的 `shared-debug`，缺少同配置基线。修订后 A 差量复验成立：[T-06:37](../../../stage_plans/active/stage-07-rr/tasks/T-06.md#任务级验收) 留存双 preset，[T-08:36、38](../../../stage_plans/active/stage-07-rr/tasks/T-08.md#任务级验收) 保留四个安装配置，默认另行 fresh 配置 `shared-debug` 并构建 `cuexis_playback`，实验用 `candidate-shared-debug`。两者均由[根 CMake](../../../../CMakeLists.txt)读取本树 `$<TARGET_FILE:cuexis_playback>`，与 T-06 同 preset、同工具链的原表比较。A 同时核读本报告 AX-01、AX-05 和 RR-F131 纠正，未发现错误。此复验核对设计与命令，没有构建。
- 主代理在本次报告补录前完成组合文档门禁，五项均退出 0：`gen_docs_map.py` 完成生成；`check_docs.py` 校验 767 篇 Markdown 和 20 个 candidate JSON/CXT 文件；`check_docs_facts.py` 报告版本漂移 0；`check_plan_schedule.py` 确认 16 张卡均已调度；`git diff --check` 仅有既有换行提示。日志与退出码见 `out/review/stage-07-rr/astra-resolution-20261122/main-checks/{generate,docs,facts,schedule,diff}.{log,exit}`。
- 同目录 `scope.json` 记录：HANDOFF 保护段哈希与原值一致；`AGENTS.md`、`schedule.md` 和[首次 Astra 复审报告](2026-11-22-astra-executor-review.md)字节未变；当批全部文件无尾随空白。

## 结果与证据

| 编号 | 修订合同与责任卡 | 实施时的验收输入和期望 |
| --- | --- | --- |
| AX-01 / P1 | [帧提交与导出](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#提交资源与交付)、[分阶段安装](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#安装与窗口)；[T-05](../../../stage_plans/active/stage-07-rr/tasks/T-05.md)、[T-06](../../../stage_plans/active/stage-07-rr/tasks/T-06.md#任务级验收)、[T-08](../../../stage_plans/active/stage-07-rr/tasks/T-08.md#任务级验收) | 两种 shared Player 实链同五个函数及具名帧成员，同一旧谱同摘要。默认源码树非授权消费者与默认安装均不可见新头；实验安装 opt-in 可移动、访问、析构。T-08 对照 T-06 同 preset、同工具链原表，默认无新增 Gameplay 导出。 |
| AX-02 / P1 | [源输入](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#源输入与-lowering)、[载体验收](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#载体验收)；[T-12](../../../stage_plans/active/stage-07-rr/tasks/T-12.md)、[T-13](../../../stage_plans/active/stage-07-rr/tasks/T-13.md#任务级验收) | 源 common=A、行=B 拒绝；两种 codec 的裸 Reader 均拒绝同一成品 K4 的逻辑 ID 混用 A/B，零行合法。全套一致改为 B、重算镜像和摘要，无 expected 可读，旧 expected 拒绝。owner、闭包和自摘要仍校验；Chart UUID 不能推出逻辑 ID，CXC 必传的包内 expected 仅证明包内一致。 |
| AX-03 / P1 | [构建与安装](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#构建头与安装)、[T-10 验收](../../../stage_plans/active/stage-07-rr/tasks/T-10.md#任务级验收) | 默认及实验 headless 启测都生成作者编译 target；`cuexis_rpa_tests` 私有链接 `cuexis::judgement_compile`，同步根 `ALLOWED`，实际构建并执行 B3 夹具。默认不依赖候选或工具开关。 |
| AX-04 / P1 | [命令与启动](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#命令与启动)、[T-14 验收](../../../stage_plans/active/stage-07-rr/tasks/T-14.md#任务级验收)、[T-15 验收](../../../stage_plans/active/stage-07-rr/tasks/T-15.md#任务级验收) | T-14 定义并消费生产 Retry，在已有 owner 通道验 P4-07：Retry 和两个不同来源 Pause 均有终态回执，覆盖成功、失败。T-15 验 R/F5 各发一条 Retry/Reload，释放和重复按下不再发送。 |
| AX-05 / P2 | [旧 profile §8](../../../formats/gameplay-v2-execution-profile.md#8-失败优先级诊断与关闭界限)、[RPA 合同](../../../formats/rpa-execution-contract.md)、[T-03 三入口验收](../../../stage_plans/active/stage-07-rr/tasks/T-03.md#任务级验收) | 普通规范化与旧 submit 分测首错。旧 profile 用不同原 tick 900/901 才构造路由后碰撞；native 901 与真实 forwarded 900→901 各自用新序号重投原始项。RPA 用 occurrence 槽验同批合法、重复和冲突。 |
| AX-06 / P2 | [typed 源与编译](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#成品reader-与身份派发)、[T-11 解析](../../../stage_plans/active/stage-07-rr/tasks/T-11.md#任务级验收)、[T-12 双入口](../../../stage_plans/active/stage-07-rr/tasks/T-12.md#任务级验收) | 声明 `tap/1`。窗口 `tap/1`、`tap/2` 均可解析并保留 ordinal；源适配与直接 typed 调用均接受前者，后者报 `judgement.execution.relation_invalid`，定位窗口 phase，不出产物。 |
| AX-07 / P2 | [场景合同](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#场景与动态表现)、[T-13 PA-02](../../../stage_plans/active/stage-07-rr/tasks/T-13.md#任务级验收) | 作者 `camera` 写入 Graph `staticChart.defaultCamera`；旧错键与双键均拒绝。保留非默认相机的 SCN1、实际投影矩阵对照。 |
| AX-08 / P2 | [写回验收](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#验收向量)、[T-16 P5-06](../../../stage_plans/active/stage-07-rr/tasks/T-16.md#任务级验收) | 库用例留 `headless-debug`；进程用 tools=ON 的 `candidate-headless-debug` 显式构建 migrator，传同树 target 路径。检查退出码、stdout、stderr、输出与报告；发布失败恢复双目标，恢复失败保留备份，缺程序即失败。 |
| AX-09 / P2 | [工具外部行为](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#工具外部行为)、[T-13 打包进程](../../../stage_plans/active/stage-07-rr/tasks/T-13.md#任务级验收)、[T-16 迁移进程](../../../stage_plans/active/stage-07-rr/tasks/T-16.md#任务级验收) | T-13 验 `cuexis_gameplay_assemble`、`cuexis_chart_candidate`、`cuexis_cxc_pack`；T-16 验 `cuexis_chart_migrator`。四个真实进程单独 `--help` 均只向 stdout 给帮助、退出 0、无文件副作用；坏参数各沿原分类。 |

**AX-05 / RR-F131 纠正。** 首次复审把同一原 tick 的 distinct press/release 写成旧 submit 的 dispatch collision 触发例，不准确。[输入检查](../../../../engine/judgement/src/input.cpp)中 `resolveDomains=false` 是普通规范化：不同序号的这两个观测可接纳，同身份才报 `same_tick_collision`。[旧 submit](../../../../engine/judgement/src/execution_kernel_submit.cpp) 用 `true`，按 §8 先查 sequence，再查同 subject 的 `duplicate_queue_entry`，同原 tick 不同 subject 先报 `same_tick_collision`，不进入路由比较。[既有 oracle](../../../../tests/judgement/execution_kernel_oracle_tests.cpp) 中的碰撞来自不同原 tick 汇到 901；随后单独接纳的 901 为 native。T-03 保留该例，另以 900 真转发及 RPA 独立槽取证。现有生产输入与旧 profile 首错无需因此改动。

## 限制与未做

主代理已完成上述文档验证，结果对应本次补录前的快照。产品实现验证未执行：本轮没有构建或运行 Player、SDK、CLI，也没有验证 ABI、GPU、设备或 hosted。

## 对登记册与合同的影响

[登记册 §15](../../../stage_plans/active/stage-07-rr/findings.md#15-astra-执行者复审2026-11-22)保留首次九项待收敛的时点并追加本次处置；先前两份阶段报告保留各自原结论。产品验收仍按[阶段矩阵](../../../stage_plans/active/stage-07-rr/acceptance.md#阶段验收矩阵)取证。[实施采用](../../../stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)遵循 D-05：RR-3 限 R1/R5/R6，R2/R3/R4 延期；`NEXT` 各包另行采用。本次修文授权不启动生产实施或发布。
