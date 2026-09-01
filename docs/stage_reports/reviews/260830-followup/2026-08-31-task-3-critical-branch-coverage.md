# 260830 Follow-up Task 3 Critical Branch Coverage Local Snapshot

状态：historical

更新日期：2026-08-31

快照日期：2026-08-31

后续关闭证据：[任务 3 hosted 完成报告](2026-08-31-task-3-hosted-verification.md)

所属计划：[任务 3 关键模块分支覆盖率](../../../stage_plans/completed/260830-followup/task-3-critical-branch-coverage.md)

## 结论

本轮补充了关键失败、回滚、异常边界和发布一致性测试，没有修改生产实现。所有新增测试均以
`Result`、diagnostic、identity、active state、published snapshot 或 summary 为断言对象，不依赖随机
sleep、真实 OOM、GPU 或物理声卡故障。

本地 adapter 覆盖率构型已在 dummy SDL 环境下通过 `523/523`。headless 和 shader-tools 的 B0
基线及新增 Chart/CXC/Playback/identity/cache 测试均已通过；真实 GPU 驱动、SDL 设备和 hosted
sanitizer/跨平台门禁仍需在 CI 或 smoke 环境关闭，因此本任务保持 `active`。

## 已补测试

| 范围 | 证据 |
| --- | --- |
| Chart v4 | 根类型/版本、参数预算与重复项、非法参数类型、CXT import 规范。 |
| CXC | manifest hash 失败不返回部分 package。 |
| Playback 与 ContentProvider | host provider 的 `std::exception` 与 unknown exception 受控转换；prepare/reload 保留 active state。 |
| Identity 与 shader cache | 单组件 identity 变异、截断 envelope、失败候选不替换 active pipeline。 |
| HostClock | 非有限、Stopped position 和 segment regression 失败不改最后 published sample。 |
| SDL | paused replacement 成功提交后发布完整时钟/settings；candidate 消耗后不可再次激活。 |
| Platform SDL | moved-from runtime/window 返回稳定错误，不影响已转移对象。 |
| OpenGL internal probe | 已构建过前置 command 后的 late frame error 不返回 partial summary。 |

## Coverage 证据

| lane | preset | covered / total branches | 说明 |
| --- | --- | ---: | --- |
| Chart v4 | `headless-coverage` | 2500 / 5412 | B0 artifact，包含新增非法输入测试。 |
| CXC | `headless-coverage` | 1428 / 3446 | B0 artifact，包含 package rollback。 |
| Playback | `headless-coverage` | 2758 / 7520 | B0 artifact，包含 provider exception rollback。 |
| Prepared identity | `headless-coverage` | 10 / 14 | 单组件 identity 变异测试。 |
| HostClock | `headless-coverage` | 47 / 68 | 当前干净 artifact；非法输入和 segment regression 不改最后 published sample。 |
| Platform SDL | `adapter-coverage` | 93 / 250 | 当前干净 artifact。 |
| AudioSDL | `adapter-coverage` | 336 / 912 | 当前干净 artifact。 |
| OpenGL | `adapter-coverage` | 216 / 2712 | 当前干净 artifact；仅涵盖无 GPU internal probe。 |
| Shader cache | `shader-tools-coverage` | 387 / 1088 | B0 artifact。 |
| Shader pipeline cache | `shader-tools-coverage` | 93 / 238 | B0 artifact。 |

Artifact 保存在 `out/build/<preset>/task-3-coverage/`，metadata 记录 implementation SHA、编译器、
gcovr、filter、测试命令及环境残余；它们是本地验证输出，不属于发布文档。

## 本地验证

| 命令 | 结果 |
| --- | --- |
| `ctest --preset headless-coverage --no-tests=error -E "^cuexis_external_consumer_"` | `481/481` passed。 |
| `SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy ctest --preset adapter-coverage --no-tests=error -E "^cuexis_external_consumer_"` | `523/523` passed。 |
| `ctest --preset shader-tools-coverage --no-tests=error -E "^cuexis_external_consumer_"` | `509/509` passed。 |

## 残余与后续门禁

- 真实 OpenGL context 的 compile/link/swap 失败：保留给 GPU smoke 与 hosted matrix。
- SDL 设备拔出、驱动格式切换和 queue failure：dummy driver 不能稳定控制，保留给 hosted/physical-device 验证。
- Linux sanitizer、shader-tools sanitizer、Windows MSVC/MinGW、Debug/Release 与 external consumer：必须在同一实现 SHA 的 CI matrix 中复核。
- 当前普通 Windows shell 缺少 Developer Prompt 环境，链接会报 `LNK1104 kernel32.lib`；这不是源码失败，不能当作通过证据。

详细 ID 关闭映射见 [任务 3 分支 backlog](../../../stage_plans/completed/260830-followup/task-3-branch-backlog.md)。
