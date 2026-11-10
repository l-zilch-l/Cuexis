# R3 完整 Packed 准备路径精确归因

状态：诊断完成，产品修复未实施。日期：2026-10-27。
返回 [New RPA 报告索引](../README.md)。

## 结论与现场

10K requirements 的主成本是 Reader 重建后调用 `semanticDiff`，在双向线性查找中
为每个候选反复生成显示用 identity 字符串。Writer 的 owner 对照、声明定位以及
通用 diff 的 mergedNamespace 匹配另有独立的平方级扫描。结构语义摘要不是本轮主瓶颈。

产品基线 `f21c2783142198df57eea0ffb2605831faa99239`，唯一工作区/分支
`D:/Cuexis-redesign` / `Cuexis-redesign`。用户先要求调查，随后明确要求打探针精确核查。
产品源码、正式测试源码、普通 Release 测试二进制保持原样；仅在 `out/` 复制 translation
units、加观测并构建独立 exe。此次没有实施产品优化或更改预算、wire、拒绝条件和超时。

前轮大规模诊断保留：`out/seek-scale-r4-20261027/` 中 10K 构建/加载约 54.7 s，
40K 在 600 s 外部时限内未完成准备；`out/seek-scale-typed-20261027/` 则绕过大图
Packed 编解码，测到 40K 运行时 Seek 约 14.8–14.9 ms。两者的边界不能混淆。
本 fixture 的“完整 Packed”是内存中的 source prepare、Packed encode、fromPacked decode
和实际 Host load，不含从磁盘读取大文件的耗时。

## 探针方法与身份

1. `out/packed-probe-20261027/packed_phase.exe` 仅给宿主测试副本加阶段时间戳，
   链接原产品 libraries，对 1K/10K 各运行一次。
2. 同目录 `packed_probe.exe` 另外覆盖 gameplay_capsule.cpp 的诊断副本，加函数级
   inclusive/self 时间和查找深度统计。查找仍执行原谓词；根据返回 iterator 计算扫描元素数，
   没有在每个候选上插入额外计数或替换查找算法。对 1K/10K 各运行一次。
3. `out/packed-probe-deep-20261027/packed_probe.exe` 再覆盖 gameplay_graph.cpp 的
   诊断副本，统计 requirementTable、keyedTable 与显示 identity 生成调用数。对 1K/10K 各一次。
4. 同目录 `packed_no_render.exe` 是单变量诊断消融：仅将 requirement 候选匹配从
   display identity 字符串相等改为原 typed identity 相等，保留两遍线性扫描与全部字段检查，
   对 10K 一次。它不作为产品修复，不证明该变更在所有输入上的诊断语义兼容。
5. `packed_table.exe` 在同一消融副本上记录各通用 diff 表的具体扫描量，对 10K 一次。

共八个新进程、24 次实际 Seek/sample/present；原 fixture 的 misses、sameResult 与成功呈现
断言全部通过。全部相同规模的探针生成完全相同的 Packed 大小及 SHA-256；10K 文件为
1,140,457 bytes，SHA-256 为 `1ebf63cfc9a2305d963c66deb52447835c956fe080d4e2a02ebdc5dc0ac269fc`。
计数公式、源码/二进制/日志身份由 `analysis.json` 独立核验。

使用原 Release `/O2 /Ob2 /DNDEBUG` 参数、正常分配器、observation OFF、mask 4、HighQoS，
一个 CPU 测试呈现对象、无音频；每组先登记固定顺序，首错停止，不重跑替代样本。
所有编译、链接、临时文件和原始证据均留在当前工作区。计时包含探针成本，
不同进程点值有噪声，不能直接视为产品性能接受或精确的优化收益比例。
显示 identity 只计调用数，不逐次读取时钟；该行的 0 ns 表示未计时，不表示其工作没有成本。

## 原产品阶段分解

仅增加外层时间戳、仍链接原产品 libraries 的测量（s）：

| 阶段 | 1K | 10K |
| --- | --- | --- |
| source prepare | 0.038120 | 0.985037 |
| Packed encode | 0.062800 | 4.980739 |
| GameplayContent::fromPacked | 0.485927 | 48.745271 |
| 实际 Host load | 0.009101 | 0.088002 |

编码后的同一 Packed 字节直接作为 decode 输入，没有后台预热或磁盘缓存环节。
这些数据先将主要问题定位为 Reader 路径，不能将 54.7 s 全部记为加载或 SHA 耗时。

## Capsule 层归因

第一组细探针，10K（s，嵌套函数不相加）：

