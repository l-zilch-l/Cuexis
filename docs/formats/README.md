# Cuexis Format Index

状态：现行格式索引

更新日期：2026-08-10

## Artifact 分层

```text
Source Project
  cuexis.project.json + Asset Index + Chart/CXT JSON + source/imported assets

CXC Exchange Package
  自包含、只读、可验证的交换和部署包候选

Compiled Runtime
  ChartRuntime、AnimationProgram、World 和缓存，不是持久化交换格式
```

## 权威矩阵

| 内容 | 权威文档 | 状态 |
| --- | --- | --- |
| ProjectConfig v1 | [ADR 0025](../adr/0025-project-config-v1-and-path-security.md) | implemented |
| Asset Index v1/v2 | [ADR 0026](../adr/0026-asset-index-and-source-resolution.md) 与 ADR 0031 | implemented |
| Chart v1/v2/v3 | [CHART_FORMAT.md](../CHART_FORMAT.md) | implemented |
| Chart v4 | [CHART_V4_FORMAT.md](../CHART_V4_FORMAT.md) | candidate, not implemented |
| CXC v1 | [CXC_FORMAT.md](../CXC_FORMAT.md) | candidate, not implemented |
| CXT v1 | [CXT_FORMAT.md](../CXT_FORMAT.md) | accepted subdecision, not implemented |
| Animation Mixing | [ANIMATION_MIXING.md](../ANIMATION_MIXING.md) | accepted Stage 4 semantics |
| Portable Presentation v1 | [PORTABLE_PRESENTATION.md](../PORTABLE_PRESENTATION.md) | implemented |

ADR 记录选择理由，格式文档记录字段和语义。CXC 不重新定义 Chart/CXT；CXT 不重新定义
ChartParameter、Template Binding 或 Animator；Animation Mixing 不重新定义序列化字段。

## 生产与候选边界

当前 Loader 只支持 `cuexis.chart` v1/v2/v3。CXC、Chart v4 和 CXT 尚未进入生产 Schema、Reader、
Writer、validator 或 Playback 公共 API。候选示例位于
[examples/chart_format_update](../examples/chart_format_update/README.md)。

运行时脚本和逐帧脚本回调无限期延后，不是任何 Cuexis 格式的隐藏扩展点。
