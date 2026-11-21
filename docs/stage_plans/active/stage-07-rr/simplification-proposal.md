# Stage 7-RR 架构精简方案（待审阅）

状态：candidate

更新日期：2026-11-21

<!-- cuexis-meta
id: PLAN-STAGE-07-RR-SIMPLIFICATION-PROPOSAL
status: candidate
authority: references
-->

**推荐：默认产品长期只维护一条 Player 执行路径。** 改暂停、换谱或结算时，只改一个会话控制者，用普通播放和显式判定两组用例守住差异；改某个候选格式时，主要验证它的适配器和共享边界，只有影响公共行为才扩展回归，不再让每种格式都配一套时钟和渲染流程。

按[计划 §4 D-05](plan.md#4-已确认方向与决策台账)，RR-3 仍限于 R1 构建隔离、R5 渲染收敛与 R6 文档减重，R2/R3/R4 继续延期。已定设计见[架构设计附页](architecture-design.md)；旧读取和迁移仍按[兼容退出政策](../../../adr/0041-legacy-format-exit-policy.md)。

## 先审哪六项

P1–P6 是本页的审阅编号，只审各项未定部分。接受一项不表示接受其他项，也不等于变更现行 API、批准实施或排期。

已接受的时间、比例、SDK 与兼容设计分别以 [D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)、[D-23](architecture-design.md#d-23-谱面画面比例与宿主尺寸)、[D-24](architecture-design.md#d-24-sdk-判定接入与实验扩展)、[D-25](architecture-design.md#d-25-兼容入口与格式合同收敛) 为准。下文[整体推荐方案](#整体推荐方案待审)补齐余项；全部待审，不表示 owner 接受或授权实施、排期。

同日续议确认 [D-25](architecture-design.md#d-25-兼容入口与格式合同收敛)：旧渲染入口逐步替代并最终删除；Packed CRC 与 CXC playback encoding 采用现行实现的明确规则，两处格式合同已更正。候选正式化的 profile/revision 与具体兼容迁移留待后续裁定。

| 待审 | 要决定什么 | 推荐与主要代价 |
| --- | --- | --- |
| P1 格式与 SDK | [数据流、构建依赖、作者表示与安装入口](#p1-数据与-sdk)；已接受的职责边界继续有效，具体推荐待审。 | 拆开作者编译与成品装载；迁移已安装成员须经过版本裁定，发行组合仍由 Stage 8 决定。 |
| P2 渲染 | [矩形坐标、整数取整与旧入口迁移](#p2-渲染与兼容迁移)；D-23／D-25 已接受的呈现归属与退出原则继续有效。 | SDK 统一计算内容矩形；兼容期保留替代探针和适用的消费者门禁。 |
| P3 时间 | [Stop 量化碰撞、起播钳位与实时准入](#p3-时间与会话时钟)。Stop 半开区间与结束反查已接受，量化退化和碰撞处理仍为待审推荐。 | 保留旧成品与旧回放规则；新实时须验证完整映射，不能只比线性区间。 |
| P4 Player 与旧宿主 | [显式请求、重试装载、无音轨与控制接管](#p4-player-与旧控制权)。R=Retry 已定；Reload 具体键位推荐 F5。 | 复用同一 prepare 路径和唯一控制者；新交互与接管安排待审。 |
| P5 合同与诊断 | [目标版本、损失报告内容与失败诊断](#p5-写回与诊断)。保存前报告已知损失、覆盖策略归编辑器的原则已认可。 | 维持已更正的 Packed CRC 与 CXC 编码；只审具体表示、实现和必要的合同修订。 |
| P6 阶段交付 | [迁移顺序、分段验收与停止条件](#p6-迁移与验收)。D-05 范围已定，本节推荐其交付与交接方式。 | 当前整理 R1/R5/R6 及 RR-4 窄迁移依据；后续实施继续等待相应授权。 |

审阅可以只回复编号和「接受／修改／延期」，也可以只改一行，例如「P4：无音轨的普通播放改为允许内容指定终点」。没有回复的项继续待审；将来若牵涉已接受的合同或 D-05，须单独裁定。

### 为什么选择按消费者收敛

比较的是**长期如何减轻同一个维护者的负担**，不重新表决是否保留单一 Player。有些路线超出 D-05，只用于说明取舍。

| 路线 | 能省什么 | 会留下什么成本 |
| --- | --- | --- |
| 只收紧文档和运行时默认值 | 不迁调用方，最易撤回 | 默认构建仍可编译、链接或安装候选；下一次改公共头或工具链仍需兼顾它们。 |
| 只隔离构建，不整理 SDK 和调用关系 | 能先减少默认构建依赖，边界容易用预设检查 | 宏两侧的公开类定义和旧调用可能仍不同；外部消费者仍需知道两套行为。可作中间步骤。 |
| 把所有候选提升为正式能力 | 一份安装包即可试用所有路径，不再选构建 | 把未成熟格式、时钟和证据变成兼容承诺，维护面反而最大；还会提前替 Stage 8 作发行决定。 |
| 归档候选实现，只保留格式资料与历史输入 | 默认产品积累最少代码 | 仍在用的工具和可复现实验失去入口；以后恢复某种格式须重新验证甚至重写。只适合已经查明没有消费者的对象。 |
| **按实际消费者隔离，逐项替代再退役（推荐）** | 默认只维护一条产品路径；有用的候选与旧谱兼容各自有明确入口和退出条件 | 前期要查调用方、比对故障行为、维护少量隔离测试；长期每条重叠路径要么有具名消费者，要么可以删除。 |

## 整体推荐方案（待审）

以下是一套连贯的技术推荐，P1–P6 均待审；已接受边界仍以对应 D 条目为准。拟名不占用版本号，本节不构成 owner 共识、合同修订、实施授权或排期。

Chart 解析权威节奏，独立时间库构造一次不可变映射，同次编译生成表现与玩法的离散 tick，连同比例保存在现有 prepared 内容中，Playback 可选接入 Judgement。源谱直开也只编译一次；成品装载直接读取保存的边界，不反向调用作者编译器。依赖为 `timing → core`、`chart → timing`、玩法作者编译器依赖 timing 及所需 judgement 编译模型、judgement 运行时只依赖 core。assembler 作者编译源码与运行时准备已编译内容拆为不同 target，复用现有容器和会话。

推荐先保留旧样本、核清安装消费者，再做当前范围的隔离与渲染替代；后续获准实施新时间和玩法后，逐项由单会话接管旧职责，最后在适用兼容窗口退出旧入口。下表明确各段的技术选择和验收。

### P1 数据与 SDK

| 事项 | 待审推荐 |
| --- | --- |
| 作者与成品 | 比例用宽、高正整数；落点用精确 Beat，窗口用带符号 ms。拟议字段 `aspectRatio`、`gradeTables`、`gradeTable` 仅作草案。源谱及成品保存各自所需表示，运行时共用已编译离散边界。 |
| 等级与计分 | 等级表具名声明，类型默认、单音符显式覆盖；未知引用拒绝，旧无表谱的 grade 为空。复用现有 ScoreRule 整数计分／连击，不造 DSL。 |
| 版本边界 | 只升级实际变化的直接读写／解释边界，不要求所有格式同号。旧成品保留原 tick、wire、identity 和规则，不重新编译；Stage 8 发行组合另行裁定。 |
| SDK 迁移 | 复用 prepare／commit／输入／推进／帧／资源职责；先加独立实验头和窄入口、改具名调用方，再在批准窗口退出旧成员。共享类跨构建一致；删除已安装成员／符号或改变稳定布局仍须版本裁定，不以宏删除或仓内零引用替代。0.x shared 消费者仍按[现行规则](../../../guides/VERSIONING.md#sdk-版本递增规范)在同 minor、工具链和构建配置下重新编译，不承诺二进制热替换。 |
| 当次输出 | 只直接交付逐阶段结算结果，包括到期 `Miss`；输入回执沿用现有回放/诊断路径。每个声明阶段一局最多结算一次，准备 `P` 个结果槽即可，`P` 为本局阶段总数。容量不足时不推进、不消费输入。宿主提供并拥有输出空间，事件不引用会话临时内存。拟用单一 `AdvanceOutcome`（名称待定）返回结果数量、处理位置、完成/故障状态和故障诊断；预检失败才返回普通 `core::Result` 错误。推进中途故障时，Outcome 同时带回已提交事件，冻结该局，不重跑，也不把未完成折叠的分数当作正常结算。 |

验收：普通宿主和具名实验宿主分别只从安装树构建，检查实际链接、安装内容与共享类；以等级覆盖、未知表、旧无表、成品往返及无输入到期 Miss 作正反对照。验证 `P` 槽容量、少一槽时状态不变、部分推进故障同次返回已提交结果且后续不会重复结算。

### P2 渲染与兼容迁移

| 事项 | 待审推荐 |
| --- | --- |
| 矩形 | 左上原点、物理像素、非负宽高；用检查过的整数算术选宿主区域内最大居中比例矩形。受约束维取满，另一维向下取整，奇数留边余量给右／下；空区域跳过。 |
| 投影与状态 | 新谱按作者比例投影，旧谱沿原行为。适配器只清宿主区域，设置内容视口与裁剪，绘制后还原借用状态；稳定帧布局随适用 API／版本迁移，不直接改写。 |
| 旧入口 | 启动、像素和摘要探针取得替代证据后，按 D-25／ADR 0041 的实际发布承诺和消费者门禁退出；公共符号保留到批准窗口。 |

验收：奇数留边、高 DPI、非零嵌入偏移、空区域及旧谱投影；检查区域外像素、状态恢复和探针替代证据。迁移期允许短暂保留旧入口。

### P3 时间与会话时钟

| 事项 | 待审推荐 |
| --- | --- |
| Stop 端点 | 完整累计映射后分别量化绝对起点 S、终点 E，不把量化时长加到 S。正时长量化后 `E <= S` 拒绝；两个不同 Beat Stop 的前 E 等于后 S 也拒绝，以保住已接受的结束 E 返回原 Beat、inStop=false，避免下一 Stop 抢占。 |
| 同 tick 与零拍 | 不同音符可同 tick；不同 Beat Tempo 阶跃同 tick 按源 Beat 稳定顺序批处理取最终状态；同 Beat Tempo+Stop 沿既有语义。Beat 0 归零只扣严格零拍前的 Stop 总长，不扣零拍自身 Stop。等级相对窗口仍按已接受的 `R(t)+R(δ)`，与 Stop 绝对端点分开。 |
| 查询时刻换算 | Playback 先估算实际所求时刻的源位置，对尚未校正的原始位置应用 `max(0, source - correction)`，再减谱面 offset；等价校正后观测不重复校正。必须在所求时刻 clamp，不能 clamp 锚点后线性外推。 |
| 起播与分支 | source=0、correction=3 ms、offset=20 ms 时逻辑时间为 -20 ms，直接扣 23 ms 错误。A 相加只在同观测基准的非 clamp 区间可作核对，不能替代完整映射或假定测试 A 自带 offset。输入补偿、显示提前各从共同逻辑时间分支应用。有音轨从源 0、无音轨从谱面时钟 0 起播，不 seek 到 offset 凑零点。 |
| 实时准入 | 新实时仅接固定 us／1:1 且身份、原点、校准可核对的映射，包含起播钳位边界；旧实时仅接具名验证适配。缺准入只拒绝请求的实时能力，保留普通播放及旧回放。旧普通路径和旧候选回放均保留原映射；设备切换／观测过期后重新核实。 |

验收：Stop S/E 及相邻 tick、零宽和不同 Beat E/S 碰撞的拒绝，同 tick 合法事件、零拍 Stop 的接受；另测 clamp 转折与跨锚点查询、正负 offset、暂停、输入和显示分支。极短 Stop 的拒绝是推荐代价，不回写旧成品。

### P4 Player 与旧控制权

| 事项 | 待审推荐 |
| --- | --- |
| 启动请求 | 官方 Player 一次启动固定来源／难度／用途／必要配置，更改时重启；第三方 SDK 宿主仍可同进程另 prepare。独立布尔选项拟名 `--judge`，显式校验失败报原因、不开局、不自动降级。 |
| 装载与按键 | R=Retry 已定，较少用的 Reload 键推荐 F5。即使首次装载失败，有启动来源仍可显式 Reload 重读同一来源，成功从零暂停；无来源时均无目标，Retry 另须有已加载可用内容。复用同一 prepare 路径。 |
| 无音轨与尾段 | 无音轨建议复用单调时钟、首版显式 Stop 结束，不从音频掉线自动切入。有音轨实机结束按 D-15 等 EOF 和未完成判定义务实际处理完毕，不增固定黑场时长或尾段字段。Retry／Reload／Stop 和局间寿命沿既有共识。 |
| 控制与故障 | 独立暂停原因按 D-09／D-12／D-14 解除；回执复用既有 session／run／operation 身份，旧回执不能改新局，不造多级协议。装载／提交失败保留实际阶段，不伪称回滚；旧职责逐项验证再接管，必要临时转发只指向同一实现。 |

验收：坏谱修正后 Reload、无来源、无可用内容 Retry、缺配置、按键冲突、无音轨长尾、掉线、独立暂停原因及迟到回执。固定官方启动选择减少状态；无音轨手动结束是首版建议的代价。

### P5 写回与诊断

| 事项 | 待审推荐 |
| --- | --- |
| 保存结果 | 保存前报告具体已知损失、覆盖策略归 Studio／第三方编辑器的原则已认可。具体实现推荐显式目标版本，不支持则报错；先产出文本和损失明细，列对象／字段、损失内容和目标版本，再由编辑器决定覆盖／另存／取消。自动保存和确认方式归编辑器，不增覆盖策略 SDK 接口。 |
| 格式边界 | 维持已定 Packed 前 92 字节 CRC 与 CXC profile/encoding；旧 Beat 上限推荐用对应 Reader 常量，只修受影响直接合同，不收紧其他版本。 |
| 诊断 | 保留底层原因，区分分配失败、容量拒绝和未知异常；未知／未测值标不可用，不冒充零或成功。新稳定码、映射和接受集合先经对应合同裁定；跨版本迁移沿 ADR 0041。 |

验收：字段级往返及损失明细，目标不支持／取消／准备失败时原字节不变；独立 CRC、编码、文件／内存错误样本和资源／未知异常注入。明细成本由写入工具承担，不带入播放运行时。

### P6 迁移与验收

| 范围 | 待审交付与停止条件 |
| --- | --- |
| 当前范围 | D-05 的 R1/R5/R6，以及既定 RR-4 窄声明迁移设计／核查；检查隔离、安装边界和渲染替代，保留旧样本与具名消费者。成员删除仍等版本裁定。 |
| 后续目标 | 新时间、新玩法和统一会话待相应实施授权，按本节顺序推进；R2/R3/R4 继续延期，不借本提案重启。 |
| 退出 | 每段检查相关行为、实际构建／安装及故障场景，不要求一次全矩阵；合同、行为证据或兼容窗口未齐时保留原唯一控制权和入口。长构建／hosted 按既有授权规则，不预定 SDK 版本或 Stage 8 格式组合。 |

## 产品、格式与安装边界

下表按源码和 CMake 记录现状。“默认 Player”指 `CUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF` 时的普通 `--chart`、`--project`、`--cxc` 入口。

整包完整性检查与显式候选解码按 [D-18](architecture-design.md#d-18-按用途编译-player)／[D-25](architecture-design.md#d-25-兼容入口与格式合同收敛)；安装消费与可选判定按 [D-19](architecture-design.md#d-19-sdk-示例与宿主试用)／[D-24](architecture-design.md#d-24-sdk-判定接入与实验扩展)。其余数据流、目标划分和窄迁移推荐统一见 [P1](#p1-数据与-sdk)，以下保留消费者现状与核查入口。

Chart v1–v4 共用普通 Player 入口。Playback 准备谱面时，兼容读取器按 v1、v2、v3 各自的规则直接生成 `ChartDocument`，保留原版本。v4 用专用读取器解析源谱，并处理谱中声明的参数、动画和模板。两路随后都用 `ChartCompiler` 编译，进入同一播放运行时。播放器默认的 ChartClock 适用于无主音乐的谱，含主音乐的谱须选择 `--mode audio` 或 `--mode host`。[读取分流](../../../../engine/chart/src/chart_v4_loader.cpp#L233) · [编译与时钟条件](../../../../engine/playback/src/playback_session_prepare.cpp#L306) · [旧谱播放样例](../../../../tests/playback/playback_migration_equivalence_tests.cpp#L242)

Stage 7-RR 最初按加载器分组，把 `v5/Packed` 计作一组。下表分行列出可编辑的 v5 源谱和工具生成的 Packed 二进制文件，以说明谁会读写它们。[原研究的五组口径](../../../stage_reports/stages/stage-07-rr/2026-11-17-architecture-research.md#L77) · [Chart 表示与转换](../../../architecture/CHART_REPRESENTATIONS.md#1-三条输入族不是一条流水线)

| 条目 | 当前消费者与状态 | 默认 Player 可直接打开 | 建议边界 |
| --- | --- | --- | --- |
| Chart v1、v2、v3 | 三版共享兼容策略：[标准文本来源](../../../../engine/playback/src/playback_source.cpp)进入[兼容 Reader](../../../../engine/chart/src/canonical_chart_loader.cpp)；显式迁移到 v4 受 ADR 0041 保护。 | 是，`--chart` 或受支持的工程/CXC 声明的谱面。 | 每版保留可读样本及显式迁移验收；播放旧谱不要求用户先转档，也不覆盖原谱。 |
| Chart v4 | 当前默认生产基线；Player 普通文本、工程与 CXC 来源均走 Playback prepare。 | 是，`--chart`、`--project`、`--cxc` 的已支持入口。 | 继续作为默认格式线；不为候选入口改写 v4 合同。 |
| CXT v1 | 已接受的动画模板，由 Chart v4 引用并在准备时读取；见 [`animation_template_loader.cpp`](../../../../engine/chart/src/animation_template_loader.cpp)。 | 否，模板是被导入的依赖，不是独立可播放文件。 | 保持模板导入语义；不增加独立 Player 文件入口。 |
| CXC v1 | 已接受的 Project 容器；[`PlaybackSource::fromCxcFile`](../../../../engine/playback/src/playback_source.cpp) 经 [`CxcPackageLoader`](../../../../engine/cxc/src/cxc_package.cpp) 读取包及其声明入口。 | 是，`--cxc` 打开容器内默认声明的受支持内容。 | 分两层校验：先验 CXC 头、manifest、entry 路径与完整性，再由所选 payload 的 Chart/CXT Reader 校验语义；容器有效不等于 payload 有效。 |
| Chart v5 | 候选。工具可编译受支持的源谱，实验版 Playback 从工程或 CXC 读取显式选中的条目。[来源选择](../../../../engine/playback/src/playback_source.cpp) | 否。实验构建也会拒绝裸 `--chart v5.json`。在实验构建中，`--candidate-entry` 须与 `--project` 或 `--cxc` 搭配使用。 | 保留工具编译与实验条目入口，对外支持范围按现有 SDK 合同执行。 |
| Packed Chart v1 | v5 静态候选谱的二进制编码，`v1` 是这套编码的版本号。工具把支持的谱面数据展开、编译后写成 Packed，可将其作为条目放进 CXC 工程包。作者编辑的是源文件。[Packed 格式](../../../formats/PACKED_CHART_FORMAT.md#1-分层与设计选择) | 否。普通 Player 的 `--chart` 读源谱。实验构建可从工程或 CXC 显式选择受支持的 Packed 条目。 | 先校验二进制头和各段边界，再校验读出的谱面内容。保留显式开发入口。[候选条目检查](../../../../engine/cxc/src/cxc_candidate.cpp) |
| CXT v2 | candidate；[`CxtV2Loader`](../../../../engine/chart/src/cxt_v2_loader.cpp) 提供读取与展开，尚未成为默认 Playback 路径。 | 否；只作为显式候选格式处理，不作独立 Player 输入。 | 随 Chart v5 candidate 隔离；模板展开结果仍需通过对应 Chart/Gameplay 校验。 |
| Gameplay Capsule v2 | candidate 的类型化玩法内容；[gameplay_packed](../../../../engine/gameplay_packed/src/gameplay_capsule_read.cpp)提供读取，执行测试有 `--gameplay-packed` 专用输入。 | 否；测试参数不是普通 Player 的文件入口。 | 将 Packed 物理封装与 Capsule 的玩法数据分层校验，再验证静态 Chart、引用闭包和玩法语义；任一层通过不替代其他层。 |
| Compiled Gameplay Graph JSON v1 | candidate typed JSON 表示；候选 `GameplayContent::fromGraph` 路径见 [`gameplay_candidate.cpp`](../../../../engine/playback/src/gameplay_candidate.cpp)。 | 否；无默认独立文件入口。 | 校验 Graph envelope/revision 后，再由 typed reader 校验记录、身份与闭包；Schema 通过不能代替语义校验。 |
| 工具 | CXC pack/unpack/validate 属开发工具，见 [`cuexis_cxc_pack`](../../../../tools/cxc/cxc_pack/CMakeLists.txt)、[`cuexis_cxc_unpack`](../../../../tools/cxc/cxc_unpack/CMakeLists.txt)、[`cuexis_cxc_validate`](../../../../tools/cxc/cxc_validate/CMakeLists.txt)；Gameplay assembler 随候选构建提供，见 [`gameplay_assembler`](../../../../tools/gameplay/gameplay_assembler/CMakeLists.txt)。 | 否；工具消费不等于 Player 默认支持。 | 保留工具作为显式开发/验证消费者；不要据此扩大默认 Player 或 SDK 合同。 |

| 构建/安装面 | 当前状态 | 建议 |
| --- | --- | --- |
| 默认构建 | [根构建](../../../../CMakeLists.txt)的候选开关默认 OFF，但[Chart 目标](../../../../engine/chart/CMakeLists.txt)仍列入 Packed、CXT v2 等源码。运行时不接收，不等于编译图已隔离。 | 产品保留上表正式及兼容路径；R1 检查实际编译源、传递链接、注册测试及安装清单。失败时保留实现，绝不为缩小安装包而悄悄砍正式能力。 |
| 显式开发构建 | `candidate-*` preset 显式打开 `CUEXIS_ENABLE_CHART_V5_CANDIDATE` 并构建开发工具；构建 flavor 为 experimental。 | candidate 只在显式开发构建、具名工具或 entry 中保留；不把“能编译/能读”描述为 production。 |
| SDK 安装 | [Playback 构建](../../../../engine/playback/CMakeLists.txt)在候选开关下公开 `CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE`，加入候选头；experimental 安装需显式 `Cuexis_ALLOW_EXPERIMENTAL`。Player 应用本身不随 SDK 安装；官方 Player 是 SDK 参考应用。D-10 将 Player 与测试共用内部会话控制实现列为未来验收目标，Studio 仍为未来计划。 | 默认 SDK 长期维护；具体窄入口、消费者分组及安装验收采用 [P1 待审推荐](#p1-数据与-sdk)，成熟流程按 D-19。 |

**R1 先隔离源码和链接。** 用两套干净构建播放同一份旧谱，再打开同一个含正式和候选条目的合法 CXC。两版都应按默认入口播放正式谱、执行整包完整性检查，且不解码未选择的候选条目；显式选择受支持的候选条目时，实验版应能播放，默认版应给出拒绝原因。

**公开声明迁移另列 RR-4。** [`PlaybackSession`](../../../../engine/playback/include/cuexis/playback/playback_session.hpp)与 [`PlaybackSource`](../../../../engine/playback/include/cuexis/playback/playback_source.hpp)目前随宏改变声明，与 [ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) 的目标不符。推荐按 [P1](#p1-数据与-sdk)先加实验替代、迁具名消费者，再申请旧成员退出的适用版本窗口。通用显式 entry 工厂在两种构建中保留，OFF 拒绝候选 entry。

RR-4 比对 static/shared 的真实安装树及公开类，检查 `cuexis_internal/` 泄漏，按 D-19 用[外部消费者](../../../../cmake/VerifyExternalConsumer.cmake)和[参考宿主](../../../../cmake/VerifyReferenceHost.cmake)仅从安装树构建并验证接入；实验安装未显式选择应拒绝。若须删除已装符号、改变布局或触及 R2，先登记并申请版本和合同裁定。R2 继续延期，仓内零引用不构成删除许可。

## 渲染入口与兼容期

比例与呈现归属按 [D-23](architecture-design.md#d-23-谱面画面比例与宿主尺寸)，坐标和取整采用 [P2 待审推荐](#p2-渲染与兼容迁移)。判定完成后的表现故障按 D-24，不重算判定或计分。下表保留需迁移的调用与探针。

| 范围 | 当前源码路径 | 建议目标 |
| --- | --- | --- |
| 启动静帧 | [Player 启动](../../../../app/player/src/player_app.cpp)在 `deviceFirst` 分支、准备内容前调用 `renderFrame(still)`。此时没有谱面快照。 | 启动占位由 Player 窗口外壳负责绘制；不要求创建谱面快照，也不把它计作当前谱面帧已呈现。无来源时的「无资源」画面同属 D-16；明确指定来源失败仍按实际来源和原因诊断。旧调用的迁移方式另行核实；用冷启动、准备前关闭和不可画的 surface 验证。 |
| 正式帧交付 | [Player 帧循环](../../../../app/player/src/player_control_frame_loop.cpp)填充 `RenderScene`，通过 [IPresentationRenderer](../../../../engine/presentation_renderer/include/cuexis/presentation_renderer/presentation_renderer.hpp) 的 `submit` / `present` 交付；`presentation.renderer.surface.zero_size` 时跳过，不 `present`。 | 保留 Render ECS 和表现层。有效帧至多提交一次，成功后才呈现；失败回执不报告呈现，重复通知不重复计分或重绘。 |
| OpenGL 兼容和 smoke | [smoke 测试](../../../../app/player/src/player_smoke.cpp)还调用 `renderPresentationFrame`，比较省略调试场景、空场景与填充场景的摘要，并有像素读回。它不是第二个产品帧循环。 | 对独立测试帧用正式 renderer 或测试专用观测比较摘要及像素，不能在同一产品帧多次呈现。替代不能验证原有差异时暂留探针；只迁调用不等于删除公开符号。 |

**退出规则已接受，迁移待实施。** 按 [D-25](architecture-design.md#d-25-兼容入口与格式合同收敛)，逐个核对旧入口的实际发布承诺及消费者；兼容包装只作过渡。[ADR 0041 D5](../../../adr/0041-legacy-format-exit-policy.md)保护的 `RenderFrame` / `renderFrame` 须取得版本与迁移裁定。若证实曾正式支持安装消费者，用代表性仓外宿主从安装树验证迁移；若证实仅有源码消费，记录安装门禁不适用，仍须取得保护入口的退出裁定。仍执行 ADR 0041 适用的 hosted consumer 等既有门禁，代表性安装宿主验收不替代这些条件。`renderPresentationFrame` 核对 OpenGL 头、适配器和消费者后，若证实仅供内部使用且替代用例通过，可单独删除。各入口须有适用的启动、零尺寸、像素或恢复场景的替代证据。

## 时间换算与判定 tick

已接受的基准、量化及窗口端点规则见 [D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)。时间映射、Stop 绝对端点和实时会话时钟的待审推荐统一见[P3](#p3-时间与会话时钟)，本节保留现有候选基准和输入事实。

**已接受的编译精度：** 新内容量化到最近整数微秒，半数取偶；遇到非法时间值、溢出或量化后失效的窗口，编译拒绝。表现与判定使用同一次编译生成的离散 tick。[Chart 连续 BPM 换算](../../../../engine/chart/src/timing_map.cpp)使用浮点积分；不承诺跨平台位级一致。精度和接入细节以 [D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算) 与 [D-24](architecture-design.md#d-24-sdk-判定接入与实验扩展) 为准，实现未开始。

**窗口端点（2026-11-21 已接受）：** 目标时刻 `t` 与带符号相对偏移 `δ` 按上述规则分别量化，端点取 `R(t) + R(δ)`。同一等级表的相对窗口宽度不随目标 tick 改变，[D-22](architecture-design.md#d-22-谱师自定义判定等级) 的相邻档共用量化端点。`int64` 转换或端点加法溢出、非法或非有限值、量化后 `start >= end` 均拒绝，不扩窗。单就两次量化，绝对端点的误差至多 `1 μs`，不含浮点 BPM 积分误差；设备实际精度不因 tick 单位改变。

Playback 负责设备观测、音源位置和输入时间的会话换算与校正。旧成品保留原时间基准和规则，不统一重算，也不改写现有 wire 或 identity。作者精确拍位保留供 Studio 编辑。具体新格式版本与迁移留待实施设计。

**现有候选基准。** 候选 Capsule/Graph 携带玩法时间基准；[`TimebaseProfile`](../../../../engine/judgement/include/cuexis/judgement/timebase_profile.hpp)声明 `unitToken`、`tickScale`（一个 tick 有多大）与节拍映射所需数据。[现有实现](../../../../engine/judgement/src/timebase.cpp)能声明 `engine.tick.us.v1`、`us`、`1/1`，但不默认给出 tempo；`mapBeatToTick` 按 tempo/stop 积分，没有按任意 `tickScale` 缩放。对 scale 的校验为正，不证明每个正值都能对齐实时输入。输入侧的 `ObservationTick` 已由校准时钟给出，不再当作 beat 二次转换。

新实时映射的准入与拒绝边界按[P3](#p3-时间与会话时钟)待审推荐；旧谱、普通播放和既有候选回放保留原基准。此处不改 wire、FrameDigest、identity 字节或公共 ABI。

普通音频仍按 ADR 0042 的原公式和起播 clamp；候选时钟与 `TimebaseProfile` 现状只作为对照，不能据非 clamp 线性区间推定全程等价。新实时换算、输入／显示分支和起播反例见[P3](#p3-时间与会话时钟)；偏移核对见[登记册 RR-F307](findings.md)。

后续正反验收覆盖见[P3](#p3-时间与会话时钟)；普通 seek 的既有舍入和旧样本仍按原规则验证，候选 `steady.us` 不自动构成生产证据。

## Player 的产品行为

无来源启动和不自动开启判定沿 D-16 至 D-18；Retry、Reload 与结束交付沿 D-13／D-15。交互和故障处置采用[P4 待审推荐](#p4-player-与旧控制权)，此处不重复会话合同。

## 旧宿主的职责与退出条件

按 D-10，Player 与无窗口测试共用同一份**非安装的会话控制实现**，由唯一控制者决定会话状态。播放与可选判定在每会话的执行线程串行推进，设备及窗口仍按各自的线程要求工作。Playback 保留装载/提交与会话内换算责任，运行时判定内核 `judgement` 只依赖 `core`。独立谱面时间库与作者编译器的依赖边界按 [D-21](architecture-design.md#d-21-谱面表现与判定共用拍位换算)。现有协调部署见 [ADR 0046](../../../adr/0046-realtime-playback-coordination.md)；变更其约束须先修订对应合同。

| 现有消费者或职责 | 合并后由谁承接 | 撤掉旧控制权前须能证明 |
| --- | --- | --- |
| Player 控制器、候选 `ExecutionHost`：装载、prepare/commit、Retry/Reload、Pause/Resume/Stop | 现有 Playback 继续校验内容与提交；由唯一控制者决定状态并发布结果；Player 外壳只翻译 CLI/窗口意图。 | 同一组内存谱面不经 SDL 驱动真实控制代码。覆盖重复 Pause、切换前失败保留 A、先命中再 Stop、未开局与真实零分、同会话 Retry 无需再加载；普通路径不实例化判定。 |
| 捕获、时钟、执行调度与 replay/seek | 输入侧提供有序的本局输入；Playback 校验时钟并换算；判定执行路径确认实际执行边界，控制者决定是否推进。 | 用[时钟用例](../../../../tests/rpa/rpa_b3_clock_tests.cpp)和[回放/seek 用例](../../../../tests/rpa/rpa_b5_seek_host_tests.cpp)对照；过期观察、未交齐的输入不能被当作已执行，回放不虚构未来输入。 |
| 帧投递、时限、回执；故障收尾 | 渲染 owner 报告本局该帧是否实际提交和呈现，音频 owner 报告设备事实；控制者独占暂停/续局及结果交付决定。 | [renderer](../../../../tests/rpa/rpa_b4_renderer_tests.cpp)、[故障收尾](../../../../tests/rpa/rpa_b5_close_tests.cpp)：重复/超时/旧回执不能重复呈现、计分或结束下一局。 |
| `--execution-test-only`、[验收工具](../../../../tools/verification/execution_acceptance/player_evidence.py)与候选测试 | 工具改驱动上述同一实现；具名且不能同步迁移的调用方才允许临时转发。 | 列出最后的调用方、替代命令及对应证据；迁完去掉转发。历史脚本和已封存证据不挪不改，测试参数与旧控制逻辑分别退役。 |

切换以行为为单位。迁移前可在隔离用例上对比旧、新实现的同一条输入记录，不能在玩家同一局挂两个控制者。若正常和故障路径不能同时对齐，就让该行为继续由原实现承担，不删旧入口；撤销新接线也不得让已经执行的命令再执行一遍。

## 合同与诊断：接受什么，拒绝什么

维持 D-25 已更正的 Packed CRC 与 CXC profile/encoding 合同；整包完整性检查与候选语义解码按上文分工。其余条目保留各自待审状态。按 D-04，改变文件接受集合、判定身份或稳定码前，须先接受具体 ADR/Spec 修订。发现与处置状态以[登记册](findings.md)为准；Reader/Writer 互读仍不能代替独立互操作验证。

| 对象 | 建议采用的规则 | 验收或不动的条件 |
| --- | --- | --- |
| Packed Header CRC（RR-F111） | 已按现行[写入](../../../../engine/chart/src/packed_chart_tables_writer.cpp)和[读取](../../../../engine/chart/src/packed_chart_tables_inspect.cpp)更正[格式合同](../../../formats/PACKED_CHART_FORMAT.md)：计算 Header 前 92 字节，末尾 4 字节保存 CRC，总长仍为 96 字节。现有产物及其 artifact identity 无需改写。 | 后续用独立固定样本验证两种计算范围的差异、损坏拒绝和 Writer/Reader 互操作。真实外部消费者尚未核实；若需保留按旧文档算法生成的文件，另行明确显式迁移与哈希更新。Reader 不自动尝试两种 CRC。 |
| GridDelta、Beat、CXT v2（RR-F112/F113/F115） | GridDelta 不作为生产 Reader/Writer 的 mode；CXT v2 的谱内调用仍是 proposal，不把工具显式展开当作 Player 功能。这两处文档已收窄，不再立项。旧 v1–v3 Reader 的 Beat 分子/分母上限建议采纳[现有常量](../../../../engine/chart/include/cuexis/chart/limits.hpp)为对应格式合同边界，不借机收紧其他版本。 | 对各上限用刚好符合、刚好超界的有理数分别验证，区分 Reader 上限与算术越界。只有获批的生产读写与消费者出现，才重开 GridDelta 或谱内 CXT 调用。 |
| CXC playback entry（RR-F114） | [CXC](../../../formats/CXC_FORMAT.md) 与 [Chart Entry](../../../formats/CHART_ENTRY_V1_FORMAT.md) 已按[实现](../../../../engine/cxc/src/cxc_candidate.cpp)对齐：`candidate.static-tap-lanes4-v1` 必须使用 `packed-chart`，`candidate.behavior-v2-v1` 必须使用 `source-json`。保留显式候选选择和原默认路径。 | 后续各用合法 profile/encoding、错配、普通 source-only、未显式选择验证接受边界。正式化时按实际不兼容程度决定是否新增 entry revision，并明确兼容迁移；具体发行组合、版本和排期仍待裁定。 |
| 判定输入与诊断（RR-F131/F135/F142） | 同一 canonical 身份同 tick 再入按 [Gameplay Spec](../../../formats/GAMEPLAY_V2_SPEC.md) 的 `same_tick_collision` / `invalid_relation` 处理；只因 tick 相同不能把不同身份说成同一身份，真正的迟到队列重复另判。内存分配失败、达到容量上限、未知异常和操作成功分开；异常不跨模块。Player 把已有底层 fault 原样保留，在没有下层原因时才按失败阶段给原因，不让 `player.execution.test_only` 遮掉诊断。 | 对三类输入分别断言接纳/拒绝、code 和 category；注入资源耗尽、真实容量拒绝、未知异常，断言不假成功、不冒充容量。只改[散落失败](../../../../app/player/src/player_execution.cpp)中已核对的调用点，不重命名已有稳定码；涉及新码或映射先裁定。 |
| Packed 两个读取入口、JSON Schema 校验（含 RR-F404） | [文件读取](../../../../engine/chart/src/packed_chart_io.cpp)与内存解码保留共同的内容错误；打不开文件仍是 I/O 错误，不复活旧码别名。[Schema 校验](../../../../engine/json_support/src/schema.cpp)的 `Ok` 是“校验过程成功”，不是“实例合法”：需要合法内容的调用方还须检查本次违规诊断。 | 同一组损坏字节经文件和内存得到同一内容码；文件不存在可区分。分别测合法实例、违规实例、schema 无法编译，消费方不得仅因 `Ok` 放行违规实例。没有差异证据不另造解码器或错误层。 |
| 防止丢谱或伪造观测（RR-F401/F419/F420） | 当前 [ChartWriter::write](../../../../engine/chart/src/chart_writer.cpp) 返回序列化文本，会把变速和停顿写成空数组。写入时建议显式指定目标版本，不支持该版本就报错。同格式手动保存时，写入工具应在编辑器写文件前说明已知会丢失的内容。按 [D-19](architecture-design.md#d-19-sdk-示例与宿主试用)，编辑器可在谱师知情确认后覆盖，也可要求另存。自动保存和确认方式留给各编辑器设计，无需为覆盖策略新增公开 SDK 接口。旧谱跨版本迁移仍按 [ADR 0041](../../../adr/0041-legacy-format-exit-policy.md)。首帧 `render_us` 没采到不当零时长；`exactLossCount=false` 不能读成“没有丢失”。 | 用非空模板、变速和停顿样本检验可表示内容的字段级往返；对有损样本核对写入前报告的具体损失，并验证未获谱师确认或目标版本不受支持时原文件字节不变。分别用已知测量和不可得测量验证；当前载体若不能区分则明确标不可用，由消费者忽略，不伪造数值或静默修改稳定 ABI。 |

这些用例是实施前需要准备的正反对照，本稿没有运行它们，也不要求一次清空所有登记项。P0/P1 按[计划 §9](plan.md#9-验收标准)逐条说明处置，P2 延期须注明接手阶段和重新评估条件。

## 文档减重、阶段交付与停止条件

| 归属 | 本方案建议的交付物 | 何时停止或交接 |
| --- | --- | --- |
| 现在：只读决策表 | 以产品、格式、时间、渲染为维度，为每个拟删/合并对象列出状态、实际消费者（产品/工具/测试/外部）、权威合同、替代者、迁移成本与阻塞。优先录入本稿各表中的对象，**仓内搜索不到生产调用不是删除许可**。 | 消费者未核清或受兼容保护，默认保留/隔离。`particles` 已处置、`debug` 已标 test-only，勿把旧发现重新列作“立即删除”；见[登记册](findings.md)。 |
| D-05 范围内：R1 / R5 / R6 | R1 比较生产与实验构建的实际编译目标、安装清单和依赖；R5 仅迁移已识别的渲染调用；R6 将同一语义留在一份 ADR/Spec，计划只记范围，附页只放提案和验收方向，代码注释只保留约束与原因。 | R1 不夹带 R2 的 SDK 删除；R5 不为删入口牺牲启动/像素验证；R6 不删历史报告、归档或哈希绑定的路径。[当前状态](../../../CURRENT_STATUS.md)仍写“选项待选”，建议另作 RR-1 订正，对齐 D-05 的范围和“尚未实施”，不改历史快照。 |
| 合同、公共面与退出 | RR-2 先裁定接受集合/稳定码再碰同一对象；RR-4 以实际安装树、[外部消费者](../../../../cmake/VerifyExternalConsumer.cmake)和[reference host](../../../../cmake/VerifyReferenceHost.cmake)为门槛。每个对象记录去留、成本（低：改路由；中：迁消费者；高：影响 wire/ABI 或时序证据）、证据及不能合并的原因。 | 如必须依赖延期的 R2/R3/R4，则记录延期、恢复条件和责任阶段，不借 RR-2/RR-4 改名开工；缺 SDK/版本、设备、跨平台或兼容证据，不宣布退出。RR-6 汇总并由 owner 接受；本稿不批准运行完整构建或 hosted。 |
| Plan 拆分试点 | 维持[阶段计划 §6](plan.md#6-工作分解与依赖)和[交接要求](../../../HANDOFF.md)：只试点 RR-0 至 RR-6；`plan.md` 总览，`schedule.md` 只记 ID/波次/前置/并行或独占，任务卡只写任务级验收，`acceptance.md` 只写阶段验收。 | 以后用 `check_plan_schedule.py` 查覆盖、波次和独占，并作一次只交一张卡的真实无上下文委派；由主代理重跑、独立抽查含阴性用例。缺一步就不宣称试点完成，不向其他 active plan 推广。这里不创建这些文件。 |
| 后续阶段接口 | [Stage 7.5](../stage-07-5/plan.md) 只接收玩法能力和限制，[Stage 8](../../future/stage-08/plan.md)按发行矩阵决定格式提升，[Stage 10](../../future/stage-10/plan.md) 的 Studio 可复用会话；预览纯播放、实机和编辑后的新局遵循既定规则。 | 审阅接受后再提炼进相应阶段的范围、边界、待决与验收方向。future 仍是提纲，不提前建任务卡、定设备阈值、实现 autoplay 或定义 Studio 公共 API。 |

**延期不等于放弃，重启要有具体理由。** R2（候选公开面清理）等 RR-4 摸清外部消费者、差异、替代与版本窗口；R3（会话层收敛）等逐项行为和故障验证能证明只有一处控制权，避免为了少几个类重写所有事务；R4（时间表示统一）只在 P3 的适配仍不能满足具名消费者、且有 identity/FrameDigest 兼容方案时重评。三项均在本阶段退出后由 owner 另作决定；不能倒过来作为本轮开工条件。超出现有判定约束的玩法、输入域与计分规则交 Stage 7.5，不在结构迁移时顺带更改。

复核顺序是**先定接受什么，再验证谁在使用，最后才撤旧入口**。结构变更前保存与当前行为绑定的正反样例；候选开/关、普通/实机与故障场景分开取证。只回退本次接线或保留旧的唯一入口，不撤已交付的成绩、不静默改写旧文件，也不以测试专用参数或旧基线冒充生产证据。
