# RPA A1–A3设计与准入审查

状态：dated review；TB-07首版继续当前局目标已获owner选择，具体语义仍为候选，未实施原型或产品

证据日期：2026-10-11（Asia/Shanghai）

## 范围与证据身份

本轮按桌面接手文档及owner继续工作的指令，只审查和修正设计/规划。
开工分支`stage-7`，HEAD=`7ca1fc6cf2dd4c7072aab3ae260ee5931f5df2d3`。
开工另有7个未提交文件：`engine/cxc/src/cxc_candidate.cpp`、`cxc_hash_internal.hpp`、
`cxc_manifest_loader.cpp`、`engine/playback/src/presentation.cpp`、`tests/core/version_tests.cpp`、
`tests/cxc/cxc_candidate_bridge_tests.cpp`、`tests/player/frame_diagnostics_tests.cpp`。
这些改动不属本轮，均保留；本报告不将整个工作树称为该提交，也不复验其行为。
接手快照的base `2ddbcaf`及“诊断未提交”已过时；HEAD中的SDK声明为0.7.5、构建26.10.10-1。
本轮未查询远端或新发行状态，不将本地提交等同于发行或新SHA验收。

权威阅读：AGENTS、[状态页](../../../../CURRENT_STATUS.md)、[总计划](../../../../stage_plans/active/stage-07/plan.md)、
[RPA计划](../../../../stage_plans/active/stage-07/plan-rpa.md)、
[归档v0.5](../../../../archive/stage-07-planning/2026-10-10-rpa-target-architecture-draft.md)、
[ADR0046](../../../../adr/0046-realtime-playback-coordination.md)、
[8.4受限关闭](../verification/2026-10-10-s7a-8-4-closure.md)、
[N3退出](../diagnostic-formalization/2026-10-10-n3-verification-and-exit.md)。
8.4证据绑定历史1ebcbdc/0.7.1；N3证据绑定其冻结工作树，M1是初跑123/124加修复后单项通过，
L2是恢复构建后的同配置完整282项无skip。这些记录均不证明RPA或当前dirty工作树已经验收。

下文“反例”是对候选设计/计划完整性的纸面检验，除注明源码核对外未运行新实验。
工作归属和准入由活动计划维护，字段和算法由所属合同维护；本报告不构成第二份活动计划。

## 继续有效的合同

| 来源 | 本轮确认的约束 | 对候选的限制 |
| --- | --- | --- |
| [V2 Spec](../../../../formats/GAMEPLAY_V2_SPEC.md) §3.7.3 | canonical observationTick来自入管线时捕获的校准会话时钟 | raw设备时间回溯不是普通适配优化 |
| [execution profile](../../../../formats/gameplay-v2-execution-profile.md) §3–5 | distinct subject同Tick碰撞拒绝、整批原子准入、timer先于输入；空Tick也封存 | 新批处理、排序或control cut不得暗改现行规则 |
| 同profile §4.1/4.2 | observation key含Tick/domain/source/channel/action/amount；contact通道仅domain/source/channel | 通道、观测身份和转换次数不可互换 |
| [Playback生命周期](../../../../api/playback-session.md) candidate组合事务 | Pause保持held且拒绝submit/advance/update；Resume不补release；已准入操作不可重复submit | reconciliation需要明确修订，不能在普通Resume暗补输入 |
| Spec S7A-5/6首用补充、[执行类型](../../../../api/gameplay-v2-execution-types.md) | frontier、workTick、Fact cursor、Fold cursor、horizon分别保存；完整pending与lastObservedTick恢复 | 最后Fact时间不是执行覆盖范围，Seek不能清空future pending |
| Playback生命周期、[实时宿主候选边界](../../../../api/realtime-host-boundary.md) | 单owner Prepared、generation失效、scope整体替换、失败保留边界 | 加锁不授予跨线程许可；同H不证明候选或资源仍有效 |
| N1/N3与[ABI](../../../../api/GAMEPLAY_V2_ABI.md) | 诊断正式投影/sourceCode及Recovery revision2；完整owning结果 | 不重复迁移、不改旧证据、不以摘要相等替代完整结果 |

