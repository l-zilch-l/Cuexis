# Cuexis Guides

状态：现行指南索引

更新日期：2026-10-01

<!-- cuexis-meta
id: INDEX-guides
status: active
authority: index
-->

本目录集中放置构建、编码、依赖和版本维护指南。产品状态以
[CURRENT_STATUS.md](../CURRENT_STATUS.md) 为准；格式合同以
[formats/README.md](../formats/README.md) 为准。

## 工程入口

- [阅读顺序](reading-order.md)：从零理解本仓库的带注解推荐路线
- [Chart 与 Packed 预算上限](LIMITS.md)：由 `limits.hpp` 生成，引用预算时指向本页而非复述数字
- [Player 命令行选项](PLAYER_OPTIONS.md)：由 `player_options.cpp` 生成，选项名与构建门控以本页为准
- [诊断码与类别](DIAGNOSTIC_CODES.md)：由 `schemas/*.codes.json` 生成的可读视图，类别语义仍归 Spec §9.2
- [构建、安装和质量门禁](BUILDING.md)
- [Chart v5 谱面编写指南](CHART_V5_AUTHORING.md)
- [编码、错误和线程政策](CODE_POLICY.md)
- [依赖政策](DEPENDENCY_POLICY.md)
- [版本政策](VERSIONING.md)

工程指南描述构建、编码、依赖和版本操作规则；格式编写指南提供面向作者的字段速查和手写示例，
不替代 `docs/formats/` 下的格式合同，也不复制一次性验证证据。阅读顺序只给出"读什么、按什么
顺序读"，不复制任何合同、字段或当前状态结论。
