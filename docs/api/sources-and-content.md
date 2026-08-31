# `PlaybackSource` 与 `ContentProvider`

状态：active

更新日期：2026-08-30

适用版本：SDK API `0.7.0`

文档角色：公共 API 参考

权威头文件：[playback_source.hpp](../../engine/playback/include/cuexis/playback/playback_source.hpp)、
[content_provider.hpp](../../engine/content/include/cuexis/content/content_provider.hpp)

## 快速结论

- `PlaybackSource` 是 indexed Playback 内容的唯一公共输入边界。
- Source 拥有 Chart 文本、项目文档、asset 描述和 provider 引用。
- Provider 负责读取 bytes，不负责解析 Chart、CXT 或 CXC。
- `IContentProvider::readBlob` 是 owner thread 上的同步调用。
- Provider 不得保留请求中的 `string_view`，异常不得跨越接口。

## 输入工厂速查

| 工厂函数 | 输入场景 | 是否需要 provider |
| --- | --- | --- |
| `fromChartText` | 单个 Chart JSON 文本 | 否 |
| `fromTypedProject` | 旧三字段 typed project | 是 |
| `fromTypedProjectSource` | entry path + owning project-document table | 是 |
| `fromFilesystemProject` | 文件系统项目入口 | 内部创建 |
| `fromCxcFile` | CXC 文件 | 内部创建 package-backed provider |
| `fromCxcMemory` | owning CXC bytes | 内部创建 package-backed provider |

优先使用与宿主已有数据形态最接近的 factory。不要先把 filesystem/CXC 转成临时 JSON 再调用
`fromChartText`，否则会丢失资源闭包和 source identity 信息。

## 资源描述

`PlaybackAssetDescriptor` 只描述以下逻辑信息：

| 字段 | 含义 |
| --- | --- |
| `id` | Chart 和 presentation 使用的 asset ID。 |
| `type` | Mesh、Material、Texture、Audio 或 Shader。 |
| `rootId` | 选择 provider root。 |
| `logicalSource` | provider 内的逻辑资源路径。 |
| `dependencies` | 资源依赖的 asset ID。 |

descriptor 不包含资源 bytes。实际读取始终通过 `IContentProvider`。

## 内容提供者速查

| 类型 | 用途 |
| --- | --- |
| `FilesystemContentProvider` | 从经过验证的 root 集合读取文件。 |
| `MemoryContentProvider` | 从 owning entries 提供确定的内存内容。 |
| `HostContentProvider` | 把宿主 callback 包装为标准 provider。 |
| 自定义 `IContentProvider` | 对接宿主 VFS、归档、网络缓存或资源系统。 |

`ContentRequest` 包含 `rootId`、`source` 和 `maxBytes`。`ContentBlob` 返回 owning bytes 与
process-local `revision`；revision 为零表示未指定。

## 身份与所有权

- `PlaybackSource` 可移动但不可复制，是独立 owning input value。
- source 被 prepare/load 消费后，产生的 `PreparedPlayback` 和 Session 状态受 owner-thread 约束。
- 同一规范内容与同一冻结参数，跨 chart text、typed、filesystem、CXC file 和 CXC memory 应产生相同
  `PreparedSemanticIdentity`。
- CXC factory 是 Playback source API；内部 `cuexis_cxc` 不是独立公共 package SDK。

## 失败与边界

- Provider 必须遵守 `maxBytes`，不能无限制分配。
- 路径越界、缺失资源、超限和回调异常必须转换为 `core::Result`。
- Provider callback 不得抛出到 Cuexis，也不得保留请求视图。
- 资源读取失败不得部分提交 candidate/active 状态。

相关内容：[PlaybackSession 生命周期](playback-session.md)、[CXC v1](../formats/CXC_FORMAT.md)、
[格式索引](../formats/README.md)。
