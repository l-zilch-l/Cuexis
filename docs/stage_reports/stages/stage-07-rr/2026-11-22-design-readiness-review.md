# Stage 7-RR 自主技术收敛与可实施性审查

状态：evidence

更新日期：2026-11-22

<!-- cuexis-meta
id: EVIDENCE-STAGE-07-RR-2026-11-22-DESIGN-READINESS-REVIEW
status: evidence
authority: none
date: 2026-11-22
-->

## 结论

**成立。** 本轮三份候选技术合同与 16 张任务卡已对齐，Huygens、Epicurus、Bacon、Nash 四组无父上下文冷启动审查及原审查者最终复验全部成立。设计可实施，无本轮未决合同项；开发者可按卡片定位首改、测试、正反例与停止条件。首轮真实发现、修订和复验过程保留在下文。

主代理最终组合文档检查**成立**：MAP 与 AGENTS 生成块已生成，文档、事实、调度、17 项检查器测试及 diff 检查全部 exit 0，实际记录在 `out/review/stage-07-rr/T-01/main-final-20261122/`。本次补录后报告定版并停止修改；Einstein 随后封存的输入和独立再验结果按下文固定代码路径读取，本报告不预记其哈希或结果。

独立调度阴性与集成路由核对在其记录快照内**成立**；主代理的 18 项 Fraction/整数算例在其算术范围内**成立**。两者分别支持文档门禁和确定算术，不证明候选实现、CTest 产品用例或安装消费者运行通过。本轮生产实现未开始。

本报告只汇总设计与证据，当前任务仍以 [CURRENT_STATUS](../../../CURRENT_STATUS.md) 为准。授权范围沿[计划唯一派单入口](../../../stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)：D-05 的 RR-3 仍为 R1/R5/R6，NEXT 仍按原采用规则执行。

## 范围与方法

代码核对的提交基线为 `9623efa321fe488e9c6b8549f7059283020c1861`，tree 为 `befa60a59527d755467934c0e9303cd1d89c5b4a`。当前技术合同和任务包含未提交修改；该 HEAD 只标识提交基线，不能代替最终文档内容哈希。主代理已在设计报告与状态页就绪后生成 MAP、亲自执行组合门禁并保存 main-final 日志；Avicenna 本次只补该次实际结果一次后停止修改；Einstein 最后执行 `--capture`，封存最终输入并独立再验。封存后的哈希和复验结果以 final 目录的实际记录为证，不再回写本报告。

