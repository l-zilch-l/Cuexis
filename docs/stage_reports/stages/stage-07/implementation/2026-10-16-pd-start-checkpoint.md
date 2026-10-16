# Stage 7-PD 实施起步存档

日期：2026-10-16（Asia/Shanghai）

状态：WIP checkpoint；按 owner 要求保存当前设计与实施起步，不是批次退出报告。

## 授权与范围

owner 本轮要求完成 PD 计划，随后要求先提交当前状态存档并显示 todolist。
存档前 HEAD 为 `d076240a3f66fae9193195c2343afb1aedf94926`，分支 `stage-7`。
此前未提交的完整 PD 设计、候选合同和跨阶段修订一并保存。
没有推送、创建 PR、合并或发布授权。

## 已有实现与证据

- `CUEXIS_PD_METRICS_TEST_ONLY` 默认 OFF；开启后通过 `CUEXIS_PD_METRICS` 指定内部数据输出前缀。
  Judgement、Projection、Runtime、Snapshot 和实际 Renderer 调用加入内部计时；样本存储有界。
  同一计时域内扣除嵌套子阶段，不将父阶段与子阶段重复求和。跨 DLL 的汇总/并发输出仍需复核。
- 增加显式选取的 `PD deterministic presentation baseline` 表征用例：固定输入与时钟、
  1280×720、64 次 warm-up、1024 次采样；验证可见性、Score 和完整 Replay 结果。
- Debug 构建了 Playback/ExecutionHost 测试目标；Release 构建了 Playback 测试目标。
  两种配置各三次表征运行，每次 5,464 条断言通过，聚合 FrameDigest 均为
  `17504891284788938091`。这是单对象确定性基线，不是 40k 性能或设备验收。
- 原始证据在 `out/pd-20261016/`：`entry-manifest.json`、`entry-worktree/`、
  `build-debug.log`、`configure-release.log`、`build-release.log`、
  `baseline-debug/run*.log` 和 `baseline-release/run*.log`，以及有界分段 JSONL。
  这些本地构建/证据产物保留原位，不作为源码加入本次 Git 提交。
- 入场及代码起步后的文档检查通过：451 份 Markdown、20 份 candidate JSON/CXT；
  差异空白检查通过。加入本报告及状态/待办更新后另行重跑文档检查。

## 未完成与验证限制

PD-0 仍进行中。初始 manifest 尚需补全工具链、设备/电源、输入/内容 hash 与统计汇总，
完整测量覆盖、计数/复制/分配指标和采样噪声仍未验收。新版本/消费者指标在旧路径不适用，
未测项不填零，不宣称生产阈值已接受。

Debug 的三次表征使用加入最终默认关闭编译分支前保存的可执行文件；该配置需要按当前
源码重新构建。Release 的三次表征开启 testOnly instrumentation。上述结果不能解释为
最终源码的 ON/OFF、static/shared 或安装消费者矩阵全部通过。

PD-1 及后续批次尚未实现；当前存档没有 PublicationRead、绑定索引、Runtime prepare/apply、
状态修订、消费注册或 journal 回收实现。4096 满槽缺陷仍未修复。
未运行全量 CTest、完整 Player 构建、完整格式门禁、安装消费者、hosted、实体设备或最终性能验收。
公共头、SDK minor 与正式合同行为均未在此快照中迁移。

## 后续与回退

逐批进度唯一维护在 [PD 计划待办](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md#13-实施待办与进度)，
当前实现摘要维护在 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。
下一步完成 PD-0 manifest/统计与当前源码验证，再沿 PD-1 → PD-2 → PD-3-a/b/c → PD-4 → PD-5。
正式合同差异按设计细则 §11 形成可审阅结果并履行其接受/版本门禁。

本次本地提交是包含完整设计和起步代码的回退点；其父提交不包含这些设计。
恢复时保留 `out/`、桌面设备证据和既有 `tools/__pycache__/`，不执行 reset/clean 删除证据。
