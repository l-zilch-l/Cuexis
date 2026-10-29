# Runtime 外观属性重复查找优化

状态：保留局部优化；R3 未退出，正式及设备接受独立

日期：2026-10-29

返回 [RPA 报告索引](../README.md)。当前状态以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准。前置数据见
[1080P 实体容量测试](2026-10-29-r3-1080p-entity-capacity.md)。

## 根因与候选

本轮承接桌面接手文档 P3，起点为 `b10a8fa`，工作树干净。
在独立临时 Runtime translation unit 中，将 `updatePrepared` 分成互斥阶段；
计时不进入产品源码，不在逐实体循环中插时钟。40K 静态进程含准备及 warmup 的 66 次调用中，
整个函数均值为 11.373 ms，其中读取/复制 resolved presentation 为 10.847 ms，约占 95.4%；
Transform plan 为 0.024 ms。它解释本夹具的 update 热点，不外推所有负载。

`applyResolvedPresentation` 对每个外观连续调用四次 `resolvedValuePtr`，每次重新二分同一
PropertyResolver 实体表。候选通过现有私有 `ResolverAccess` 一次定位实体，返回 visible、
material、opacity、tint 四个 candidate 指针；立即按原类型判断消费，材质仍复制到 owning 字符串。
缺实体/缺属性仍返回 null，错误类型仍按原回退，不增加跨帧缓存、World 访问或新物化路径。
公开安装 API、SDK 0.9.1、序列化与身份算法均未改变。

维护成本为一个内部批量查询函数；借用仅在 resolver 未变更时有效，没有保存到 Runtime 状态。
组件存在性检查、事务提交、Transform 失效及 Snapshot 保活合同保持原流程。

## 普通构建固定对照

证据根：`out/verification-20261029/runtime-presentation-v1/`。
沿用 fixture revision 3，RTX 4060 Laptop、1920×1080、VSync OFF，静态实体共享三角形 mesh / material，
每帧 update / extract / submit / present；普通分配器，metrics / GPU timestamp OFF。
测量使用默认 CPU mask 65535、BelowNormal、执行节流 OFF；至少 8 帧且 2 秒 warmup。
构建使用独立对象、单任务、affinity 3 / BelowNormal，既有已封存构建和证据只读。

预先固定 1K / 10K / 40K，各三对 A/B，新进程顺序 AB、BA、AB，每进程 120 测量帧。
共 18 个有效进程、2160 帧；非法进程即停止，预算失败及慢值保留，不追加样本。
下表取三个进程各自平均时间的中位数，不是整批合并帧的 p99。

| 实体数 | A update | B update | update 变化 | A 完整 cycle | B 完整 cycle | cycle 变化 |
| --- | --- | --- | --- | --- | --- | --- |
| 1K | 0.185665 ms | 0.082488 ms | -55.57% | 1.774661 ms | 1.657855 ms | -6.58% |
| 10K | 2.942098 ms | 1.439510 ms | -51.07% | 17.753841 ms | 16.373975 ms | -7.77% |
| 40K | 11.413630 ms | 5.554907 ms | -51.33% | 74.064850 ms | 68.292137 ms | -7.79% |

三个规模 update 的每对结果均改善，完整 cycle 未见对应退化，因此保留候选。
1K B update 的相对极差为 18.76%，超过原 15% 门；10K / 40K B 分别为 5.42% / 0.43%。
这不是正式重复性或 R4 接受。40K B 的完整 cycle p99 为约 73.6–77.2 ms，仍远超 4.167 ms，
本轮没有重测稳定容量，不能更新之前 1800 / 1300 的已确认容量点。

## 功能与证据核验

新 World 用例覆盖全有、全无、部分属性、错误类型基线、256 字节材质、层写入、beginFrame 回退和
拒绝非法 opacity 后的值，逐项与原 accessor 的 candidate 地址和 null 结果对照。
无上下文 Luna Max 审阅未发现实现缺陷，提出的稀疏/错误类型覆盖已补齐。

| 普通 MSVC Release STATIC 定向套件 | cases / assertions |
| --- | --- |
| World | 26 / 498 |
| Runtime | 53 / 11,591 |
| Playback | 184 / 34,747 |
| Host | 61 / 42,306 |

World / Runtime 测试 translation units 重新编译；Playback / Host 使用此前最终 B 的不变测试对象，
显式链接本轮新 Runtime / World 对象与只读 SDK 0.9.1 静态库，绑定记录逐个保留输入哈希。
本轮不是 fresh 全矩阵，也不宣称 Debug、SHARED、MinGW 或安装消费者重新通过。
架构、修改文件格式和文档检查另存日志；`audit.py` 重算 raw samples、日志哈希、统计及旧 GPU
构建/证据哈希。原始对照、源码及二进制绑定见 `comparison-plan.json`、`comparison-results.json`、
`comparison-summary.json`、`profile/`、`batch/`、`regression-v2/` 和 `audit.json`。

初次测试编译因 `entt::null_t` 与实体枚举混合 initializer list 推导失败，保留 `regression/` 日志；
显式转换后在新目录构建，没有删除失败。归因组中的 animated 设备进程通过 18 assertions，
控制器却复用了 static 的 `moved_entities == 0` 条件而标无效；保留其原 verdict，排除于 A/B。
其原始分段只作为下一步调查线索，不作普通性能结论，也未重跑原组。

## 剩余工作

P3 的静态 update 仍约 5.55 ms / 40K，extract 和正常 OpenGL submit 仍有显著成本。
动画进程的 resolver layers 占主要成本，源码 `markTouched` 每条写入线性扫描此前实体，
下一候选应独立验证成员资格记录能否消除平方增长，并保留多属性/多层、错误首序与失败回退。
本报告不关闭 P1/P2/P4/P5/P6，旧正式窗口、原门限、40K 内存和设备恢复缺口仍按原登记执行。
