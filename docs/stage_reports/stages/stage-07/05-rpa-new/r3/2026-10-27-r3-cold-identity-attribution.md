# R3 首次 Seek 身份计算与归档体积归因

状态：诊断完成；未修改产品代码，未实施进一步优化。首次 20 ms 门仍未通过，R3 未退出。

日期：2026-10-27。实施工作区为 `D:/Cuexis-redesign`，分支 `Cuexis-redesign`，HEAD
`c4bc5d4456ffe4778a90f9bae410638c0016a8eb`，包含此前未提交重构差异。
返回 [New RPA 报告索引](../README.md)，前一轮见 [重构报告](2026-10-27-r3-redesign.md)，
当前状态以 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准。

## 1. 结论与范围

当前 CPU fixture 的首次 Seek 主要阻塞在 S0 的 canonical archive 身份计算。
内核恢复加速已经取得局部结果，但身份路径仍先展开完整 Replay，做 payload FNV，再做全归档 SHA-256。
2048 requirements / 33 条推进记录对应 47,023,215 bytes：约 95.88% 为每条记录的完整 KernelProjection。
本次分解的首次身份中位数为 322.708 ms，与上一轮约 310 ms 的首次 CPU 宿主请求为相同量级；
两者是不同进程/探针，不能把前者当作新的端到端测量或与后者直接相减。

本轮仅在 `out/` 创建独立诊断源和构建。产品 engine/app、既有测试、此前固定测量控制器和登记保持原字节。
没有修改 wire、身份算法、线程拓扑、预算或接受标准，也没有运行其他分支或工作树。

## 2. 登记和等价检查

第一组为 `out/redesign-cold-20261027/`，构建位于 `out/build/redesign-cold-diagnosis/`。
一个进程按 64、256、2048 requirements 固定顺序运行，每规模生成三个新的封存归档身份。
实际宿主 fixture、33 次推进、CPU renderer、无音频，与前轮宿主诊断共用相同测试源；
探针从宿主 owning Replay 取得内部归档，直接测真实 canonicalIdentity/encodeReplay/SHA/FNV。
分段 clone 只对健康 fixture 对照同样的 Header/body/envelope 编码；普通增长及精确 reserve 两种
诊断副本都与生产 encodeReplay 的完整字节逐个比较，而非仅比较摘要。

所有测量采用 MSVC Release `/O2 /Ob2 /MD`、STATIC 现有产品库、普通分配器、观测宏 OFF，
mask=4、HighQoS。先保存 `registration.json` 的二进制、源码、fixture、编译数据库身份、
固定顺序和首错停止条件，再运行。原始 `raw.log` 保留全部 57 行结构/分段数据；
1 case / 1077 assertions 通过，无失败替补。`result.json` 与 `analysis.json` 分别保存原始结构化行和中位数。

第二组为 `out/redesign-cold-records-20261027/`，构建位于 `out/build/redesign-cold-records-diagnosis/`。
固定 256 requirements，分别执行 1、9、33、129 次真实推进；每规模三个新归档。
同样先登记再运行，保留全部 12 行，1 case / 1184 assertions 通过；
每个归档同时验证精确 SHA，并逐值比较所有结果行的 facts/phases 相等。
两个进程均为有限诊断，不是 R4、设备、生产、p99 或峰值内存接受。

## 3. 成本拆分

第一组各规模三次的中位数，单位 ms。独立 encode 包含 FNV；分段 body/FNV 是字节相同的
诊断副本，不能再与独立 encode 相加。各行分别取中位数，不承诺求和等于首次总数。

| 项目 | 64 | 256 | 2048 |
| --- | --- | --- | --- |
| 首次 canonicalIdentity | 10.067 | 39.579 | 322.708 |
| 同一归档再次取 identity | 时钟分辨率以下 | 0.0002 | 0.0003 |
| 独立完整 encodeReplay（包含 FNV） | 4.349 | 16.558 | 138.231 |
| SHA-256（完整归档） | 5.706 | 22.601 | 181.061 |
| 分段 prepare/结果扫描 | 0.0033 | 0.0031 | 0.0035 |
| 分段 Header/初始缓冲 | 0.069 | 0.113 | 0.658 |
| 分段记录序列化/缓冲增长 | 2.786 | 10.234 | 85.577 |
| 分段 payload FNV | 1.615 | 6.417 | 51.535 |
| 独立归档字节释放 | 0.068 | 0.218 | 1.482 |

