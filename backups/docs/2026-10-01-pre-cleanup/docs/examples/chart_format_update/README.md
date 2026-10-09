# Stage Chart Format Update 候选示例

状态：CFU-B 评审入口；CFU-C1 已将接受示例复制为正式 fixture，本目录仍不直接参与生产测试

更新日期：2026-08-11

这些 `.json` 和 `.cxt` 文件用于评审 [ADR 0038](../../adr/0038-cxc-v1-and-chart-v4-boundary.md)、
[CXC_FORMAT.md](../../formats/CXC_FORMAT.md)、[CHART_V4_FORMAT.md](../../formats/CHART_V4_FORMAT.md) 和
[CXT_FORMAT.md](../../formats/CXT_FORMAT.md)。`.cxt` 本身是 UTF-8 JSON。文件名中的
`valid` / `invalid` 表示相对于候选合同的期望，不表示当前 v1/v2/v3 实现已经支持它们。

| 文件 | 期望 | 目的 |
| --- | --- | --- |
| `cxc_manifest_v1.valid.candidate.json` | 接受 | manifest、排序、hash 和完整项目入口 |
| `cxc_manifest_cxt.valid.candidate.json` | 接受 | manifest 显式收录被 Chart 引用的 `.cxt` 闭包 |
| `chart_v4_static_migration.valid.candidate.json` | 接受 | v3 -> v4 空动画的语义等价形状 |
| `chart_v4_animation.valid.candidate.json` | 接受 | Clip、Layer、Group、Instance、循环和 mask |
| `chart_v4_parameterized_transform.valid.candidate.json` | 接受 | 只在允许的 transform/camera 数值位置引用参数 |
| `templates/move-y.valid.candidate.cxt` | 接受 | 单模板 CXT JSON、local Y additive 和固定 application |
| `chart_v4_cxt_template_binding.valid.candidate.json` | 接受 | 两个不同 parent/startBeat/X 的实体复用同一 CXT 与参数 |
| `chart_v4_template_animator.valid.candidate.json` | 接受 | Object Template 保存并实例化完整 Animator component |
| `cxc_manifest_unsorted.invalid.candidate.json` | 拒绝 | entries 不是 path 升序 |
| `cxc_manifest_case_conflict.invalid.candidate.json` | 拒绝 | 两个 entry path 仅大小写不同 |
| `chart_v4_additive_material.invalid.candidate.json` | 拒绝 | Additive 写入 material.opacity |
| `chart_v4_mask_conflict.invalid.candidate.json` | 拒绝 | 相同 priority 的 Layer mask 重叠 |
| `chart_v4_mask_overlap.invalid.candidate.json` | 拒绝 | 同一 mask 的 property 与 prefix 重叠 |
| `chart_v4_animator_deep_patch.invalid.candidate.json` | 拒绝 | Object 对 Animator 内部数组元素执行深层 patch |
| `chart_v4_parameter_asset_use.invalid.candidate.json` | 拒绝 | ParameterRef 被用于 Asset reference 字段 |
| `chart_v4_discrete_partial_weight.invalid.candidate.json` | 拒绝 | 离散属性使用部分 Layer weight |
| `chart_v4_cxt_missing_import.invalid.candidate.json` | 拒绝 | import source 不在 Project/CXC 闭包 |
| `chart_v4_cxt_id_mismatch.invalid.candidate.json` | 拒绝 | Chart import ID 与 CXT templateId 不一致 |
| `chart_v4_cxt_parameter_type.invalid.candidate.json` | 拒绝 | number 参数被用于 rational durationScale |
| `move_y_runtime_script.invalid.candidate.cxt` | 拒绝 | CXT 出现未知 `script` 核心字段并试图执行宿主输入 |

候选稳定诊断：

```text
cxc_manifest_unsorted.invalid.candidate.json
  cxc.entry.order_invalid

cxc_manifest_case_conflict.invalid.candidate.json
  cxc.entry.duplicate

chart_v4_additive_material.invalid.candidate.json
  chart.animation.additive_unsupported

chart_v4_mask_conflict.invalid.candidate.json
  chart.animation.mask_conflict

chart_v4_mask_overlap.invalid.candidate.json
  chart.animation.mask_conflict

chart_v4_animator_deep_patch.invalid.candidate.json
  chart.patch.path_unsupported

chart_v4_parameter_asset_use.invalid.candidate.json
  chart.parameter.use_not_allowed

chart_v4_discrete_partial_weight.invalid.candidate.json
  chart.animation.discrete_weight_unsupported

chart_v4_cxt_missing_import.invalid.candidate.json
  cxt.import.missing

chart_v4_cxt_id_mismatch.invalid.candidate.json
  cxt.template.id_mismatch

chart_v4_cxt_parameter_type.invalid.candidate.json
  chart.parameter.type_mismatch

move_y_runtime_script.invalid.candidate.cxt
  cxt.template.invalid
```

以下违规依赖真实 ZIP envelope，不能由 manifest JSON 单独表达。CFU-C 必须增加二进制 fixture，
分别覆盖 `0xFFFF`/`0xFFFFFFFF` ZIP64 sentinel、ZIP64 locator/extra field、非规范 local/central
metadata、两者不一致、entry range 重叠和 EOCD 后 trailing bytes；规范 writer 还需要跨平台
byte-for-byte golden。

CFU-C1 已把 JSON/CXT/manifest 示例复制到 `tests/fixtures/chart_format_update/`，并由 Schema 与 typed
Reader 交叉验证。依赖 project-document lookup 的缺失 import 和 templateId mismatch 继续由 CFU-C2
resolver 关闭；本目录保留评审文件名和说明，不作为生产测试输入。
