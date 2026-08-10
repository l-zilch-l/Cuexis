# Cuexis CXC v1 Candidate

状态：candidate；ADR 0038 的 CXC 载体合同仍待接受，尚未实现

更新日期：2026-08-10

依据：[ADR 0038](adr/0038-cxc-v1-and-chart-v4-boundary.md)

## 1. 范围

CXC v1 是自包含、只读、可验证的单文件 Project 交换和部署包候选。本文只定义 CXC 的物理载体、
manifest、路径、闭包、identity、预算、诊断和 pack/unpack 边界。

Chart v4 和 CXT 的字段分别由 [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md) 与
[CXT_FORMAT.md](CXT_FORMAT.md) 定义。CXC 不重新定义这些内容格式，也不是 Runtime、World、
AnimationProgram、FrameSnapshot 或 ZIP library API。

ADR 0038 整体接受和 CFU-C 实现前，`.cxc`、manifest 和 capability 都不能作为已支持能力对外承诺。

## 2. Artifact 模型

```text
Source Project directory
  cuexis.project.json
  asset roots / cuexis.asset-index.json
  charts / .cxt templates / source and imported resources
        |
        | explicit validate + pack
        v
CXC exchange package (.cxc)
        |
        | validate + package-backed ContentProvider
        v
PlaybackSource -> PreparedPlayback -> active PlaybackSession
```

CXC 不是编辑器文档，也不是编译缓存。Pack、unpack、Chart migration 和 Session prepare 是四个
不同操作，不得互相隐式触发。Unpack 只恢复包内播放闭包，不能重建未打包的原始创作资产、
Importer 中间数据、脚本或 Studio 历史。

## 3. 物理载体

### 3.1 文件身份

```text
extension       .cxc
manifest        cuexis.cxc.json
format          cuexis.cxc
version         1
MIME candidate  application/vnd.cuexis.cxc
```

文件 API 只接受显式 `.cxc` locator。Memory/host API 由调用方声明输入类型，随后仍验证 manifest
format/version。Loader 不根据 ZIP magic 猜测普通 `.zip` 是 CXC。

### 3.2 ZIP 子集

允许：

```text
single-disk ZIP32
Stored method (method 0)
regular-file entries
portable ASCII paths
CRC32 plus manifest SHA-256
```

拒绝：

```text
Deflate or other compression
ZIP64
encryption
multi-disk
data descriptor
archive or entry comments
extra fields
directory entries
symlink or special files
duplicate local/central entries
local/central metadata mismatch
trailing bytes or overlapping entry ranges
```

规范 writer 首先写 `cuexis.cxc.json`，随后按 manifest `entries[].path` 升序写入。时间、权限、
platform、version 和 reserved metadata 使用实现规范冻结的唯一值，并由 binary golden 锁定。

## 4. Manifest v1

### 4.1 顶层

```json
{
  "format": "cuexis.cxc",
  "version": 1,
  "project": "cuexis.project.json",
  "entries": [],
  "requiredExtensions": [],
  "extensions": {}
}
```

六个字段均必需，未知核心字段失败。

| 字段 | 类型 | 规则 |
| --- | --- | --- |
| `format` | string | 固定 `cuexis.cxc` |
| `version` | integer | 固定 `1` |
| `project` | string | v1 固定 `cuexis.project.json` |
| `entries` | array | 非空、按 path 升序、path 唯一 |
| `requiredExtensions` | array | 使用稳定 ID/version 形状 |
| `extensions` | object | 未知可选数据保留，受预算限制 |

### 4.2 Entry

