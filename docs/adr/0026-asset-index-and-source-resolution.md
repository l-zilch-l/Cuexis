# ADR 0026：Asset Index 与来源解析

日期：2026-07-18

状态：已接受

## 背景

阶段 1B 的 ProjectConfig 只定义资产根和入口定位，不能让 AssetDatabase 通过目录枚举或文件名猜测建立 `AssetId -> type -> source` 映射。Chart 已经保存逻辑 AssetId，资源内容格式又属于后续 Mesh/Material/Shader 阶段，因此需要一个窄而可审计的来源索引。

## 决策

### 固定索引文件

每个 ProjectConfig asset root 的根目录下使用固定文件名 `cuexis.asset-index.json`。索引格式身份为 `cuexis.asset-index`，v1 结构为：

```json
{
  "format": "cuexis.asset-index",
  "version": 1,
  "assets": [
    {
      "id": "mesh.note",
      "type": "mesh",
      "source": "meshes/note.mesh.bin",
      "dependencies": []
    }
  ],
  "extensions": {}
}
```

v1 `type` 只包含 `mesh`、`material` 和 `texture`。`id` 是逻辑 AssetId，不是绝对路径；`source` 是该 root 内的 portable 相对路径；`dependencies` 是逻辑 AssetId 数组。索引不定义资源内容、材质参数、Shader、压缩格式或 GPU 对象。

索引约束如下：

- 每个 root 至多一个固定索引；索引自身必须位于声明 root 内。
- `AssetId` 全局唯一；同一 ID 的类型冲突、来源冲突或跨 root 重复均使 AssetDatabase 构建失败，不采用 root 顺序覆盖。
- `source` 遵循 ADR 0025 的路径规则，并再次检查物理 containment。
- 最多 100000 条记录；单条最多 256 个依赖，依赖闭包深度最多 64。
- 依赖图按 AssetId 稳定排序遍历；依赖环是错误并报告完整环路。
- 核心未知字段是错误，`extensions` 中的 opaque 数据可保留并 warning。

### AssetDatabase 与 ResourceManager 边界

`AssetDatabase` 是不可变的 `AssetId -> AssetRecord` 索引和安全来源读取接口；它不拥有运行时槽位。`ResourceManager` 是槽位、状态、依赖、generation 和 contentRevision 的唯一所有者，不读取 ProjectConfig，也不调用 OpenGL。

阶段 1B 只实现同步 CPU blob loader，并对单文件读取和解码后内存设置代码级安全上限。正式 Mesh、Material、Texture 内容解析和 GPU 派生对象留给后续阶段。

ResourceHandle 在现有 `index + generation` 之外增加非序列化 manager token，防止不同 ResourceManager 间的相同槽位别名。generation 不允许回绕到有效值。ResourceLease 是 move-only 强引用，ResourceScope 对同一 `(kind, AssetId)` 去重并持有完整依赖闭包。

引用策略固定为：

```text
Required：缺失、类型错误或依赖失败使请求事务失败
Fallback：使用固定且类型匹配的内建 CPU 占位资源并产生 warning
Optional：跳过资源并产生结构化诊断
```

阶段 1B 不实现异步 Future、协程、取消、LRU 或文件监听热重载；保留状态和 contentRevision 字段以便后续扩展。热重载成功/失败语义只在真实消费者出现后实现完整 API。

### Chart 与 Runtime 依赖方向

Chart 保留自己的 `chart::AssetId` 和后端无关文档边界，不直接依赖 Assets、World 或 OpenGL。Runtime 在准备阶段将 `(kind, chart::AssetId)` 稳定排序、去重并转换为 `assets::AssetId`，再把所得 typed Handle 交给 ChartWorldInstantiator。

RuntimeSession 注入外部 ResourceManager；PreparedRuntimeSession 绑定创建它的 Session/Manager。World 先销毁，ResourceScope 后释放。失败 prepare/reload 不发布部分 World 或 Scope，成功 replacement 才交换旧状态。

## 备选方案

### 目录扫描或由 AssetId 猜文件名

拒绝。扫描顺序、别名、重复来源和类型无法形成稳定可审计契约，也会把逻辑 ID 与文件路径错误绑定。

### 把资产表塞进 ProjectConfig

拒绝。ProjectConfig 只负责项目定位和入口；资产发现、来源和依赖属于 AssetDatabase/Asset Index 所有者。

### 现在冻结正式 Mesh/Material/Texture Schema

拒绝。Material/Shader 内容消费者属于后续阶段，提前冻结会产生占位字段和重复迁移。

### 让 Handle 自己拥有强引用

拒绝。高频 Component 会扩散引用计数，无法表达 Session 级资源边界。

## 影响

`cuexis_assets` 从 INTERFACE target 升级为实现 AssetDatabase、ResourceManager、Lease、Scope 的静态库；Asset Index 解析可由应用侧 Project 前端或 Assets 的窄输入适配器完成，但 JSON 类型不得泄漏到公共资源 API。

阶段 1B demo 至少登记 Mesh、Material、Texture 资源，Runtime 实例化真实 typed Handle，Player 继续通过 DebugDraw 显示坐标轴；真实 Mesh GPU 绘制不提前进入本阶段。

## 后续风险

索引写回、导入器、缓存、异步加载和 GPU 派生资源需要各自的阶段性设计，不能把 `cuexis.asset-index` 逐步扩张成无边界通用资产格式。

## SDK 转型补充（2026-07-20）

AssetDatabase 继续拥有不可变 AssetId/类型/逻辑来源/依赖索引，但实际字节读取将委托给注入的 ContentProvider。FilesystemContentProvider 保留 ADR 0025 的 containment；Memory/Host Provider 连接宿主 VFS、归档或内存。ResourceManager 只能请求已索引的逻辑来源，不直接打开任意路径。

ContentProvider 的具体 C++ API 在阶段 1E 前确认；同步加载与现有 Handle/Lease/Scope 语义保持不变，不因 SDK 转型提前冻结异步任务系统。
