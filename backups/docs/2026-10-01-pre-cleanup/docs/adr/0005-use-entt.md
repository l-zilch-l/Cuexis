# ADR 0005：使用 EnTT 管理运行时 Entity

日期：2026-07-17

状态：已接受

## 背景

Cuexis 需要统一表示 Note、Element、装饰物和其他运行时对象，并支持按 Component 组合行为。

## 决策

World 使用 EnTT Registry 管理运行时 Entity。Component 只保存数据，System 处理逻辑。ChartDocument、资产和编辑器文档不使用 EnTT 作为保存模型。

## 备选方案

继承式对象树不利于自由组合；自研 ECS 增加长期维护成本，因此不采用。

## 影响

EnTT 只进入 World 及依赖 World 的运行时模块，Chart 编译结果不保存 `entt::entity`。

## 后续风险

不得把 EnTT Registry 演变成全局服务定位器；跨 Session 引用必须使用稳定业务 ID。

## SDK 转型补充（2026-07-20）

ADR 0027 将 World/EnTT 明确为 Playback SDK 内部实现。安装后的 Playback 公共头、FrameSnapshot、JudgementResult 和 ReplayData 不得包含 `entt::entity`、Registry 或 World；宿主跨帧和跨 Reload 定位只使用 Cuexis 稳定 ID。
