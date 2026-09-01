# Cuexis CXC v1

状态：accepted contract；CFU-C3/C4 内部 archive/tools 与 CFU-E Playback source/prepare/identity
已实现；CFU-F hosted consumer/determinism/safety gates 已关闭；G3 hosted、G4、G5 report-SHA
revalidation 与 G6 owner acceptance 已完成；最终产品封存已记录

更新日期：2026-09-01

依据：[ADR 0038](../adr/0038-cxc-v1-and-chart-v4-boundary.md)

## 1. 范围

CXC v1 是自包含、只读、可验证的单文件 Project 交换和部署包。本文只定义 CXC 的物理载体、
manifest、路径、闭包、identity、预算、诊断和 pack/unpack 边界。

Chart v4 和 CXT 的字段分别由 [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md) 与
[CXT_FORMAT.md](CXT_FORMAT.md) 定义。CXC 不重新定义这些内容格式，也不是 Runtime、World、
AnimationProgram、FrameSnapshot 或 ZIP library API。

内部 `cuexis_cxc` 已能读写和验证 CXC bytes，CFU-C4 developer tools 已在本地与 hosted 门禁中验证；
CFU-E 已把 CXC file/memory source 接入 Playback prepare、semantic identity 与 consumer/export 门禁。
CFU-F 已关闭最终实现 SHA 的 hosted consumer、确定性、安全与性能门禁。CFU-G 经项目所有者接受
后，`.cxc` 的内部工具与 Playback source/prepare 闭包已封存；本阶段不交付公共 CXC package API，也不形成
公共 CXC package component。

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
不同操作，不得互相隐式触发。Pack 保留已校验 ProjectConfig、Asset Index、Chart、CXT 和资源的
精确 source bytes，不规范化、不迁移、不裁剪 Asset Index。Unpack 只恢复包内播放闭包，不能重建
未打包的原始创作资产、Importer 中间数据、脚本或 Studio 历史。

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

ZIP32 v1 的 archive entry 总数上限是 `65,534`，包含固定 manifest，因此 manifest `entries[]`
最多包含 `65,533` 项。EOCD 的 16-bit entry count 值 `0xFFFF`，local/central header 的 32-bit size
值 `0xFFFFFFFF`，以及 central header/EOCD 的 32-bit offset 或 directory size 值 `0xFFFFFFFF`，
在对应字段中保留为 ZIP64 sentinel；出现这些 sentinel、ZIP64 locator 或 ZIP64 extra field 都以
`cxc.archive.feature_unsupported` 失败。

规范 writer 首先写 `cuexis.cxc.json`，随后按 manifest `entries[].path` 的 portable ASCII bytes
升序写入。以下 metadata 固定并由 binary golden 锁定：

| 字段 | CXC v1 规范值 |
| --- | --- |
| general purpose bit flags | `0` |
| compression method | Stored (`0`) |
| modified time/date | `1980-01-01 00:00:00` |
| filename encoding | portable ASCII；不设置 UTF-8 flag |
| extra field / comment | 长度 `0` |
| internal/external attributes | `0` |
| disk number | `0` |
| version needed | `10`（ZIP 1.0） |
| version made by | `0x000A`（MS-DOS/FAT host、ZIP 1.0） |

规范 writer 固定写 `version needed = 10`。Reader 为兼容已生成的 ZIP32 Stored 输入，接受
`version needed` 在 `[0,20]` 范围内的值；大于 `20` 表示未支持的 ZIP 特性并以
`cxc.archive.feature_unsupported` 拒绝。该 reader 宽容不改变 writer 的规范输出。

Local header 与对应 central header 的 flag、method、CRC、compressed/uncompressed size 和 filename
必须完全一致。Central directory 的 count、span 和 offset 必须与 EOCD 完全一致并恰好覆盖全部
entry。Writer 不写目录 entry、padding 或尾随字节。

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

规范 manifest bytes 使用 UTF-8、无 BOM、两个空格缩进、LF、对象 key 按 ASCII 升序和恰好一个
结尾换行。`entries` 已按 path 升序；其他数组的规范顺序由拥有该字段的格式合同定义。Reader
拒绝重复 JSON key，但可以接受不满足上述空白布局的语义等价 manifest；重新写出时必须使用规范
布局。

### 4.3 路径