## TB-01–07逐项审查

这里“产品目标”是计划期望，“待选”没有默认值；所有数值仅为反例，不是预算。

| 项目 | 最小反例与影响路径 | 现行合同/产品目标/待选项 | 必要修正或首用依赖 |
| --- | --- | --- | --- |
| TB-01 捕获时点 | 物理按键100ms，主线程500ms才poll；把原始100ms回填为canonical会改变Hit/Miss，而worker只能改善poll之后的交付 | 现行唯一canonical来源保持；目标是可解释捕获延迟；待选可靠ingress位置及是否另提历史重建 | A2分开physical/captured/delivered/admitted证据，无法测物理时刻就标未知；历史重建先修Spec §3.7.3/ABI/identity |
| TB-02 封窗与控制 | 候选完整前缀界限B=101，但序号7、t=100的release仍未交付；序号8的Pause先到，直接宣告cut完成可能让bodyEnd先seal。另一个已接受t=200输入可以在Pause时仍future pending | 现行late-policy与pending保留有效；目标是有证据的推进许可；待选序号结局、cut与时间界限关系 | 缺口解决与future执行分开；记录准入/拒绝/取消归属，不能把排队Pause当完成或为清空pending把H推进到200；候选F(H)<B另需边界证明 |
| TB-03 同Tick | 同通道同Tick的press/release/press且两次press amount相同：首尾press的观测身份相等，和重复投递无法仅凭当前key区分；按action排序也可能改写真实转换次序。不同键同Tick又是另一种distinct碰撞 | 现行拒绝/重复规则继续；目标是明确首版chord和快速转换支持范围；待选继续拒绝、有限batch或引入可恢复的发生次序 | 身份、timer顺序、输入仲裁、Fact全序各自说明；仅放宽碰撞或仅排序均不足。若发生序号成为语义，审查profile/identity/恢复编码及独立oracle，禁止adapter加Tick |
| TB-04 估计与欠载 | published仍Playing，source consumed已停而device mixed随静音增加；用后者推进H会封存并不存在的歌曲进度 | 现行快照不保证精确声卡位置；目标是音频独立供给和失效可解释；待选有效期、误差及恢复策略 | A2分开source/device/presented estimate与采样时刻；失效、设备切换和迟到旧segment观察不能合并；A3仅把音频owner独立视为待验证方向 |
| TB-05 offset与舍入 | 441源帧/44.1kHz为10ms，误按48kHz换算约9.1875ms；若presented estimate已扣输出延迟，校准再扣一次会移动窗口 | 现行整数校准、checked运算与单次规定舍入保持；目标是可追溯时间映射；待选offset所属环节 | A1逐字段标单位、域、应用次数、负值与溢出；A2独立有理数预期，不复用生产换算函数充当oracle |
| TB-06 无音频/尾判 | 音轨止于1000，合法chart的最后timer在1020；把音轨结束当Gameplay完成漏判。暂停100ms若继续积累host clock又产生假推进 | 无音频显式时钟、暂停不累积；目标是尾段可靠结束；待选音轨结束后的显式时间延续策略与最终H | 分开audio EOF、执行终态、表现排空、归档/导出完成；校验最终horizon的checked范围，失效音频不得静默fallback；归A1/A2、以后A7/F验收 |
| TB-07 暂停释放 | K held→Pause→release→Resume；只保留held可能将未实际持续的按住算入后续body。release再press回到相同最终键态仍不能证明连续性 | 现行Pause保持held、Resume不补release；owner后续已选A reconciliation以继续当前局，具体Hold/尾判语义待定 | 按已选范围补trace缺失/focus、控制回执、Hold/body/tail与Replay预期；原B保留为未选比较项，不作正常场景默认 |

TB-03已只读核对`input_boundary.hpp`的`CanonicalIngressSubject`及`input.cpp`的
`deriveCanonicalSubject`：action与amount属于观测身份，ingressSequence不属于该身份。
因此这里没有把“同通道”误称为“同subject”；也不把现行有意拒绝描述为新发现的算法缺陷。

