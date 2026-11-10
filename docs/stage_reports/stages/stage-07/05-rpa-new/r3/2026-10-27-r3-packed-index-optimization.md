# R3 完整 Packed 索引优化与规模复验

日期：2026-10-27。状态：已实施并完成本地核验；R3 未退出。

唯一实施工作区为 `D:/Cuexis-redesign`，分支 `Cuexis-redesign`。恢复点为
`8c3c95266899c865a969bde20f1b9ab11bff0fcc`。本批承接用户继续优化授权及
[精确归因](2026-10-27-r3-packed-prepare-attribution.md)，没有操作其他分支。

## 1. 改动及行为边界

[Graph 比较](../../../../../../engine/judgement/src/gameplay_graph.cpp)以局部索引替代
Requirement 和通用 keyed table 的双向平方扫描。每条匹配键只在建索引或遍历时生成，
索引查找为 O(log n)，整体匹配为 O(n log n)，另加真实字段比较和报告生成成本。
索引继续使用原显示字符串，重复键保留右表首条匹配，报告仍按原左右表顺序输出；
不以 typed key 改写原路径、非规范输入行为或差异内容。

[Capsule 编解码](../../../../../../engine/gameplay_packed/src/gameplay_capsule.cpp)增加
owner 索引、Requirement stableId 多值索引及有序 declaration 二分查找；
named Measure 在每个 Requirement 的 measure 匹配之前计算一次。
Writer 消费 assembler 已 canonical 排序的不可变 Prepared 表；Reader 在任何
declaration 二分前先严格验证物理顺序。stableId 多值索引保留原存在性检查的全部候选。
索引仅在调用期间存在，没有新增缓存、公共 API、依赖或 wire revision。

duplicate、unknown、missing ownership 的拒绝和全部内容/预算/哈希校验继续执行。
新增测试覆盖 owner 顺序不影响编码、三类拒绝消息，以及 257 行表重排、字段改变、
缺失/新增、反向比较和输入不变。

## 2. 完整 Packed 普通进程结果

原始证据在 `D:/Cuexis-redesign/out/packed-opt-20261027/`。
`registration.json` 固定六个新进程，按 33/129 条历史、各 1K/10K/40K 顺序执行。
每进程首次一次及重复两次，六进程共 18 行均有效，exit 0，sameResult 和目标呈现检查通过。
没有重跑或替换样本。

配置为 Release STATIC、正常分配器、observation OFF、source binding、mask=4、HighQoS，
Packed v4；CPU 测试 renderer 只有一个呈现对象、无音频。5 s Host 回复期限与 600 s
外部保护保持原设置。“完整”指内存 source prepare→Packed encode→fromPacked decode→Host load，
不包含磁盘/CXC IO 或真实 GPU/audio，不使用 typed 捷径。准备时间与 Seek 分别入账。

| requirements | 历史记录 | 完整准备 s | 首次请求到呈现 ms | 两次重复 ms |
| --- | --- | --- | --- | --- |
| 1K | 33 | 0.177710 | 0.590 | 0.380 / 0.370 |
| 10K | 33 | 3.010819 | 4.162 | 4.245 / 4.120 |
| 40K | 33 | 27.324168 | 16.458 | 16.253 / 16.489 |
| 1K | 129 | 0.152505 | 0.713 | 0.438 / 0.413 |
| 10K | 129 | 2.896554 | 4.450 | 4.366 / 4.403 |
| 40K | 129 | 27.314148 | 16.163 | 16.473 / 16.012 |

历史普通完整入口的 10K 准备约 54.7 s，本批为 2.90–3.01 s；历史 40K 在 600 s
内未完成，本批约 27.3 s 完成。历史组与本批是独立进程组，这些是本机固定夹具点值，
不提供置信区间、p99 或生产接受结论。原失败及 typed 旁路结果原位保留。

## 3. 独立阶段诊断与剩余成本

