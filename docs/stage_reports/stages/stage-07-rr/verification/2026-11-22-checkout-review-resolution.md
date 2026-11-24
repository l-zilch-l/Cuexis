# Stage 7-RR：独立 checkout 复审处置

状态：evidence

更新日期：2026-11-22

<!-- cuexis-meta
id: EVIDENCE-STAGE-07-RR-2026-11-22-CHECKOUT-REVIEW
status: evidence
authority: none
date: 2026-11-22
-->

## 结论

**部分成立。** owner 转交的复审基于独立 checkout 的 `8599c83af82a2a74b074db387f1b3766d366b9e4`。逐项核实为 9 项成立、8 项部分成立、3 项不成立；成立的合同接线、类型可见性、创建责任及门禁缺口已修订。先前 AX-01–09 的通过只覆盖当次九项修订，不能据此宣称整个计划已经没有实施输入缺口。

## 范围与方法

本轮复用原代理继续核实和修订，随后交叉检查；这不是新增的无父上下文独立审查。代码与已接受合同用于判断当前事实，候选任务用于判断实施输入是否齐全。核实成立的缺口补入候选合同及任务卡；已有责任卡尚待创建的功能不作现成运行能力。主代理负责 T-01、调度检查器、阶段边界及可提交证据。

两名代理最终差量复验均成立，本轮指定范围内未留下已证实的具体设计阻断。该结论只覆盖所列缺口及其修订；产品实施、运行验收与逐卡采用仍按计划完成。

## 执行记录

主代理在基线副本删去 T-05 的 `## 停止条件`，原检查器退出 0。补齐检查后，同一反例退出 1，报告七段结构错误。交叉检查又复现缩进代码伪标题被误认、completed 卡被误拒；修订后 27 项检查器测试通过，active/candidate/completed 分列，active/completed 不得依赖 candidate，completed 不进入当前派单名单。

技术交叉检查发现两处残留：Stop 旧概述仍在提交末批前关闭 kernel 接收，T-15 增加 SDL 事件注入的测试依赖却未同步根 ALLOWED。前者统一为关闭捕获路由、收齐并编号、query、原子核对并关闭 kernel 接收后 seal；后者补同卡根依赖白名单及适用依赖记录。原审查意见、差量和最终复验分别追加在 [A 回执](../evidence/2026-11-22-checkout-review-A.txt)、[B 回执](../evidence/2026-11-22-checkout-review-B.txt)，保留每次审阅的文件哈希。

主代理最终组合检查使用只复制 Git 跟踪文件与本轮可提交新增文件的隔离副本，不复制 `.git`、`out/` 或构建缓存。文档、事实、调度及检查器测试在副本运行，diff 与 HANDOFF 保护段检查在原工作区运行；实际命令、平台、stdout/stderr 与退出码见[组合日志](../evidence/2026-11-22-checkout-validation.json)，修订输入见[LF 归一化 SHA-256 清单](../evidence/2026-11-22-checkout-inputs.json)。这份清单结合基线提交即可在另一 checkout 核对本轮改动，日志与清单排除彼此的自引用。

## 结果与证据

### 运行类型、输入前缀与构建

