# Stage 7 document and evidence reorganization

状态：completed documentation maintenance

日期：2026-10-26

## 范围与入口

先完整备份 docs，再按 A、B、旧 RPA、新 RPA、PD 及独立诊断、共享材料整理。[计划位置总表](../../../../../stage_plans/active/stage-07/README.md)与[报告总表](../../README.md)按阶段和批次定位。阶段与批次目录使用英文；同批报告、审查、交接和附件集中保存。各阶段 evidence-catalog.md 列出原文证据路径，不表示这些历史路径目前已逐个验证存在。B 阶段现有材料没有独立报告，索引明确说明。

## 备份与保全

完整备份：D:/Cuexis-worktree/out/documentation-backups/20261026T031011-stage7-reorganization/docs-before.zip。
逐文件 manifest 共 558 文件、60,043,603 原始字节，ZIP SHA256 为 4e30f8c68fe76ae5e881e058b4b821911ceec4c2e13fa276fdbb9db4629eefb1；归档内容已逐文件核验。179 个文件移动映射与结果存于同备份目录。原始文件均在映射目标保留，43 个非 Markdown 文件逐一 SHA256 一致。正文仅修 Markdown 导航；历史状态、日期、代码块、绝对路径和登记身份保留。旧路径集中映射于各 legacy-paths.md。

原始 out、ETL、claim/seal、注册身份、失败窗口与只读历史工作树保持原位。桌面两份接手文件先单独备份，再更新导航和本次补充；其历史 source/result manifests 与测试记录保留原值。

## 验证与证据边界

检查器只适配新的目录、计划归属与诊断映射文件位置；章节权威、目录索引、可达性、相对链接和负例门禁继续运行。文档检查使用已有 default-off 配置生成的 target facts：out/build/consolidated-default-docs-20261026/generated/cuexis-targets.txt；debug 配置的观察/执行宿主目标不同，不将该列表作为默认文档合同。

精确命令、退出码、附件与正文保全结果见 out/documentation-backups/20261026T031011-stage7-reorganization/final-verification.json。本次没有修改 C++/CMake 构建输入，不重跑全套 C++ 构建或性能/设备测量；文档整理不改变 R2、PD-5、RPA 或 Stage 7 的接受状态。
