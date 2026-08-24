# Stage Chart Format Update CFU-E Close

状态：CFU-E complete；项目所有者已接受；本地 E 批次门禁与 hosted 跨平台验证均完成

快照日期：2026-08-14

实现提交：`2dc5f6cc1f413132502896705fd46163fec760b2`（`Close CFU-E3/E4 prepared semantic identity locally.`）；该 SHA 是最终 hosted 验证基线。

后续关闭：[CFU-D3 报告](260814-chart-format-update-d3-equivalence.md) 已关闭运行时等价。[CFU-D 关闭报告](260814-chart-format-update-d-close.md) 已按所有者“未提供外部资产”决策关闭整包 CFU-D。本文件仍是 CFU-E 快照。

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md) §5.7

批次证据：[E0](260814-chart-format-update-e0-api.md)、[E1](260814-chart-format-update-e1-source.md)、[E2](260814-chart-format-update-e2-prepare.md)、[E3](260814-chart-format-update-e3-identity.md)、[E4](260814-chart-format-update-e4-gates.md)

## 1. 结论

项目所有者于 2026-08-14 确认检查全部通过，并指示关闭阶段 E。CFU-E0–E4 现已关闭。Playback 已能从 chart-text、typed project-document、filesystem 和 CXC file/memory source 对静态/参数化 Chart v4 做 prepare；成功 prepare 在资源获取与 typed 校验之后组装 `PreparedSemanticIdentity`；非空动画在 Stage 4 前以 `playback.capability.unsupported` 稳定拒绝；失败 reload 不替换 active Playback 或 identity。

该关闭满足计划 5.7 的 E 批次退出条件，以及计划 7.2 要求的最终交付 SHA 跨平台重跑。它不是完整 CXC 公共产品支持、公共 package API、完整 v4 动画 Playback、整包 CFU-D、CFU-F/CFU-G 或 Stage 4。

## 2. 计划 5.7 退出对照

| 工作包 | 退出条件 | 结果 |
| --- | --- | --- |
| CFU-E0 | 公共 API sketch 单独评审；冻结 ParameterSet、prepare options、semantic identity、owning typed source、CXC factory 和 SDK `0.6.0` | 已于 2026-08-14 经项目所有者接受 |
| CFU-E1 | 全部 factory 建立同一 owning source state；旧 factory 与 `TypedPlaybackProject` 布局保留；static/shared consumer 与 export/import 通过 | 已关闭；证据见 E1 报告 |
| CFU-E2 | public ParameterSet 每次 prepare 转换；parse/semantic 先于 capability；静态 v4 进入既有 Runtime；非空动画稳定拒绝 | 已关闭；证据见 E2 报告 |
| CFU-E3 | 资源获取后组装 PreparedSemanticIdentity；跨 source 同内容同参数同值；失败 reload 保持 active identity | 已关闭；证据见 E3 报告 |
| CFU-E4 | 旧 v1/v2/v3 与 typed/filesystem 回归；empty v4 跨 source 成功；非空动画拒绝；参数化 identity/FrameDigest 稳定；安装头与 shared export 只增加 E0 已批准 symbol | 本地门禁见 E4 报告；最终 SHA hosted 证据见下文 |

## 3. 本地证据

E4 在本 worktree 记录的本地矩阵继续有效，不再重写为新结果：

```text
Debug full CTest                                             362/362 passed
Release fresh configure + clean-first full CTest             362/362 passed
shared-debug package/export/import focused gates             10/10 passed
cuexis_format_check                                          passed
tools/check_docs.py                                          123 Markdown / 20 JSON-CXT passed
tools/update_version.py --check                              passed (26.08.01-1)
git diff --check                                             passed
```

完整覆盖说明见 [CFU-E4 报告](260814-chart-format-update-e4-gates.md)。

## 4. Hosted 验证

最终 SHA `2dc5f6cc1f413132502896705fd46163fec760b2` 的三套 workflow 全部成功：

| Workflow | Run | Jobs / evidence |
| --- | --- | --- |
| Linux Quality | [31815749254](https://github.com/l-zilch-l/Cuexis/actions/runs/31815749254) | GCC Release、GCC Shared Release、Clang Shared Debug、Clang ASan + UBSan、GCC Coverage、clang-tidy 全部成功 |
| Windows MSVC | [31815749288](https://github.com/l-zilch-l/Cuexis/actions/runs/31815749288) | Debug、Release 全部成功 |
| Windows MinGW | [31815749253](https://github.com/l-zilch-l/Cuexis/actions/runs/31815749253) | Debug、Release 全部成功 |

无 pending 或 failed job。Linux Quality 的 sanitizer/coverage/clang-tidy 随该 SHA 通过，但不关闭 CFU-F4；跨平台 writer/identity golden 与最终产品关闭仍属于 CFU-F/CFU-G。

## 5. 兼容边界

本关闭保持以下边界：

```text
无完整 CXC 公共产品支持
无公共 Cuexis::Cxc component
无 Stage 4 动画采样、混合或 AnimationSystem
无 runtime scripts、per-frame callbacks 或 bytecode
FrameSnapshot / FrameDigest v3 结构不变
CxcPackageIdentity 不进入 PreparedSemanticIdentity
整包 CFU-D 未关；仓库外旧 Chart 资产仍未确认
```

## 6. Handoff

下一批次是 CFU-D3：证明 lift 后的 v4 与旧 v1/v2/v3 在 FrameSnapshot / FrameDigest v3 / seek-stop 上运行时等价。CFU-F 继续拥有 hosted consumer、跨平台 golden 与 sanitizer/allocation 专项门禁；CFU-G 拥有最终产品关闭与 Stage 4 交接。不得从 CFU-E 已关闭推断动画运行时、公共 CXC package API 或格式阶段完成。