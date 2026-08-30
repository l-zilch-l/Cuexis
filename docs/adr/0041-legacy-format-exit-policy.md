# ADR 0041: Legacy Format Exit Policy

日期：2026-08-30

状态：已接受

关系：细化 [ADR 0038](0038-cxc-v1-and-chart-v4-boundary.md)、
[ADR 0030](0030-playback-preview-api-version-and-result.md)、
[ADR 0026](0026-asset-index-and-source-resolution.md) 与
[ADR 0037](0037-stage-3-portable-presentation-contracts.md)。

## 背景

Full Review 指出部分新版路径仍可能接受未定义的 legacy payload、隐式降级，或继续把
旧接口描述为现行合同。项目需要统一的兼容退出规则，同时保留已接受 ADR 明确承诺的
旧 Reader 和迁移入口。

## 决策

### 新版不产生旧格式

新版 Writer、pack、prepare 和 OpenGL adapter 不生成 legacy presentation、旧 identity
编码或未定义的 record-level extension。新功能必须使用当前版本化 envelope 和 capability。

### 不进行隐式 fallback

当输入属于旧 payload、未定义扩展或不兼容组合时，Reader/prepare 必须在最早可判定位置
稳定拒绝，并返回带 field path 和上下文的诊断。禁止把 legacy presentation 静默转换为
Unlit，禁止把未知结构猜测为旧版本语义。

### 兼容读取按既有合同保留

Chart v1/v2/v3 Reader、显式 v1/v2/v3 到 v4 迁移、document-level opaque `extensions`
以及 SDK 0.7.0 已公开的 legacy API 仍按各自 ADR 保留。此 ADR 不删除这些入口。

### 兼容路径按版本退出

旧 API 或兼容行为的删除、收紧或 `deprecated` 标记必须单独列出迁移说明、external-consumer
门禁和 SDK/API 版本目标。SDK 0.7.0 不直接删除 `RenderFrame/renderFrame` 或收紧
两参 `TimingMap::create` 的既有行为；后续版本可在新的版本决策中退出。

### Identity 一次性迁移

PreparedSemanticIdentity 采用 canonical 全保真 Chart bytes。旧 identity 算法不保留双轨
读取；受影响 golden 和 cache namespace 一次性迁移，旧 cache 失效，不得命中新语义。

## D1-D6 结论

| 决策 | 结论 |
| --- | --- |
| D1 / PB-01 | v4 renderable 必须使用 portable `CXPRES01`；legacy presentation 稳定拒绝。 |
| D2 / CX-01 | Asset Index record-level `extensions` 稳定拒绝；document-level opaque extensions 保留。 |
| D3 / PB-04 | 保留 `frame.value_invalid` 作为 Validation Sink/未来边界码；extract 只发 non-finite 诊断。 |
| D4 / CH-03 | SDK 0.7.0 保留两参 BPM 的既有行为并标记为 legacy；严格 limits 路径继续执行域检查。 |
| D5 / RT-04 | `RenderFrame/renderFrame` 标记为 legacy/diagnostic-only；SDK 0.7.0 暂不删除。 |
| D6 | canonical 全量 identity 一次性迁移；旧 cache 失效，不保留双轨算法。 |

## 排除项

此 ADR 不删除 Chart v1/v2/v3 Reader，不改变 FrameDigest v1-v3、canonical order、
Playback 公共观察面、owner-thread 合同、runtime-script 无限期延后边界或 Stage 5 状态。
这些变化需要新的 owner/spec 决策和版本门禁。

## 验收

每项退出实现必须提供 characterization、稳定拒绝/迁移测试、diagnostics fingerprint、
identity/digest/canonical parity、Debug/Shared/Release 门禁和适用的 hosted consumer 证据。
