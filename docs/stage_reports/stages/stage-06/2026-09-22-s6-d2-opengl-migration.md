# S6-D2 OpenGL 迁移与 Player 接线

状态：in_progress（本地 GPU smoke 已通过；本报告不是 D2 退出证据）

日期：2026-09-22

本报告记录 OpenGL adapter 接到 D1 渲染合同的本地实现。它没有把 hosted 矩阵或
owner acceptance 写成通过。

## 1. 实现边界

- `OpenGlBackend` 实现 `IPresentationRenderer`。Player 正式路径用 `prepare`、
  `activate`、`discard`、`resize`、`submit` 和 `present`。
- `renderPresentationFrame` 保留为 SDK `0.7.0` 兼容入口，只给 smoke 的 debug summary
  对照和既有测试使用。正式帧不再调用它。像素探针仍由具体 backend 的
  `lastPixelProbe` 提供给 smoke。
- submit 画出当前帧但不交换缓冲；present 才调用 `SDL_GL_SwapWindow`。准备失败仍沿用
  原来的错误码，不替换 active cache。
- rebuild 在当前 context 上释放 GPU 资源后再递增 renderer generation，不销毁窗口或
  context。`injectPresentFailure` 是故障注入，不是真实设备丢失。
- 绘制排序和 digest 仍由既有 OpenGL builder 生成，再复制进中立 `DrawSummary`。
  因此 Validation Sink 的 summary golden 没有改。
- 零尺寸表面让 submit 返回 `presentation.renderer.surface.zero_size`，Player 跳过该帧
  而不是把尺寸抬到 1。本次 smoke 窗口不是零尺寸。

## 2. 本地证据

| 检查 | 结果 |
| --- | --- |
| `cuexis_presentation_renderer_tests` | 通过 |
| OpenGL summary、scratch、draw bounds 与 Player scene 表征测试 | 通过 |
| `cuexis_player.exe --smoke-test` | 退出码 0。驱动 `windows`，GPU 为 NVIDIA GeForce RTX 4060 Laptop GPU，OpenGL 3.3.0。6 帧完成，digest 与像素检查、失败 reload、outstanding candidate 和 debug summary parity 均通过 |

这是本机真实窗口证据，不是故障注入。它不能代替同 SHA 的 Linux、Windows MSVC 和
Windows MinGW hosted 运行，也没有覆盖最小化、恢复和物理设备拔出。

## 3. 尚未完成

- D2 退出、Stage 6 完成和 owner acceptance。
- C2、E1/E2 及后续批次。
- Vulkan 仍延期。
