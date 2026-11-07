# R4 World Component Page Storage Remediation Implementation

状态：implemented; correctness verified; no memory re-admission

日期：2026-11-05

返回 [整改设计](2026-11-05-world-storage-remediation-design.md) 与
[语义内存失败报告](2026-11-05-semantic-memory-remediation.md)。owner 明确批准按组件使用
128-slot EnTT packed pages，并要求继续完成实施。O1 limits 未变更。

后续获准的新批已于 2026-11-06 验证同一 active World 子集减少 71.04%，见
[效果与结果边界](2026-11-06-world-storage-effect.md)。本报告仍保存 2026-11-05 实施批的原始范围。

## 1. 实施内容

在七种非空 World registry 组件声明上加入 `static constexpr std::size_t page_size = 128`：
Transform、Hierarchy、WorldTransform、Behavior、Renderable、Appearance 和 Camera。
EnTT 的 `component_traits` 读取组件类型自身的 `page_size`，因此不改变全局
`ENTT_PACKED_PAGE=1024`，也不影响 Cuexis 之外或未列入 World 的 EnTT pools。
NoteTag/ElementTag 是 empty components，没有 payload page，不需要覆盖。

组件实例字段和对外的 Playback SDK API 未改变。类型级 page configuration 是源码头文件上的
EnTT storage property；每个拥有这些 component type 的编译单元通过同一头文件看到相同值。
项目构建号从 `26.11.05-1` 递增到 `26.11.05-2`；SDK API 保持 `0.9.1`。

新增 World page-boundary 测试：对七种组件分别创建 129 个实体，检查前 128 个实例容量为128，
第129个实例使 capacity 扩为256；删除一个实例后容量保留256且剩余实体访问正确。七个
`entt::component_traits<T>::page_size` 均有编译期 128 断言。测试没有使用 allocation observer、
性能计时或 memory workload。

## 2. 验证

候选源码 SHA：`8627b0ac760ef037ca2371bc932982bf201fcb86`（page-size 实现、版本更新及边界合同）；
随后的两次提交仅修正测试 include path 和 EnTT storage 引用访问，没有改变产品组件或页容量。

两次普通 STATIC、candidate ON、插桩 OFF 的 fresh MSVC 构建均绑定候选
`8627b0ac760ef037ca2371bc932982bf201fcb86` 的测试代码及页容量实现，目标为 `cuexis_world_tests`、`cuexis_runtime_tests` 和
`cuexis_playback_tests`：

| 配置 | 构建 | World / Runtime / Playback CTest | 架构、版本与新页测试 |
| --- | --- | --- | --- |
| Debug | 通过 | 102/102 通过 | 3/3 通过 |
| Release | 通过 | 102/102 通过 | 3/3 通过 |

新页边界用例在 Debug 和 Release 均通过；架构 allowlist 和 version gate 均通过。
37 项 execution-acceptance Python tests、文档检查、版本一致性、格式检查及 `git diff --check`
通过。源码修改后的普通回归不等于完整 11-row F1/F2 或 same-SHA hosted closure。

保留了两次 harness 修正之前的失败构建日志：第一次测试 include path 缺少 `engine/assets/include`；
第二次测试将 EnTT `registry.storage<T>()` 返回引用误作指针。修正后 fresh 第三版 Debug 与
Release 均成功；原构建目录和日志不覆盖。

## 3. 结果边界与待办

本次只证明页大小配置、128/129 边界扩容及现有 World/Runtime/Playback 行为回归。没有做
N=64/2048/40K performance comparison、分配/allocator 计数或任何 memory/P8 诊断；没有用
预计节省重算预算。原有 64/high active World 358,241 B 下界超 327,680 B 的有效失败仍保留，
不得报告为已修复或 memory admitted。

128 pages 对稀疏小池减少预留槽，但大规模高密度池的页分配次数会增加；字符串 buffer、完整
语义 graph、prepare/current/candidate/scratch/retained、native/GPU/driver、最终设备退休仍未闭合。
Memory/P8 继续停止。下一步若需确认 cap，应先提交具名诊断计划与独立授权，绑定新的普通源码
candidate、预算与工具；不自动重用已耗尽的两轮额度。

证据构建树及原始绑定位于
`out/build/r4/world-pages-debug-v3/`、`out/evidence/r4/world-pages-debug-v3-build/`、
`out/build/r4/world-pages-release/` 和 `out/evidence/r4/world-pages-release-build/`。
