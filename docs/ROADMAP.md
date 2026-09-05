# Cuexis Roadmap

状态：现行路线图

更新日期：2026-09-05

产品边界由 [ADR 0027](adr/0027-playback-sdk-product-boundary.md) 冻结。本文维护
阶段顺序、依赖关系和交接，不复制阶段内部的完整字段合同或完成证据。

## 路线原则

Cuexis 的实施顺序以可玩的音游闭环和可演进的格式合同为中心：

```text
稳定当前产品基线
  -> 建立 Chart v5 存储基础和 v5-first candidate path
  -> 冻结最小 Input / Judgement / Score / Replay 闭环（Stage 7A）
  -> 正式冻结高密度 Chart 发行格式（Stage 8）
  -> 持续扩展高级 Input / Judgement 能力（Stage 7B+）
  -> 扩展 Presentation Environment 与模型表现
  -> 建设 Studio 创作工作流
  -> 验证性能、移动端和其他后端
  -> 冻结稳定 SDK ABI
```

Chart 格式不能脱离 Judgement 语义单独演进。CXC 是发行容器，不是独立的玩法系统；
天空盒、模型、形变和后处理属于 Presentation 扩展；运行时脚本不在当前主路线中。
共同玩法抽象见 [音乐游戏玩法抽象模型](architecture/GAMEPLAY_ABSTRACTION_MODEL.md)。

## 当前状态

Stage 5 已于 2026-08-28 关闭并合并至 `master`；260830 follow-up 已于 2026-09-01
完成。当前下一实施阶段为 Chart Format Foundation。它先解决 Chart v5 的高密度物理
存储和 CXT v2 Core，再进入以 Chart v5 Core/Packed candidate 为主要开发基线的 Stage 6。
完整 Chart v5 尚未正式发行；Input/Judgement 和 Studio 也均未进入生产实施。

历史交接状态保持如下：CFU-F consumers and determinism closed; final-SHA hosted gates passed
2026-08-16。CFU-G final closure                                 completed; G6 owner acceptance recorded
2026-08-24。Stage 5 已于 2026-08-28 关闭；stage_plans/completed/260830-followup/plan.md
记录 Chart/CXC parse-once 和关键模块分支覆盖率的维护交接。

```text
生产 Playback 兼容基线   Chart v4 / CXT v1 / CXC v1 / SDK 0.7.0
下一实施阶段              Chart Format Foundation
Stage 6 主要开发基线      Chart v5 Core / Packed candidate
Stage 6 兼容回退基线      Chart v4 / CXT v1 / CXC v1
Stage 7A                   最小 Input / Judgement / Score / Replay Kernel
Stage 7B+                  Slide、多指、校准和高级判定能力持续演进
Stage 8                   Chart v5 正式发行与语义收敛
当前默认发行格式          Chart v4
Chart v5                  candidate；Stage 8 后成为默认发行格式
运行时脚本                无限期延后
```

## 已完成阶段

| 阶段 | 交付 | 证据 |
| --- | --- | --- |
| Stage 0 | 工程骨架、Core、Platform、World、OpenGL Player | [报告](stage_reports/stages/stage-00/completion.md) |
| Stage 1A-1E | Chart、Project、资源、Behavior、Audio、Prepared Playback、C++ consumer | [报告](stage_reports/stages/stage-01/README.md) |
| Stage 2 | Chart v3、TimingMap、Behavior/Step Event | [报告](stage_reports/stages/stage-02/completion.md) |
| Stage 3 | Portable Presentation、Validation、OpenGL adapter | [报告](stage_reports/stages/stage-03/completion.md) |
| Stage 4 | 表现动画运行时、CXT v1、Animation Mixing | [报告](stage_reports/stages/stage-04/completion.md) |
| Stage 5 | Material/Shader 管线和能力 Profile | [报告](stage_reports/stages/stage-05/completion.md) |

## 主实施路线

| 阶段 | 目标 | 生产基线 | 主要交付 |
| --- | --- | --- | --- |
| [Chart Format Foundation](stage_plans/future/chart-format-foundation/plan.md) | Stage 6 前解决高密度谱面存储风险 | Chart v4 subset / v5 candidate tools | CXT v2 Core、Packed 原型、40k/16 MiB 门禁、CXC entry 设计 |
| [Stage 6](stage_plans/future/stage-06/plan.md) | 以 v5 为主线完成 Playback/Player/CXC candidate path | Chart v5 Core/Packed candidate；v4 fallback | v5 subset 播放验证、Player、CXC v1、媒体、后端中立渲染 |
| [Stage 7A](stage_plans/future/stage-07/plan.md) | 冻结最小可玩闭环和 Judgement Kernel | Chart v5 Core/Packed + v4 compatibility | Input、Judgement、Score、Replay、Tap/Hold/Release |
| [Stage 7B+](stage_plans/future/stage-07/plan.md) | 持续扩展高级输入与判定能力 | Stage 7A contracts + selected v5 capabilities | Slide、Flick、方向、连续轨迹、多指、校准 |
| [Stage 8](stage_plans/future/stage-08/plan.md) | Chart v5 正式发行和语义收敛 | v5 candidate + Stage 7A | v5 Spec、CXT v2、Packed、CXC playback entry、迁移、默认 Writer |
| [Stage 9](stage_plans/future/stage-09/plan.md) | Presentation 扩展 | Chart v5 | 天空盒、环境、静态模型、有限形变 |
| [Stage 10](stage_plans/future/stage-10/plan.md) | Studio 创作工作流 | Chart v5 / Packed Chart | 编辑、预览、编译、打包、资源闭包 |
| [Stage 11](stage_plans/future/stage-11/plan.md) | 规模化和平台扩展 | Chart v5 | 性能、移动端、Vulkan、粒子和高级表现 |
| [Stage 12](stage_plans/future/stage-12/plan.md) | 稳定 SDK v1 | Chart v5 / CXC v1 | 稳定 C ABI、公共生命周期和发布政策 |

