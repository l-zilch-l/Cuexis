# S7A-3 / S7A-4 execution design ambiguity audit

状态：completed；仅本轮文档复核与修订，不是实现验收

更新日期：2026-10-05

复核基线：stage-7 / `6b11d102f6dcac4d9782cd03d0bae15add54af28`，新增设计仍未提交。
上一版[设计交付报告](../implementation/2026-10-05-s7a-3-4-execution-design.md)是较早快照；
用户要求再次检查后确实发现遗漏，不能继续把上一版“所有已识别选择均闭合”当作无缺陷证明。
本次只修改文档、补合同与验收项，不实施产品代码，不创建提交/推送/PR。

## 1. 复核方法与发现

交叉读取现有 C++ 字段/签名、CXT expansion、Capsule 表、主 Spec、候选补充与交接命令。
源码仅用于核实接口和兼容边界；未执行新 runtime、不把阅读源码称为运行验证。
以下均已在各自 ADR/Spec/ABI/Plan 落点修订，本报告只登记发现及证据导航。

| 项 | 上一版的缺项或矛盾 | 本次明确的落点 |
| --- | --- | --- |
| A01 | advance 中途失败，全调用回滚还是保留已 seal prefix 未明确 | execution §7.3：逐 Tick seal；保留成功 prefix，失败 Tick 不发布；A/P/fault 区分，含空区间与最小 Tick |
| A02 | 多窗口 matcher、死分支、自环、多 atom/ε 重试不够具体 | execution §5.1：单次初始化、单语言步、多分支、最早 accept、跨 gap 保留、ε 只尝试一次 |
| A03 | observe 是否有 phase Miss、何时终止、记录数量不明确 | execution §5.1：Pending/Observed/Expired，独立观察记录、不进入 Ledger、不占 fanout |
| A04 | timer 按 kind 全局执行和按 E 合并提交可能被混为一谈；生成点缺失 | execution §7.1：明确 activation/open/bodyEnd/close/D 构造；执行全局 kind/E，TimerBatch 仅 regroup |
| A05 | query 的既有返回类型无法仅靠返回类型 overload；运行配置对账缺失 | typed §2：保持 Result<JudgementProjection>，kernelView 保活；完整参数/声明核对；显式 engine registry 与运行 identity |
| A06 | 无资源 Hold 可能制造虚拟 lease；相同配置不能区分两个 run；ID 最后值边界不清 | execution §§4,6,7 + typed §1：optional lease/intent、runScope 仅有效性、UINT64_MAX 最后合法再 exhausted |
| A07 | submit 空批、重复/碰撞优先级、pending receipt、admission A/P 记录不明确 | execution §§3,8：空操作、既有码/整批拒绝、两个 admission 游标分别记录；时钟倒退不被 queue 豁免 |
| A08 | CXT source 使用第二套 emissions AST，无法证明现有 Foundation Repeat 等价 | author §3：真实 prototype/pattern expansion；全实例挂接，一个 entity 可多/零 requirement；无第二 AST |
| A09 | E.repeatIndex 与 Foundation marker、affine 零基 index 混淆；entryId 与 ChartId 混淆 | author §3：emit0/repeat(index+1)，rank 减一；entryId 与 GeneratedEntityIdentity.chartId 独立并显式对账 |
| A10 | 现有 CxtV2Loader 拒绝非空 extension，不能直接复用旧入口 | author §1：组合 Reader 先验证原始 root，再分派注册 payload/Foundation view；未知不能删掉，source bytes 用原文 |
| A11 | author leaf/optional/derived/context/unused definitions 与 owning 输出不完整 | author §2：完整 leaf keys、源声明/派生字段分开、Pattern/Measure 未引用定义保留、self-owned AuthorPreparedArtifact |
| A12 | graphRevision 写成 string token，与已有 u64/wire V 不相容；版本矩阵仍列 rev2/历史未决 | Capsule §12 + 主 Spec 版本表 + FORMAT_ENTRY_AND_IDENTITY：typed/wire numeric2，source规范十进制字符串；rev1/2/3；旧 open 标历史 |
| A13 | Tap close→Miss、head terminal新 press、owner update、tail amount、body窗口/D/迟到 release 尚有空白 | execution §§2,4–6：完整状态表与例外；无 tail D=end；body仅覆盖 timer；不能反写已 seal body |
| A14 | ingress journal 的 reserve 不足以证明无分配发布；borrowed/SSO 可能悬空 | typed §2：journal 分配 heap nodes，live vectors reserve，noexcept move 发布；不借 caller view 或假定 hash节点已分配 |
| A15 | CTest -R executable target 名可能只命中脚本而漏 Catch2 cases | handoff §5 + typed §4：实施先注册三组 labels，各组 -N 非零核对，再 labels 单元测试 + 实际脚本名 + 完整矩阵 |

源码证据：JudgementSession 已有 query 返回 JudgementProjection；FrameResolution 实际 enumerator 是
staticDeclaration；graphRevision 是 uint64_t；CanonicalEntityIdentity 的 marker 注释明确 0/index+1；
CxtV2Loader 要求 extension 空；两个现有 Catch2 target 的 catch_discover_tests 没有 TEST_PREFIX/labels。
本报告不以“改文档”声称这些新 fields/labels/Reader/runtime 已实施。

## 2. 修订后的实施入口与边界

唯一顺序入口是 [S7A-3/4 实施交接](../../../../archive/stage-07-planning/s7a-3-4-implementation-handoff.md)，
其链接的 [execution](../../../../formats/gameplay-v2-execution-profile.md)、
[author](../../../../formats/gameplay-v2-author-profile.md)、[typed](../../../../api/gameplay-v2-execution-types.md)
与 [Capsule §12](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) 拥有实际合同。
新增 X-prefix/P-state/A-source/I golden 卡覆盖上述修订；不能以文档检查代替这些产品测试。

本次检查范围内，以上已发现歧义都有选定规则；没有把它们留给实施模型自行选择。
这不是形式证明或“未来不会发现缺陷”的保证：真实反例须回报受影响条款并修合同。
没有生产业务参数默认值；测试构造显式 fixture；合法但从不命中的 Pattern 可 prepare 并 Miss，
不要求实施者自造全局 Hit 可达性 solver。容量/性能阈值与 state-budget 缺测归 S7A-9，
S7A-5/6/7/9、SDK/owner-only gate 与整阶段关闭不转入本范围。

## 3. 本轮验证

完成最终导航与 Desktop 补充后已运行：

| 检查 | 实测结果 |
| --- | --- |
| python -B tools/check_docs.py | exit0；358 Markdown / 20 candidate JSON/CXT |
| python -B tools/check_docs_status_contract_tests.py | exit0；4 tests OK |
| python -B tools/check_docs_target_contract_tests.py | exit0；2 tests OK |
| git diff --check | exit0；仅现有 LF/CRLF 配置提示 |
| 全部 untracked Markdown 显式 whitespace/conflict 扫描 | 9 文件、0 问题 |
| 文档算例复核 | 5 late 边界、6 affine ranks 与 E marker 映射符合列出的期望 |

算例只核对文档公式，不是 L2/kernel runtime 测试。未运行 C++ builds、CTest、hosted、设备、
容量或性能测试；没有新增产品实现证据。集中码表、三组 CTest labels 和 source Schema 仍待实施。
Desktop 接手文档追加本次复核入口；历史文本和历史测试数字保留。本轮没有修改 memory。
