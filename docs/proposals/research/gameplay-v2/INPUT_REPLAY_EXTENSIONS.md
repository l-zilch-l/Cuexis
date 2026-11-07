# V2 Input、Replay 与扩展注册

状态：candidate；研究提案，未接受，未实施

更新日期：2026-10-02

## 1. Input 不是设备事件转发

V2 把 L1 定义成“规范观测生成器”，而不是简单映射层。它负责：

- 设备事件排序与单一 ingress sequence；
- 时间域转换、连续量重采样和 discontinuity；
- contact/axis/orientation 的统一逻辑模型；
- domain/action vocabulary 映射；
- 设备能力和最低上报率协商；
- 将不确定的设备差异显式变成拒绝或声明的降级策略。

谱面只引用稳定的 `action` 和 `domain` 名称，不引用键盘扫描码、SDL 枚举、触摸指针地址或设备序列号。映射 profile 属于 session；它不会改变 Canonical Gameplay Graph。

## 2. Replay 规范

Replay 的权威载荷是规范化 Observation 流，而非原始设备包。头部建议包含：

```text
formatVersion
engineJudgementIdentity
rulesetIdentity
chartSemanticIdentity
sessionJudgementIdentity
normalizationProfileMetadata   诊断用途，可不作为校验门
lateEventPolicy
eventCodecId
eventCount / byteBudget
```

Replay 校验使用规范化流、Canonical Graph、Ruleset 和 engine tables。换键位、换手台、换渲染器、换皮肤和改 source 注释不应使同一规范流失效。若规范化算法本身改变，应提升 engine/input semantic revision，并明确旧 Replay 的接受或拒绝矩阵。

实时输入与 Replay 必须进入同一 `submit observation -> advance -> coordinate -> fold` 路径；不能为 Replay 保留一套“直接发 Fact”的快捷路径。

## 3. Session transaction

Input mapping、CalibrationProfile、Loadout、Ruleset Package 和 Chart Graph 在 `prepare` 前形成 session transaction。提交失败时保留旧 session；运行中不回读配置文件、不读取宿主状态、不追溯修改已提交 Fact。

设备热插拔只允许三种明确结果：继续产生满足声明能力的规范流、暂停等待新 profile 并保持旧状态、或终止 session 并给出诊断。禁止把缺失样本猜成静止、成功或自动补按。

## 4. Capability registry

每个扩展能力使用全局唯一 `capabilityId` 和单调 `revision`，并声明：

```text
semanticKind
requiredFormat
staticBudget
snapshotCost
replayImpact
supportedDomains
stableRejectCode
```

能力来源分为 engine、ruleset package、chart module、session adapter 四类。扩展不能通过覆盖既有 ID 的含义来“更新”；改变语义必须新增 ID 或 revision，并更新 canonical identity 和迁移表。

支持矩阵至少区分：未知、已知但未启用、已启用且可用、已知但资源/预算不足。四者必须有不同诊断，不能都变成 `unsupported_capability`。

## 5. 兼容性层次

V2 将兼容性拆成四层：

1. **格式兼容**：能否解析 entry 和版本。
2. **语义兼容**：能否得到相同 Canonical Graph。
3. **判定兼容**：engine/ruleset/session 是否能产生相同 Fact Ledger。
4. **设备兼容**：当前 L1 是否能提供声明的规范事件能力。

向后兼容只在对应层明确声明时成立。格式能解析不代表判定可重放；设备能连通不代表输入能力满足要求。