## 阶段依赖

```text
Stage 5
  -> Chart Format Foundation
  -> Stage 6
      -> Stage 7A
          -> Stage 8
              -> Stage 9
                  -> Stage 10
                      -> Stage 11
                          -> Stage 12

Stage 7A
  -> Stage 7B+ (持续能力线，可跨越 Stage 8 继续)
  -> Stage 8 (仅依赖 7A，不等待全部 7B+)
```

允许的并行工作：

```text
Foundation 的工具和容量验证
Stage 6 的 v5 candidate path、v4 fallback 和 CXC 合同准备
Stage 7A 的最小 Judgement 合同研究
Stage 7B+ 的高级判定设计研究
```

但生产实现交接必须遵守主链：Foundation 只交付候选 Core/Packed 和验证工具；Stage 6
可以加载、验证和消费受支持的 v5 candidate subset，同时保留 v4 回退；Stage 7A 冻结
Chart v5 所需的最小 Input/Judgement/Replay 合同；Stage 8 只依赖 Stage 7A，不等待
全部高级判定能力；Stage 7B+ 必须通过版本化 capability 持续增加，不能破坏 7A 合同；
Stage 9 不得改变判定语义。

## 格式路线

```text
Chart v4 / CXT v1 / CXC v1
  Stage 6 保留兼容和回退
       |
       +------------------------------+
                                      |
Chart v5 Foundation                   |
  Stage 5 与 Stage 6 之间建立候选     |
  CXT/Packed、容量和 CXC entry 合同    |
       |                              |
       v                              |
Stage 6 v5-first candidate path       |
  受支持 v5 subset 可加载、验证、播放  |
       |                              |
       v                              |
Stage 7A 最小 Judgement contract       |
       |                              |
       +--> Stage 7B+ 高级能力持续演进  |
       |                              |
       v                              |
Chart v5 / CXT v2 / Packed Chart       |
  Stage 8 正式发行和语义收敛           |
       |
       v
Chart v6 / Model v1
  仅在 Stage 9 的模型批次实际需要时启动
```

Chart v5 必须同时冻结：

```text
判定要求与判定域引用
有限行为和效果调度
CXT 模板、参数、Pattern 和确定性展开
Packed Chart 物理编码
40,000 实体 / 16 MiB 验收合同
CXC 中 Packed Chart 的 entry 语义
```

Chart v6/v7/v8 不再作为脱离主路线的漂浮格式计划：

| 能力 | 归属 |
| --- | --- |
| CXT 模板、Pattern、Packed Chart | Stage 8 / Chart v5 |
| Presentation Environment、天空盒 | Stage 9 |
| 静态 glTF、Model、Mesh、submesh | Stage 9 |
| 曲线形变和有限 Geometry Deformation | Stage 9 |
| 后处理、模型动画、粒子 | Stage 11 |
| Studio 对上述能力的编辑 | Stage 10 |

现有 v6/v7/v8 讨论文件保留为设计输入，恢复实施前必须分别建立 ADR、正式 Spec
和对应 Stage 9/11 工作包。

## CXC 路线

```text
Chart Format Foundation
  冻结 entry 映射、Packed 验证原型和容量门禁

Stage 6
  在 v5 candidate path 中验证 CXC v1 的容器、manifest、entry kind、closure、identity
  和发行校验，同时保留 v4 compatibility entry

Stage 8
  在 CXC v1 内正式发行已验证的 Chart v5 Packed playback entry

Stage 9/10
  扩展 portable Model / Environment 资源和 Studio 打包工作流
```

CXC v1 不需要因为 Chart v5 升级容器版本。Foundation 只冻结 entry 映射和验证工具；
Stage 6 验证 v5 candidate playback entry 的装载路径；Stage 8 才正式启用 Chart v5
发行入口。Chart v5 的 Playback entry 必须是 Packed Chart；
作者 JSON、CXT source 和原始导入资源只能作为 Source Project 或显式 source entry
保留，不能冒充 v5 发行入口。

## 脚本路线

当前主路线只实现声明式能力：

```text
Behavior Event
Animation
Step Track
Judgement Result Binding
有限 Effect Schedule
```

这些能力必须有固定类型、确定性采样、资源闭包和安全预算。任意运行时脚本、逐帧
回调、字节码、宿主函数调用和动态对象生成无限期延后。重新启动脚本议题必须先
建立独立 ADR、威胁模型、ABI、预算和阶段计划。

## 阶段退出规则

每个阶段必须同时具备：

```text
接受的合同
实现和 focused tests
失败与回滚路径
公共边界和架构检查
跨平台或目标平台证据
文档、状态和交接更新
owner acceptance
```

未满足退出条件时，文档只能称为 candidate、future 或 deferred，不能称为生产支持。

## 无限期延后

运行时脚本和逐帧脚本回调没有排期，不属于 Stage 6–12 的隐含任务。

## 维护规则

- 当前状态只在 [CURRENT_STATUS.md](CURRENT_STATUS.md) 更新。
- 阶段目标、批次和门禁只在对应 stage plan 更新。
- 格式字段只在格式 Spec 或 ADR 更新。
- 完成证据只写入 stage report。
- 路线变化必须同步更新路线图、阶段索引、相关计划和当前状态。
