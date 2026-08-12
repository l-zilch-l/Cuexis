# Cuexis Format Index

状态：现行格式索引

更新日期：2026-08-11

## Artifact 分层

```text
Source Project
  cuexis.project.json + Asset Index + Chart/CXT JSON + source/imported assets

CXC Exchange Package
  自包含、只读、可验证的交换和部署包；合同已接受，实现中

Compiled Runtime
  ChartRuntime、AnimationProgram、World 和缓存，不是持久化交换格式
```

## 权威矩阵

| 内容 | 权威文档 | 状态 |
| --- | --- | --- |
| ProjectConfig v1 | [ADR 0025](../adr/0025-project-config-v1-and-path-security.md) | implemented |
| Asset Index v1/v2 | [ADR 0026](../adr/0026-asset-index-and-source-resolution.md) 与 ADR 0031 | implemented |
| Chart v1/v2/v3 | [CHART_FORMAT.md](../CHART_FORMAT.md) | implemented |
| Chart v4 | [CHART_V4_FORMAT.md](../CHART_V4_FORMAT.md) | accepted contract, implementation in progress |
| CXC v1 | [CXC_FORMAT.md](../CXC_FORMAT.md) | accepted contract, implementation in progress |
| CXT v1 | [CXT_FORMAT.md](../CXT_FORMAT.md) | accepted contract, implementation in progress |
| Animation Mixing | [ANIMATION_MIXING.md](../ANIMATION_MIXING.md) | accepted contract; runtime deferred to Stage 4 |
| Portable Presentation v1 | [PORTABLE_PRESENTATION.md](../PORTABLE_PRESENTATION.md) | implemented |

ADR 记录选择理由，格式文档记录字段和语义。CXC 不重新定义 Chart/CXT；CXT 不重新定义
ChartParameter、Template Binding 或 Animator；Animation Mixing 不重新定义序列化字段。

## 生产与实施边界

当前 Playback Loader 只支持 `cuexis.chart` v1/v2/v3。CFU-C1/C2 已提供 CXC manifest、Chart v4 和
CXT 的生产 Schema、内部 typed source Reader、Chart/CXT canonical Writer、参数解析/identity、CXT
import 与 deterministic lowering。严格 ZIP32 archive、CXC package API、工具和 Playback 门禁仍未
关闭。评审示例位于
[examples/chart_format_update](../examples/chart_format_update/README.md)。

运行时脚本和逐帧脚本回调无限期延后，不是任何 Cuexis 格式的隐藏扩展点。
