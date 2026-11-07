# R3 哈希实现与编码器恢复点

状态：本地候选，首次 Seek 总预算未通过。

日期：2026-10-27。返回 [New RPA 报告索引](../README.md)。
前置诊断见 [首次身份归因](2026-10-27-r3-cold-identity-attribution.md)。

## 改动与来源

用户授权先提交恢复点，再引入开源哈希实现和继续重构编码器。
第一个恢复点为 `855829a09ade235b88bcf92cf33630dcb261ef0e`。
仅操作 `D:/Cuexis-redesign`、分支 `Cuexis-redesign`。

SHA-256 压缩采用固定版本 SHA-Intrinsics，运行时核对 SHA、SSSE3、SSE4.1 后调用，
保留 portable 回退和禁用加速构建。FNV-1a 采用固定版本 lcn2/fnv 的 64-bit multiply 分支，
顺序展开循环；算法、offset basis、prime、wire revision 与摘要均不变。
来源、原文件 SHA-256、适配说明和许可位于 `third_party/`，依赖登记见
[依赖政策](../../../../../guides/DEPENDENCY_POLICY.md)，两项许可随包安装。

Writer 缓存不可变 SharedSequence 的编码字节并保持来源 owner。
缓存自己持有字节，避免输出清空或改写后引用旧输出区间；缓存上限 8 MiB、128 个序列，
达到上限走普通编码。两遍遍历使用同一 fields/revision，先测量精确尺寸，再预留输出容量。
计数与容量运算均检查表示范围。

## 已验证与限制

提交前重新运行普通 Release：Core 36、Judgement 227、RPA 69、Playback 180、Host 61，
共 573 个用例通过，日志为 `out/hash-opt-20261027/checkpoint-*.log`。
覆盖 SHA 硬件/portable 对照、未对齐和流式分割、FNV 原递推及 continuation、
Writer 分离写入、输出清空/改写后复用、缓存容量回退和测量长度。
`portable-test.log` 的独立禁用加速 Core 36 个用例通过。
legacy/RPA replay 和 snapshot wire 的长度及 SHA 指纹一致，见 `sized-wire.log`。

第一版固定登记为 `out/hash-perf-v1-20261027/diagnostics-registration.json`：
2048 requirements 的 CPU 宿主首次请求到呈现从上一轮约 310 ms 降至 101.443 ms；
256 为 13.043 ms，64 为 3.661 ms，重复约 0.2–1.0 ms。
独立成本登记 `out/hash-cold-v1-20261027/registration.json`：2048 的 SHA 约 26 ms、
序列化主体约 19 ms，FNV 仍约 52 ms。该身份早于精确尺寸和最终缓存修复，
不复用为本恢复点完整性能证明，也不是 R4/设备/生产接受。

Debug、SHARED、observation 的增量构建完成；尚未完成这些配置的最终回归、
新实现的最终导出/消费者矩阵及精确尺寸后的端到端复测，不沿用前一轮接受结果。

## 下一实验

用户允许提交当前状态后比较不同方案，收益或正确性不佳可在当前工作区撤销新差异。
接下来比较 canonical 内容摘要绑定和运行时不可变来源绑定，分别测首次请求和长历史成本，
核验错误来源/cut/代次、外部导入验证及 owning 生命周期。
来源身份不等于数据验证通过，也不改变文件摘要合同。