`phase-registration.json` / `phase-results.json` 是另外三个新进程。
宿主诊断副本仅加外层时间戳，链接本批优化后的产品 libraries，没有覆盖 Capsule/Graph
translation unit。原登记的 scope 文字沿用前轮探针模板，描述不准确；
`registration-clarification.json` 明确更正解释并保留原登记原字节。
该独立组外部保护为 300 s，Host 期限仍为 5 s；不合并进普通组的 18 行样本。

| requirements | source prepare s | Packed encode s | content decode s | Host load s |
| --- | --- | --- | --- | --- |
| 1K | 0.044139 | 0.030457 | 0.075097 | 0.009875 |
| 10K | 0.981691 | 0.327507 | 1.359015 | 0.090711 |
| 40K | 10.544861 | 1.348472 | 12.263322 | 0.386508 |

前轮同类 10K 阶段为 source prepare 0.985037 s、encode 4.980739 s、
decode 48.745271 s。本批消除了已测的重复匹配主成本，source prepare 基本保持原量级。
40K 目前主要剩余在 source prepare 与包含共享 re-prepare 的 Reader decode；
尚未对这两个阶段内部重新打细探针，不能仅凭源码断言剩余成本的具体函数占比。
下一轮应先测 assemble、resource claims 和 prepare 内部，再决定索引或数据布局改动。

## 4. 兼容与回归

- 1K/10K Packed 大小和 SHA-256 与前轮阶段探针完全一致。10K 为 1,140,457 B，
  SHA-256 `1ebf63cfc9a2305d963c66deb52447835c956fe080d4e2a02ebdc5dc0ac269fc`。
  本批 40K 为 4,779,779 B；此前完整入口没有成功样本，不声称其跨版本字节对照已通过。
- 同一独立驱动链接旧 Graph 目标文件与当前 Graph library，运行 96 个确定性场景，包含
  重排、字段变化、增删、重复显示键和空表。完整差异序列共 207,022 B，两个输出指纹均为
  `4e07de442a8c7d200c6ac882d5f4eaa86bf40bc74492d24ab960c51b01e53728`。
  这是诊断 oracle，普通产品未加入插桩。
- `[redesign-wire]` 通过 1 case / 44 assertions；Replay/Snapshot 两组 wire 指纹与来源绑定
  实验完全一致。
- Release STATIC、Debug STATIC、Release SHARED、Release observation STATIC 均构建并运行
  Core、GameplayPacked、Judgement、RPA、Playback、ExecutionHost、HostProtocol 七套相关测试。
  Release STATIC 分别通过 36/49/228/69/181/61/62 cases。
  SHARED Playback 为 172 passed / 1 skipped；跳过原因是 test-only parse probe 对各 DLL 私有，
  其余相关套件通过；各配置完整断言数见 `audit.json`。
- 最终 Release STATIC/SHARED 全目标增量构建成功。STATIC 架构及已安装 Gameplay 消费者
  2/2 通过；SHARED 同两项加导出面检查 3/3 通过。
- 当前未运行全仓 CTest、Linux sanitizer、真实设备/GPU/audio、prepare-only 峰值堆或
  decoded semantic heap 测量；本批结果不替代这些门。

## 5. 证据与继续边界

`entry.json` / `entry-source.zip` 保存恢复点；`build-*.log`、各配置回归日志、
`final-build-*.log` / `final-gates-*.log` 保存构建和接受工具输出。
`results.json` / `source-*.log` 保存普通六进程结果，`phase-results.json` / `phase-*.log`
保存阶段结果。`diff_oracle.cpp`、`build_oracle.py`、`oracle-*.log` 保存差异对照方法。
`audit_results.py` 复核两份登记源码哈希、测量二进制、raw log 哈希、行数、指纹和回归摘要；
`audit.json`、`final-source.zip`、`final-manifest.json`、`commit-receipt.json` 保存最终身份。

本批可作为下一轮恢复点；完整 40K 准备可完成，但约 27 s 仍有优化空间。
R3c、R4 正式登记、设备、分域内存和生产接受继续按原计划独立执行，不宣布 R3 或 Stage 7 关闭。