路径必须使用 portable ASCII 和 `/`，是非空相对路径；segment 不得为空、`.` 或 `..`。拒绝
反斜杠、冒号、NUL/control、绝对路径、尾随点/空格、Windows 保留名称和大小写折叠冲突。同一
archive/manifest 路径集合不得同时包含一个 regular file 路径及其 descendant，例如 `file` 与
`file/child`；该集合冲突以 `cxc.entry.duplicate` 失败。

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
类型或依赖图。CXC v1 使用“项目声明闭包”：每个声明 asset root 的固定 Asset Index 及其中全部
Asset record、source 和 dependency 都进入闭包，即使入口 Chart 当前没有引用该 AssetId。Pack 不
重写或裁剪 Asset Index。Chart import 把被引用 CXT 纳入闭包；CXT 内 Asset reference 继续通过
Asset Index 解析。

Archive entry 必须能由以下至少一条关系到达：固定 ProjectConfig、声明 root 的固定 Asset Index、
入口 Chart、Chart 的 CXT import、Asset Index source 或 dependency。未被这些关系声明的额外 Chart、
CXT、资源或其他 entry 是隐藏 payload，并以 `cxc.entry.unlisted` 失败。Source Project 目录中未被
声明的 authoring 文件不会被 pack，也不构成错误。

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

CXC 明确区分：

```text
CxcPackageIdentity
  SHA-256(exact .cxc bytes)
  用于完整性、传输缓存和 package equality

PreparedSemanticIdentity
  SHA-256(domain-separated canonical Chart/CXT/resource/parameter identities)
  用于 prepare、reload、determinism 和语义缓存
  字节布局见 CHART_V4_FORMAT.md
```

Filesystem、memory、host 和 CXC source 对相同规范内容与参数必须得到相同
`PreparedSemanticIdentity`；只有 CXC source 额外具有 `CxcPackageIdentity`。Archive offset、CRC、
时间戳、Provider revision 或 source path 不能替代语义 identity。规范 writer 必须使相同 source
bytes 和相同 manifest 输入产生相同 `.cxc` bytes。

## 8. 预算和诊断

```text
package bytes / listed entry bytes  512 MiB
entry bytes                           64 MiB
manifest bytes                         1 MiB
archive entries including manifest    65,534
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
cxc.entry.order_invalid
cxc.entry.duplicate
cxc.entry.missing
cxc.entry.unlisted
cxc.entry.size_mismatch
cxc.entry.hash_mismatch
cxc.budget.exceeded
cxc.project.invalid
```

## 9. 工具边界

CXC 的 archive/manifest/closure 实现由内部静态 target `cuexis_cxc` 拥有。它不作为公共
`Cuexis::CXC` component 暴露，也不安装 CXC 公共头；在静态库分发中，该实现 target 会随
`CuexisTargets.cmake` 导出并由包配置声明其 `minizip-ng` 等静态依赖，以满足链接闭包。
它不向 Playback 公共头传播 ZIP library、JSON DOM、archive offset 或文件句柄。Playback
可以私有依赖它；pack/validate/unpack 工具直接复用同一实现。

第三方 archive 依赖必须允许检查 local/central header、ZIP64 sentinel、extra field、entry range、
overlap 和 trailing bytes。若库不暴露全部信息，Cuexis 可以增加窄的 envelope validator，但不得
自行实现压缩算法。

```text
cuexis_cxc_pack
  validate project-declared closure and write canonical CXC; preserve entry bytes; never migrate,
  trim Asset Index or run scripts

cuexis_cxc_validate
  validate archive, manifest, Project, Asset Index, Chart, CXT and resource closure

cuexis_cxc_unpack
  validate and write into an empty (new or existing) directory; never overwrite existing content,
  migrate or rebuild authoring data
```

Chart migration 是独立操作，见 [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md)。Pack/unpack 输入输出路径
必须不同；失败时删除临时输出并保留原始内容。

## 10. 候选示例

正反例见 [examples/chart_format_update](../examples/chart_format_update/README.md)。CFU-C1 已将 manifest
副本提升到 `tests/fixtures/chart_format_update/`，由生产 Schema 与内部 typed manifest Reader 验证；
CFU-C3 已实现 strict ZIP32 archive、闭包和 owning CXC package，CFU-C4 developer tools 的本地证据
见 [C4 报告](../stage_reports/chart-format-update/2026-08-13-c4-tools.md)。公共 Playback 输入、
hosted 跨平台 consumer/determinism/safety gates 与 owner acceptance 已关闭；本节只记录已
封存的内部实现证据，不宣称交付公共 CXC package API。
