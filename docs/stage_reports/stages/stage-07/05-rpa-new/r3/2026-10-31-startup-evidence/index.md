# 启动响应与渲染归因附件

状态：冻结证据索引。返回 [批次报告](../2026-10-31-r3-startup-and-render-attribution.md)。

- [复制来源、原始字节及哈希](manifest.json)
- [身份修正、候选取舍及审查限制](identity-and-selection-supplement.json)
- [最终格式化探针正常/关闭检查](gl-startup-final-check-v1/results.json)
- [旧状态缓存组中断](gl-state-measure-v3/interrupted.json)
- [实例化固定 screen 决定](gl-instancing-screen-v2/decision.json)
- [启动阶段归因](startup-attribution-measure-v1/results.json)
- [JSON 子阶段归因](json-attribution-measure-v1/results.json)
- [JSON 差分/反例检查](json-sax-check-v3/decision.json)
- [SAX 独立固定组](json-prepare-measure-v1/decision.json)
- [parse-once 组合固定组](legacy-parse-once-measure-v1/decision.json)
- [渲染分段摘要](render-attribution-summary.json)
- [独立产品分页 writer](product-sequence-writer-measure-v1/decision.json)
- [独立产品分页 Seek](product-sequence-seek-measure-v1/decision.json)

其余源码、命令、日志和失败记录按组目录保存，完整路径见 manifest。
日志以 `.log.txt` 原字节复制；源码以 `.cpp.txt` / `.hpp.txt` 保存为未选用附件。
两份未选用 patch 为原型恢复输入，不等于纳入产品或通过接受。
