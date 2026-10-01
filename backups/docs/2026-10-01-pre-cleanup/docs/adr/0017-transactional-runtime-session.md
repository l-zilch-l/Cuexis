# ADR 0017：事务式 RuntimeSession 生命周期

日期：2026-07-17

状态：已接受

## 背景

逐步修改现有 World 的加载和 Reload 会留下半初始化状态，并可能在失败时破坏正在播放或预览的 Session。

## 决策

RuntimeSession 使用 PreparedRuntimeSession 事务准备，并在主线程帧安全点 commit。Reload 构建完整 Replacement，成功才交换，失败保留旧 Session。v1 不做 Entity 增量修补。

Session 拥有 World、ChartRuntime、ResourceScope、对象映射、PropertyResolver 和每 Session 系统状态，不拥有文档、资源管理器、音频、Renderer 或平台对象。

Unload 顺序为停止更新、使提取数据失效、销毁 World、释放 Scope。Reload 后旧 entt entity 全部失效。

## 备选方案

原地构建/修补会暴露部分成功状态；全局 Registry 多 Session 会让生命周期互相污染，因此不采用。

## 影响

创建与重载 API 返回结构化诊断，Player 和 Studio 共享相同 prepare/commit 路径。

## 后续风险

完整重建可能在大型谱面上产生延迟。未来可优化准备线程，但不能牺牲事务提交语义。

## SDK 转型补充（2026-07-20）

PlaybackSession 对宿主提供事务 load/commit/reload/unload 门面，并在内部复用本 ADR 的 RuntimeSession 强保证。PlaybackSession 还负责让旧 FrameSnapshot、Judgement 快照和 Replay 状态按明确规则失效；它不能通过暴露 RuntimeSession/World 让宿主绕过事务边界。
