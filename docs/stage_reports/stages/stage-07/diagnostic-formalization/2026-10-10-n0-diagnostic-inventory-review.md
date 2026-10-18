# RPA-N0 当前诊断码清单与额外命名复核

状态：dated audit；N0静态复核完成，N1合同与兼容策略未定，N2/N3未实施

证据日期：2026-10-10（Asia/Shanghai）。分支：`stage-7`。
代码HEAD：`2ddbcafe22bd3facc63b4917cf99eeccc3d81cd1`。
审计包含开始时已有未提交文档，不将其视为该提交内容；工作区清单与哈希随证据保存。

## 范围与方法

用户授权派子代理完成N0、复核当前清单并寻找其他命名不规范处。本次三个子代理分别检查
逐码清单、额外命名、公共投影与恢复影响，主代理独立核对计数、关键调用、历史ZIP和工作区保留。
不修改产品实现、格式/ABI合同、集中码表或版本，不执行提交、推送、PR或发行。
原有未提交文档迁移全部保留；N0不授权架构产品实施或RPA-F关闭。

以[RPA正式命名工作线](../../../../stage_plans/active/stage-07/plan-rpa.md#正式命名工作线)
为范围，复用[历史审计](2026-10-09-diagnostic-phase-name-audit.md)，读取其中原始ZIP清单，
按当前工作树扫描engine/app/tools/tests/schemas/cmake/docs的1538份受Git跟踪或非忽略文本。
扫描常量及其符号引用、完整字符串、动态前后缀、码表family元数据，并手工核查关键调用。
逐码机器表包含定义/行号、生产者、消费者、类别证据、公开性、测试/合同引用与载体影响。
这属于静态证据，不证明任意运行时外部输入的错误字符串已穷举，不代表新码运行验收。

## 当前逐码结果

| 口径 | 当前结果 |
| --- | --- |
| 阶段诊断定义 | 67：s7a2为18、s7a3为41、s7a4为8 |
| 与历史67项比较 | 无新增、无删除 |
| 既有处置候选 | 66条正式名迁移、1个无调用占位删除 |
| 公开性 | 集中表公共13、明确src-only 13、未登记内部41 |
| 生产状态 | 有活动生产者63、仅声明无活动生产者3、无调用占位辅助函数1 |
| 原66条拟定目标 | 无相互碰撞；加入额外provisional码后有一处目标重合 |

独立完整字面量扫描得到70个token，其中3个是集中表的family元数据
`judgement.s7a2.input` / `late` / `timebase`，不是新增诊断；诊断定义仍为67。
family元数据和前缀分流同样是迁移消费者，不能只改完整code值。
`execution_profile.cpp` / `execution_kernel.cpp`按s7a4前缀加后缀的动态生产路径已纳入。

三个仅声明、无活动生产者的码是：

- `judgement.s7a2.late.queue_hop_exceeded`；
- `judgement.s7a3.grace.quantization_mismatch`；
- `judgement.s7a3.resource.state_transition_invalid`。

它们的声明/码表或注释可描述意图，不能冒称当前有运行路径。是否保留正式常量由N1处理，
不得通过补加行为来制造命名覆盖，也不直接扩大原占位删除范围。

`kSecondHalfPendingCode`仅由无调用的`secondHalfError`引用；对应remediation/section/path
亦为该辅助函数残留。确认可将删除候选限定为函数及其专属token，不把`second_half`升为业务域。
本次没有实际删除。

## 历史清单之外的两个活动命名问题

| ID | 现行码与证据 | 影响与N1输入 |
| --- | --- | --- |
| NN-01 | `judgement.provisional.session.lifecycle_order`；`engine/judgement/src/session.cpp:24,78,331,338,341,375,413` | 仍用于legacy和selected-profile入口，包括submitCanonical、archive、restore及typed seek；含开发状态段。去段后与`s7a4.session.lifecycle_order`拟定目标相同，必须比较完整上下文/错误语义，决定显式统一或按业务原因分开 |
| NN-02 | `judgement.provisional.semantics.unfrozen`；`session.cpp:25,83,139,156,171,181,197,221` | legacy无参动词与无kernel兜底仍输出；当前typed执行已存在，开发阶段描述易误述能力。`judgement.session.legacy_entry_unsupported`仅为候选，需证明覆盖所有触发条件，保留原拒绝及与生命周期错误的区分 |

两者未登记集中公共表，judgement头也不安装；内部Result可观察不等于已注册公共诊断能力。
NN-01沿`invalid_relation/error/faulted=false`，NN-02沿`capability_disabled/error/faulted=false`。
N1需核查完整消费者/可能投影，不能借正式化提升公开性。
扩展复核集合为原67项加这2项，共69个名称处置审查项；它与集中表69个公共码是不同集合。

## 同码多原因与弱命名审查

以下为已有清单的质量问题，不增加独立code数量，也不在N0拆码：

- `judgement.s7a3.grace.chart_supply_not_allowed`：
  `gameplay_assembler.cpp:3870`用于两个竞争grace输入，走`invalid_relation`；
  `:3876`用于不允许chart供给，走`budget_exceeded`。
  前一原因不能由现有名称准确表达。N1比较保留与拆分方案，保留每条路径原类别及拒绝行为。
- `judgement.s7a3.content.unsupported_form`：
  `gameplay_assembler.cpp:381`映射`boundedRelationInstance`，走`non_terminating_source`；
  `:420`为switch兜底，走`capability_disabled`。
  名称宽泛且源码有多类别路径；兜底对生产typed输入的可达性未证明，不冒称运行反例。
- `playback.override.mask`（`engine/playback/src/playback_session.cpp:229`）终段只命名对象、原因较模糊。
  `playback.override.property_mask_rejected`仅为可选建议，不计确认违规、不自动纳入N2。

无调用占位辅助函数接受类别参数，不将其静态多类别意图记作实际运行问题。

## 必须保留的例外与附带残留

| 项目 | 结论 |
| --- | --- |
| `presentation-target-missing` / `partial-group` | Spec明确冻结“不加族前缀、不改拼写”（`GAMEPLAY_V2_SPEC.md:2001`）；形态例外不得自动改名 |
| `identity_closure_incomplete` | 已登记公共投影码；与category同串不等于两层相同，不机械添加前缀 |
| `codec.*`、`graph.*`、`ruleset.*`、`snapshot.*`、`replay.*`、`capability.*`、`input.*`、`format.*` | 既有业务族；两段码不能仅按段数判违规 |
| `playback.prepare.stage.*` | stage表示prepare事务步骤，不是项目编号阶段 |
| `grace.resolver.s2.v1`、`local.close.t4.v1` | 算法identity token，保持原值 |
| `gameplay.execution.t4-k4.v1`等profile/semantic token | 含设计标签但非诊断码，且影响identity/接受检查；不计本轮自动迁移 |
| `material.stage2`等fixture、测试标签、历史报告/ZIP/日志 | 不是活动诊断code；保留原件及历史值 |

附带陈旧注释位于`source_codes.hpp:5,203`、`session.cpp:20`、`diagnostic.cpp:4`、
`tests/judgement/judgement_session_tests.cpp:7`，仍有“码表尚未建立”的表述。
`judgement_session.hpp:83,90,99`的拒绝说明需明确适用legacy动词，避免覆盖typed重载。
这些是必要说明整改候选，不计新诊断、不清除有用的历史provenance。

## 公共投影、恢复与验证影响

- `engine/playback/src/gameplay_internal.hpp:22`按s7a2/s7a3前缀选择投影；5个输入/时基码透传，
  其余按九类别投影并附加`sourceCode`，s7a4直接透传。直接去前缀会绕过原投影。
- faultDiagnostic完整code/message/ordered context/cause进入Recovery wire；payload digest覆盖字节。
  完整Kernel/ReplayEvaluation/Snapshot/Checkpoint比较包含错误链，不只Score/Facts。
- `execution_kernel.cpp:1483`附近faulted Snapshot恢复严格检查旧kernel码或`ruleset.transaction_failed`；
  不可用放宽检查替代兼容决策。
- 当前Recovery envelope revision=1、Snapshot stateSchema=1、eventCodec=`canonical.discrete.le.v1`；
  没有独立diagnostic revision。旧新载体拒绝/独立读取/显式迁移及相应revision留N1裁定。
- PreparedIdentity没有独立code字段；wire revision和semantic/profile revision不能混同。
  无诊断正常归档字节不变尚未实测。
- 集中表验证脚本硬编码旧kernel故障码，严格比较、动态生产者、工具输出和测试预期均需同步。
  机器清单和兼容矩阵列出实际引用；N0不代替消费者运行验证。

## 实际检查与证据入口

运行CMake脚本`cmake/VerifyGameplayDiagnosticsCodes.cmake`，指定当前source和码表，退出0：
69公共码、9类别、3严重度、2故障行为、19拒绝项、20 pending项（13项src-only）。
它证明当前码表内部一致性，不证明命名规范、生产者覆盖或N2/N3正确。

原始历史ZIP与HEAD旧路径文件逐字节相同，SHA256为
`bb95958a0ae2946aca73e5ee8e4dae6f0824d834c7236c317e975daa475f1f63`。
汇总报告生成前重新校验1768项原工作区文件哈希，变化0；后续仅新增本报告/证据及更新索引与N0状态。

[本次机器清单、脚本、命名发现、兼容矩阵与检查日志](2026-10-10-n0-diagnostic-inventory-evidence.zip)
保留原始审计目录结构和SHA256清单。`inventory/inventory.json` / CSV持有67项逐码结果；
`naming/findings.json`持有额外问题/例外；`compatibility/impact-matrix.md`持有逐载体影响；
`verification.json`持有主线程校验结果，baseline文件持有开始时HEAD/status/工作区哈希。

本次未运行C++构建、CTest功能矩阵、安装消费者、字节等价实测、GPU/设备或hosted检查。
文档检查结果另附证据日志；不将其当作新码运行证据。

## N0交付与后续边界

N0当前静态复核已交付。N1需处理：原66条映射、额外两个provisional码及生命周期目标重合、
两处同码多原因、仅声明常量处置、显式公共投影、逐载体兼容与实际版本差异。
本报告所有新增名称均为候选，未冻结；没有预选SDK patch/minor或旧载体接受策略。
N1/N2/N3及RPA整体仍未完成，新架构原型和A4–A6不得以N0报告替代N3退出证据。