## 跨条目反例与需补的证明

| ID | 反例 | 影响与处理 |
| --- | --- | --- |
| DR-01 无Fact区间 | 最后Fact在10，健康advance已覆盖100，中间无工作；若按100−10报执行积压，会错误停音。另在11消费Fold due但无新Fact，Fact计数仍不变 | A1分别定义已处理范围与工作/Fact/Fold位置；A2记录frontier和成功阶段，不冻结`committedFactTick`为调度依据。依据profile §3.2、Spec S7A-5补充 |
| DR-02 表现消费与倒退 | T已发布120，执行边界暂在100；把T钳回100会触发`Presentation Tick rewind requires explicit recovery`。Fold在Tick110失败时，kernel已seal的Fact也不能成为成功表现输入 | A1区分允许领先、停止后续采样、显式恢复三类候选，不能普通倒退T；游标只在对应成功发布事务推进，恢复整体替换scope。只读核对`playback_session.cpp`现有拒绝及生命周期回执 |
| DR-03 合并帧与反馈 | 消费Fact后生成一次反馈帧，尚未present就被下一完整帧合并；最新画面仍正确不等于反馈曾显示 | 保留现有RemainingFrames含义；若产品要求首见机会，另裁定具体回执、到期/取消及保留预算。未选择前不能把renderer消费、成功提交和人眼可见混为一个保证 |
| DR-04 晚回执与Close | 旧reload资源准备回执在新scope激活后到达；或Close在主线程join任务，而任务等待主线程释放GPU资源 | 候选须绑定内容/资源/控制版本和实际依赖，过时回执仅清理不激活；Close先解决owner依赖，不能以timeout/detach伪称清理完成。软件事务与已发生设备切换分开报告 |
| DR-05 外部结果寿命 | 调用者持有owning结果R，Session关闭并销毁回收队列后才释放R；若R析构必须回投已销毁队列，则泄漏、悬空或阻塞 | A1/A3须证明CPU结果和回收设施谁保活、最后释放在哪个owner执行；设备资源另守renderer owner。本轮不选实现，仅禁止Close等待所有合法外部结果销毁这一循环依赖 |

上述均是候选首用风险；现有组合API已经区分成功Fold与Runtime发布失败，不能称其缺少全部回执。
RPA应消费这些现有信息，再证明拆域后的映射，不重造一套kernel/Fold或历史格式。

## A2测量设计补齐

此表只定义未来证据要求，没有运行测量或新增工具。测试用数值不升级为生产阈值。

| 场景组 | 固定项与变量 | 必须观察及独立预期 |
| --- | --- | --- |
| 只改渲染节奏 | 固定内容、配置、canonical/admission/control及advance轨迹；只改sample/present | 比完整kernel/Fold/Replay终态；输入捕获、源时钟和工作量不同时不得解释为仅渲染差异 |
| 捕获后延迟交付/延迟捕获 | 两组分开；前者保留捕获Tick，后者捕获Tick可能改变 | 排队时间、准入结果、frontier及拒绝原因；独立late表预期，不要求所有延迟结果相同 |
| 同Tick/控制/无Fact | 人工明确输入、timer、cut及各阶段预期，包含拒绝整批和Fold due无Fact | 记录requested H、成功horizon、processed frontier、失败Tick、Fold/表现游标；单独覆盖空advance，不以最后Fact充当oracle |
| 长历史与外部读者 | 分别增长日志长度、内容活动数、旧结果数量及保留时长 | 推进/发布/query/恢复/关闭/最后释放耗时、分配复制量、活动/日志/读者/设备峰值；既测无读者也测保留读者，不能只把复制移到worker |
| 平台和音频 | 显式配置及源/设备rate、映射段；分别改变GPU等待、欠载和设备切换 | 捕获能力、实际停点、segment/有效性、回执迟到和缺失；独立物理时间证据不可得时标未知，不从Replay相等推导同步精度 |

