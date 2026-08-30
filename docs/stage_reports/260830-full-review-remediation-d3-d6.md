# 260829 Full Review 整改 D3-D6

状态：completed；2026-08-30 本地实现与门禁完成

本报告记录 Full Review 决策门 D3-D6 的接受决策、实现证据和本地验证。原始审查报告
`260829-full-review.md` 与 `docs/CURRENT_STATUS.md` 未修改；本报告不改变 Stage 4/5 状态。

## 基线与范围

- 基线 SHA：`5742415907186b88c0302bdc9c0bdc1abdfa6568`
- 实现 SHA：`a85eb32d18a35527313eb8ecfec21b5e3ed41503`
- 实现分支：`260829-full-review`
- 任务：D3/PB-04、D4/CH-03、D5/RT-04、D6 identity migration A1
- 决策依据：[ADR 0041](../adr/0041-legacy-format-exit-policy.md)

## 实现与合同

- **D3 / PB-04**：保留 `playback.presentation.frame.value_invalid` 作为
  Validation Sink/未来边界码；World/Chart boundary 负责 opacity/tint `[0,1]` 校验，生产
  extraction 对计算得到的 non-finite 值报告 `frame.non_finite`。
- **D4 / CH-03**：SDK 0.7.0 两参 `TimingMap::create` 保留任意有限正 BPM 的 legacy 行为；
  带显式 tempo events/stops 的四参入口执行 `[1, 65536]` BPM 域及各 4096 项预算，offset
  仍要求有限值。
- **D5 / RT-04**：`RenderFrame/renderFrame` 在 SDK 0.7.0 保留并标记为 legacy/diagnostic-only；
  `renderPresentationFrame` 是当前 Presentation 主路径。
- **D6 / identity A1**：legacy v1/v2/v3 `PreparedSemanticIdentity` 改用
  `ChartWriter::writeCanonicalJson` 对完整 Chart source bytes 做一次性 canonical identity。
  timing stops、tempo events、templates 和 keyframe tracks 不再因投影到 `ChartDocument`
  而从 identity 中丢失；旧 identity 算法不保留双轨。

## 修改文件

- `docs/formats/CHART_V4_FORMAT.md`
- `docs/formats/PORTABLE_PRESENTATION.md`
- `docs/formats/TIMING_MODEL.md`
- `engine/chart/include/cuexis/chart/timing_map.hpp`
- `engine/playback/src/playback_session.cpp`
- `engine/render/include/cuexis/render/render_backend.hpp`
- `engine/render_opengl/include/cuexis/render_opengl/open_gl_backend.hpp`
- `tests/chart/timing_map_tests.cpp`
- `tests/playback/playback_identity_tests.cpp`

## 验证

- `cmake --preset debug --fresh`：通过。
- `cmake --build --preset debug --clean-first`：通过，235 个目标完成。
- Chart、Playback、Audio 聚焦可执行测试：分别通过 `896/121`、`9925/102`、`86/10`。
- VS 2026 x64 Developer Prompt 下 `ctest --preset debug --no-tests=error`：`535/536` 通过，
  Windows symlink 条件测试 1 项跳过。
- 同一 Developer Prompt 下 7 个 external/package consumer gate：`7/7` 通过，包含
  architecture、installed Playback leak 和 public-header ASCII 检查。
- `cuexis_format_check`、`clang-format --dry-run --Werror`、`python -B tools/check_docs.py`、
  `python -B tools/update_version.py --check`、`git diff --check`：全部通过。

首次从普通 PowerShell 运行完整 CTest 时，7 个 external gate 因子 consumer link 找不到
`kernel32.lib` 而失败；这是未加载 VS Developer 环境造成的工具链问题。使用同一 x64
Developer Prompt 串行重跑后全部通过，未修改代码。

## 保留合同与残余风险

FrameDigest v1-v3、canonical order、Playback 公共观察面、candidate/active rollback、
owner-thread 合同、公共头 ASCII、runtime-script 无限期延后边界和 Stage 4/5 状态保持不变。
D6 有意改变 legacy v1/v2/v3 的 PreparedSemanticIdentity；对应 canonical bytes、Chart
identity 和 semantic identity 回归已通过，现有 v4 golden 未变化。Lane A 的 shader cache
键域统一和 Player cache consumption 不在本报告范围内。Hosted Linux/MinGW 及 owner acceptance
仍需在对应环境按实现 SHA 重验。本批未执行 push。
