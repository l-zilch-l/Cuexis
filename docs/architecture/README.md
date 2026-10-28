# Architecture Index

状态：现行架构索引

更新日期：2026-10-28

按架构问题检索 Cuexis 文档：

- [架构总览](OVERVIEW.md)：产品结构、宿主边界、内容流和时间确定性。
- [音乐游戏玩法抽象模型](GAMEPLAY_ABSTRACTION_MODEL.md)：时间区间、判定域、动作以及
  Chart、Input、Judgement、Behavior、Presentation 的统一语义。
- [模块边界](MODULE_BOUNDARIES.md)：模块职责、依赖方向和格式阶段边界。
- [Player 应用结构](PLAYER_APPLICATION.md)：参考播放器的源文件、窄接口和关闭顺序。
- [实时协调设计](../adr/0046-realtime-playback-coordination.md)：已接受的时间、执行、发布与线程职责；当前分支已有 candidate 实现，验收状态见 CURRENT_STATUS。
- [执行状态、恢复与准备路径](execution-state-and-recovery.md)：当前 candidate 的分页共享状态、准备与锚点复用、来源身份、Transform 缓存和编码边界。
- [Stage 6 产品化边界](STAGE6_PRODUCTIZATION_BOUNDARIES.md)：candidate、renderer、Player support、media importer 和安装分层的 A2 合同。
- [RuntimeSession](RUNTIME_SESSION.md)：内部事务式运行时生命周期合同。
- [项目指南](../PROJECT_GUIDE.md)：工程原则、开发顺序和完成定义。
- [ADR 索引](../adr/README.md)：架构决策的背景、取舍和接受状态。

当前实现状态以 [CURRENT_STATUS.md](../CURRENT_STATUS.md) 为准；本页不复制模块合同。
