# 260829 Full Review 整改 W20-W26

状态：local completed；2026-08-29 第 2 批渲染、音频和 Presentation resource lookup 整改完成

本报告记录 Full Review 第 2 批 W20-W26 的本地实现与验证证据。原始审查报告
`260829-full-review.md`、`docs/CURRENT_STATUS.md` 和整改计划均未修改；本报告不改变
Stage 4/5 状态。

## 基线与范围

- 实现分支：`260829-full-review`
- 实现提交：`01e6976801c41c466caa8342e57bf97a8249d366`
- W20-W21：`AG-R1-BOUNDS-T/I`、`AG-AUD1-CLOCK-T/I`
- W22-W23：`AG-R2-UNIFORM-T/I`、`AG-AUD2-SDL-T/I`
- W24-W25：`AG-R3-SCRATCH-T/I`、`AG-PB12-T/I`
- W26：本地批次集成门禁
- Findings：`RT-26`、`RT-27`、`RT-28`、`AP-08`、`CM-21`、`CM-22`、`CM-23`、
  `CM-24`（代码侧）和 `PB-12`；`CM-03` 的跨线程实现也在本批完成。

## 实现与行为

- OpenGL `GpuMesh` 在 prepare/upload 阶段缓存经过 finite 校验的 bounds 与 center；热帧
  读取缓存，不再扫描 Portable resources 查找 bounds。
- Parameterized program 在 link 后一次解析 numeric uniform locations；热帧保留可选
  uniform 的负 location 跳过语义，不再按名称查询 GL。
- OpenGL presentation state 复用 opaque/transparent/debug scratch；summary 为空时不
  复制 command 或计算 summary digest；summary 非空时保留既有 pass、objectId、depth key
  和 digest 顺序。Player 将 `RenderScene` 提升到帧循环外复用。
- `HostClock` 使用有界 seqlock publication，snapshot 在并发 submit 下返回完整 sample；
  SDL effective settings 同步发布为同一 coherent tuple。
- SDL replacement 打开失败进入 Error，不恢复已关闭的旧 stream；unload 后 effective
  settings 回到 Empty。presented frame 的候选值增加显式 upper-bound/monotonic clamp。
- Playback `PresentationResourceKey` 使用透明 comparator 和异构 lookup，保留 assetId/type
  的 canonical 排序、重复检查、manifest 顺序及 identity 组成。

## 合同保持

FrameDigest v1-v3、PreparedSemanticIdentity、canonical bytes/order、golden、Playback 公共
观察面、candidate/active rollback、owner-thread 合同、公共头 ASCII、architecture allowlist
和 runtime-script 无限期延后边界均保持不变。未修改 SDK API、格式字段、默认 capability 或
安装面。

## 验证

- Debug fresh configure/clean build：通过；Debug 全量 CTest：`532/532` 通过，1 项 Windows
  symlink 条件测试跳过。
- Shared Debug fresh configure/clean build：通过；Shared CTest：`535/535` 通过，symlink
  条件测试和 PB-08 shared allocation 注入按构型跳过。
- Release fresh configure/clean build：通过；Release 全量 CTest：`532/532` 通过，1 项
  Windows symlink 条件测试跳过。
- 聚焦可执行文件：Chart `890`、Playback `9815`、Playback allocation `257`、Audio `86`、
  Audio SDL `481`、Core `149`、Runtime `1005`、OpenGL presentation `117` assertions，
  全部通过。
- `cuexis_format_check`、`python -B tools/check_docs.py`、`python -B tools/update_version.py
  --check`、`cmake -DCUEXIS_SOURCE_DIR=. -P cmake/VerifyArchitecture.cmake` 和 `git diff --check`：
  全部通过。

## 残余风险

D1 PB-01、D2 CX-01、D3 PB-04、D4 CH-03、D5 RT-04、D6 identity migration 仍无独立
owner/spec acceptance，本批没有自行选择这些语义。Hosted Linux/MinGW、shader-tools hosted
revalidation 和 owner acceptance 仍需外部证据；本地结果不能替代这些关闭条件。RT-29 万级
对象排序、T1/T2/T3/T4 触发批次继续按计划延后。