```json
{
  "path": "assets/charts/main.cuexis.chart.json",
  "byteCount": 4096,
  "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

三个字段均必需且不允许未知字段。`byteCount` 是 entry bytes；Stored 模式下等于 compressed size。
`sha256` 是精确 entry bytes 的 SHA-256，以 64 个小写十六进制字符表示。

Manifest 自身不列入 `entries`。Archive 除 manifest 外必须恰好包含 entries 列出的文件。

### 4.3 路径

路径必须使用 portable ASCII 和 `/`，是非空相对路径；segment 不得为空、`.` 或 `..`。拒绝
反斜杠、冒号、NUL/control、绝对路径、尾随点/空格、Windows 保留名称和大小写折叠冲突。

Archive entry、manifest path、ProjectConfig root/path 和 Asset Index source 使用同一规范。Loader
不得“清理后继续”；任何非规范输入直接失败。

## 5. Project 与资源闭包

CXC v1 恰好包含一个根 `cuexis.project.json`，继续使用 `cuexis.project` version 1。CXC 不增加
ProjectConfig 专用字段。

```text
projectId / assetRoots / entry.chart 保持现有语义
每个 asset root 对应 manifest 中的逻辑目录前缀
每个 root 包含 cuexis.asset-index.json
entry.chart 是 manifest 列出的 regular entry
所有 Chart、CXT、Asset Index 和 asset source 位于同一个 CXC
```

Asset Index 继续拥有 `AssetId -> type -> source -> dependencies`。CXC manifest 不复制 AssetId、
类型或依赖图。Chart import 把 CXT 纳入闭包；CXT 内 Asset reference 继续通过 Asset Index 解析。
完整闭包遍历后不得存在未使用的隐藏 payload。

CXC v1 不支持外部 URL、绝对路径、宿主隐式文件、另一个 CXC、运行时下载、未索引资源自动发现、
运行时脚本，或按模板名称从 SDK/网络补全内容。

## 6. 验证顺序

```text
input size and ZIP structure
-> ZIP v1 subset / entry range / CRC
-> manifest size / JSON / format / version
-> path / order / duplicate / size / SHA-256
-> ProjectConfig typed validation
-> Asset Index and dependency closure
-> Chart typed Reader and references
-> CXT import and resource references
-> ChartParameterSet validation and normalization
-> required extension and capability preflight
-> resource acquisition and typed validation
-> PreparedPlayback candidate publication
```

任何失败都不发布部分 AssetDatabase、ChartRuntime、ResourceManager、World、AnimationProgram 或
presentation candidate。

## 7. Capability 与 identity

`cuexis.source.cxc.v1` 只表示 CXC source 能力。包内 Chart/CXT/Animation 仍声明并验证各自的
capability，见 [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md)。

规范 writer 必须使相同规范输入产生相同 `.cxc` bytes。CXC 整体 SHA-256 是包缓存 identity；
Chart、CXT 和资源继续保留各自语义 identity。Archive offset、CRC、时间戳或文件顺序不能替代内容
identity。

## 8. 预算和诊断

```text
package bytes / listed entry bytes  512 MiB
entry bytes                           64 MiB
manifest bytes                         1 MiB
entries                               65,536
path bytes                             4,096
path depth                                64
diagnostics                            1,024
```

稳定诊断至少包括：

```text
cxc.format.unsupported
cxc.version.unsupported
cxc.archive.invalid
cxc.archive.feature_unsupported
cxc.entry.path_invalid
cxc.entry.duplicate
cxc.entry.missing
cxc.entry.unlisted
cxc.entry.size_mismatch
cxc.entry.hash_mismatch
cxc.budget.exceeded
cxc.project.invalid
```

## 9. 工具边界

```text
cuexis_cxc_pack
  validate Source Project closure and write canonical CXC; never migrate or run scripts

cuexis_cxc_validate
  validate archive, manifest, Project, Asset Index, Chart, CXT and resource closure

cuexis_cxc_unpack
  validate and write into a new empty directory; never overwrite, migrate or rebuild authoring data
```

Chart migration 是独立操作，见 [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md)。Pack/unpack 输入输出路径
必须不同；失败时删除临时输出并保留原始内容。

## 10. 候选示例

正反例见 [examples/chart_format_update](examples/chart_format_update/README.md)。ADR 0038 整体接受
并建立生产实现以前，它们不是当前 Loader 可接受的 fixture。
