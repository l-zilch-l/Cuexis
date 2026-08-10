# Stage Chart Format Update 候选示例

状态：CFU-B ADR 评审输入；CXT/参数子决策已接受，但不是当前 Loader 可接受的生产 fixture

更新日期：2026-08-10

这些 `.json` 和 `.cxt` 文件用于评审 [ADR 0038](../../adr/0038-cxc-v1-and-chart-v4-boundary.md)、
[CXC_FORMAT.md](../../CXC_FORMAT.md)、[CHART_V4_FORMAT.md](../../CHART_V4_FORMAT.md) 和
[CXT_FORMAT.md](../../CXT_FORMAT.md)。`.cxt` 本身是 UTF-8 JSON。文件名中的
`valid` / `invalid` 表示相对于候选合同的期望，不表示当前 v1/v2/v3 实现已经支持它们。

| 文件 | 期望 | 目的 |
| --- | --- | --- |
| `cxc_manifest_v1.valid.candidate.json` | 接受 | manifest、排序、hash 和完整项目入口 |
| `cxc_manifest_cxt.valid.candidate.json` | 接受 | manifest 显式收录被 Chart 引用的 `.cxt` 闭包 |
| `chart_v4_static_migration.valid.candidate.json` | 接受 | v3 -> v4 空动画的语义等价形状 |
| `chart_v4_animation.valid.candidate.json` | 接受 | Clip、Layer、Group、Instance、循环和 mask |
| `templates/move-y.valid.candidate.cxt` | 接受 | 单模板 CXT JSON、local Y additive 和固定 application |
| `chart_v4_cxt_template_binding.valid.candidate.json` | 接受 | 两个不同 parent/startBeat/X 的实体复用同一 CXT 与参数 |
| `cxc_manifest_unsorted.invalid.candidate.json` | 拒绝 | entries 不是 path 升序 |
| `chart_v4_additive_material.invalid.candidate.json` | 拒绝 | Additive 写入 material.opacity |
| `chart_v4_mask_conflict.invalid.candidate.json` | 拒绝 | 相同 priority 的 Layer mask 重叠 |
| `chart_v4_cxt_missing_import.invalid.candidate.json` | 拒绝 | import source 不在 Project/CXC 闭包 |
| `chart_v4_cxt_parameter_type.invalid.candidate.json` | 拒绝 | number 参数被用于 rational durationScale |
| `move_y_runtime_script.invalid.candidate.cxt` | 拒绝 | CXT 出现未知 `script` 核心字段并试图执行宿主输入 |

候选稳定诊断：

```text
cxc_manifest_unsorted.invalid.candidate.json
  cxc.entry.order_invalid

chart_v4_additive_material.invalid.candidate.json
  chart.animation.additive_unsupported

chart_v4_mask_conflict.invalid.candidate.json
  chart.animation.mask_conflict

chart_v4_cxt_missing_import.invalid.candidate.json
  cxt.import.missing

chart_v4_cxt_parameter_type.invalid.candidate.json
  chart.parameter.type_mismatch

move_y_runtime_script.invalid.candidate.cxt
  cxt.template.invalid
```

ADR 0038 整体接受后，CFU-C 将把这些示例复制或转换为 `tests/fixtures/` 中由 CXC/CXT/Chart
Schema、typed Reader、validator 和 compiler 共享的正式 fixture。
