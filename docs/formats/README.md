# Cuexis Format Index

状态：现行格式索引

更新日期：2026-08-30

## Artifact 分层

```text
Source Project
  cuexis.project.json + Asset Index + Chart/CXT JSON + source/imported assets

CXC Exchange Package
  自包含、只读、可验证的交换和部署包；合同与实现已通过 CFU-F，CFU-G hosted 验证与 G6 封存已完成

Compiled Runtime
  ChartRuntime、AnimationProgram、World 和缓存，不是持久化交换格式
```

## 权威矩阵

| 内容 | 权威文档 | 状态 |
| --- | --- | --- |
| ProjectConfig v1 | [ADR 0025](../adr/0025-project-config-v1-and-path-security.md) | implemented |
| Asset Index v1/v2/v3 | [ADR 0026](../adr/0026-asset-index-and-source-resolution.md)、ADR 0031 与 [MATERIAL_SHADER.md](MATERIAL_SHADER.md) | implemented; v3 adds `shader` |
| Chart v1/v2/v3 | [CHART_FORMAT.md](CHART_FORMAT.md) | implemented |
| Chart v4 | [CHART_V4_FORMAT.md](CHART_V4_FORMAT.md) | accepted and implemented; C1–C4, CFU-D/E/F/G gates closed; Stage 4 animation runtime closed |
| CXC v1 | [CXC_FORMAT.md](CXC_FORMAT.md) | accepted and implemented internally; archive/tools and Playback source/prepare/identity gates closed; no public CXC package API |
| CXT v1 | [CXT_FORMAT.md](CXT_FORMAT.md) | accepted contract; Reader/Writer/lowering and prepare import/lookup implemented; CFU-F and G4 hosted gates closed; Stage 4 animation execution closed |
| Animation Mixing | [ANIMATION_MIXING.md](ANIMATION_MIXING.md) | accepted contract; format-stage gates closed; Stage 4 runtime closed |
| Portable Presentation v1 | [PORTABLE_PRESENTATION.md](PORTABLE_PRESENTATION.md) | implemented |
| Material/Shader v1 | [MATERIAL_SHADER.md](MATERIAL_SHADER.md) | accepted contract; S5-A through S5-H completed; Stage 5 closed and merged into `master` 2026-08-28 |

ADR 记录选择理由，格式文档记录字段和语义。CXC 不重新定义 Chart/CXT；CXT 不重新定义
ChartParameter、Template Binding 或 Animator；Animation Mixing 不重新定义序列化字段。

## 生产与实施边界

Playback 继续保留 `cuexis.chart` v1/v2/v3 生产路径，并已能 prepare 静态/参数化 v4 与求值非空合法
动画。S4-F 已把 animation capability 加入默认 Playback 集合；S4-G 已关闭本地安全、分配与性能门禁；
S4-H 已关闭 hosted 验收，Stage 4 已完成。CFU-C1/C2 已提供 CXC manifest、Chart v4 和 CXT 的生产 Schema、内部 typed
source Reader、Chart/CXT canonical Writer、参数解析/identity、CXT import 与 deterministic
lowering。CFU-C3 已提供内部 strict ZIP32 archive/package、owning file/memory loader、
package-backed Asset ContentProvider 和独立 project-document table；CFU-C4 已提供
developer pack/validate/unpack tools 和 round-trip gates。CFU-D1/D2 已关闭：显式
`migrateToV4` / `--target 4` JSON lift，默认 CLI 仍输出 v3。CFU-E 已关闭公共 API、source
factory、prepare/capability 与 `PreparedSemanticIdentity`。CFU-D3 已关闭 Playback
FrameSnapshot / FrameDigest v3 / seek-stop 等价。整包 CFU-D 已于 2026-08-14 经项目所有者
记录“未提供外部资产”并关闭；兼容窗口不缩短。CFU-F 已关闭最终实现 SHA 的 hosted consumer、
确定性、安全与性能门禁；CFU-G hosted 验证、completion report 和 owner acceptance 已完成，格式阶段已封存。
该检查点仍不是完整 CXC 产品支持
或公共 package API。评审示例位于
[examples/chart_format_update](../examples/chart_format_update/README.md)。

运行时脚本和逐帧脚本回调无限期延后，不是任何 Cuexis 格式的隐藏扩展点。
