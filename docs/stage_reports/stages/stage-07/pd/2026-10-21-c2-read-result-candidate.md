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
工具快照、源码库存、host probe、库与全部构建绑定在该 plan 中逐项保存；有限诊断结果待执行后写入。

## Standards

只读复核未发现硬规则违规：新重载留在内部头，Result 被显式处理，前置错误不写输出，成功时完整
重置，输出槽在原同步窗口内消费；未扩大 installed SDK 或所有权边界。无需另加重构。

## Spec

只读复核发现 F1 任意来源绑定问题，已修正固定库存与构建来源；复核另发现负向测试依赖本机历史
产物，已改临时归档并复核。修正后无未处置发现。C2 产品改动未见语义或范围偏离。

审查汇总：Standards 0 项；Spec 2 项 P2 均已处置。本轮不因自检数值通过自动保留 C2，不建立正式
preparation/registration。旧窗口 3 继续停止，原 1.10/15% 及全部生命周期规则不变。
