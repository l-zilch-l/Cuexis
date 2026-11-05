# R4 World Component Page Storage Remediation Design

状态：design proposal；owner approved design work only; implementation and new diagnostics are not authorized

日期：2026-11-05

返回 [R4 语义内存失败报告](2026-11-05-semantic-memory-remediation.md)。本设计消费
[R4 计划 §4](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md#4-独立修复重开与停止规则)
和[语义整改原始证据](2026-11-05-semantic-memory-evidence/index.md)。本轮只检查既有源码、
配置和归档日志；没有产品代码修改、构建、workload、正式样本或新增内存诊断。

## 1. 决定目标

owner 选择 A：保持 O1 已接受的 reference caps，先完成存储整改设计；实施和新增诊断需另行
具体裁定。触发事实是 64/high 的真实 Playback-active World 唯一下界 358,241 B，超过
`state.world` 327,680 B 至少 30,561 B。

设计目标是减少小规模 World 中 EnTT component payload pages 的空置槽成本，同时保持组件
值语义、实体映射、信号、Transform transaction 和 Playback 边界。内存目标仍使用原
327,680 B；不得按本次观测或本设计预计节省重定 cap。`AppearanceComponent`/`CameraComponent`
的嵌套字符串、ObjectEntityMap、World transform scratch、其他 retained/current/candidate
拥有者以及 native/GPU/driver 字节是分离问题，单改 component page size 不会自动闭合这些上界。

## 2. 源码与实测依据

当前固定 EnTT 配置在
`out/build/r4/post-o1/semantic-memory-release-v3/vcpkg_installed/x64-windows/include/entt/config/config.h`
将 `ENTT_PACKED_PAGE` 默认设为 1024。
`entt/entity/component.hpp` 的 `component_traits` 对非空类型采用该默认值，但优先读取
组件的 `Type::page_size`；`entt/entity/storage.hpp::assure_at_least` 每页实际分配该数量的
完整组件，并以 pages × page size 报告 capacity。因此这是可按类型控制的页槽数，不是字节数。

语义诊断的 active census 顺序直接读取 Appearance、Camera、WorldTransform 的真实 storage
页。其 1024 槽 payload 分别为 65,536 B、81,920 B、65,536 B，对应 MSVC 本候选布局每实例
64 B、80 B、64 B；合计 212,992 B。三页只代表三个分配的 payload，不含其私有 allocator
alignment prefix/tail；诊断对 payload union 保守计费，未把 backing requested/storage 当作
语义 payload。

三页的槽位容量合计 3,072，而工作集只有 64 Chart entities 和一个内部相机 entity。该比较说明
小规模页内有显著空槽，不推导其他 workload 的精确节省。其余 stable registry 子集还有
145,249 B，且完整 World 图仍未覆盖。

权威原始值见[第二轮 stdout](2026-11-05-semantic-memory-evidence/index.md)所指归档中的
`round-2/stdout.log`、`round-2/analysis.json`；源码事实来自上述固定 EnTT 头文件与
[World component declarations](../../../../../../engine/world/include/cuexis/world/components.hpp)
、[render component declarations](../../../../../../engine/render/include/cuexis/render/renderable_component.hpp)
、[camera declaration](../../../../../../engine/render/include/cuexis/render/camera_component.hpp)。

## 3. 方案比较

| 方案 | 做法 | 优点 | 成本、风险与边界 |
| --- | --- | --- | --- |
| A：按组件覆盖 packed page size，先以 128 为候选（推荐） | 对实际进入 World registry 的非空 component 类型，在类型定义处声明统一、固定的 `page_size=128`；覆盖 Transform、Hierarchy、WorldTransform、Behavior、Renderable、Appearance、Camera。空 tag 无 payload page，不需设置。若审计发现其他 non-empty World component，再纳入同一清单。 | 使用 EnTT 现成类型级能力；只改变 Cuexis 自己的组件池，不影响 EnTT 的全局默认和无关用户类型；64 entities + camera 共 65 个实例时，每个高密度池一页；容量粒度明确。 | 128 是待验证设计值，不是测量结果。2049 个实例需 17 页，40,001 个实例需 313 页；页分配次数比 1024 方案约高 5.7×/7.8×，可能增加 allocator、目录和 cache-crossing 成本。不同组件密度不同，不能只按 entities 数乘统一页数代替完整布局。 |
| B：全局把 `ENTT_PACKED_PAGE` 改为 128 | 通过 build-wide CMake/vcpkg define 改变所有没有类型自定义的 EnTT component。 | 改动点少；全项目未显式定页的非空池行为统一。 | 影响所有 EnTT 客户端和非 World pool；compile definition 必须跨 Cuexis targets 与外部模板实例化保持一致，否则可能出现 ODR/容量语义不一致。把第三方全局默认改变为产品策略，范围过宽，不推荐。 |
| C：重做组件表示或自有分块存储 | 将字符串搬入稳定 ID/string table、减少组件对象宽度，或以 Cuexis 自有 paged/SoA storage 替换相关 EnTT pool。 | 可进一步控制嵌套字符串与数据布局，长期可优化稀疏访问和目标设备缓存行为。 | 设计与迁移范围大；字符串生命周期、Presentation 查询、World callback、Transform derived storage swap、信号和实体销毁语义都需重审。仅缩小对象仍乘默认 1024 槽，不能保证消除本次固定页成本；自建 storage 也增加维护面。 |

## 4. 推荐设计详述

### 4.1 候选页容量

以 128 作为首个待实施/待验证配置，原因是它能容纳 N=64 工作负载中最多 65 个 World
实例，且仍为 2048、40K 大型工作集提供固定、可整除的分页粒度。不得运行时自适应 page
size，也不得按结果动态改变类型布局。若未来经声明容量审计发现 64 个对象以外的辅助实体
超过 128，应由容量模型决定另一个固定值，而不是在运行时跨越隐藏阈值。

仅作页槽算术示意：高密度池在 `1024 -> 128` 时，N64+camera 从 1024 槽降至128槽；
N2048+camera 从3072槽降至2176槽；N40000+camera 从40960槽降至40064槽。该算术只描述
页分配容量，不是全 World 节省量、真实业务实体密度、总 requested/storage、性能读数或预算
预测。对 N64 的三个已观测页，若各自仍只有一页，payload bytes 会按槽位比例缩小为八分之一；
完整 cap 是否通过仍取决于其他固定槽、完整语义 heap、生命周期交叠及未覆盖域。

EnTT 使用 `fast_mod`/分页索引，页大小需是正的二次幂。每种类型按真实最大同时存活数量
求 `ceil(count / page_size)` 页；应另列实体稀疏索引页，它由 Entity traits/`ENTT_SPARSE_PAGE`
控制，不应误认为本次改动会减少该索引页。

### 4.2 布局应用边界

首选在各组件类型定义旁提供 `static constexpr std::size_t page_size = 128`，利用当前 EnTT
版本的类型级覆盖机制。这样 traits 与类型声明处于同一权威位置，所有实例化该组件 storage
的 TU 读到相同值，且不增加每个组件实例字段。若后续兼容策略不接受第三方 storage 参数出现在
组件声明中，可评估集中式 `entt::component_traits` 显式特化；不允许分散私有宏或仅在部分 TU
定义特化。

候选清单应静态枚举所有会进入 `world::World::registry_` 的非空类型：
`TransformComponent`、`HierarchyComponent`、`WorldTransformComponent`、
`BehaviorComponent`、`RenderableComponent`、`AppearanceComponent`、`CameraComponent`。
`NoteTag` 与 `ElementTag` 是空类型，不产生同类 payload page。新增非空 component 必须更新清单、
预算模型和 census coverage。

页大小不是 component count 限制，不得改变实体数、资源闭包、Appearance/Camera 交换、
Transform prepare/apply/finish、derived pool signal、Close/reload 或 World owner 语义。
EnTT page capacity 可由同步 `World::withRegistry` 查询，外部不得把私有 storage 指针或引用留出
回调。改变页大小仍会改变分配边界、容量报告和地址分布，应列为实现行为变化复核。

### 4.3 预算关系

继续使用已接受 `state.world = 4096N + 65536` reference ceiling，N=64 时为327,680 B；
它不根据128页算术调整。World 域正式计量仍需全语义 owning closure，不重复计共享块；完整
requested bytes、allocator storage、process/private commit、GPU/driver 各自单列。

三项高内存 component 页预计是首要 savings source；Transform/Hierarchy/Renderable/
Behavior 等 stable registry pages 也会按本地 type page size 调整。收益按每种组件实际
present/capacity计算，不按 N 盲目套同一元素系数。内嵌 `std::string` 对象尺寸计入 component
payload；字符串字符 buffer 是另一分配，需按实际持有者/迁移关系单独归属，不能计入本页节省。

## 5. 后续若批准实施，必须满足的验证门

以下为后续实施提案，不构成本轮授权：

1. **声明容量与封闭清单**：从 chart schema/fixture、内部 camera、hierarchy 和 optional component
   的最大 count 推导每个 pool 页数；证明所有 EnTT query/registry callbacks、Runtime transactions、
   derived storage swap 和 external host wrapper 仍兼容。
2. **页容量合同测试**：编译期断言每个 type page size 恒为128且为二次幂；运行小于、等于、超过
   页边界的实体/组件数，检查可创建、销毁、迭代、lookup、transform 和 signal 正确性、capacity
   与释放；加组件可选/稀疏密度用例。不得把只测64的成功冒充2048/40K验证。
3. **CPU 与分配退化门**：至少比较 N=64/2048/40K 和每类组件实际密度，分别报告 page 数、
   payload/backing bytes、allocation count、迭代与 prepare/update成本；产品计时、semantic observer
   和普通构建分开绑定；不从 memory-collector 时间代替产品性能。正式协议及诊断额度须在运行前
   另行批准。
4. **R4 正确性与兼容性**：完成 Debug/Release、STATIC/SHARED、candidate ON/OFF、安装消费者和
   至少三平台验证；新候选、构建、工具和证据一起冻结。老 SHA hosted 结果不能代验。
5. **语义内存重新准入**：保留当前失败；在对同一 O1 未改 ceiling 的获准诊断中先核对所有 active
   component pages、字符串/owned maps、prepare-only、shared/candidate/scratch、count/age/final
   release 及 observer reference overhead。active unique complete upper-bound coverage 缺项仍
   unrated；仅下界低于 cap 不能证明通过。40K/P8 继续单独阻断，除非其明确准入恢复。
6. **停止条件**：任一已验证 required profile 的完整 requested bound 超 cap 时记录失败，不继续
   改类型标签或按观察值改 cap；页边界引入性能退化则重新设计，不能只接受小规模节省。

## 6. 待裁定范围

owner 已裁定先完成本设计；以下尚未授权：

- 是否批准以 type-local page size=128 实施；
- 若批准，是否为实现/平台正确性提供独立工作范围；
- 是否另批最多具名、有时限的 memory diagnostic，以及各 profile 的合法 workload；
- 组件表示/string ownership 是否另行纳入，或保留独立未闭合项；
- 何时重开 memory/P8、是否采用不同适用范围；O1 不随实现自动变化。

在收到具体实施裁定前，候选页值仍是设计参数；保留产品实现、当前失败和停止状态。
