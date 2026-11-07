# ADR 0024：配置所有权与格式分阶段冻结

日期：2026-07-18

状态：已接受

## 背景

Cuexis 的项目内容、用户偏好、设备能力、设备校准和单次启动选项具有不同的持久化位置、生命周期与确定性要求。如果把它们合并为任意模块可读写的全局配置，或使用无类型的“最后写入者优先”规则，项目行为会受本机状态隐式影响，设备硬限制也可能被普通偏好绕过。

这些配置不会在同一阶段首次消费。阶段 1A 需要先冻结跨阶段职责和阶段 1B ProjectConfig 的格式身份，但不能因此提前设计尚无真实消费者的 ProjectConfig v1 字段、设备预算、用户设置或校准 Schema。

## 决策

### 所有权与组合边界

Player 与 Studio 的应用组合层共享 Project/Config 前端，负责定位、读取、迁移、校验和诊断持久化文件。引擎模块不读取配置文件，也不反向修改配置来源；它们只接收已经校验、确定字段所有权的 typed config 或不可变快照。

配置职责固定如下：

| 类别 | 唯一职责与所有者 | 生命周期与持久化 |
| --- | --- | --- |
| ProjectConfig | Player 与 Studio 共享的项目身份、资产根、入口内容和项目默认策略 | 项目根目录中的版本化文件，可随项目提交 |
| UserPreferences | 各应用的本机表现偏好和已经实现的当前 profile ID | 应用的用户配置目录；不得保存项目确定性内容或复制 profile 字段 |
| DeviceProfile | 创建前能力要求、硬预算和兼容目标约束 | 随发行包提供；由 PreflightCapabilities 匹配，不是用户偏好或探测结果副本 |
| PreflightCapabilities | 创建 Window、RenderBackend 和 Audio 前实际可知的能力事实 | 进程内只读数据，不是文件 |
| AudioDeviceProfile | 音频输出设备匹配身份与输出校准 | 用户本机版本化文件；不保存输入延迟或主观判定校准 |
| InputProfile | 输入设备绑定与输入延迟 | 用户本机版本化文件；不保存输出校准或判定规则 |
| CalibrationProfile | 用户主观时序偏移 | 用户本机版本化文件；不保存谱面 offset、设备延迟或判定规则 |
| LaunchOptions | 项目定位、测试、诊断和本次会话的显式选项 | 仅当前进程，不自动写回任何来源 |
| ResolvedAppConfig | 创建各应用子系统所需的已校验请求值及来源 | 应用层生成的不可变快照，不是文件或后端所有者 |
| ResolvedSessionConfig | 单次播放或预览中影响 Runtime、输入和确定性结果的最小配置子集 | Session 创建前冻结，不在活动 Session 中回读可变来源 |
| EffectiveSettings | 子系统创建后的实际能力、生效值和协商回退 | 子系统只读诊断输出，不回写或覆盖请求来源 |

Chart、Behavior、Animation、Material、Shader 和 Particle 的领域参数继续属于各自版本化文档或资产，不得迁入 ProjectConfig 或 UserPreferences。派生资源和 Shader 目标格式由版本化 ImporterProfile / ShaderTargetProfile 唯一拥有，ProjectConfig 只引用其身份，DeviceProfile 只声明兼容身份或能力约束。

### 覆盖与约束

配置组合不提供无类型递归合并、任意 key/value 注入或通用“最后写入者优先”层级。每个字段必须有唯一来源所有者和一个代码默认值来源，并在首次实现时声明其为启动期静态、可显式动态应用或需要重建 Session/Backend。

组合遵循以下规则：

```text
ProjectConfig 的确定性项目内容不能被 UserPreferences 隐式覆盖
UserPreferences 只影响其明确拥有且已存在消费者的字段
LaunchOptions 只能覆盖逐项命名并明确允许的请求字段
PreflightCapabilities 是事实输入，不是覆盖层
DeviceProfile 提供选择条件、兼容约束和硬上限，不参与普通值合并
用户偏好和 LaunchOptions 不得突破 DeviceProfile 或模块硬约束
ResolvedAppConfig / ResolvedSessionConfig 是组合结果，不能反向修改来源
EffectiveSettings 只报告实际结果，不自动写回、校准或改变缓存身份
```

如果 ProjectConfig 选择的 ImporterProfile / ShaderTargetProfile 与 DeviceProfile 不兼容，初始化必须稳定失败；运行时不得静默修改项目、重新导入、替换缓存身份或重选 DeviceProfile。某个可调值超过硬约束时，具体是显式裁剪、确定性降级还是失败，由该字段首次消费阶段冻结并产生诊断，不能静默处理。

跨阶段解析和创建顺序固定为：先用最小 LaunchOptions 定位项目并加载 ProjectConfig，再收集 PreflightCapabilities、加载当阶段已经定义的 profile 与 UserPreferences，随后生成各模块 typed config、应用能力与硬约束并发布 ResolvedAppConfig。子系统创建后只把实际结果汇总为 EffectiveSettings；实际能力复核通过后才能创建 AssetDatabase 和 RuntimeSession。复核失败不得通过静默重选 profile 或循环重建子系统恢复。

### ProjectConfig 的近期格式身份

阶段 1B 的 ProjectConfig 是项目根目录下固定名称的 UTF-8 JSON 文件：

```text
<project-root>/cuexis.project.json
```

其顶层 `format` 身份固定为：

```text
cuexis.project
```

应用只能通过该固定身份识别 ProjectConfig，不得根据字段形状、文件内容或其他扩展名启发式猜测格式。显式给出的 ProjectConfig 文件路径必须指向上述固定文件名，并以其父目录作为项目根；给出项目目录时只定位上述固定文件名。

