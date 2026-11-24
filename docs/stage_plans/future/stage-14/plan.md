# Stage 14 计划（提纲）：稳定 ABI 与 Playback SDK v1

状态：future；未启动，仅为方向提纲

更新日期：2026-11-23

<!-- cuexis-meta
id: PLAN-future.stage-14.plan
status: future
authority: none
-->

本文件是**未启动阶段的提纲**，不是执行计划。启动本阶段时，用 [plan.md](../../../templates/plan.md)（详细规划协议）重写，并在变更记录里注明替换关系。

归档来源：原 Stage 12 稳定 ABI 计划的重新编号版本，旧路径映射见 [legacy-paths](../../legacy-paths.md)；阶段位置见 [ROADMAP](../../../ROADMAP.md) 与 [Stage 13](../stage-13/plan.md) 的可选能力边界。

## 阶段目标

候选，未承诺：在 C++ preview、Gameplay/Replay 生命周期、发行格式和目标平台证据稳定后，冻结稳定 C ABI，提供薄 C++ RAII wrapper，并发布正式 Cuexis Playback SDK v1。

## 范围与边界

- 稳定 C ABI 的唯一归属阶段是 Stage 14；Stage 7A/7.5/8 只交付 C++ typed preview（见 [ROADMAP](../../../ROADMAP.md)）。
- 本阶段不增加第二套 Playback、Judgement、Replay 或 Presentation 语义，也不必等待 Stage 13 全部可选能力完成。
- 承接 [RR §24](../../active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)实际接受的公共类型分类、兼容迁移和内部头治理；稳定 C ABI 在此基础上冻结，不重新建立一套 SDK/应用基础架构。RR 实验验证、Stage 8 的 Chart v5 + Gameplay v3 正式发行与 Judgement 默认 SDK 晋升分别取证；正式 v3 作为新创作默认和已验证播放组合，不使所有实验接口自动公开，也不替代 SDK 成熟度与逐组退役条件。已承诺旧内容兼容保留，不再要求完整新交付 v2 正式化。
- 与 [Stage 13](../stage-13/plan.md) 的边界：只纳入已接受的高级表现 capability，v1 不因该阶段其余可选能力未完成而阻塞。

## 入口条件

- Stage 7A 公共生命周期、external consumer 与确定性回放门禁完成。
- Stage 8 Chart v5 + Gameplay v3 正式发行合同及验收完成，纳入 v1 的 Stage 7.5 capability 已明确；旧内容兼容范围有具名验证。
- [Stage 9](../stage-09/plan.md)、[Stage 11](../stage-11/plan.md) 与 [Stage 12](../stage-12/plan.md) 的 Presentation、性能和目标平台矩阵已被接受。
- 公共 FrameSnapshot、JudgementResult、ReplayData、ContentProvider 与错误生命周期无开放语义问题。
- 若纳入 Stage 13 的高级表现能力，其 capability 与预算合同已接受。
- RR 公共面清单、旧/新消费者对照、逐组迁移记录和内部依赖边界可复核；尚未满足退出条件的旧入口继续有具名保留策略。v1 纳入的默认/实验能力及 Judgement 成熟度决定明确，不能以 RR 已实现、Stage 8 正式 v3 验收或本阶段启动自动晋升公共接口。

## 已确认约束

- 稳定 C ABI 的唯一归属是 Stage 14；Stage 7A/7.5/8 只交付 C++ typed preview。
- 不增加第二套 Playback、Judgement、Replay 或 Presentation 语义。
- 沿 [D-19](../../active/stage-07-rr/architecture-design.md#d-19-sdk-示例与宿主试用)逐组退役：至少一次实际过渡发布、替代稳定、参考应用和独立安装消费者证据齐备后，才按版本规则批准删除；稳定 C ABI 发布不自动授权清空旧 C++ API。共享类型保留到所有受支持入口不再依赖，已完成的迁移复用原证据，未完成组单独核验，不设统一删除版本。
- 应用会话控制器、`app/player_support`、内部 Writer 和作者工具不进入稳定 ABI；编辑器仍决定损失覆盖。各平台 Player 的设备等待超时由应用实现，SDK 只交付操作状态与实际结果，不新增统一超时策略或默认期限。C ABI 映射已有能力与生命周期，不改变这些职责。
- C consumer 不依赖 C++ 标准库、异常、RTTI 或第三方实现类型；C++ wrapper 与 C consumer 对相同输入产生相同 FrameSnapshot/Judgement/Replay 结果。
- 若采用 PD 基线，沿 [PD 细则 §12.7](../../completed/stage-07/04-pd/design.md) 核对跨 Close owning 结果、取消终态和释放责任。内部 shared_ptr、optional、span 和借用读桥不成为 C ABI 布局；opaque handle 的 allocator、有效期、释放线程、CRT/模块卸载限制必须独立接受，不能由引用计数推断安全。
- 若 v1 纳入未来重建任务，须补任务 handle/取消/关闭矩阵；未纳入则明确不支持，不要求本阶段等待全部 Stage 13 可选能力。

## 候选方向（非承诺）

启动时可能按以下工作线细化，均未承诺：

- ABI 合同：opaque handle、allocator、字符串、数组、回调、快照有效期、线程 owner、重入、取消和释放合同。
- 版本与兼容：独立 C ABI version、符号可见性、capability 查询、兼容和弃用政策。
- 平台矩阵：Windows CRT、Debug/Release、static/shared 和支持平台矩阵。
- 宿主与消费者：至少一个正式宿主 adapter、纯 C external consumer，以及安装、升级、部署和许可证文档。
- 薄 C++ RAII wrapper，与 C ABI 共享同一语义实现。

## 待决问题（启动前必须回答）

1. `1.x` 版本政策由哪份 ADR 接受？由 ABI owner 与 owner 决定。
2. 实际发布版本 `1.0.0` 与同 SHA 证据何时批准？由 owner 在发行前决定。
3. v1 是否纳入 Stage 13 的重建任务 handle/取消/关闭矩阵？由 owner 在启动时决定。
4. 支持平台矩阵（Windows CRT、Debug/Release、static/shared 及具体平台）的最终范围由谁冻结？
5. 许可证、部署与升级文档由谁批准并作为发行证据？

## 验收标准

启动时定义。当前只写方向性判据：

- C consumer 不依赖 C++ 标准库、异常、RTTI 或第三方实现类型。
- 正反例覆盖对象、错误、回调、快照、版本拒绝和二进制兼容。
- C++ wrapper 与 C consumer 对相同输入产生相同 FrameSnapshot/Judgement/Replay 结果。
- `1.x` 版本政策有接受的 ADR；实际 `1.0.0`、同 SHA 证据和 owner acceptance 完整。
- 至少一个正式宿主 adapter 与纯 C external consumer 可作为消费证据。
- 安装包独立消费者不能取得 app/内部头或作者工具依赖；保留组旧消费者与获准退出组的新消费者分别验证。每个删除组有实际过渡发布、稳定性与接受记录，Judgement 晋升有独立决定；不以 ABI 测试通过代替格式发行、平台性能或未测旧语料认证。

具体矩阵、命令、数值与证据形式在启动时由 owner 与规划模型制定，本提纲不承诺。

## 明确不包含

- 宿主插件 ABI、编辑器 ABI 或渲染后端 ABI，除非另有 ADR。
- 暴露 RuntimeSession、World、EnTT、SDL、OpenGL、JSON DOM 或宿主引擎类型。
