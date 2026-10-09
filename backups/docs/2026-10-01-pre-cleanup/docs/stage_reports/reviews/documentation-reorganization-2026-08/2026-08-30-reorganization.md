# Cuexis Documentation Reorganization Report

状态：completed

快照日期：2026-08-30

## 结论

本报告记录 `docs/` 信息架构整理的实施证据。它不改变 Cuexis 产品阶段状态、Full Review 关闭结论、
Stage 5 的 hosted/owner acceptance 前置条件，或任何历史报告的原始验证事实。

整理前已创建并解压验证完整文档备份：

- archive: `C:\Users\Zilch\Desktop\Cuexis_docs_pre_reorganization_20260830-235703.zip`
- SHA-256: `98E19E0650F0F3D66002E39211931D616877EA40C3D98A97D1A443E4E49E843F`
- extracted entries: `224`

## 实施范围

- 19 份 canonical stage plan 迁入 `active`、`completed`、`future`、`deferred`、`reviews` 或
  `historical` 阶段/专题目录。
- 74 份 canonical stage report 迁入阶段目录、Chart Format Update 目录、Full Review 专题或
  SDK transition 专题；带日期报告规范化为 `YYYY-MM-DD-topic.md`。
- 旧逻辑路径由各自的 `legacy-paths.md` 集中映射到 canonical 文档；批量重组不保留单文件 stub，避免
  在原目录重新制造平铺文件。原始历史正文未被改写为新的状态或验证结果。
- 每个新增阶段目录包含 README；根索引按当前、已完成、未来、延期、阶段和跨阶段专题导航。
- 新增 `docs/api/`，覆盖 Playback session、source/content provider、frame/digest/timeline、
  presentation/capability、diagnostics/identity/compatibility 和 internal module catalog。
- `CURRENT_STATUS.md` 收敛为当前摘要；逐批 CFU 和 Full Review 证据仅保留在相应报告目录。
- `DOCUMENTATION_POLICY.md` 与 `tools/check_docs.py` 增加目录化、命名、compatibility entry、
  API reference 和阶段根目录防回退合同。

## 验证

`python -B tools/check_docs.py` 通过，验证 326 个 Markdown 文件和 20 个 candidate JSON/CXT 文件。
`git diff --check` 通过，无 whitespace error；Git 的 LF-to-CRLF 提示仅反映工作区行尾策略。

## 后续维护

如需对外发布逐文件 redirect，必须作为独立兼容需求明确提出。当前的 `legacy-paths.md` 保留旧路径到
canonical 文档的可审计映射，但不承诺旧的逐文件 URL 继续存在。
