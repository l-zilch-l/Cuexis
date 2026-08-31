# 260829 Full Review 整改 W06/W07

状态：completed；2026-08-29 Playback、Runtime 和 OpenGL 合同文档整改完成

本报告记录 Full Review 第 0.5 批后续 W06/W07 的实现证据。原始审查报告
`260829-full-review.md` 与 `CURRENT_STATUS.md` 未修改；本报告不改变 Stage 4/5 状态。

## 基线与范围

- 基线 SHA：`fcdcc7a928c16f71500e2ba9061906048f1a6b87`（B05 报告提交）
- 实现 SHA：`3cf5da7594b9f4089ad3e50907168242e40a44d1`
- 实现分支：`260829-full-review`
- 任务：`AG-B05-PB`（W06/W1）、`AG-B05-RT`（W07/W2）
- Finding：PB-05、PB-06、PB-10、RT-05、RT-06、RT-08、RT-33、RT-34。

## 实现

- `PORTABLE_PRESENTATION.md` 说明成功 `PlaybackSession::update()` 会递增 generation，旧
  `PreparedPlayback` 后续 commit 返回 `playback.prepared.stale`，必须重新 prepare。
- ADR 0030 增加 2026-08-29 SDK API `0.7.0` 快照和当前 `find_package(Cuexis 0.7 ...)` 示例，
  并将 `0.1.0` 至 `0.4.0` 段落标为历史背景。
- Playback 公共头补充 owner-thread、跨线程移动/析构 `std::terminate` 和 `PlaybackSource`
  独立所有权边界说明。
- Runtime 调试快照文档补齐 Stage 4 字段并链接 `ANIMATION_MIXING.md`；Runtime/OpenGL
  头和实现注释冻结 bad_alloc、SDL main-thread、candidate terminate 和透明深度量化合同。
- RT-25 因 D5/RT-04 无 owner/spec acceptance 未修改。

## 验证

- Debug 增量构建（MSVC x64 Developer environment）：通过。
- `cuexis_playback_tests.exe`：9502 assertions/87 cases，退出码 0。
- `cuexis_runtime_tests.exe`：975 assertions/37 cases，退出码 0。
- `cuexis_render_opengl_tests.exe`：45 assertions/9 cases，退出码 0。
- `python -B tools/check_docs.py`：通过（174 Markdown、20 candidate JSON/CXT）。
- `git diff --check`：通过。
- Runtime/OpenGL 修改文件的 `clang-format --dry-run --Werror`：通过。
- Playback 两个公共头的 `clang-format --dry-run --Werror`：退出码 1；诊断集中在原有顶部
  注释风格及本次 owner-thread 注释，未运行全量格式目标，未进行无关格式重排。
- Public-header ASCII scan：通过；所有本次修改公共头为 ASCII。

## 保留合同与残余风险

未改变 Playback 公共 API 签名、错误码、FrameDigest v1-v3、canonical bytes/order、identity、
默认 capability、golden、candidate/active rollback、owner-thread 实现、架构 allowlist 或
runtime-script 无限期延后边界。所有改动为文档/注释。

D1 PB-01、D2 CX-01、D3 PB-04、D4 CH-03、D5 RT-04、D6 identity 迁移继续 unresolved；
未实施依赖这些决策的行为选择。PB-04 未下移范围校验，RT-25 未删除或标记 legacy。
Chart reader 拆分、identity/cache、math、音频同步、prepare 分层和 parse-once 等后续批次
未开始。

本波未执行 Release、hosted Linux/MinGW、完整 CTest discovery 或 push；这些门禁仍待后续批次
或最终关闭阶段统一执行。
