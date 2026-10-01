# Cuexis 阶段 4 验收报告

状态：S4-H complete；最终 SHA hosted Linux/MSVC/MinGW 已通过，项目所有者已接受，Stage 4 已关闭

报告日期：2026-08-27

权威计划：[Stage 4 实施计划](../../../stage_plans/completed/stage-04/plan.md) §7.8、§12。

## 1. 结论与接受状态

S4-A 至 S4-G 已交付 typed `AnimationProgramInput` 编译、绝对 Beat 采样、Layer 混合、统一
PropertyResolver、Playback 事务所有权、默认 animation capability 与安全/分配门禁。S4-H 在最终
SHA 上完成本地门禁引用与 hosted Linux Quality、Windows MSVC、Windows MinGW 验收。

默认 Playback Session 可求值非空合法 Chart v4 / CXT Binding 动画；缺少
`cuexis.animation.clip.v1` / `cuexis.animation.layers.v1` 的显式裁剪 Session 仍以
`playback.capability.unsupported` 稳定拒绝。公开观察面仍是 PlaybackSession、FrameSnapshot 和
FrameDigest v3。`engine/animation/` 不解析 JSON、CXC 或 CXT。

项目所有者接受记录：**已接受（2026-08-27；明确指示在 hosted 全部通过后完成文档并关闭 Stage 4）**。

本关闭不表示 Shader 管线、Studio、Judgement、公共 CXC package API 或运行时脚本已开始。

## 2. SHA、版本与公共边界

| 项目 | 值 |
| --- | --- |
| 最终实现 SHA | `3df92747e575e97b3606aa4a2030a04643c46473` |
| 前序实现 SHA | `8135891bf967b4bf063545e00cbb2a1cf14aaede`（S4-E 至 S4-G） |
| 警告修复 SHA | `0a245b371829c7c1feeed4420da266384f41a45a` |
| Build version | `26.08.01-1` |
| SDK API version | `0.6.0` |
| FrameDigest | v3，公共结构未改变 |
| 默认 capability set | version 1；追加 `cuexis.animation.clip.v1` 与 `cuexis.animation.layers.v1` |

`cuexis_animation` 仍是内部 STATIC target，不是安装 package component。shared Playback 把实现
私有链接进 Playback；静态 consumer 只看到 `Cuexis::Playback`。稳定 C ABI 仍延期到 Stage 12。

## 3. S4-A 至 S4-H 交付摘要

| 批次 | 实际交付与证据 |
| --- | --- |
| S4-A | STATIC `cuexis_animation`、typed input 头、compile 合同与诊断冻结；见 [S4-A](2026-08-24-s4-a-module-wiring.md)。 |
| S4-B | owning compile、绝对 local Beat、fill/iterations、continuous/step 采样；见 [S4-B](2026-08-25-s4-b-absolute-sampling.md)。 |
| S4-C | Override/Additive Layer 混合写入 PropertyWriteBuffer；见 [S4-C](2026-08-26-s4-c-layer-mixing.md)。 |
| S4-D | 统一 PropertyResolver、OverrideToken、BasePropertyCommand 与 Runtime 编排；见 [S4-D](2026-08-27-s4-d-property-resolver.md)。 |
| S4-E | Playback compile/own/commit 动画状态；失败 reload 不改 active；见 [S4-E](2026-08-27-s4-e-playback-ownership.md)。 |
| S4-F | 默认 Session 接受合法非空动画；裁剪 Session 仍拒绝；跨 source identity 与 FrameDigest v3 一致；见 [S4-F](2026-08-27-s4-f-consumer-capability.md)。 |
| S4-G | ChartLimits/600,000 write 上限、诊断 sentinel、warmed 零分配或有界合同、F4 风格探针；见 [S4-G](2026-08-27-s4-g-safety-allocation.md)。 |
| S4-H | 本地 Debug/Release 门禁见 [S4-H 本地快照](2026-08-27-s4-h-local-validation.md)；最终 SHA hosted 见下文。 |

## 4. §12 退出条件对照

| # | 条件 | 状态 | 证据 |
| ---: | --- | --- | --- |
| 1 | S4-A 到 S4-G 退出门禁关闭 | PASS | 各批次报告与最终 SHA CTest |
| 2 | 默认 Playback Session 可求值非空合法 Chart v4 / CXT Binding 动画 | PASS | S4-F/S4-G Playback 测试 |
| 3 | 宿主观察面仍只有 Playback/FrameSnapshot/digest；World/EnTT 未泄漏 | PASS | architecture 与 external consumer 门禁 |
| 4 | hosted Linux Quality、Windows MSVC 和 Windows MinGW 在同一最终 SHA 成功 | PASS | 下文 run URL；attempt 1 |
| 5 | completion report 经项目所有者接受 | PASS | 本报告；2026-08-27 明确关闭指示 |

## 5. 最终 SHA hosted 门禁

最终 SHA `3df92747e575e97b3606aa4a2030a04643c46473`。push 与 pull_request 各跑一套相同 workflow，
结论一致。下表以 push run 为关闭证据；PR #18 对应 run 也全部 success。

| Workflow | Run | 结论 | 第一失败步骤 |
| --- | --- | --- | --- |
| Linux Quality | [33077681555](https://github.com/l-zilch-l/Cuexis/actions/runs/33077681555) | success | 无；attempt 1 |
| Windows MSVC | [33077681518](https://github.com/l-zilch-l/Cuexis/actions/runs/33077681518) | success | 无；attempt 1 |
| Windows MinGW | [33077681550](https://github.com/l-zilch-l/Cuexis/actions/runs/33077681550) | success | 无；attempt 1 |

Linux Quality 在该 SHA 上通过 GCC Coverage、Clang ASan + UBSan、GCC Release、GCC Shared Release、
clang-tidy 与 Clang Shared Debug。Windows MSVC 与 MinGW 的 debug 与 release 均 success。

同 SHA 的 pull_request run：Linux Quality
[33077687873](https://github.com/l-zilch-l/Cuexis/actions/runs/33077687873)、Windows MSVC
[33077687833](https://github.com/l-zilch-l/Cuexis/actions/runs/33077687833)、Windows MinGW
[33077687843](https://github.com/l-zilch-l/Cuexis/actions/runs/33077687843)。

最终 SHA 之前的失败已由后续 commit 关闭，不阻断本关闭：

| SHA | 失败 | 关闭 |
| --- | --- | --- |
| `8135891` | Linux/MinGW Release `-Werror=missing-field-initializers` / `maybe-uninitialized` | `0a245b3` |
| `0a245b3` | Clang ASan `alloc-dealloc-mismatch`（nothrow `operator new` 对 `free`） | `3df9274` |

## 6. 本地门禁引用

[S4-H 本地快照](2026-08-27-s4-h-local-validation.md) 记录 hosted 发布前的 MSVC Debug/Release
fresh configure、clean build、完整 CTest `442/442`、format、architecture、public-header ASCII、
version `26.08.01-1`、`check_docs.py` 与 `git diff --check`。该快照当时尚无最终 SHA。关闭证据以
第 5 节 hosted 结果为准。

## 7. 明确不包含

- 运行时脚本、逐帧脚本回调、任意表达式或通用状态机
- Shader、粒子、UI、骨骼动画
- 公共 CXC package API，或把 `cuexis_cxc` 提升为安装组件
- Studio 编辑器、Judgement/Replay
- FrameDigest 升级或 SDK API 版本变更

关闭后的下一阶段是 [Stage 5](../../../stage_plans/completed/stage-05/plan.md)，状态为
unblocked / not started。