| 编号 | 对基线反馈的裁定 | 处置与证据 |
| --- | --- | --- |
| IR-01：身份声明无法由独立消费者构造 | 成立 | [运行头链](../../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#judgement-运行与旧作者头链)由 T-10 创建 `prepared_identity.hpp` 的完整纯值定义；records 直接包含，旧作者头转引，SessionConfiguration 保留按值身份。独立消费者须实际构造 view 并 prepare。 |
| IR-02：StableDeclarationId 越界 | 成立 | T-10 将唯一完整定义原样移至运行 `phase_types.hpp`，旧 graph reference 转引；T-12/T-13 消费同一类型，以自包含和实际运行消费者验证。 |
| IR-03：Stop 上界生产与传递 | 部分成立 | 基线已有编号语义，但缺具名读取和生产调用。[Stop 接线](../../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#prepared-与当次推进)补 kernel 单一游标、`receiveCanonicalBatch`、`acceptedIngressThrough` 及同 owner 读取/封定；CLK-02 区分内核未来输入与真实 capture，用生产入口生成 0–7，禁止手填上界。 |
| IR-04：CLI target 无创建卡 | 成立 | [T-10](../../../../stage_plans/active/stage-07-rr/tasks/T-10.md)首创 `cuexis_gameplay_author_cli`，迁现有 CLI/helper 的源、include 和边，两个可执行程序改链；T-13 追加新分支。根 ALLOWED、开关和旧工具验证同卡负责。 |
| IR-05：共享头双重认领 | 成立 | [T-12](../../../../stage_plans/active/stage-07-rr/tasks/T-12.md)唯一首创 context/rows 并完成旧头转引；[T-13](../../../../stage_plans/active/stage-07-rr/tasks/T-13.md)只消费。四类运行身份先由 T-10 交付，context 不重复定义。 |
| IR-06：构建表与目录顺序 | 部分成立 | 原文混淆 target 表与子目录拓扑，已在[构建合同](../../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#构建头与安装)单列合法顺序、同目录多 target 及启用条件。A 的最小 CMake 探针允许前向 alias，故不能仅凭表内先后断言必然 configure 失败；产品完整配置仍待实施。 |
| IR-10：当前无 v3 schema/新码 | 不成立 | 文件尚不存在属实，原卡已列创建责任和未执行状态。现进一步点名容量码归 T-10，v3 payload/v5 基础壳与两条 Stop 码归 [T-11](../../../../stage_plans/active/stage-07-rr/tasks/T-11.md)，壳扩展和成品 adapter 归 T-13；各自交付后验收。 |

本组基线源码位置、修订文件哈希及 CMake 探针源码/实得输出见 [A 回执](../evidence/2026-11-22-checkout-review-A.txt)。

### 渲染、Player 和 Writer

| 编号 | 对基线反馈的裁定 | 处置与证据 |
| --- | --- | --- |
| IR-07：投影及反算 | 成立 | [呈现合同](../../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#p2同提交布局与呈现)规定快照唯一保存 `Pout=Q*R*P`，渲染直接使用、位置只逆完整矩阵一次；T-05 创建组合 helper，T-06 接快照及渲染。补非单位 Q 与 90° 的精确向量。 |
| IR-08：帧桥接线 | 部分成立 | 原 binding/target 总体合同已有，具体调用仍缺。[T-06](../../../../stage_plans/active/stage-07-rr/tasks/T-06.md)补 worker 的 variant 放入、统一 snapshot view、payload 单份计量、`presentOwnedFrame` 到 `OverlayRenderer`/binding 的实际分支，以及旧 adapter 的能力检测与拒绝。 |
| IR-09：旧谱旋转 | 部分成立 | 原公式覆盖四旋转，但旧谱向量不充分。现在明确旧谱 `Q=I`，90°/270° 的 reciprocal aspect 后仍应用 R 一次；0° 保留原矩阵，并验重复重排不累乘。 |
| IR-11：命令矩阵不能由当前类型表达 | 成立 | [完整准入矩阵](../../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#完整准入矩阵)补 Context 的内容/来源/首装/用途/暂停事实和 Decision 的动作/成功策略；同一 owner 在设备确认后读取 pending Pause 再提交状态，撤掉无条件状态赋值。T-14 创建与接线，T-15 消费。 |
| IR-12：物理键接线 | 成立 | [T-15](../../../../stage_plans/active/stage-07-rr/tasks/T-15.md)明确 `event.key.scancode → namedKey(SDL_Scancode)`，覆盖全部保留控制键；真实 SDL 注入 keycode/scancode 不一致事件，验证命令和玩法释放。同卡将测试的 SDL3 边加入根 ALLOWED，并登记依赖用途。 |
| IR-13：首次加载失败后的 Reload 与 D-13 相斥 | 不成立 | D-13 原句限定源码现状，后续 D-16 已接受加载失败恢复。已在[架构附页](../../../../stage_plans/active/stage-07-rr/architecture-design.md#retryreload-与失败d-132026-11-19-owner-已确认的未来设计)明分“无 A、有来源”的 Failed 与“无来源”的 Empty；前者可显式重读，后者拒绝。 |
| IR-14：F5/--judge 的决定层级 | 部分成立 | 附页说明它们是已授权细化范围内的当前候选技术选择，实施卡有确定拼写；保留 owner 已接受的行为和历史记录，不把技术选型追认成 owner 逐字批准。 |
| IR-15：Writer 故障注入不可实施 | 成立 | [T-16](../../../../stage_plans/active/stage-07-rr/tasks/T-16.md)补同 TU checked-write、按调用传入的 tests-only 私有 hooks、创建/注册位置；输出变异仍经真实 Reader/比较，异常检查点及实际预算分别验，CLI 由真实有损源验收。 |

本组源码核实、裁定与自检原文见 [B 回执](../evidence/2026-11-22-checkout-review-B.txt)，A 的交叉复验见 [A 回执](../evidence/2026-11-22-checkout-review-A.txt)。

### 计划、验收和证据

| 编号 | 对基线反馈的裁定 | 处置与证据 |
| --- | --- | --- |
| IR-16：T-01 平台命令与基线 | 部分成立 | PowerShell 专有哈希命令无法直接在 Linux 执行，已换标准 Python。旧 `9623efa3` 可作历史对照，但不应固定为每次接卡基线；[T-01](../../../../stage_plans/active/stage-07-rr/tasks/T-01.md#步骤)改为采用提交与实际 HEAD/tree 分开记录。 |
| IR-17：七段任务卡结构 | 成立 | 已复现删除标题仍通过；[检查器](../../../../../tools/verification/check_plan_schedule.py)新增完整、有序、非空的七段检查，代码块和注释里的标题不计。[基线回执](../evidence/2026-11-22-ir17-before.json)退出 0，[修订回执](../evidence/2026-11-22-ir17-after.json)退出 1；回归见[检查器测试](../../../../../tools/verification/test_check_plan_schedule.py)。 |
| IR-18：候选任务混在调度 | 部分成立 | 单一调度本来只拥有依赖，计划已限制候选采用；原工具输出却未显示边界。现在分列 active/candidate/completed，active/completed 不能依赖 candidate，已完成卡仍校验但不派发。候选检查通过仍不授权派单。 |
| IR-19：A-01–A-05 未验候选实现 | 不成立 | 这与 D-05 及现行采用范围一致。[阶段验收](../../../../stage_plans/active/stage-07-rr/acceptance.md#阶段验收矩阵)明确 A-N 仅交付候选设计，后继采用须将对应任务向量纳入后继阶段运行与退出验收。 |
| IR-20：独立 checkout 缺历史原始回执 | 部分成立 | 已提交报告确有回执转录，但未交付的 `out/` 原始日志无法在新 checkout 取得。今后必需的设计回执与短检查输出随报告放入 `evidence/`。历史记录保留其原证据等级，本轮重新验证，不补造旧原文。 |

## 限制与未做

本轮范围是设计合同、任务与文档检查器。生产 Player、SDK、CLI、ABI、GPU 和设备路径未构建、未运行。Windows 中实跑 Python 与文档检查；未把跨平台命令替换写成 Linux 产品验收通过。

## 对登记册与合同的影响

本次缺口及处置由[登记册](../../../../stage_plans/active/stage-07-rr/findings.md)续记。原报告保留当时结论；现行状态只引用本次核实范围。已有实施授权仍依[计划](../../../../stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)，受影响任务须先通过 T-01 的修订输入核对再派发。
