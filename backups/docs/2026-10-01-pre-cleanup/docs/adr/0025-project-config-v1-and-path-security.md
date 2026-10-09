# ADR 0025：ProjectConfig v1 与项目路径安全

日期：2026-07-18

状态：已接受

## 背景

ADR 0024 已冻结 ProjectConfig 的固定文件名和格式身份，但没有冻结 v1 字段、入口定位、资产根规范化或路径威胁模型。阶段 1B 需要让 Player 和未来 Studio 使用同一份项目描述，同时避免把本机路径、用户偏好或设备能力混入项目确定性内容。

## 决策

### 文件与最小 Schema

ProjectConfig 是项目根目录下的 UTF-8 JSON 文件 `cuexis.project.json`，格式身份严格为 `cuexis.project`，v1 顶层结构为：

```json
{
  "format": "cuexis.project",
  "version": 1,
  "projectId": "019b0000-0000-7abc-8def-000000000100",
  "assetRoots": [
    { "id": "main", "path": "assets" }
  ],
  "entry": {
    "chart": {
      "root": "main",
      "path": "charts/stage1b_example.cuexis.chart.json"
    }
  },
  "extensions": {}
}
```

v1 只包含项目身份、命名资产根、主 Chart bootstrap locator 和可选扩展区。不加入显示名称、窗口、音频、用户目录、设备、预算、ImporterProfile 或 ShaderTargetProfile 字段。

- `projectId` 必须是规范小写 UUIDv7；它是稳定项目身份，不是文件路径或缓存路径。
- `assetRoots` 必须非空，最多 16 个；root `id` 必须匹配 `[a-z][a-z0-9._-]{0,63}` 且唯一。
- `entry.chart` 使用 `{root, path}`，只用于应用启动时定位主 Chart，不进入 ChartDocument、ChartRuntime 或 AssetId。
- `extensions` 必须是 object；v1 扩展只允许不改变 v1 行为的 opaque 数据。
- 核心对象未知字段是错误；扩展数据保留并产生一次稳定 warning。

ProjectConfig loader 的默认安全预算为：输入 1 MiB、JSON 深度 32、单个字符串 16 KiB、规范化路径 4096 bytes、诊断 1024 条、asset root 16 个、扩展成员 256 个。它们是解析和拒绝资源耗尽的代码级上限，不是 DeviceProfile 或用户可调预算。

ProjectConfig Schema 使用 Draft 7 artifact。运行时稳定诊断仍由 Cuexis-owned JSON Reader 和 typed semantic validator 负责，不能把第三方 Schema Validator 的错误文本作为公共契约。

### 版本、迁移与写入

当前只接受 `version: 1`。缺失、非整数、未来版本和不支持的历史版本均稳定失败，不推断 v0，不自动降级，也不自动改写源文件。迁移接口按内存中的 `N -> N+1` 链设计，v1 当前没有历史迁移。

显式保存使用同目录临时文件：独占创建、完整写入、flush/close、重新加载校验，然后通过平台原子 replace 替换目标。任何写入、重校验或 replace 失败都保留上一有效文件；加载和迁移过程永不隐式写回。

### 路径与物理边界

配置路径采用 portable ASCII、正斜杠分隔的相对路径。拒绝空路径、绝对路径、盘符、UNC/device path、反斜杠、NUL/control、空段、`.`、`..`、冒号、尾随点/空格和 Windows 保留名称。

项目定位只接受目录或精确文件名 `cuexis.project.json`。项目根取配置文件父目录。每个 root 和入口路径在拼接后执行 component containment 检查，并对已存在对象做物理 canonical/file-identity 检查：

- root 必须存在且为目录；入口必须是 root 内的 regular file。
- root 之间不得是同一物理目录、大小写别名、junction/reparse 别名或父子重叠目录。
- root、入口和后续资产来源不得通过 reparse point 逃逸项目根或声明 root。
- 诊断只输出 root ID、项目相对路径和字段路径，不输出本机绝对路径。

这些规则优先保证跨 Windows/Linux 的确定性和安全性；Unicode 文件名支持需另行定义规范化和大小写策略后再升级格式。

## 备选方案

### 把入口直接写成 AssetId

未选择。ProjectConfig 的 bootstrap 阶段需要先找到 AssetDatabase，直接使用 AssetId 会把数据库自身的发现问题提前耦合进项目定位。

### 允许任意路径或仅使用字符串前缀检查

拒绝。字符串前缀和 `lexically_normal()` 不能处理 junction、大小写别名、保留名称和物理路径逃逸。

### 加入可选的 name、窗口或设备字段

拒绝。当前没有真实消费者，会产生重复所有权和未来迁移负担。

## 影响

新增应用侧 `cuexis_project` 前端；它输出已校验的 typed ProjectConfig/PreparedProject，不向引擎模块暴露 JSON DOM。`cuexis_assets` 只接收已规范化的 typed roots 和 asset records。

ProjectConfig 的解析、路径、迁移、原子写入和失败语义必须有独立测试。ProjectConfig 失败时不能发布 AssetDatabase、入口 Chart 或任何 Resolved 配置快照。

## 后续风险

保存时的断电持久性是否需要目录级 flush，留待出现真实 Studio 写回消费者时由平台实现 ADR 冻结。当前承诺为进程内原子可见和失败保留旧文件。

## SDK 转型补充（2026-07-20）

本 ADR 的固定文件名、项目根和物理 containment 规则继续约束 Player/Studio 的 FilesystemContentProvider。嵌入 SDK 可以从已经读取的 ProjectConfig 文本或 typed project source 启动；非文件系统 Provider 没有物理 root，但仍必须执行格式、ID、逻辑路径、重复 root、预算和未知字段校验。

该补充不修改 ProjectConfig v1 Schema，也不允许宿主通过内存入口绕过内容校验。