2048 下，独立 encode + SHA 为约 319.3 ms，解释了首次身份的绝大多数成本。
准备/索引扫描、归档字节释放不是当前主要阻塞。SHA 当前走 core 中的逐块标量压缩实现；
FNV 对 payload 做另一轮全量扫描，二者在此源上合计约 232.6 ms。

精确 reserve 是诊断消融，不是产品修复：2048 下，分段 encode 从 137.740 ms 降至 118.938 ms，
只相差约 18.8 ms；SHA/FNV 的全量工作仍存在。因此仅调整缓冲容量不足以解决 20 ms 门。
消融顺序固定、样本有限，不将差值推广为正式 allocator 或内存收益。

## 4. 归档为什么放大

33 条记录均有各自的 KernelProjection。内存里的 phases/facts 已共享页面，但当前 wire 的
ReplayRecord 仍序列化 result 全值，KernelProjection 再序列化 phases、facts 等完整字段。
这一路径不消费内存共享页的引用关系，没有 delta 或回指。

| 2048 fixture 的项目 | 字节数或数量 |
| --- | --- |
| 完整 Replay | 47,023,215 bytes |
| 33 个完整结果合计 | 45,083,742 bytes，约 95.88% |
| 单行 facts | 1,059,590 bytes；33 行约占全归档 74.36% |
| 单行 phases | 306,098 bytes；33 行约占全归档 21.48% |
| 不同结果对象 / facts 首页面 / phases 首页面地址 | 33 / 1 / 1 |

首页面地址只证明这一个页面的共享，不冒充整个容器的所有权审计。
第二组进一步直接比较所有结果行的完整 facts/phases 值，全部相等，仍出现以下增长：

| 256 requirements 的记录数 | 完整归档 bytes | 首次身份中位数 |
| --- | --- | --- |
| 1 | 414,359 | 2.520 ms |
| 9 | 1,777,007 | 11.524 ms |
| 33 | 5,864,951 | 38.214 ms |
| 129 | 22,216,727 | 148.399 ms |

该 fixture 即使没有新增 facts，增加历史推进记录也会重复写出相同 facts/phases。
固定结果规模下，其当前序列化工作随记录数增长；分页共享解决内存复制，未改变这项 wire 工作。

## 5. 缓存生命周期与 Player 路径

`ExecutionKernel::archive()` 在活动 RPA 日志没有 seekSource 时，每次创建新 ReplayData、
NativeRecoveryIndex 和 canonicalCache。`ReplayArchive::canonicalIdentity()` 的 call_once
只作用于这一个封存来源的缓存。第一组同一宿主在没有新增推进时重复取新归档，2048 的三个
首次身份仍为 320.276–322.936 ms，而相同归档副本命中后约 0.0003 ms。

Player 的 `player_execution.cpp` 保留 seekHistory，连续在旧历史范围内 Seek 会复用它。
当没有历史，或当前 horizon 超过 seekHistoryHorizon 时，会重新 archive；随后 host 的
execution_host.cpp 在进入后续协议与恢复前同步取 archiveIdentity。
因此此前约 1 ms 的重复 Seek 结果适用于同一已取过身份的历史归档，不能推广为任意新捕获来源。

## 6. 后续技术方向与未做事项

优先审查首次来源身份的生命周期及完整结果重复编码的必要性，再评估 SHA 实现。
任何格式或身份生命周期方案都必须明确精确 canonical SHA、外部来源验证、首次总成本和 owning 释放边界。
不能以移到 Close、隐式预热、改用不同摘要或将缓存命中当首次成本宣告通过。

本轮到归因完成为止：未删除 FNV，未替换 SHA，未改变格式，未把 reserve 消融写入产品。
只需对新报告/索引运行文档与 diff 检查；产品不变，不借本轮重复整套产品构建和回归。