| 路径 | 实测耗时 | 调用 / 扫描 |
| --- | --- | --- |
| encode 总计 | 4.785954 | 1 次 |
| makeModel | 4.564609 | 含下面 owner/声明/validate |
| owner 存在性检查 | 1.828761 | 10,000 次；50,005,000 个候选 |
| requirement→owner 定位 | 1.852380 | 10,000 次；50,005,000 个候选 |
| namedMeasure 内声明查找 | 0.567361 | 30,000 次；150,105,000 个候选 |
| decode 总计 | 45.896618 | 1 次 |
| 其中 reconstruct | 45.172467 | 包含再 prepare 与 semanticDiff |
| reconstruct 内共享 prepare | 0.963312 | 1 次 |
| reconstruct 内 semanticDiff | 44.180965 | 1 次，占 decode 约 96.3% |
| Reader hashModel 整段 | 0.085180 | 包含计数、字段遍历和 SHA |
| Writer hashModel 整段 | 0.087262 | 同上 |

定位：Writer 的 owner 扫描在
[makeModel](../../../../../../engine/gameplay_packed/src/gameplay_capsule.cpp)；
Reader 在 `reconstruct` 中先调用共享 prepare，再调用完整 `semanticDiff` 验证重建前后语义一致。
Writer 和 Reader 的 validate 都存在“声明逐项查 requirement”及 namedMeasure 逐项查声明，
不会因为仅消除主 diff 热点而自行消失。

普通字段字符串/引用字典使用 `lower_bound`，本轮没有证据将其列为线性字典查找主因。
上述实际热点是独立的 owner、声明和 diff 记录匹配。未测全局 new/delete 次数或分域堆峰值，
因此显示字符串调用数不冒充确切分配次数。

## semanticDiff 下钻与消融

深探针 10K：`semanticDiff` 为 49.949804 s，其中 requirementTable 47.467568 s，
通用 keyedTable 合计 2.482208 s。

| 指标 | 1K 原算法 | 10K 原算法 | 10K 去候选字符串生成消融 |
| --- | --- | --- | --- |
| requirement 双向查找次数 | 2,000 | 20,000 | 20,000 |
| requirement 实际候选检查 | 1,001,000 | 100,010,000 | 100,010,000 |
| renderRequirementIdentity 调用 | 1,003,000 | 100,030,000 | 20,000 |
| requirement 查找内部耗时 s | 0.395923 | 47.415160 | 3.494325 |
| 完整 semanticDiff 耗时 s | 0.432583 | 49.949804 | 6.366402 |
| 完整 decode 耗时 s | 0.506910 | 51.775370 | 8.055319 |

代码位置是 `engine/judgement/src/gameplay_graph.cpp` 的 compareRequirementTable
和 renderRequirementIdentity：左表每条在右表线性查找，右表再反查一次，
每个 predicate 都拼接六元 identity 的显示字符串。对本组无差异、唯一对应的 N 条表，
候选检查精确等于 `N*(N+1)`，显示字符串调用等于 `N*N+3*N`，已由 1K/10K 探针核对。
消融保留了一亿次候选检查，只减少重复显示字符串工作，仍保留约 3.49 s 查找及其他成本。
这支持“平方扫描叠加重复字符串生成”这一因果解释，不能把消融副本当作已完成优化。

按表探针另证实：mergedNamespace 有 10,003 行，双向检查 **100,070,012** 个候选，
耗时 **2.399156 s**；其余 12 个通用表合计仅 28 个候选。
这条旁支也会随 requirements 平方增长。

40K 原算法预计 requirement 候选检查为 **1,600,040,000**、显示 identity 调用为
**1,600,120,000**。这是在相同 fixture 形状下由已核对公式得到的推导，
不是本轮新取得的 40K 完整计数或时间；此前 600 s 停止记录保持原结论。
本轮没有为了重复证明而再次运行那个超时组。

## 后续修复的技术顺序

先处理语义比较的匹配方式和显示字符串生成时机，保留逐字段检查与缺失/新增/错误诊断；
再处理 mergedNamespace 通用表匹配和 Writer owner/声明索引。
不能通过删除 semanticDiff、信任调用方输入、降低检查门槛或取消文件摘要来冒充修复。
共享 prepare 和 Host load 是独立剩余成本，本轮没有声称它们已优化。
产品实现尚未修改；R3/R4、设备与生产接受状态不因本诊断改变。

## 原始材料

- `out/packed-probe-20261027/`：阶段/第一组细探针源码、编译参数、链接参数、登记、raw logs、结果。
- `out/packed-probe-deep-20261027/`：深探针、单变量消融、按表探针，各独立登记与原始结果。
- 同目录 `analysis.json`：八进程/24 次结果、Packed 指纹、实际计数公式、产品源码与原二进制身份核验。
- 同目录 `*.patch`：相对产品源码的全部诊断差异，消融仅存在于该目录。
