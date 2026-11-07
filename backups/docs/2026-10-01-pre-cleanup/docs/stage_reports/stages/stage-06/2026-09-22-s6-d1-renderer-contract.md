# S6-D1 渲染合同与测试 renderer

状态：in_progress（本地无 GPU 证据；本报告不是 D1 退出证据）

日期：2026-09-22

本报告记录 S6-D1 的内部渲染层。它没有把 OpenGL 迁移、Player 接线、hosted 矩阵或
owner acceptance 写成通过。

## 1. 实现边界

- 新内部静态库 `cuexis_presentation_renderer` 直接依赖 `cuexis_core`、`cuexis_playback`
  和 `cuexis_render`。它不在安装导出列表里，头文件不含 SDL 或 OpenGL。
- `IPresentationRenderer` 拥有 capability、prepare、`accepts`、activate、discard、submit、
  present、resize、rebuild 和 close。submit 与 present 分开，同一帧不能重复提交。
- `PreparedPresentation` 是 move-only。renderer 同时最多一个 outstanding candidate。
  候选析构会 discard。rebuild 使旧 renderer generation 失效，并清掉已保留的资源。
- activate 只在 `accepts` 成功后无失败交换。过期 token 由 `accepts` 返回
  `presentation.renderer.token.stale`，不在测试里用进程终止来表达。
- 零尺寸表面暂停 submit。resize 不改变 Playback 身份，也不推进 renderer generation。
  可恢复 present 失败保留 active cache。不可恢复 present 失败把表面标为 lost，直到 rebuild。
  close 不接收窗口，也不能在候选尚未丢弃时关闭。
- `buildPresentationCommands` 是唯一的排序、分 pass 和 summary digest 实现。测试 renderer
  消费它，不另写一套排序。digest 字段顺序与现有 OpenGL summary v1 相同。
- `cuexis_render_opengl` 仍走原来的 `renderPresentationFrame`。把 adapter 接到这个合同是 D2。

## 2. 本地证据

| 检查 | 结果 |
| --- | --- |
| `cuexis_presentation_renderer_tests` | 2 cases，87 assertions，通过 |
| `cmake -P cmake/VerifyArchitecture.cmake` | 通过；新层不含 SDL/GL，且不在两个安装 target 列表中 |
| Debug configure | 通过；allowlist 接受新 target 及其测试 |

测试覆盖成功帧、capability/invalid prepare、重复候选、discard、旧 token、rebuild、
零尺寸、可恢复和不可恢复 present 失败，以及 close。排序用例检查 opaque 按 id、
transparent 按深度从远到近，并检查 digest 随命令变化。

这些是本机 MSVC Debug 结果。它们不能代替同 SHA 的 Linux、Windows MSVC、Windows MinGW
hosted 运行，也不能代替 D2 的 GPU、像素和 OpenGL summary 回归。

## 3. 尚未完成

- D2：OpenGL adapter 改为消费同一命令和事务，Player 正式路径不再直接调用
  `OpenGlBackend::renderPresentationFrame`。
- D1 退出、Stage 6 完成和 owner acceptance。
- C2 及后续批次。
