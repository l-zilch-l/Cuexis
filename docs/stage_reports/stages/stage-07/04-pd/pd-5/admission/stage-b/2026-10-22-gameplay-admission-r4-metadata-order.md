# PD-5 阶段 B：R4 metadata 拒绝顺序修复

状态：本轮实施与本地验证完成，交回 owner 核验；阶段 C 未开始。

R4 原先在 shared prepare 之后才比较 metadata，后段错误会遮住 metadata 错误。
本轮将 encoding/Revision 检查前移到外层，并把 semantic metadata 检查置于 Header 摘要
校验与 reconstruct 之间；失败时不进入重建或后续 prepare。

## 范围与固定源码

本轮由 owner 指定从隔离分支 `codex/pd5-gameplay-40k-admission` 的
`8308f36a2ded994160c4ae08ed3c44c637e4d74f` 开始，只修复阶段 B 的 R4 metadata
顺序。开始前留档 `12f14b9`；实施源码 `7ccccf994aecee64244ef4449c00037530ce2c04`。
未扩大 revision 4 的实现范围；旧报告作为其时点记录保留。
合同以 [Capsule §13](../../../../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) 和
[Gameplay Entry](../../../../../../../formats/CHART_ENTRY_V1_FORMAT.md) 为准；
前轮证据见 [R4 入口接入](2026-10-22-gameplay-admission-revision4-entries.md)。

## 修复后的调用关系与兼容边界

`PlaybackSource::fromGameplayEntry` 先保留原 metadata 解析、路径及 exact artifactIdentity
检查，再在涉及 R4 的 packed-chart 组合上比较 encoding 与 Header revision。
Header 4 + capsule.3、Header 3 + capsule.4 均在外层拒绝；Graph.1 不走此分支。
合法 capsule.3 / Header 3 保留原处理顺序。

R4 入口同步借用已解析 metadata 中的预期 compiledSemanticIdentity，经私有
`fromGameplayEncoded` / `gameplayContentFromPacked` 传至不安装的 DecodeContext；
原公共 GameplayContent::fromPacked 复用同一私有函数，不提供 metadata 期望值。
公共 factory 签名、对象布局、SDK 版本和 wire 均未改变。

核心调用顺序为：物理/Header/directory/CRC/七预算 → 有界 typed decode/结构/闭包 →
预算 count/check → 单次流式
SHA → Header semanticIdentity 比对 → R4 metadata compiledSemanticIdentity 比对 →
reconstruct → prepareResolvedGameplay → Judgement prepare → 原剩余完整 metadata 闭包比较
与 source 发布。预算、Header 和 metadata 三个边界不会互相绕过。

metadata 比较直接读取本次已验证 digest 的 nibble，借用期望文本；不生成额外 hex string、
preimage、模型副本或动态 hash workspace，不再求语义 SHA。固定 view/比较控制局部量由既有
65536-byte 控制预留覆盖，四维上限不变。期望 view 仅在同步调用期间有效，不存入返回 content。
Header 与 metadata 继续复用同一 verifiedSemanticDigest。

新 packed.metadata.mismatch 仅用于该 R4 解码边界，Playback 适配为既有
playback.gameplay.invalid / identity_closure_incomplete / error / faulted=false，保留
metadata.compiledSemanticIdentity fieldPath 与 sourceCode。旧 revision 2/3 忽略该内部期望值；
旧 revision、Graph.1 的语义路径、拒绝行为及诊断不改写。

## 双故障与回归证据

原实现先添加公开入口回归，确认 semantic identity 和 encoding 两项错误均被真正的
capability.disabled shared prepare 错误遮住（red 日志四项断言失败）。修复后：

| 情况 | 结果 |
| --- | --- |
| 有效 R4 metadata/配置 | 成功，语义 SHA=1，reconstruct/shared prepare 各进入一次 |
| R4 semantic metadata 错误 + capability-disabled prepare | metadata 错误优先；SHA=1，reconstruct=0，sharedPrepareCalls=0 |
| R4 encoding 错误 + capability-disabled prepare | 外层 metadata encoding/Header 错误优先；调用前返回 |
| 移除 metadata 错误，保留 capability-disabled | 确实进入 shared prepare 并失败，证明第二故障有效 |
| artifactIdentity 错误 + encoding 错误 | exact artifact hash 错误仍优先 |
| CRC 合法的错误 Header digest + 错误 metadata + prepare 故障 | Header semantic identity 错误优先，未重建 |
| 收紧 preimage 到 0 + 错误 metadata | 预算先拒绝，SHA=0，未重建 |
| revision 3 / Graph.1 metadata 错误 + prepare 故障 | 仍返回原先的 prepare 错误；Graph.1 单 metadata 错误仍为原 closure mismatch |

小规模有效 R4 的预期值检查和无 metadata 解码拥有相同 allocationCount、allocationBytes、
scratchHighWater、shaBytes 及缓存 digest；动态存活计数归零。测试观测是每次操作私有的两个
计数器，分别位于实际 reconstruct 入口和 shared prepare 调用前，无全局 hook/cache。

