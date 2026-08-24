# Stage Chart Format Update CFU-E0 API Review

状态：completed

快照日期：2026-08-14

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

## 1. 结论

项目所有者于 2026-08-14 指示完成 CFU-E0，接受本文冻结的 Playback 公共 API 与版本方案。
CFU-E0 已关闭；下一实施批次是 CFU-E1。该关闭只批准 API surface 和版本方向，不表示已经修改
安装头、实现 Playback v4/CXC 接入或切换生成的 SDK 版本。

```text
当前仓库 CUEXIS_SDK_API_VERSION   0.5.0
CFU-E1 实施目标 SDK API          0.6.0
FrameSnapshot / FrameDigest      unchanged; FrameDigest remains version 3
stable C ABI                     still deferred to Stage 12
```

CFU-D3 继续等待 CFU-E 完成。本报告不是完整 CXC 产品支持、公共 package API、Playback 支持、
CFU-D 完成或 Stage 4 开始证据。

## 2. Parameter 与 prepare options

下列名称和形状冻结为 CFU-E1 的公共 Playback API：

```cpp
struct ChartParameterNumber final {
    double value{};
};

struct ChartParameterRational final {
    std::int64_t numerator{};
    std::int64_t denominator{1};
};

struct ChartParameterWeight final {
    double value{};
};

using ChartParameterValue =
    std::variant<ChartParameterNumber, ChartParameterRational, ChartParameterWeight>;

struct ChartParameter final {
    std::string id;
    ChartParameterValue value;
};

struct ChartParameterSet final {
    std::vector<ChartParameter> values;
};

struct PlaybackPrepareOptions final {
    ChartParameterSet parameters;
};
```

三个 wrapper 类型是稳定的显式 type tag。`number` 与 `weight` 即使都使用 `double` 也不能合并为
同一个 variant alternative。`ChartParameterRational` 使用固定宽度整数，不暴露
`chart::RationalBeat`；positive denominator、约分、finite、weight `[0,1]`、重复、未知、缺失和
声明类型匹配都在 prepare 时由现有 typed resolver 校验和规范化。

`PlaybackPrepareOptions` 是每次 prepare/load/reload 的 owning input，不属于 `PlaybackSource`、
ProjectConfig、CXC manifest 或 active Chart bytes。公共调用返回后，candidate 不借用调用者的
string、vector 或 variant storage。

## 3. Prepared semantic identity

```cpp
struct PreparedSemanticIdentity final {
    std::array<std::uint8_t, 32> sha256{};

    friend bool operator==(const PreparedSemanticIdentity&,
                           const PreparedSemanticIdentity&) = default;
};
```

该类型只公开 owning SHA-256 bytes 和 equality，不增加分配字符串的 `hex()` 成员。成功 prepare 的
v1-v4 candidate 必须具有 semantic identity；empty 或 moved-from `PreparedPlayback` 没有 identity。
package path、archive metadata、provider revision 和 `CxcPackageIdentity` 不参与该值。

观察入口冻结为：

```cpp
[[nodiscard]] auto PreparedPlayback::semanticIdentity() const noexcept
    -> std::optional<PreparedSemanticIdentity>;

[[nodiscard]] auto PlaybackSession::semanticIdentity() const
    -> core::Result<PreparedSemanticIdentity>;
```

active Session 没有 committed 内容时沿用现有 Result error 约定。commit 后 active identity 等于
candidate identity；失败 reload 不得改变 active identity。

## 4. Owning project-document source

现有 `TypedPlaybackProject` 三字段 aggregate layout 不变。多 document typed host source 使用新类型：

```cpp
struct PlaybackProjectDocument final {
    std::string path;
    std::string utf8Text;
};

struct TypedPlaybackProjectSource final {
    std::string sourceId;
    std::string entryChartPath;
    std::vector<PlaybackProjectDocument> projectDocuments;
    std::vector<PlaybackAssetDescriptor> assets;
};
```

`path`、`entryChartPath` 和 ID 使用既有 portable ASCII 规则；`utf8Text` 是 owning UTF-8 document
bytes。`entryChartPath` 必须精确匹配一个文档。Chart 与 CXT 在 project-document table 中，AssetId
bytes 继续由 `IContentProvider` 提供。

新增 factory 冻结为：

