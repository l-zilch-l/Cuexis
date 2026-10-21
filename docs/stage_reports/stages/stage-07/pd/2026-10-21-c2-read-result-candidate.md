# PD-5 C2 单候选验证与交回核验

日期：2026-10-21。当前实施状态由 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 汇总；
冻结范围和验收条件由 [C2 计划](../../../../stage_plans/active/stage-07/pd5-read-result-candidate.md) 拥有。
本报告只承载本轮候选证据，不更改门禁，不是正式测量登记。

## 身份与隔离

本轮从主树 F1 `6014611` 开始，先以 `a87e80d` 留档；字典调查工具/合同证据提交 `bdaa347`，
调查结果与唯一 C2 预登记提交 `020a5ff`，有限诊断工具提交 `827d454`，F1 身份约束修正提交
`e4a1295`。旧 C1 不复用，不追加第二个读取候选。独立的
[Gameplay-host 字典与预算报告](2026-10-21-gameplay-host-string-budget.md) 给出生产准入结论。

C2 产品实现仅在分支 `codex/pd5-c2-read-result`，提交
`747ef18dc8ba3933f0e9b68cee7cbf8a2ee6e2c6`；工作树位于
`out/pd5-c2-20261021-020a5ff/candidate-source`。主树保持 F1 产品实现，未合入 C2。
证据根 `out/pd5-c2-20261021-020a5ff` 保存预登记、原始日志、构建绑定、代码生成与统计输入。
本轮是“一个候选→完成验证→交回核验”，保留决策和任何全新正式组均待后续核验。

F1 历史原始源码库存 `1bfebe4b6e49dcc291136d3d2dec13e64e296341493e3e3d28023afcdecd6da3`
保持固定。新 F1 从 `020a5ff` Git archive 构建，完整库存为
`e019fb974abbe1002534262ab51e69b75b5d31ce1d29ef02ddfc88283e0e0fc0`：归档 LF 与旧 Windows
CRLF、非编译 Python 测试演进使整个库存摘要不同。绑定工具先按硬编码历史摘要校验完整旧库存及
原始封存目录，再将所有产品、构建、资产和 C++ harness 输入逐项比较，仅统一 CRLF/LF、排除
tests 下的 Python 文件；实际编译输入一致，归一化库存摘要为
`7863dfad81342ac1d8da8dc1c39d74a299df6b14620a76b3f9a50da82627b304`。
旧 baseline probe 摘要仍为历史锚点，不冒充本次新二进制。

两套正常 Release 树分别为 `out/build/pd5-c2-020a5ff-f1-normal` 与
`out/build/pd5-c2-020a5ff-c2-normal`，同工具链、STATIC、candidate ON，metrics/trace/lifetime
observer OFF；不启用 IPO/LTCG。配置和逐项构建日志均保留。工具核对 CMake 源目录与传入源码
一致、两构建目录不同，并冻结 exe、library、cache、compile_commands、ninja 与源码摘要。
完整 CTest 使用第三套 `out/build/pd5-c2-020a5ff-verify`，避免分发 gate 重编译冻结测量树。

## 实现与代码生成

改动仅为内部 `readObject(index,ObjectPresentation&) -> Result<void>`、共享 checked 实现、
唯一物化器复用一个中性栈槽和对应风险测试。旧值返回接口保留薄封装；所有 owner/thread、
active/serial、索引、entity、默认字段、资源匹配及错误传播仍执行。未增加缓存、组件 storage
指针、数组、owner 或线程。`candidate.diff` 保存四个文件的可审阅差异。

新正常构建的完整 dumpbin 输出与 `codegen-excerpts.txt` 逐符号保存：F1 物化器 0x135 调用
值返回接口，readObject 在 0x5A0–0x5D0 将 128 bytes 写入 expected payload；C2 物化器 0x1B9
调用输出槽接口，直接写 caller 中性值，成功返回不再复制该 128-byte payload。
C2 在 0x5F9–0x61F 仍初始化 Result<void> 的 Error 存储；输出默认重置与字段读取也仍有成本。
旧兼容封装依然复制 value payload，但本物化器不调用它。代码生成支持所登记机制，不证明收益。

## 验证

独立验证树完整构建通过；runtime、playback、playback allocation、world 四个实际可执行测试
分别运行并全部退出 0，日志及 `affected-results.json` 保留。新增测试验证连续对象的默认重置、
缺组件、前置拒绝时不改输出、窗口失效和暖态零分配。四个修改 C++ 文件 clang-format 检查通过。

测量工具 Python 57 项通过。F1 绑定的负向测试覆盖候选冒充、历史归档改变、构建目录来源错误；
测试用临时归档独立构造，不依赖本机 out。工具初次审查发现只记录来源不足以固定 F1，已在绑定
和采样前修正；没有替换原生测量样本。

独立完整 Release CTest 退出 0：1273 项中 1272 通过、1 项 Windows symlink 能力相关跳过，
0 失败，耗时 849.03 秒。架构、外部消费、安装、reference host、Player 分发与版本合同门禁通过。
`release-ctest.log` 和 `validation-summary.json` 保留实际结果，没有把跳过项说成已执行通过。

采样前绑定 plan.json SHA256 为
`63ad989f39e3ee086478ea299790e8ccd7366c205b244a34abd4eba84661df49`，formal=false、pending_review=true；
预登记方案摘要仍为 `09411990ae0cb2db35f3f80ad18f01dc919c78997c74323656d4ebeb13a11587`。
F1 / C2 baseline probe 新摘要分别为
`c17f8ddbfcf99b840111f5f14da214edd75484cd3309fb05644c82199e80287f` /
`dc1ec17d39b8015047f18bcc411679923f63e9cd2121d74cf8feb127c2befcb7`。
工具快照、源码库存、host probe、库与全部构建绑定在该 plan 中逐项保存。

