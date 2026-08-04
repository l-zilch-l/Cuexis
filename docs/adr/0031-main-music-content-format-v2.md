# ADR 0031: Main Music Content in Chart and Asset Index v2

状态：已接受

日期：2026-07-27

## 背景

Asset Index v1 只接受 Mesh、Material 和 Texture，Chart v1 也没有主音乐字段。直接向
任一 v1 格式增加 Audio 会改变已经验收的未知字段和类型拒绝语义，并使旧 reader 把新内容
误报为 v1。阶段 1D 同时需要一个受 Asset Index、ContentProvider 和资源预算约束的主音乐
引用，不能使用未索引路径或把设备配置写入 Chart。

## 决策

新增以下格式版本：

```text
cuexis.asset-index version 2
  保留 v1 的 Mesh、Material、Texture，并增加 Audio

cuexis.chart version 2
  保留 v1 内容，并增加可选的 typed audio block

cuexis.project version 1
  保持不变，entry.chart 仍是唯一 bootstrap locator
```

Chart v2 的主音乐结构为：

```json
"audio": {
  "version": 1,
  "mainMusic": {
    "domain": "asset",
    "id": "audio.main"
  }
}
```

省略 `audio` 表示明确没有主音乐。存在 `audio` 时，`version` 和 `mainMusic` 都是必需字段；
`mainMusic.domain` 必须为 `asset`。主音乐引用必须解析到 Asset Index v2 的 Audio 记录。

Asset Index v2 的 Audio 记录使用既有 `source` 字段：

```json
{
  "id": "audio.main",
  "type": "audio",
  "source": "audio/main.wav",
  "dependencies": []
}
```

阶段 1D 的 Audio 是叶节点：其 `dependencies` 必须为空，其他 v2 资产也不得把 Audio 作为
依赖。主音乐只能由 Chart v2 的 typed 引用发现。该限制避免普通 Renderable 资源闭包隐式
加载音频；未来 stems、cue sheet 或复合音频需要新的格式决策。

Reader 必须先读取并校验显式版本，再使用对应版本的字段表和类型表：

```text
Asset Index v1  只接受 Mesh、Material、Texture
Asset Index v2  接受 Mesh、Material、Texture、Audio
Chart v1        顶层 audio 是未知核心字段并失败
Chart v2        接受可选 audio block
Simple Chart    阶段 1 继续只有 v1，不自动升级为 canonical v2；ADR 0035 后续在阶段 2A 移除
```

Typed `ChartDocument`、`ChartRuntime`、`AssetIndexDocument` 和 AssetDatabase 输入必须保留
显式 format version。程序化构造的数据接受与文件 reader 相同的版本/类型组合校验，不能仅因
C++ enum 已经包含 Audio 就让 v1 文档接受 Audio。

加载器不自动迁移、不写回，也不把 v2 数据伪装为 v1。Player 使用只有 Chart 文件、没有
Project/Asset Index 的 `--chart` 入口加载带主音乐的 v2 Chart 时必须明确失败；v1 canonical
和 simple 继续作为无音频 ChartClock 回归入口。

## 影响

- `schemas/` 增加 Asset Index v2 与 Chart v2 artifact，并保留全部 v1 artifact。
- Chart、Project、Assets 和 Playback 测试必须覆盖版本路由、未知字段、类型匹配和无迁移。
- ProjectConfig v1、设备选择、输出增益和校准格式不变。
- AudioSource 继续通过 AssetDatabase、ResourceManager 和 ContentProvider 读取，不能绕过
  索引使用物理路径。

## 被拒绝的方案

### 向 v1 增加可选 audio 字段和类型

拒绝。它会改变 v1 的未知字段和类型拒绝语义，使同一 format/version 在不同版本程序中表示
不同内容。

### 把主音乐路径写入 ProjectConfig

拒绝。Chart 决定其确定性主音乐内容；ProjectConfig 只定位入口 Chart 和资产根。

### 由目录枚举或文件扩展名发现音乐

拒绝。它绕过 AssetId、ContentProvider、路径安全和内容预算边界。