每份未来run manifest绑定源码提交及dirty差异、构建配置/工具链、profile/revision、场景输入、
硬件、预热与重复规则、采样时钟域和观测开销；报告原始分布与缺失记录，不先指定合格分位数/阈值。
阶段耗时使用可比较的单调时钟；不同owner的设备域时间未校准不能直接相减。
性能trace可按声明丢样，Input/Fact/Replay不因此允许丢弃。正式预算仍归F3实测与owner接受。

## A3五方向纸面比较输入

共同依赖为A1首用语义、A2口径、单kernel/Fold及平台owner限制。音频服务均须脱离GPU等待链；
下列职责安排是比较假设，线程数量未选定。所有方向必须满足完整恢复及owning结果寿命。

| 方向与所有权 | 正常播放；Pause/Resume | Seek/reload；失败与迟到回执 | Close与资源保留 | 成本假设与淘汰条件 |
| --- | --- | --- | --- | --- |
| A 同步模块化会话：执行/表现在同owner，音频单独服务 | 顺序推进与采样；控制在单owner串行停点，等实际音频回执；Resume按选定TB-07 | 同owner准备/切换，仍重验generation；prepare失败留旧组合，设备失败报实际阶段，旧音频回执不得解开新控制 | Session同步清理，audio脱离callback再终止；外部CPU结果独立保活 | 协调面最小，适合作基线；若同owner动画/present阻塞使已定输入/推进目标不能满足，则淘汰为生产主方案 |
| B 执行核心与输出域分离：执行owner；表现/绑定/renderer局部输出域；音频服务 | 可靠成功Fold前缀交付输出域，完整帧可合并；Pause分执行停点、输出停点和音频回执，Resume需前缀一致 | owning准备资源跨域，候选依赖重验，scope/cursor整体切换；发布失败留旧画面，旧资源回执仅清理 | 先关闭准入、解决输出owner工作再join；旧frame/设备使用与外部结果分别保留 | 少于全流水线的控制参与者；需证明慢输出有界保留和主线程输入可行性，不能闭合Prepared拆分或前缀保留则淘汰 |
| C 独立服务流水线：执行、表现、渲染各有状态owner，音频服务 | 各域独立推进，显式连续前缀；每次Pause/Resume收齐必要阶段回执，部分失败关闭相应许可 | 多方candidate和激活点；旧回执按scope/资源/控制版本拒绝，不可逆设备切换不声称全回滚 | owner间依赖有向清理；每条在途消息、读者和设备使用纳入保留图 | 隔离动画最坏耗时，但协调/排队成本最高；若B已达目标且C不能证明必要收益，或关闭状态难以穷尽则淘汰 |
| D 不可变状态驱动表现：执行发布完整可重建状态；表现任务读取快照；renderer独立owner | 采样读取固定版本并可丢弃未发布任务；Pause冻结版本，Resume仍依TB-07，不能从最终键态重建历史 | Seek/reload切换完整可重建状态和资源版本，旧任务完成只回收；若绑定有隐藏未来队列须纳入完整状态 | 任务取消不撤销已发布值；旧快照依读者保留，GPU清理仍属renderer | 可减少活动表现协调；若不能重建去重/聚合/未来效果/寿命，或完整投影复制与保留不可接受则淘汰 |
| E 依赖图任务执行：任务只处理不可变输入/候选；提交仍由指定owner串行 | advance/采样任务显式依赖成功前缀；Pause停止授予新提交许可，Resume恢复合法版本任务 | 取消旧依赖图不代表任务已停；Seek/reload候选最后重验，过时结果仅清理，任务失败不能绕过提交原子性 | 排空需主线程执行的清理任务后join；任务持有资源直到完成/取消确认 | 准备/恢复可并行；若窄任务收益无法覆盖调度/取消复杂度，或演变为通用框架/隐藏屏障则淘汰 |

