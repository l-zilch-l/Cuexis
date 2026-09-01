# 260829 Full Review 整改 W14

状态：completed；2026-08-29 W14 characterization 与 C1 合同复核完成

本报告记录 Full Review 第 1 批 W14。原始审查报告
`260829-full-review.md`、`docs/CURRENT_STATUS.md` 和整改计划均未修改。

## 基线与范围

- 基线 SHA：`6873a873b569eb3607d79767884ef9dfa6b718f7`
- 实现分支：`260829-full-review`
- 任务：`AG-A3-PIPELINE-T`（W14 W1）与 `AG-C1-STATUS-CONTRACT`（W14 W2 复核）
- A3 生产集成 `AG-A3-PIPELINE-I` 不在本波范围内，未实施。

## A3 characterization 结果

W14 要求的 cache miss、compile failure、commit rollback 和 activate 顺序均已有稳定
测试或可观察 Player 事务证据，无需在 `LK-SHADER` 文件锁下重复添加测试：

- ShaderPipelineCache 的 compile-disabled miss 保持无 active/candidate；
- worker compile 失败返回冻结 diagnostic，并保留既有 active pipeline；
- 成功 candidate 在 `activate()` 后成为 active，candidate 被清空；
- Player 的 prepare -> Playback commit -> activate 顺序在
  `app/player/src/player_app.cpp` 中保持；commit 失败调用 `discardPresentation()`，成功
  reload 才调用 `activatePresentation()`；
- outstanding candidate、adapter prepare 失败和 legacy candidate rejection 均验证 active
  presentation 未被污染。

本波未修改生产代码、Shader 测试或公共合同。identity、FrameDigest、canonical bytes/order、
Playback public surface、candidate/active rollback 和 owner-thread 合同保持不变。

## C1 status contract 复核

`AG-C1-STATUS-CONTRACT` 已在 W10 完成。本波重新运行其正/负例 checker，确认 required、stale、
date 和短 stale fragment 校验仍可定位失败，且当前合同与状态文档一致。

## 验证

- `cmake --build --preset debug-shader-tools --clean-first --target cuexis_shader_tests cuexis_render_opengl_tests cuexis_player_diagnostics_tests`：通过，构建完成。
- `ctest --test-dir out/build/debug-shader-tools --no-tests=error -R "S5-F|S5-D|S5-H|OpenGL configuration|OpenGL backend|Built-in OpenGL|A newer OpenGL|Moving an OpenGL|failed OpenGL" --output-on-failure`：通过，33/33。
- `ctest --test-dir out/build/debug-shader-tools --no-tests=error -R "Player scene|Frame diagnostics" --output-on-failure`：通过，4/4。
- `python -B tools/check_docs_status_contract_tests.py`：通过，3 个测试。
- `python -B tools/check_docs.py`：通过，180 个 Markdown 文件与 20 个 candidate JSON/CXT 文件。

## 残余风险与下一任务

`AG-A3-PIPELINE-T` 的 characterization 已完成，但 A3 生产集成仍须等待 A2 合同可读并按
`AG-A3-PIPELINE-I` 单独实施。Lane A identity migration 仍受 D6 owner/spec 决策阻塞；不得
在后续任务中自行选择 identity 方案。W15 可继续进行 A3 pipeline integration 与 D1 reload
characterization，仍需保持 render 与 runtime 文件锁分离。
