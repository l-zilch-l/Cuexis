# Chart v5 CXT 调用合同（候选设计，未实现）

状态：candidate

更新日期：2026-11-17

<!-- cuexis-meta
id: PROPOSAL-CXT-CHART-V5-INVOCATION
status: candidate
authority: none
-->

本文原为 `docs/formats/CXT_V2_FORMAT.md` §6，2026-11-17 因“全仓无 `cxtImports` /
`cxtInstances` 的 reader、writer 或 validator”而移出格式合同，保留为候选设计输入。
它不是现行合同；实现前不得据此改代码或宣称能力（RR-F115）。

以下是 Chart v5 新的**候选 Core 集成字段片段**，不是完整合法 Chart：

```json
{
  "cxtImports": [
    { "id": "pattern.stair", "source": "templates/stair.cxt" }
  ],
  "cxtInstances": [
    {
      "bindingId": "intro-stair",
      "module": "pattern.stair",
      "export": { "kind": "pattern", "id": "stair" },
      "startBeat": { "numerator": 16, "denominator": 1 },
      "parent": null,
      "parameters": [
        { "id": "groups", "value": 4 }
      ],
      "slotBindings": []
    }
  ]
}
```

import 的 ID/source 均唯一，source 遵守 project-relative portable path 和小写 `.cxt`
规则，并与 CXT moduleId 匹配。CXT 不伪装成 Asset record。bindingId 在整个 Chart 内
唯一；module/export 必须解析且类型一致。未绑定 Parameter 使用 default，未知参数、
重复参数和错误类型拒绝。parent 只允许显式 Chart object reference 或 null。

`startBeat` 是 rational literal，作为统一 invocation 时间平移，**只在生成
Requirement start/end Beat 时加一次**；不平移资源、Transform 或全局 Timing。
Prototype/Pattern 的 Beat 都是 invocation-local，负值可用但须符合最终 Chart 范围。
duration 值若未来开放不加 startBeat，必须注册为不同的时间单位。

Core cxtInstances 的 export kind 只接受 prototype/pattern。prototype invocation
生成一个实体，`slotBindings` 使用 `{ "slot": "...", "value": typed literal }`；
pattern invocation 的 slotBindings 必须为空，Pattern 自己完成 emit bindings。
这是一套新字段，不把 v4 `animationTemplateImports` 或 Animator Template Binding
误用为实体生成入口。Animation 模块依旧由版本化 Animator binding 集成。

生成实体与显式实体最终合并到同一 Canonical Semantic Chart；不新增作者层 notes
数组。首版不允许 Chart 字段用字符串猜测 generated ID 并跨 invocation 绑定它；
这种目标选择能力需要独立 typed reference 合同。

展开只能由 `CxtV2Loader::expand` 经工具显式传参触发；默认 Playback 路径不消费这两个字段。
