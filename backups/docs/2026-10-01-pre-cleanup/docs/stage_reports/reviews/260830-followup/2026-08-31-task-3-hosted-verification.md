# 260830 Follow-up Task 3 Hosted Verification

状态：completed

更新日期：2026-08-31

快照日期：2026-08-31

所属计划：[任务 3 关键模块分支覆盖率](../../../stage_plans/completed/260830-followup/task-3-critical-branch-coverage.md)

前置本地快照：[任务 3 本地快照](2026-08-31-task-3-critical-branch-coverage.md)

## 结论

任务 3 已完成。最终 SHA `299596c533a8c66a78b5c4ada341b1163528fb25` 的 Linux Quality、Windows
MSVC 和 Windows MinGW 均为 `success`。Linux Quality 同时完成 headless、adapter、shader-tools
coverage，以及 GCC/Clang、shared、sanitizer、external-consumer 和 clang-tidy 门禁。

最终提交修正了五个 shell 单引号中的 gcovr 正则转义。修正前，Chart v4、Prepared identity、HostClock
和两个 shader cache artifact 会错误地显示 `0 / 0`；同一测试集的全局 `engine/`、CXC、Playback 和
adapter 统计未变。因此本报告只将修正后的定向 artifact 作为最终 coverage 证据，不将该配置修正写成
测试覆盖收益。

## 最终 Hosted Coverage

`headless-coverage` 的全部 `engine/` 为行覆盖 `14,657 / 18,747`（78.18%），分支覆盖
`14,840 / 36,042`（41.17%）。以下 lane 来自同一 SHA 的 Linux Quality artifact；adapter 与
shader-tools 是独立构建配置，不能与 headless 总数相加形成伪造的仓库总覆盖率。

| lane | preset | covered / total branches | coverage |
| --- | --- | ---: | ---: |
| Chart v4 | `headless-coverage` | 2392 / 5174 | 46.2% |
| CXC | `headless-coverage` | 1410 / 3400 | 41.5% |
| Playback | `headless-coverage` | 2728 / 7460 | 36.6% |
| Prepared identity | `headless-coverage` | 10 / 14 | 71.4% |
| HostClock | `headless-coverage` | 49 / 72 | 68.1% |
| Platform SDL | `adapter-coverage` | 93 / 250 | 37.2% |
| AudioSDL | `adapter-coverage` | 337 / 930 | 36.2% |
| OpenGL | `adapter-coverage` | 216 / 2708 | 8.0% |
| Shader cache | `shader-tools-coverage` | 386 / 1086 | 35.5% |
| Shader pipeline cache | `shader-tools-coverage` | 93 / 238 | 39.1% |

最终 artifact 分别为 `cuexis-task-3-core-coverage-299596c533a8c66a78b5c4ada341b1163528fb25`、
`cuexis-task-3-adapter-coverage-299596c533a8c66a78b5c4ada341b1163528fb25` 和
`cuexis-task-3-shader-tools-coverage-299596c533a8c66a78b5c4ada341b1163528fb25`。其 metadata 记录
implementation SHA、GCC 13.3、gcovr 7.0、filter、测试命令及环境残余；artifact 是 CI 证据，不进入发布树。

## Hosted 验证

| workflow | run | 结果 |
| --- | --- | --- |
| Linux Quality | [33387378354](https://github.com/l-zilch-l/Cuexis/actions/runs/33387378354) | `success`；GCC Coverage、adapter coverage、shader-tools coverage、sanitizer、Debug/Release/shared、external consumer 和 clang-tidy 均通过。 |
| Windows MSVC | [33387378378](https://github.com/l-zilch-l/Cuexis/actions/runs/33387378378) | `success`；Debug、Release、architecture、determinism 与 formatting 均通过。 |
| Windows MinGW | [33387378418](https://github.com/l-zilch-l/Cuexis/actions/runs/33387378418) | `success`；Debug、Release、architecture 与 determinism 均通过。 |

Linux artifact 的 coverage 命令分别完成 `481/481` headless、`523/523` adapter 和 `509/509`
shader-tools 测试；对应完整 discovered-test 集合为 488、530 和 518 项，external consumer 按命令排除。

## 环境残余

- 真实 OpenGL context 的 compile/link/swap 失败：保留给 GPU smoke 与 hosted matrix。
- SDL 设备拔出、驱动格式切换和 queue failure：dummy driver 不能稳定控制，保留给 hosted/physical-device 验证。

这些是明确的环境排除，不替代为普通单元测试，也不阻断本任务关闭；同 SHA CI 已覆盖可复现的
sanitizer、跨平台、Debug/Release 和 external-consumer 门禁。
