# 260829 Full Review 整改 W08/W09

状态：completed；2026-08-29 Chart reader characterization 与职责拆分完成

本报告记录 Full Review 第 0 批后的 Chart 内聚性前置任务证据。原始审查报告
`260829-full-review.md` 与 `CURRENT_STATUS.md` 未修改；本报告不改变 Stage 4/5 状态。

## 基线与范围

- 基线 SHA：`ce0ac9d4b06d75a6254e9054d4478be6e54dfd5c`（W08 characterization）
- 实现 SHA：`a186329eb8e258f50ac42d85d446c27d8d6f72fd`
- 实现分支：`260829-full-review`
- 任务：`AG-CH-COHESION-T`（W08）、`AG-CH-COHESION-I`（W09）
- Finding：无新增 finding；任务用于 CH-08/CH-14/parse-once 前的职责边界与行为 parity。

## 实现

- `tests/chart/chart_v4_c2_contract_tests.cpp` 增加 ChartLoader 路由、空/最小输入、V4
  import 排序、portable project path、诊断路径/顺序以及 canonicalSource/semantic identity
  parity characterization。
- `engine/chart/src/chart_v4_reader_internal.*` 按职责拆分为：
  - `chart_v4_common_reader_internal.*`：V4 通用诊断、stable ID、RationalBeat、required extensions；
  - `chart_v4_animation_reader_internal.*`：animation/CXT property、clip、Animator、blend/fill/iterations；
  - `chart_project_path_internal.*`：portable project path、CXT 后缀和 ASCII case-fold key。
- `engine/chart/CMakeLists.txt` 登记三个内部实现源；三个现有调用方仅更新对应内部 include。
- W08 的诊断顺序 characterization 修正为当前已存在的完整稳定指纹：`chartId` 两项、
  `format` 两项、`version` 一项；未改变生产诊断。
- 对上一批 Playback 公共头执行纯 clang-format 机械整理，使全量格式目标可运行。

## 验证

- `cmake --preset debug --fresh`：通过。
- `cmake --build --preset debug --clean-first`：通过，230 个构建步骤完成。
- `cuexis_chart_tests.exe`：849 assertions/117 cases，退出码 0。
- `cuexis_playback_tests.exe`：9502 assertions/87 cases，退出码 0。
- `cuexis_audio_tests.exe`：83 assertions/9 cases，退出码 0。
- `ctest --preset debug -R "cuexis_chart_tests|cuexis_architecture_tests"`：CTest discovery
  仅发现 architecture 条目；`cuexis_architecture_tests` 通过。Chart 测试以直接可执行文件运行并通过。
- `cuexis_format_check`：通过；此前失败的 Playback 头注释已作机械格式整理。
- `ctest --preset debug -R "^cuexis_external_consumer_(add_subdirectory|add_subdirectory_playback|find_package|find_package_playback)$"`：在 VS Developer 环境下 4/4 通过，包含 installed Playback leak 与 public-header ASCII gate。
- `python -B tools/check_docs.py`：通过（175 Markdown、20 candidate JSON/CXT）。
- `python -B tools/update_version.py --check`：通过，版本 `26.08.01-1` 一致。
- `git diff --check`：通过。

普通 PowerShell 环境下首次 external consumer 尝试因未加载 VS Developer 环境而在链接探测阶段
报 `LNK1104 kernel32.lib`；随后在 `vcvars64.bat` 环境中重跑并通过。未将首次环境错误计为产品失败。

## 保留合同与残余风险

未改变公共 API、Chart/CXT 格式、diagnostics code/message/fieldPath/order、canonical bytes/order、
semantic identity、FrameDigest、capability、golden、Playback 边界或 architecture allowlist。
未新增 public header，也未上提 project path helper 到 `core`。

D1 PB-01、D2 CX-01、D3 PB-04、D4 CH-03、D5 RT-04、D6 identity migration 继续 unresolved；
未实施依赖这些决策的行为变化。Chart reader 拆分仅完成前置结构任务，CH-14 及后续 parse-once、
identity/cache、math、audio synchronization、prepare 分层仍未开始。未运行 Release、hosted
Linux/MinGW 或 push。