```cpp
[[nodiscard]] static auto fromTypedProjectSource(
    TypedPlaybackProjectSource project,
    std::shared_ptr<content::IContentProvider> provider)
    -> core::Result<PlaybackSource>;

[[nodiscard]] static auto fromCxcFile(const std::filesystem::path& locator)
    -> core::Result<PlaybackSource>;

[[nodiscard]] static auto fromCxcMemory(std::vector<std::byte> packageBytes)
    -> core::Result<PlaybackSource>;
```

CXC memory 输入按值取得所有权并允许 move。公共 API 不暴露 `CxcPackage`、archive handle、limits、
offset、manifest DOM 或 package identity；`cuexis_cxc` 继续是 Playback 的内部实现依赖。

## 5. Session overload

options 参数固定放在末尾并按 `const PlaybackPrepareOptions&` 传入：

```cpp
prepareLoad(std::string_view, PlaybackMode, const PlaybackPrepareOptions&);
prepareLoad(PlaybackSource&&, PlaybackMode, const PlaybackPrepareOptions&);

prepareReload(std::string_view, const RuntimeFrame&, ReloadPolicy,
              const PlaybackPrepareOptions&);
prepareReload(PlaybackSource&&, const RuntimeFrame&, ReloadPolicy,
              const PlaybackPrepareOptions&);

load(PlaybackSource&&, PlaybackMode, const PlaybackPrepareOptions&);

reload(std::string_view, const RuntimeFrame&, ReloadPolicy,
       const PlaybackPrepareOptions&);
reload(PlaybackSource&&, const RuntimeFrame&, ReloadPolicy,
       const PlaybackPrepareOptions&);
```

所有现有无 options overload 原样保留，并委托 `PlaybackPrepareOptions{}`。不使用 options 默认参数，
避免 overload ambiguity。`loadChart(std::string_view)` 继续是旧 ChartClock convenience，不新增 options
签名；参数化 one-shot 调用使用 `PlaybackSource::fromChartText` 加 `load(..., options)`，或
`prepareLoad(..., options)` 加 `commit`。

## 6. Capability 与版本

E0 批准以下 public constants；它们在 E2 实现，但不声明 Stage 4 动画执行能力：

```cpp
inline constexpr std::string_view capabilityChartV4 = "cuexis.chart.v4";
inline constexpr std::string_view capabilitySourceCxcV1 = "cuexis.source.cxc.v1";
inline constexpr std::string_view capabilitySourceCxtV1 = "cuexis.source.cxt.v1";
```

SDK API `0.6.0` 已冻结，因为 E1 将增加安装类型、factory、overload 和 shared exported member
functions；旧 `0.5.0` package 无法表达这些能力。E1 必须在同一批次更新
`CUEXIS_SDK_API_VERSION`、生成头输入、package compatibility rejection、static/shared consumer 和
symbol/export gate。日期构建版本和 `vcpkg.json` 不因 E0 自动变化。

## 7. 兼容与排除

```text
保留 fromChartText / fromTypedProject / fromFilesystemProject
保留 TypedPlaybackProject aggregate layout
保留全部无 options overload 与 v1/v2/v3 Reader/Playback 行为
public header 源文件必须纯 ASCII
public methods 捕获内部异常并返回 Result/diagnostics
不公开 Chart/CXC/JSON/archive/resolver implementation types
不公开 CxcPackageIdentity
不改变 FrameSnapshot 或 FrameDigest v3
不实现 animation sampling、mixing 或 Runtime callbacks
不预留 runtime script field、capability、ABI 或 execution hook
```

被拒绝的 API 方案包括：给 `TypedPlaybackProject` 追加字段、把 CXT 伪装成 AssetId、把 ParameterSet
保存在 source、用单一 `double` alternative 丢失 number/weight tag、暴露 `chart::RationalBeat`、返回
archive/package handle，以及通过默认 options 参数替换现有 overload。

## 8. E0 验证与交接

E0 是设计和评审门禁，没有修改生产头、CMake SDK version、生成头或实现文件，因此不要求 C++
构建。文档检查与 `git diff --check` 是本批次验证。CFU-E1 可以从本报告冻结的 surface 开始统一
`PlaybackSource` 内部形状；任何签名或版本变化都必须先重新打开 E0 评审。
