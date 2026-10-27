# R3 运行时来源绑定对照实验

状态：本地候选保留；R3/R4 未退出，设备与生产未接受。

日期：2026-10-27。返回 [New RPA 报告索引](../README.md)。

## 恢复点与变更

恢复提交 `198ab962de36c7d1ca97f4ce96e9f570bf8d4bc7` 固定前一轮 SHA 加速和有界编码器。
本次只在 `D:/Cuexis-redesign`、`Cuexis-redesign` 分支实施，未操作其他分支或 worktree。
用户明确授权提交恢复点、比较不同方案，并撤销效果不佳的新差异。

内置 ExecutionHost 默认使用独立小对象的 owning source token 绑定实际 GameplayReplay storage，
跳过 Seek 中完整 Replay 的 canonical 编码、FNV 和 SHA 扫描。canonical SHA 路径仍可选择。
token 不保留归档历史，不证明内容验证通过，不赋予 imported/public Replay 原生恢复资格。
原有文件校验、wire 字节、cut/H/context/fence 和请求绑定不变。具体语义以
[RPA 合同 §4](../../../../../formats/rpa-execution-contract.md#4-t2-fact-identityreplay-与-seek)为准。

`StepTarget` 及其嵌套 request/proof/receipt 的 candidate C++ 布局改变；消费者必须整套重建。
没有发行 SDK 或宣称旧二进制兼容。安装版外部宿主仍可使用 canonical SHA 绑定。

## 固定对照

登记与 raw logs：`out/seek-binding-20261027/diagnostics-registration.json`、
`diagnostics-result.json` 和四份 `diagnostic-*.log`。
同一 Release STATIC 二进制、正常分配器、observation OFF、mask 4、HighQoS，
一个 CPU 测试呈现对象、无音频；采用真实 advance 与 capture/cut，不预先计算归档 digest。
四个新进程固定顺序为 canonical/33、source/33、source/129、canonical/129，
各进程固定 64/256/2048 requirements，每规模首次及两次重复 Seek，共 36 行。
每次完成实际 Seek、sample 和目标帧 present，并检查与原结果 sameResult。
首次 invalid 即停止，不重跑或替换样本。

首次请求到目标呈现的点值（ms）：

| 历史记录 | requirements | canonical SHA | owning source |
| --- | --- | --- | --- |
| 33 | 64 | 3.266 | 0.380 |
| 33 | 256 | 11.874 | 0.323 |
| 33 | 2048 | 91.809 | 1.016 |
| 129 | 64 | 11.001 | 0.376 |
| 129 | 256 | 42.157 | 0.358 |
| 129 | 2048 | 333.624 | 1.057 |

2048 的重复请求：33 条 canonical 为 0.617/0.553 ms，source 为 0.642/0.658 ms；
129 条 canonical 为 0.889/0.878 ms，source 为 0.799/0.653 ms。
两种路径的 archive 捕获均为 0.019–0.034 ms，cut 在整微秒精度下均为 0；
捕获独立计时并公开，未将 canonical 编码挪到 capture、prepare 或后台。
原有自然锚点的持续记录开销本轮没有单独比较。

保留 source 默认路径：它解决了本 fixture 首次 Seek 的全量序列化扫描。
这些是各规模单个首次样本的描述性对照，没有统计区间、设备呈现或生产尾延迟结论；
本地首次点值小于 20 ms 不等于完整预算接受。历史失败和旧登记保持原样。
主动导出文件时仍需完整编码和校验，本轮没有声称该路径已消除历史长度成本。

## 正确性和兼容性

新增验证覆盖同 storage 副本、新 capture、相同字节 decode、空/双/无绑定、
损坏文件、无效外部 Replay 及 token 不保活历史。
真实 worker 在首次提交前及完成后的同 key 重传拒绝替换来源/错误 cut/双或无绑定，
失败保持 generation，原请求重传仍幂等成功。
observation 配置同时执行两种身份路径，source 路径应无 canonical encode/bytes，
canonical 路径继续验证原相位计数。

普通 Release STATIC、Debug STATIC、Release SHARED、Release observation 四配置完整增量构建通过。
每配置实际运行 Core、Judgement、RPA、Playback、ExecutionHost、HostProtocol 六套相关用例：

| 配置 | Core | Judgement | RPA | Playback | ExecutionHost | HostProtocol |
| --- | --- | --- | --- | --- | --- | --- |
| Release STATIC | 36 | 227 | 69 | 181 | 61 | 62 |
| Debug STATIC | 36 | 227 | 69 | 181 | 61 | 62 |
| Release SHARED | 36 | 227 | 69 | 172 + 1 skipped | 61 | 62 |
| observation STATIC | 36 | 227 | 70 | 181 | 68 | 62 |

所有执行的断言通过；SHARED 的既有跳过项不记为通过。
日志为 `release-*.log`、`debug-*.log`、`shared-*.log`、`observation-*.log`。
Release 最新 Host 反例以 `release-host-adversarial.log` 为准，最新 Playback 信任反例以
`release-playback-final.log` 为准；早期同目录日志保留、不覆盖。
`release-package.log` 的架构与 STATIC 安装版 Gameplay consumer 两项通过；
`shared-package.log` 的架构、SHARED 安装版 Gameplay consumer 与原导出面三项通过。
`wire.log`/`wire-comparison.json` 对照前一恢复点的两种 dialect Replay/Snapshot，
长度和 SHA-256 指纹完全一致。修改文件格式、文档链接和 diff 检查通过，见 `format-check.log`、
`docs-check.log`；`final-manifest.json` 保存最终源码/二进制/日志身份和性能登记一致性核验。

独立只读审阅覆盖 token/ledger/导出/异常/ownership，未发现 Critical/Important 问题；
建议的真实 worker 已完成请求换源重放、双/无绑定反例已补入并通过普通 Release Host。

## 后续边界

后续仍须完成 R3c 语义门、原计划全部首次/容量/生命周期要求与相应退出核验。
长历史导出、真实设备音画同步、生产 profile、40k 分域内存和正式 R4 均未由本实验接受。