## 有限诊断结果

12 个固定被测原生进程全部完成，另各有一次独立 30 秒 CPU 热身；无重试、替换或额外进程。
六个 scale、六个 host 的原始日志及 process.json 保存 native PID/QPC、log SHA、begin/end
PD_ENV 与退出状态。每对相同内容/序列、viewport、对象/相机、U/F/R 及输出摘要核对通过。
read 块满足 100 ms 下界；内部三个块不增加自由度，每进程取中位后只有三对观察值。
`analysis.json`、九份 input.json、host-endpoints.json 保留完整复算输入。

下表均为 C2/F1，95% 配对 log-ratio 区间；重复性按已接受定义分侧计算。A=F1，B=C2。

| 指标 | 几何比值 | 95% 区间 | F1 重复性 | C2 重复性 |
| --- | --- | --- | --- | --- |
| read1 | 0.9285 | 0.6229–1.3841 | 1.95% | 25.19% |
| updated1 | 0.9080 | 0.5967–1.3816 | 0.10% | 26.32% |
| read8 | 1.1541 | 0.5937–2.2437 | 2.98% | 43.42% |
| updated8 | 1.0993 | 0.6357–1.9011 | 4.72% | 41.92% |
| read64 | 1.0214 | 0.6515–1.6011 | 0.74% | 38.12% |
| updated64 | 0.9638 | 0.8915–1.0419 | 28.66% | 35.41% |
| read256 | 0.9063 | 0.8417–0.9758 | 42.03% | 35.82% |
| updated256 | 0.9220 | 0.8099–1.0497 | 49.40% | 36.65% |
| request→ready | 0.9305 | 0.8394–1.0316 | 1.63% | 7.72% |

主指标 read64 的三对进程中位为 F1 4.160693/4.165194/4.134380 µs，C2
3.865813/5.241358/3.767729 µs；其区间上界 1.6011 不满足 ≤0.95。
全部指标上界 ≤1.10、全部分侧重复性 ≤15% 两项也均不成立。256 对象点估计及公共 ready
不能代替预声明主指标；scale 各项按重复性或区间判不可接受/不可判定，未证明成本下限或确定回归。
`self_check_met=false`、三项 criteria 均 false；执行 errors 为空。

host 的每进程 1024 操作分别保留分布：F1 ready 中位 52.05/52.90/52.30 µs，C2
50.50/49.20/46.70 µs；p99 为 F1 95.8/104.8/103.3 µs、C2 101.6/101.1/101.2 µs。
这些是短轨迹的操作分布，未合并成 SDK p99；read/updated 表中的时间是块平均。
ready→present-return、request→present-return、present-call→return 在 host-endpoints.json
分别保存；旧 ACK 定义不改，enqueue/drain/GPU 未具备边界，不由当前结果填补。

环境为 Ryzen 7 8845H，8 核/16 逻辑处理器，Windows 10.0.26300；scale mask=4，host mask=65535。
采样前后电源方案 GUID 均为 381b4222-f694-41f0-9685-ff5bb260df2e，允许 CPU 集合相同。
后台进程快照分别 302/294 项；原生 PID、affinity 与起止快照核对通过。powercfg/CIM 的中文标签
在 UTF-8 解码时有替换字符，GUID、CPU、版本与数值字段仍可辨认；快照不构成连续温度、频率或
竞争跟踪，不将本轮高噪声归因于特定硬件/后台进程，也不为缩窄区间补采样。

本轮 C2 未达到保留所需的预声明自检条件，候选完整差异保留在独立分支供核验，主树不合入。
按用户要求先交回身份、结果和门禁核验，不自动决定保留/撤回，不登记 C3，不开始正式组。
正式接受仍未完成；旧窗口 3 保持停止，当前有限 plan 不能用于正式 preparation/registration。

## Standards

只读复核未发现硬规则违规：新重载留在内部头，Result 被显式处理，前置错误不写输出，成功时完整
重置，输出槽在原同步窗口内消费；未扩大 installed SDK 或所有权边界。无需另加重构。

## Spec

只读复核发现 F1 任意来源绑定问题，已修正固定库存与构建来源；复核另发现负向测试依赖本机历史
产物，已改临时归档并复核。修正后无未处置发现。C2 产品改动未见语义或范围偏离。

审查汇总：Standards 0 项；Spec 2 项 P2 均已处置。本轮不因自检数值通过自动保留 C2，不建立正式
preparation/registration。旧窗口 3 继续停止，原 1.10/15% 及全部生命周期规则不变。

## 交回材料

证据根的 `final-handoff.json` 保存最终主树/候选 SHA、干净工作区、冻结身份、历史校验与提交列表；
`final-integrity.py` 只读核对冻结工具/源码/二进制、前轮 3167 项证据包、四个保护根 1184 项及
既有暂停/构建绑定，不运行任何 probe 或正式登记。主树产品编译输入仍须匹配固定 F1。

`handoff-evidence.zip` / `handoff-receipt.json` 是本轮新增审阅包及 ZIP 摘要；解压后的
`verify_packet.py` 按 evidence-index.json 逐项核对。包内包括两个报告、冻结方案、完整原始日志与
统计输入、工具快照、候选 diff/源码、新正常 probe、构建元数据、反汇编及被审计 object、字典
调查的隔离源码/插桩差异和历史 F1 原始归档。完整构建目录仍按 plan 的绝对路径与摘要保留。
包的完整性通过不表示性能验收或 Stage 7 退出；保留与正式组后续动作等待身份/结果/门禁核验。