设计入口为[架构共识记录](../../../stage_plans/active/stage-07-rr/architecture-design.md)、[可实施性索引](../../../stage_plans/active/stage-07-rr/design-readiness.md)、[运行与时间合同](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md)、[成品合同](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md)和[渲染／Player／Writer 合同](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md)。任务级验证入口归[任务索引](../../../stage_plans/active/stage-07-rr/plan.md#任务索引)、[调度](../../../stage_plans/active/stage-07-rr/schedule.md)和[阶段验收](../../../stage_plans/active/stage-07-rr/acceptance.md)。

四名独立审查者从无父上下文开始，沿合同、任务卡及实际代码接线寻找实施缺口。A Hilbert、B Dalton、C Beauvoir 按各自写域补设计，原审查者复验。首轮问题归为构建隔离、运行计分、成品源与身份、异步控制与宿主四组，没有新增需求框架。代理补充的技术细节不记录为 owner 逐字接受。

本整理单元读取了上述合同中的相关章节、全部任务卡及 `out/review/stage-07-rr/T-01/` 内的集成和算术记录，并核对可定位证据。四组判断是主代理在本轮对话转交的原审查者工具（tool）回执转录，本报告本身作为四组耐久回执；没有另行生成的原始回执文件，不引用此类不存在的文件。下文区分回执转录、卡片静态读回与已落盘命令日志。活合同使用章节／符号引用；带行号证据以本报告记录的文件哈希绑定。

## 执行记录

提交身份核对：

```text
git rev-parse HEAD 'HEAD^{tree}'
9623efa321fe488e9c6b8549f7059283020c1861
befa60a59527d755467934c0e9303cd1d89c5b4a
```

Einstein 的独立集成回执实际记录以下命令；本整理单元已核对 JSON 与对应日志输出一致，未将其重标为最终组合重跑：

| 执行者与命令 | 记录结果 | 落盘证据 |
| --- | --- | --- |
| Einstein：`python -B tools/check_docs.py` | exit 0 | `schedule-negative-20261122-pyii5098/docs.log` |
| Einstein：`python -B tools/verification/check_plan_schedule.py` | exit 0，覆盖 16 卡 | 同目录 `schedule-current.log` |
| Einstein：`python -B tools/verification/test_check_plan_schedule.py` | exit 0，17 项 | 同目录 `schedule-tests.log` |
| Einstein：`python -B tools/verification/check_docs_facts.py --list` | exit 0，漂移 0 | 同目录 `facts.log` |
| Einstein：`git diff --check` | exit 0，仅行尾转换提示 | 同目录 `diff.log` |
| Einstein：同目录 `verify_owned_routes.py` | exit 0；11 文档、322 链接、6 presets 可定位 | 同目录 `route-check.json` |

上表相对目录均位于 `out/review/stage-07-rr/T-01/`。主代理另提供 `main-arithmetic-20261122.json`，记录 18 项独立 Fraction/整数算例；本整理单元读取结果，没有重跑其计算或据此补造命令日志。

主代理随后只读检查本批 31 篇阶段、报告和状态文档：822 个本地链接（含标题锚）全部可达，尾空白为 0，HANDOFF 保护段哈希与指定值一致。`main-links-20261122.json` 记录文档数、链接数、保护段哈希及 `errors: []`；尾空白为 0 来自主代理同次补充回执。本整理单元已读取该 JSON 并绑定下列文件哈希；本项按当时文档快照解释，最后组合检查单列如下。

本整理单元在报告与路由落盘后执行了以下检查；这是本次文档更新的检查。最终组合门禁由主代理执行，最后输入封存及独立再验由 Einstein 在补录结束后执行。

| 命令／核对 | 实际结果 |
| --- | --- |
| `python -B tools/check_docs.py` | exit 0；765 篇 Markdown、20 份候选 JSON/CXT 校验通过 |
| `python -B tools/verification/check_docs_facts.py --list` | exit 0；事实漂移 0，生成物新鲜度通过 |
| `git diff --check --` 后接本单元已跟踪写域 | exit 0；仅 Git 行尾转换提示；新报告另查无尾部空白 |
| 报告固定 H2 与模板逐项比较 | 一致 |
| HANDOFF 保护段按 LF 文本提取后计算 SHA256 | `593c14953999fa0e291da868288b0b2ca4fbb69deb7d781da9e1efdf20be532c`，与指定值一致 |
| findings 在 §14 之前的文本与本单元读入基线比较 | 一致 |

### 主代理最终组合检查

主代理于 2026-11-22 亲自执行以下命令，全部 exit 0。证据目录为 `out/review/stage-07-rr/T-01/main-final-20261122/`，五项检查的命令、退出码、输出与日志名在 `results.json` 中；本整理单元逐份读取日志，确认文本与 JSON 一致。生成器结果见同目录 `generate-map.log`，其 exit 0 来自主代理本次执行回执。日志均直接位于该目录，不另设 `logs/` 子目录。

| 实际命令 | 结果 | 同目录日志 |
| --- | --- | --- |
| `python -B tools/verification/gen_docs_map.py` | exit 0；生成 267 行 MAP 和 AGENTS 状态块 | `generate-map.log` |
| `python -B tools/check_docs.py` | exit 0；765 篇 Markdown、20 份候选 JSON/CXT | `docs.log` |
| `python -B tools/verification/check_docs_facts.py` | exit 0；无 `--list` 的正式校验，漂移 0 | `facts.log` |
| `python -B tools/verification/check_plan_schedule.py` | exit 0；16 卡调度通过 | `schedule.log` |
| `python -B tools/verification/test_check_plan_schedule.py` | exit 0；17 项全部 OK | `schedule-tests.log` |
| `git diff --check` | exit 0；只有 LF/CRLF 转换提示 | `diff.log` |

此轮结果证明主代理检查时的文档与调度输入，包含本报告定版前的版本；本次实际结果补录完成后才进行最终字节封存。报告自身以及状态页的最终字节由 Einstein 的后续 `--capture` 绑定，不能把主代理的先行日志当作已包含本次补录的快照证明。

## 结果与证据

### Huygens：构建隔离、安装与逐符号退出

首轮发现涉及 R1 运行闭包与 prepare/CXC 隔离不完整，T-10 静态归档与独立消费者接线不足，以及公共面收缩漏列 friend／内部函数、shared 导出和安装前置。设计与任务现已明确以下落点：

| 首轮问题 | 具体设计补全 | 复验判断与入口 |
| --- | --- | --- |
| 只按 target 名称判断隔离，不能证明真实运行闭包 | R1 明列八个 Playback TU，并分开 prepare 与 CXC；T-10 分阶段静态归档，已给定要求独立消费者实际运行的验收 | 成立；[构建、头与安装](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#构建头与安装)、[T-10](../../../stage_plans/active/stage-07-rr/tasks/T-10.md) |
| 公共面迁移遗漏内部访问和函数 | 明列 `PlaybackSession`、`PreparedPlayback`、`ExecutionFence` 三处 friend，以及 `validateExecutionControlProof`、`executionAdvanceRetention` 的仓内消费 | 成立；[RR-4 逐符号兼容迁移](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#rr-4-逐符号兼容迁移) |
| shared 导出范围及安装测试前置不足 | allowlist 使用完整限定名；T-08 安装前先构建所需消费者／fixture，包括 `cuexis_chart_tests` staging | 成立；[安装与窗口](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#安装与窗口)、[T-08](../../../stage_plans/active/stage-07-rr/tasks/T-08.md) |
| T-12 共享产物头与真实运行禁 timing 的判据混淆 | 只给具名 `compiled_gameplay_rows.hpp` 扫描例外；真实运行 include/link closure 继续禁止 timing | 成立；[T-12](../../../stage_plans/active/stage-07-rr/tasks/T-12.md) |

主代理转交 Huygens 最终回执：上述设计与接线复验**成立**，无残余。独立消费者、安装、shared 导出等产品级验证仍是实施任务中的要求。

### Epicurus：计分、时间输入源与 Stop 恢复

首轮发现集中在暂停／恢复、计分单元资格与排序、时间输入同源、完整终态、恢复及序号边界。设计已补齐同局状态和 canonical ingress 的保持、资格／排序与分母的确定规则、TimingMap 对源输入的精确校验、唯一完整终态以及 Stop 前缀和恢复校验；对应[计分与 Prepared 记录](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#归一化计分与-prepared-记录)、[谱面时间 API](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#谱面时间-api)、[Playback 时钟与实时准入](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#playback-时钟与实时准入)及[验收向量](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#验收向量与任务前置)。

Epicurus 首次复验确认原六项规则及整数复算成立，但因以下三个 fixture 差量给出**部分成立**。A/B 修正后，主代理转交原审查者最终回执：三项均**成立**，结合之前六项规则，本议题设计闭合、无残余。A 自检完成，技术合同稳定。

| 首次复验差量 | 最终具体设计补全 | 最终复验判断 |
| --- | --- | --- |
| 短 Hold | GP-02 的 body 与唯一 Body 窗均 `[850,851)`，Body target=851，保留 Tail target 和尾窗；迟到 release 得到指定 Tail 结果 | 成立；target、窗口及路由已核对 |
| 双 Tap 共用具名 Measure | 两个 stable declaration 显式 `sameDocument ordinal101`，引用同一 GPD0 Measure；JPR1 分别选 `main/five`；错开两次 press，中间 release | 成立；具名引用、独立选表与实际输入顺序已核对 |
| Stop 接收锁 | CLK-02／P4-05：seq6@105 保留编号和归档但 Stop@100 不结算，seq7@90 结算 Hit；锁后 tick95 无编号、无归档、不结算 | 成立；接收前缀与截止对照已核对 |

双 Tap 是 Epicurus 与 Bacon 的同一交叉 fixture，合并追踪，不能把同一次修改计为两份独立运行证据。算术位宽按主代理更正及合同记录为 **160/128 位**。

### Bacon：作者根、定义体交接、身份与 Reader

首轮五项中四项先经原审查者复验闭合；当时仍为**部分成立**，真实残余是 `BeatGameplaySourceView` 没有携带具名 Pattern/Measure 的定义体，已有 `LocalDeclaration` 只提供引用，无法独自完成定义出版与稳定合并。A/B 补入 typed spans 和 stable merge 交接后，原审查者最终确认该阻断关闭，五项均**成立**。

| 首轮问题 | 具体设计补全 | 复验判断与入口 |
| --- | --- | --- |
| 成品编译入口混用旧 inline 作者根，CXT 与完整实体 owner 不清；具名定义体没有完整交接 | 真实 Chart 作者根、CXT 来源与 full E 已具体化；`PatternDeclaration`／`BeatMeasureDefinition` spans 随 stable merge 传完整定义体及未用项，由 adapter 持有至同步调用返回；具名定义双向一一对应，匿名定义照传并沿既有 inline 路径 | 成立；最终定义体 DTO 与关联 fixture 复验关闭阻断；[源输入与 lowering](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#源输入与-lowering)、[成品、Reader 与身份派发](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#成品reader-与身份派发) |
| 编译需要身份、身份又依赖编译结果；时间输入可能异源 | 先产生 owning 未终结行，再求 digest、绑定身份并最终 prepare；TimingMap 与编译使用同一源并校验 | 成立；[身份终结](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#身份终结) |
| Reader 头闭包可能倒流作者编译依赖 | 独立 Reader 头与编译边界具体化，提供独立 include／强制链接反例 | 成立；[拥有关系与入口](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#拥有关系与入口) |
| 相机验收缺实际帧接线，BHV 锚可能由其他行掩盖 | 完整 camera transform 接实际 view/frame；BHV 使用独立锚点验证 | 成立；[SCN1 场景补量](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#scn1-场景补量)、[完整作者夹具](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#完整作者夹具) |
| Graph hex 解码及新增表限额缺实际预算来源 | 明列 decoded hex 与总 decoded、分配前限额，以及源／两种 codec 的一致限额接线 | 成立；[限额与稳定拒绝](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#限额与稳定拒绝) |

主代理转交 Bacon 最终回执：双 Tap 明确 `doc.alpha/4`、`doc.alpha/5` 各引用 `sameDocument ordinal101`，同一 GPD0 Measure 与不同 JPR1 `main/five` 选表一致；两次 error=−30000 分别得到 `Good/Early`、`G2/Early`。unused Pattern 的 `instant→skip` 两版均合法、两版 compiled digest 不同，每版源／Packed／Graph 三路 digest 相同，结算与计分轨迹相同。原定义体 DTO 阻断关闭，其余四项沿此前成立，无残余。

定义体交接只补源编译边界，wire／Reader 沿既有设计。B 的机械文案同步已经读回：匿名定义照常传入，仅具名定义参与双向一一对应；该同步没有新增合同选择。

### Nash：实际 seal、身份、Player、Writer 与 FBO

| 首轮问题 | 具体设计补全 | 复验判断与入口 |
| --- | --- | --- |
| T-05 借用 FBO 路由和附件拒绝／恢复不完整；T-06 未比较完整目标 | 从 GL_NONE 显式路由 color0，验证附件并恢复状态；完整 currentTarget 比较中 host.x 从 100 变 200 时非故障弃帧并保留 retained | 成立；[提交、资源与交付](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#提交资源与交付)、[兼容目标及向量](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#兼容目标及向量) |
| T-14 调度回复未证明对应实际 seal | 实际 seal 接原 terminal/ACK，单 P 槽；H115/W15 且 prefix100 时 Waiting，prefix101 才执行；Pause/Resume/Stop 故障按已封前缀交付 | 成立；[实时调度的封定与单次回复](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#实时调度的封定与单次回复)、[所有权、暂停与收尾](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#所有权暂停与收尾) |
| NEXT Resume 身份与旧 host/capture 守卫不一致 | NEXT 保持同 RunKey 并增加 segment，各接收点一致检查；legacy 仍沿既有身份规则 | 成立；[Playback 时钟与实时准入](../../../stage_plans/active/stage-07-rr/contracts/runtime-timing.md#playback-时钟与实时准入) |
| T-15 载体选择及键位锁定时机不完整 | 后继 CXC v2 显式 entry；keymap 全校验成功后锁定，失败时 F5 可以重读 | 成立；[命令与启动](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#命令与启动)、[CXC 条目、源包及派发](../../../stage_plans/active/stage-07-rr/contracts/prepared-artifact.md#cxc-条目源包及派发) |
| T-16 Writer 参数反例脱离真实 Resolver，失败产物语义不清 | 在原 JSON 容器合成 `[5,param(y),0]`，用 y 两值走真实 Resolver 正反例；失败 `artifact=nullopt`，调用方只读 diagnostics | 成立；[写入规则](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#写入规则)、[损失定位与迁移映射](../../../stage_plans/active/stage-07-rr/contracts/render-player-writer.md#损失定位与迁移映射) |

原审查者还核对了 A 的完整终态 score 为唯一来源，与 C 的同局内容留存到 EOF、期间 Stop 不重复 seal 相容。主代理转交 Nash 最终回执：五类及直接关联接口复验**成立**，无残余；其范围为静态设计和现有代码接线，未运行产品用例。

### 逐卡冷启动定位记录

本整理单元逐卡读回输入、写域、步骤、任务级验收和停止条件，记录如下。每行的卡片链接就是静态定位证据；四组设计判断来自前述 tool 回执转录，DOC 卡另有 Einstein 的集成路由证据。下表的正反结果是已经给定的**实施验收预期**，本轮没有执行产品测试。T-01 实际文档／调度日志单列于上文；其余卡片的证据目录和完整构建命令见各卡，不能把未来路径写成已有日志。

测试列中具名 CTest 按卡片指定 preset、`--no-tests=error` 和精确 `-R` 运行；新增名称在对应实施卡中注册。首改列给出开发起点，不扩大卡片写域或 NEXT 采用范围。

| 卡片／核对来源 | 首改入口 | 测试入口 | 正例／反例的给定预期 | 停止条件 |
| --- | --- | --- | --- | --- |
| [T-01](../../../stage_plans/active/stage-07-rr/tasks/T-01.md)／Einstein | `out/review/stage-07-rr/T-01/` 基线及派单清单 | `check_plan_schedule.py`、其 17 项测试、`check_docs.py`、diff | 未改副本 exit0；只加 T-05→T-06 前置 exit1，唯一 earlier-wave 错误且正本哈希不变 | SHA／修订不可复现、入口不存在或仍需询问产品规则，停派该卡 |
| [T-02](../../../stage_plans/active/stage-07-rr/tasks/T-02.md)／集成路由与卡片读回 | `docs/formats/README.md` 的 RR-F101–F105 状态摘要，对照版本权威 | `gen_docs_map.py --check`、`check_docs_facts.py --list`、docs | 现行漂移清零；副本插入错误 SDK 字面量须报告文件／行／值，按现有 warning 判据；历史值保留 | 需改变接受集、稳定码或版本政策时冻结该条；误改历史则退回 |
| [T-03](../../../stage_plans/active/stage-07-rr/tasks/T-03.md)／集成路由与卡片读回 | Beat Loader 对应 Chart Spec；`engine/judgement/src/input.cpp` | candidate-debug 的 `rr2_contract_packed/entry/beat/input`；Chart／CXC／Judgement targets | 原 CRC、匹配 profile、Beat 边界和同 tick 不同 canonical 身份接受；旧 CRC、错 encoding、Beat 越界、同身份碰撞按具名码拒绝且不发布半批 | 新 wire／identity／诊断语义超出现行合同，保留行为并登记 |
| [T-04](../../../stage_plans/active/stage-07-rr/tasks/T-04.md)／Huygens、Einstein | `engine/playback/CMakeLists.txt`、prepare stages、`engine/cxc/CMakeLists.txt` | 四配置 `cuexis_rr_playback_isolation`、architecture、`rr_r1_playback_sources`、`rr_r1_cxc_package_integrity`；工具及安装消费者 | 默认拒候选、实验按原合同加载；将 candidate TU／prepare／CXC 解码放回默认闭包须指出具体来源；普通整包损坏不提交 | 默认可达候选、普通校验／诊断／安装回归，恢复原装配 |
| [T-05](../../../stage_plans/active/stage-07-rr/tasks/T-05.md)／Nash | 合同具名 Playback／OpenGL `experimental/frame_layout.hpp` 与几何／GL helper | debug：`rr_frame_layout` | host 非零原点、旋转和 GL_NONE→color0 正确；非法比例／附件拒绝，区域外像素不变且全部触及状态恢复 | 越区写入、漏恢复，或需新 Reader 才能验证，停在几何／helper |
| [T-06](../../../stage_plans/active/stage-07-rr/tasks/T-06.md)／Nash | `app/execution_host` 的 FramePayload／请求／呈现桥及 Playback 帧消费 | candidate-debug：`rr_laid_out_frame` | 同 backend 与同帧投影；host.x 100→200 非故障弃帧、保 retained；模式不符／过期拒绝，旧谱 digest 不漂移 | 帧／资源身份不一致、旧谱漂移或需新格式，回滚兼容新增 |
| [T-07](../../../stage_plans/active/stage-07-rr/tasks/T-07.md)／集成路由与卡片读回 | `docs/formats/TIMING_MODEL.md` 重复段、World／hasPreparedState 注释 | docs／facts／MAP check；适用时 `cuexis_format_check` | 正确替代链接与 owner 约束保留；副本指向 `RUNTIME_SESSION_REMOVED.md` 须 broken relative link，恢复后通过 | 丢失独有规则、需改行为或历史路径，恢复该项 |
| [T-08](../../../stage_plans/active/stage-07-rr/tasks/T-08.md)／Huygens、Einstein | Playback CMake、`cmake/VerifySharedExports.cmake`、安装消费者 | 默认／实验 static/shared 消费者、`cuexis_rr_sdk_facade_compat`、`cuexis_shared_export_surface`、`rr_sdk_execution_host_shared` | 新旧 facade 同输入结果一致；错 Resume 身份保持 Paused；缺导出、未列完整限定名或默认包含实验头必须失败，staging 先建 chart tests | 旧消费者／布局／符号回归或跨库未解析，恢复安装面并推迟相应退役 |
| [T-09](../../../stage_plans/active/stage-07-rr/tasks/T-09.md)／集成路由与卡片读回 | 采用后的 `YYYY-MM-DD-exit-and-handoff.md` 与 A-01–A-05 证据索引 | 退出快照 docs／schedule／MAP／facts／diff；人工证据核对 | 每项能回溯实际 SHA 和接受范围；删必需负例或延期接受记录分别判缺证据／未接受并保持 active | 缺必要记录、未接受延期或交接范围冲突，保持 active |
| [T-10](../../../stage_plans/active/stage-07-rr/tasks/T-10.md)／Huygens、Epicurus | Judgement／judgement_compile 源归属、`normalized_score.hpp/.cpp` 与当次输出 | candidate-debug：`cuexis_rr_advance_contract`；旧恢复／codec、安装及 `tests/consumers/judgement-runtime` | GP／CLK 精确计分、短 Hold、Stop 前缀及 fault seal 预期给定；容量不足不推进；独立消费者引入作者／timing 头须失败 | 作者头倒流、预检前改状态、推进后分配或吞已封结果，停止 |
| [T-11](../../../stage_plans/active/stage-07-rr/tasks/T-11.md)／Epicurus、Bacon、Nash | `schemas/candidate/cuexis.gameplay-author.beat-us.v3.schema.json` 及 Chart typed 解析／画幅接线 | candidate-debug：`rr_candidate_schema`；headless-debug：`cuexis_gameplay_diagnostics_codes`；facts | GP-01、合法有档 Miss、画幅原值／顺序接受；缺 mode／Exact、重复键、旧字段、非法比例／Stop 拒绝，默认不启 v3 | 需放宽旧接受集、默认启用候选或更改发行矩阵，停在候选合同 |
| [T-12](../../../stage_plans/active/stage-07-rr/tasks/T-12.md)／Huygens、Epicurus、Bacon | `engine/timing`、`beat_us_compile.hpp` 的定义 spans 与作者编译接线 | candidate-debug：`cuexis_rr_timing_contract`；独立编译消费者及 architecture | 同源锚点和量化一致、unused Pattern 两合法变体生成不同 GPD0；错 timing source、缺失／孤立／重复具名定义拒绝；运行 TU 引入共享产物／timing 头须失败 | 旧向量漂移、重复量化或运行闭包牵入时间／作者头，停止 |
| [T-13](../../../stage_plans/active/stage-07-rr/tasks/T-13.md)／Bacon、Epicurus、Nash | `engine/chart/src/chart_v5_loader.cpp`、`engine/gameplay_packed/src/compiled_source.cpp` 与 read/source 根 | candidate-debug：`cuexis_rr_compiled_capsule_contract`；独立纯 Reader 消费者 | 源／Packed／Graph 各版 digest 与结算一致；双 Tap 同 GPD0、异 JPR1；unused Pattern 两版 digest 不同但轨迹相同；错误身份／owner／限额与作者头／符号倒流拒绝 | 身份／边界冲突、纯读重编、误拒旧内容或污染 active，停止 |
| [T-14](../../../stage_plans/active/stage-07-rr/tasks/T-14.md)／Epicurus、Nash | `execution_scheduler.cpp::tryStart`、`session_worker_dispatch.cpp`、host／Playback 控制接线 | candidate-debug：`rr_session_control`、`cuexis_rr_compiled_clock_contract` | 真实 prefix100 Waiting、101 seal；Stop 与 EOF 单次完整结果、NEXT 同 RunKey 增 segment；锁后输入不归档、旧回执／错身份不推进，fault 交已封前缀 | 双控制、旧回执改新局、错误推进或丢旧失败处理，保留旧控制权 |
| [T-15](../../../stage_plans/active/stage-07-rr/tasks/T-15.md)／Nash | `app/player/src/player_control.cpp` 的载体选择及 Player／SDL 键位装配 | candidate-debug：`rr_player_startup`、`rr_player_commands` | 旧 bootstrap 保持、后继显式 entry；R 缓存、F5 重读；未知 pair 首败后修正可重读，完整校验才锁定；缺能力和不相容请求具体拒绝 | 破坏同局 A、首败无恢复来源或绕过唯一控制者，停止接外壳 |
| [T-16](../../../stage_plans/active/stage-07-rr/tasks/T-16.md)／Nash | `engine/chart/src/chart_writer.cpp`、私有 migrator 及具名调用方 | headless-debug：`rr_chart_write`，覆盖 P5-01–07 与 CLI 失败注入 | `[5,param(y),0]` 经真实 Resolver 两 y 值成立；错误分量 patch／占位向量被反例检出；有损失败 artifact=nullopt，仅 diagnostics，已有文件保持或按既有机制恢复 | 吞损失、拒交改原文件、未尝试恢复或需公开私有头，停止切换 |

上述定位记录支持“无需再询问本轮合同选择”的设计判断。未来实施仍须在实际代码与采用范围上逐卡取证；卡片写域、停止条件、具名 gate 的存在和正反输出都要在实施时重新核实。

### 独立集成、阴性对照与算术

| 结论 | 证据（file:line；路径基于 `out/review/stage-07-rr/T-01/`） | 状态 |
| --- | --- | --- |
| 未改副本 exit0；只把 T-05 前置由 T-04 改为 T-04、T-06，exit1 且仅有 `line 19: prerequisite T-06 must be in an earlier wave` | `schedule-negative-20261122-pyii5098/integration-receipt.md:7`；同目录 `receipt.md`、`evidence.json`、`schedule.diff`、`positive.stdout.log`、`negative.stdout.log` | 成立 |
| 正本 schedule 未被阴性实验改写，当前读取哈希与回执一致 | 同目录 `evidence.json`；正本 SHA256 为 `06d9eee89c9d1265da42c39fcc482545f98ae02a2baa780ca75d67812e123059` | 成立 |
| 11 文档／322 链接／6 presets 可定位；文档、事实、调度及 17 项检查器测试在该快照通过 | `integration-receipt.md:9`、`:24`、`:30`；`route-check.json`、`integration-checks.json` 及五份命令日志 | 成立 |
| 18 项独立精确算术覆盖取整、Stop 端点、计分、分组单元、160/128 位归一化与溢出、动态锚、循环、相机点和比例栅格 | `main-arithmetic-20261122.json:2` 起；主代理执行回执与算例结果 | 成立 |
| 31 篇文档的 822 个本地链接及标题锚全部可达，尾空白 0，保护段哈希一致 | `main-links-20261122.json:2` 起及主代理同次尾空白检查回执 | 成立 |
| 主代理生成 MAP 并执行最终组合门禁，全部 exit 0 | `main-final-20261122/results.json:5` 起、同目录五份日志及 `generate-map.log:1`；生成器退出码由主代理回执确认 | 成立 |
| 最终输入封存与独立再验 | `final-20261122/manifest.json`、`results.json`、`receipt.md`；由 Einstein 在本报告定版后生成，读取口径见下节 | 无法核实 |
| 候选产品的实现、构建、安装及运行验收 | 本轮未进入生产实现，没有对应运行证据 | 无法核实 |

相机算例的 actual 写作 Fraction 表达、expected 写作整数列表；数值相等不要求字符串相等。算术文件不是生产测试日志。

本次已读取证据的 SHA256 如下；最终合同输入另待封存，不能用这些日志哈希代替：

| 文件（同一 T-01 证据目录） | SHA256 |
| --- | --- |
| `schedule-negative-20261122-pyii5098/integration-receipt.md` | `4981750b3cb2dccf951c23e2c21fc56f2cf3fdae58505f1e92b4c3d607b55ea8` |
| `schedule-negative-20261122-pyii5098/receipt.md` | `ed6e208b85e819b839cb945583106bf66306112ac3c7c8f4b0aed908b1b68e16` |
| `schedule-negative-20261122-pyii5098/evidence.json` | `1d24bd19b8dc375e573cad547fc4f72846a68f443f157e4b9f80488bfb91910c` |
| `schedule-negative-20261122-pyii5098/integration-checks.json` | `202040ddd929e43ed424efab02577754c6cddc65fc405ea6cfb12efb93426fee` |
| `schedule-negative-20261122-pyii5098/route-check.json` | `130aef9957607311df60d694e5d5574b55b800fc5c55cd623195c6127bd26c31` |
| `main-arithmetic-20261122.json` | `b566c5ddb44801e1fe6953630b32d5a48ec439bb81ac0661685da1e6e20d0b08` |
| `main-links-20261122.json` | `9f6bbb53ce51ac1aa9ffb216b0c419433490995dc533db64c8d43501818ca3d0` |
| `main-final-20261122/results.json` | `ad6072f3377e2f9af2348971645080219c24119433de6d7fa9e1bbd04c652b57` |
| `main-final-20261122/generate-map.log` | `08539fbe011b47700da90e1a2374eeae250130e6073b21eaf23ca7018ec914c9` |

Einstein 回执还披露初次取证驱动在推导仓库根路径时失败，尚未复制或改动正本即中止，修正后完整重跑。并行修订期间的复制与路由读取不是原子快照，应按记录的逐文件哈希解释。

### 最终输入封存的读取口径

本报告及状态页完成唯一一次 main-final 补录后停止修改。Einstein 使用 `out/review/stage-07-rr/T-01/final-20261122/capture_final.py --capture` 封存最终文件并独立再验；报告定版时不预填该次运行的 snapshot 哈希、成功状态或回执内容。读取固定入口如下，均为代码路径：

- `out/review/stage-07-rr/T-01/final-20261122/manifest.json`：读取实际 HEAD/tree 与逐文件 `inputs`，按原始字节 SHA256 绑定设计、卡片、报告和状态文件；提交基线不替代未提交输入哈希。
- `out/review/stage-07-rr/T-01/final-20261122/results.json`：核对 `input_bytes_and_git_state_unchanged`、`snapshot_hashes_valid`、各 `gates` 的退出码与预期、`diff_check_exit`、`reused_negative` 及最终 `passed`，不得仅凭文件存在判成立。
- `out/review/stage-07-rr/T-01/final-20261122/receipt.md`：与同目录 `commands.json`、`logs/`、`SHA256SUMS` 和 `snapshot/` 交叉核对。文件缺失、输入漂移、快照不符或门禁失败均按实际结果保留，不补造成功回执。

封存与独立再验只证明其绑定输入及文档门禁，不证明产品构建、运行或阶段采用。该证据包生成后不再向本报告回填自身 SHA 或结果，避免封存后的文件变化。

## 限制与未做

四组设计复验及主代理最终组合检查均成立，无本轮未决合同项。最终输入哈希与独立封存复验以本报告定版后的证据包为准；本页不预记其结果，也不把已有结论外推到之后的改动。尚未生成的 receipt 只记代码路径，不预建 Markdown 链接。

本轮只做设计、任务与文档门禁工作；未构建产品、未执行候选 CTest、未运行安装消费者、Player／GL 或正式性能测量。任务卡列出的这些命令和反例属于未来实施验收。静态代码接线核对、检查器测试和独立算术不能代替产品运行证据，也不提高已有受限退出、testOnly、实现或认证状态。

四组耐久回执来源为主代理转交的原审查者 tool 回执，独立日志来源为上述已落盘文件。作者自检、静态读回与原审查者最终判断分别记录，设计成立不替代阶段采用、实施或运行验证。

## 对登记册与合同的影响

本轮入口写回 [findings §14](../../../stage_plans/active/stage-07-rr/findings.md#14-自主技术收敛与任务对齐2026-11-22)，保留 §0–13 的历史审查基线与判断。报告索引增加本页；CURRENT_STATUS 只更新当前文档任务与审查状态；HANDOFF 只路由到本页和现行任务入口，并保留 Plan 文档拆分段原文。

架构开放技术项已路由至三份候选合同，技术字段和算法由合同维护。本轮设计与卡片冷启动判断成立；主代理 main-final 实际结果已在本次补录完成，本报告及状态页定版并停止修改。最终输入哈希及独立再验由 Einstein 在证据目录保存。已有共识、D-05 和 NEXT 采用边界继续以计划及架构记录为准，生产实施未开始。
