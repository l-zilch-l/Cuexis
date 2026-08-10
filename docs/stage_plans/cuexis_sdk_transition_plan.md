# Cuexis SDK Transition Plan

状态：historical transition plan；产品转型方向已由 ADR 0027 接受，当前路线见 `docs/ROADMAP.md`

更新日期：2026-08-10

本文路径保留用于兼容旧链接。整理前的完整内容已归档到
[CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md](../archive/CUEXIS_SDK_TRANSITION_PLAN_2026-08-10.md)。

## 当前权威

- 产品边界：[ADR 0027](../adr/0027-playback-sdk-product-boundary.md)
- 当前状态：[CURRENT_STATUS.md](../CURRENT_STATUS.md)
- 阶段路线：[ROADMAP.md](../ROADMAP.md)
- 活动计划：[Stage Chart Format Update](stage_chart_format_update_implementation_plan.md)

## 已完成的 transition

阶段 1C-1E 已建立 PlaybackSession、headless FrameSnapshot、ContentProvider、HostClock/CuexisAudio、
static/shared package 和 external consumer。阶段 2、3 已继续验证同一公共门面、portable
presentation 和跨平台 package 边界。

稳定 C ABI、语言绑定和正式 SDK v1 仍等待 Stage 11 Judgement/Replay 公共生命周期完成后进入
Stage 12。

运行时脚本和逐帧脚本回调无限期延后，不属于 transition roadmap。
