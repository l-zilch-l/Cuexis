# R3 Seek 发布归因与批量 Fact 候选处置

状态：批量候选未选用，继续定位 H5；不是接受结果

日期：2026-10-30

返回 [RPA 报告索引](../README.md)。当前状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 边界归因

以未选用 R3c v4 为基线，独立插桩实际调用边界。每组固定 1K/40K 各两个进程，每进程原三次
请求；QPC wall、thread cycles 和 GetThreadTimes 同时记录。后者粒度很粗，0 不代表没有 CPU
工作。插桩结果不能作为普通性能接受；没有修改旧固定组。

40K H902 的第二版归因中位数：kernel Seek 86.8 µs，fork 91.25 µs，query 0.7 µs，
发布投影 2213.15 µs，重建 realtime 8.65 µs，worker dispatch 2354.9 µs。
主要成本在投影读取，不能归因为固定等待或调度器重建。

单独登记的 H5 组表现不同：40K advanceRecorded 12595.95 µs，kernel Seek 12624.1 µs，
发布仅 37.15 µs，dispatch 16086.45 µs。嵌套区域不相加；两区域之外也有释放成本。
H5 会执行首次 activation，改写大量 Dormant phase；原只按 Facts 数量触发的可选续点没有保存
这段无 Fact 的状态工作。是否捕获 activation 状态，需要重新验证 writer 成本和多保留的 phase 页。

## 批量候选与普通对照

候选新增非安装桥 `readFacts()`：每批校验 owner/lifetime/epoch，最多复制64个借用描述符到
Playback 本次调用的栈缓冲；原适配器继续检查每条 Fact 的 kind、phase、outcome，空 bindings
也不跳过。没有缓存跨调用的借用或改变公开 API / wire。

新目录 `publication-build-v2` 全新 Release STATIC 构建，四相关套件共429 cases通过：
RPA 79、Judgement 237、Host 62、Packed 51；架构和独立/嵌入头依赖 CTest 2/2通过。
首版配置漏开 candidate 导致目标不存在，尚未编译；失败目录保留，v2只修正配置。

普通组固定20个进程：1K H902、40K H5、40K H902 各三对 AB/BA/AB，1K final一对。
同一 fresh 头/库定义，baseline仅恢复单条 callback，双方都包含v4 kernel。mask4、BelowNormal、
execution throttling OFF 逐进程核对；构建与测量串行。所有功能、目标Snapshot raw全字节/hash一致。

| 目标 | baseline中位数 | candidate中位数 | 变化 |
| --- | --- | --- | --- |
| 1K H902 | 0.273 ms | 0.261 ms | -4.40% |
| 40K H5 | 16.396 ms | 17.728 ms | +8.12% |
| 40K H902 | 2.263 ms | 1.520 ms | -32.83% |
| 1K final | 0.328 ms | 0.346 ms | +5.49% |

所有候选请求仍在20ms内，但两个保护项超出5%，1K H902的双方相对范围约29%，超过15%。
**本版不选用，不补样到通过。** H5的变化尚不能由批量读取解释为确定因果，按未归因保留。
只读审查没有发现确定实现缺陷，但要求晋升前补真实64/128边界、非零起点、跨批失效和第二批
错误时的事务回退集成测试；当前小夹具新增测试尚未覆盖这些，不能称为完整批量合同验证。

完整原始目录 `out/verification-20261030/r3-next-v1/` 保持原样；有限结果、脚本、源码补丁和
哈希见 [manifest](2026-10-30-publication-evidence/manifest.json)。`selection=false`，
产品未引入该方法；本地诊断不替代 R3/R3c/R4、load、GPU、设备或生产接受。