维护性优先的纸面建议：下一轮优先比较A/B，A用于确定同步整改的成本下限，B用于检验有限拆域能否满足目标；
C在证明动画必须再隔离时重新纳入，D/E先提交局部重建/取消证明。这是审查建议，未淘汰任何方向、
未替owner选型，也不等于A/B已经取得原型准入。特别是SDL主线程捕获限制，A/B/C均不能凭worker自动解决。
ADR0046的部署表用于约束各候选的可行部署，不是与本表并列的第二个架构选择结果。

原型准入卡尚待填写：所消费A1裁定、N3冻结基线及当前差异、A2比较口径、隔离目录/资源范围、
停止条件、产物回退方式和独立实验授权。无界保留、依赖未定语义或违反owner约束时停止对应实验。
本轮没有原型授权，也没有可作为A3最终选型依据的新成本证据；A1–A3均未据此退出。

## 首个owner选择与后续依赖

以下保留本轮向owner提出的TB-07首版范围选项，沿用宿主草案的A/B名称，不与架构A–E混用。

| 选择 | 对K held→Pause→release→Resume的体验 | 代价和仍需落定的内容 |
| --- | --- | --- |
| 恢复候选B：有相关转换/trace缺口则拒绝直接续跑 | 保留暂停与旧结果，说明需显式重启或合法恢复；不自动清空成绩 | 提问时的推荐，owner未选择；仍需真实转换/缺口检测、操作回执及恢复出口，不能只比较最终键态；有体验中断 |
| 恢复候选A：首版支持真实转换reconciliation | 目标是继续当前局，但释放如何计入body/tail必须先定义 | 必须补恢复边界、捕获/准入时间、同Tick/迟到/重复、完整Replay及兼容；单纯批准体验目标还不能冻结算法 |

2026-10-11后续owner答复：“首版支持继续当前局，进一步设计真实松键如何影响 Hold、尾判和 Replay（体验连续，语义与兼容工作更多）”。
据此选择恢复候选A的首版产品范围；B不再作为正常完整轨迹的首版目标。具体计分语义仍open，
选择A不授权本轮实施或改Recovery格式。TB-01–06、DR-01–05和架构选择仍分别开放。

后续已补[TB-07候选细化](../../../../api/realtime-host-boundary.md#tb-07首版范围与候选语义2026-10-11)：
比较暂停点普通release、只作恢复状态对齐、重新按住延续Hold三方向，补control cut、记录顺序、
恢复事务、音频失败、Replay/Seek及最小反例。推荐状态对齐方向尚未裁定，不把继续当前局解释为当前Hold必然免罚。

## 文档修正与验证范围

| 修正 | 依据 | 本轮落点 |
| --- | --- | --- |
| 诊断尚未迁移、SDK当前0.7.1、下一步N1等过时文字 | 当前HEAD版本声明及N3报告 | RPA计划、CURRENT_STATUS；不改N0/N3或8.4原证据 |
| A1仅列主题、未显式要求区分上述易混边界 | TB反例及DR-01–05 | 活动计划补首用证明要求；本报告保留具体分析 |
| N3退出易被读成可直接原型，A2缺跨域/保留测量细项 | 计划原准入顺序与本轮授权 | 活动计划补独立实验授权、隔离/停止条件及测量身份 |
| 宿主草案TB-07表格被空行割断 | Markdown表连续性 | 合并回同一TB表；仅导航/排版，不改候选语义 |
| owner追加要求订正当前SDK0.7.5的过时记录 | canonical版本文件、Reference Host与安装consumer最低请求 | AGENTS、API/构建/版本指南及活动计划；旧阶段/ADR快照明确历史角色，旧最低请求的历史含义保留 |

本轮文档检查通过：415份Markdown、20份candidate JSON/CXT；status/section/target文档合同测试
分别4/5/2项通过，`python -B tools/update_version.py --check`通过（26.10.10-1），`git diff --check`通过。
开工7个既存源码/测试修改文件的SHA-256在交付前逐项核对一致，本轮未修改它们。
未运行C++构建/CTest、Release性能、原型、设备或hosted验证；本报告不新增这些验收结论。