本 ADR 不冻结 ProjectConfig v1 Schema。顶层字段集合、`version` 的具体表示、项目 ID、资产根、入口内容、扩展区、默认值、路径约束、迁移和未知字段策略均在阶段 1B 随真实 AssetDatabase 与 RuntimeSession 消费者一并定义和测试。

### 失败、写入与诊断

缺失、损坏、`format` 不匹配、版本不支持、迁移失败或校验失败的 ProjectConfig 必须使项目加载失败，不能用默认项目替代。所有可写配置采用同目录临时文件、完成写入与校验后原子替换；失败时保留上一有效文件，迁移不得先破坏原文件。

UserPreferences 缺失、损坏或版本不支持时，可以记录诊断并使用该应用唯一代码来源的安全默认值。DeviceProfile 损坏时只能失败，或按该格式首次消费阶段明确规定的、显式命名的内建 profile 回退。UserPreferences 缺失或安全重置可以选择文档化的内建 profile ID；已经显式选择的 profile 缺失、损坏、版本过新或无法匹配时不得静默换用其他 profile。

解析、迁移、约束和回退必须产生结构化诊断。诊断至少能够稳定标识配置类别、来源类型、错误码、严重级别和字段路径，并按格式定义的稳定顺序输出；来源追踪记录最终请求值及其所有者，但日志不得暴露不必要的本机绝对路径或配置数据。配置失败不得留下部分发布的 ResolvedAppConfig、ResolvedSessionConfig 或已被改写的来源文件。

### 按首次消费阶段冻结格式

格式只在存在真实消费者、输入输出、失败路径和测试时冻结。阶段安排如下：

| 配置或快照 | 首次冻结阶段 |
| --- | --- |
| ProjectConfig 文件名与 `format` 身份 | 阶段 1A，由本 ADR 冻结 |
| ProjectConfig v1 Schema、迁移和未知字段策略 | 阶段 1B |
| ImporterProfile / ShaderTargetProfile 的格式与缓存身份 | 阶段 5B |
| Player UserPreferences、最小 AudioDeviceProfile、完整应用配置组合和快照规则 | 阶段 6 |
| StudioPreferences 及 Studio 写回行为 | 阶段 7 |
| Desktop DeviceProfile 的文件身份、Schema、匹配和预算 | 阶段 9A |
| Android DeviceProfile | 阶段 9B 恢复实施时 |
| InputProfile / CalibrationProfile 及 UserPreferences 中对应选择 ID | 阶段 11 |

LaunchOptions、PreflightCapabilities、ResolvedAppConfig、ResolvedSessionConfig 和 EffectiveSettings 不是持久化通用格式；其 typed 字段只随实际消费者增加。阶段 1B 可以实现定位 ProjectConfig 所需的最小启动输入，但不能借此提前冻结阶段 6 的完整配置组合 API。

本 ADR 明确不定义 UserPreferences、DeviceProfile、AudioDeviceProfile、InputProfile 或 CalibrationProfile 的文件名、`format` ID、字段、版本和迁移，也不定义阶段 6、9 或 11 的具体 Schema 和预算。

## 备选方案

### 全局可变 EngineConfig

拒绝。它会让模块自行读取和修改不属于自己的设置，使线程、生命周期、来源追踪和确定性边界不可验证。

### 通用配置层按优先级递归合并

拒绝。ProjectConfig、用户偏好、能力事实和设备硬约束不是同一种值来源，统一层叠会允许本机状态隐式改变项目内容或绕过硬限制。

### 在阶段 1A 一次性定义所有配置 Schema

拒绝。阶段 6、9 和 11 尚无足够的真实消费者与测量依据，提前冻结只会产生占位字段、重复所有权和不可靠迁移承诺。

### ProjectConfig 使用任意文件名或启发式识别

拒绝。它会让项目根定位、诊断和 Studio/Player 互操作产生歧义，并增加错误格式被误读的风险。

## 影响

阶段 1B 必须复用应用侧结构化读取和字段路径诊断基础，实现 `cuexis.project.json` / `cuexis.project` 的 v1 Schema；Player 与 Studio 后续共享同一 ProjectConfig 加载、迁移和规范化路径。引擎模块的公共接口只接收所需 typed config，不暴露 JSON 值或全局配置访问器。

所有新增持久化字段必须在其首次消费阶段同时覆盖默认值、解析、范围、版本迁移、未知字段、来源追踪、原子写入和失败测试。影响确定性结果的值必须进入 ResolvedSessionConfig 的规范化身份或内容 hash。

## 后续风险

不同阶段逐步增加 typed config 时可能形成重复默认值或来源记录。评审必须持续检查单一字段所有者和单一默认值来源，并阻止为了复用解析器而建立无边界的通用字典。

项目根可能通过命令行、文件选择器或平台入口获得；这些入口可以不同，但最终必须解析到同一个固定 ProjectConfig 文件与项目根语义。

## SDK 转型补充（2026-07-20）

ProjectConfig 继续是 Player/Studio 的标准文件入口；嵌入宿主可以直接提供 typed/memory project source、HostCapabilities 和 ResolvedSessionConfig，不读取 Player/Studio UserPreferences。Playback SDK 和内部模块仍不得自行读取应用配置目录或反向修改来源。

原启动顺序适用于 Filesystem 应用组合。嵌入模式用宿主 ContentProvider 和 capability snapshot 替代物理项目定位与平台探测，但必须在创建 PlaybackSession 前完成同等 typed 校验、预算约束和来源记录。JudgementConfigSnapshot 与 ReplayData 的具体格式在阶段 11 随真实消费者冻结。