最终固定源码的全新 Release 构建启用 STATIC、warnings-as-errors、Chart-v5 candidate，
PD metrics 与 trace harness 均关闭；使用记录在证据中的 MSVC showIncludes override，
Ninja 的 Capsule object 依赖数为 75，包含实际私有头文件。

| Release 回归 | 用例 | 断言 | 结果 |
| --- | ---: | ---: | --- |
| Gameplay packed | 45 | 5476 | 通过 |
| Playback | 174 | 27975 | 通过 |
| Chart | 227 | 42425 | 通过 |
| Judgement | 216 | 35624 | 通过 |
| CXC | 64 | 975 | 通过 |
| 合计 | 726 | 112475 | 通过 |

## 原样六配置 wire-only 与最终检查

保持原样 `1/64/40000 × low/high`，只运行原有隐藏 physical-envelope 测试；
344 条断言通过。全部七项物理预算通过，完整摘要、preimage bytes、所有 section sizing/
record count 和 CRC 与冻结 Stage A 相符；六个 wire 文件均与上轮 R4 原件逐字节相同。

| 对象/reuse | file bytes | decoded bytes | 最大 section | strings/refs | preimage bytes | Writer/Reader scratch | SHA W/R |
| --- | ---: | ---: | ---: | --- | ---: | --- | --- |
| 1/low | 2295 | 1751 | 1213 | 49/13 | 4296 | 66086/66086 | 1/1 |
| 1/high | 2295 | 1751 | 1213 | 49/13 | 4296 | 66086/66086 | 1/1 |
| 64/low | 9962 | 9418 | 4092 | 112/13 | 158979 | 77237/77237 | 1/1 |
| 64/high | 9954 | 9410 | 4084 | 112/13 | 158979 | 77237/77237 | 1/1 |
| 40000/low | 6818424 | 6817880 | 3725942 | 40048/13 | 99588531 | 7145909/7145909 | 1/1 |
| 40000/high | 6746664 | 6746120 | 3654182 | 40048/13 | 99588531 | 7145909/7145909 | 1/1 |

两种 40k 完整语义摘要：

- low：`78c23507481084ba7aa900efd56f72065992d8ad2409f657fcf194784252038d`
- high：`e41eccde1593ca70b4b9c17696827f36b8884523d66e6c0ecb111a92ffc1cfcf`

架构 CTest 1/1、全仓 cuexis_format_check 均通过。本报告和索引/当前状态写入后，
check_docs.py（478 Markdown / 20 candidate JSON/CXT）、git diff --check 及
git diff 8308f36 --check 均通过；原始结果见 docs-final.log / diff-check-final.log。

绑定在运行前封存，运行后复核：2041 个源码文件、29 个构建载荷均未变化；
六组冻结目录的 6506 个文件及主树 F1 未变化、主树保持干净。最终交回提交只增加/修改
本报告、PD 索引和 CURRENT_STATUS；与固定实施源码的差异由 receipt 的 docs-only 清单约束。

owner 询问耗时后，按原件时间戳进行了只读状态对照：上轮 low 约 21 分 57 秒、
high 约 22 分 31 秒；本轮 low 约 21 分 43 秒、high 约 21 分 22 秒。
这不是正式性能采样或门禁通过结果；测试包含造数、encode/decode/reconstruct 和 shared prepare，
未定位主要热点，不将耗时归因于 SHA，也不据此认定 40k 读取/prepare 性能正常。

既有小规模 Release 自动化回归不作为阶段 C 功能验收；六配置专项仅离线 wire。

## 独立静态复核

### Standards

全部九个改动文件按 AGENTS.md、CODE_POLICY.md、模块和 SDK 边界只读审阅，无可行动问题。
借用 lifetime、内部 API 归属、错误适配及操作级观测符合原约束，无需修正的具体 smell。

### Spec

按 owner 本轮要求及 Capsule §13 / Gameplay Entry 合同只读审阅，无实施偏差。
未把静态审阅当作实际 Release、六配置或绑定证据。

两轴结果：Standards 0，Spec 0。

## 原始材料与停止边界

新证据目录：`out/pd5-r4-metadata-order-20261022-8308f36/`。包含实施 diff、red/green、
最终五组 Release 日志、独立审阅、源码原始字节 zip/inventory、二进制/build 配置 zip/绑定、
冻结清单、旧 fixture/golden/预算比对及六配置新 wire 原件和验证脚本。

范围审计确认仅九个授权源码/测试文件改变；189 个 fixture/golden/预算/格式/原样矩阵文件
与上轮源字节清单完全相同，且 Git 基线未修改这些文件。初版审计工具把原始 CRLF 与 Git
规范化 LF blob 直接比较而报错，已改为对封存 raw-byte 清单核对；产品及证据未因此变更。
另有开发单测曾把内部诊断预期写为公共适配后的 capability.disabled，已修正测试预期为
judgement.closure.capability_disabled，最终独立构建通过；原始失败日志保留。

四上限维持 134217728 / 1000000 / 4096 / 8388608，七项物理预算及旧 golden 不变。
本轮不进入阶段 C、不运行 40k Host/Snapshot/Seek/Replay 功能验收、不建立正式测量登记、
不合并主树；旧窗口 3 继续停止。主树固定 F1，等待 owner 核验后再决定后续。
