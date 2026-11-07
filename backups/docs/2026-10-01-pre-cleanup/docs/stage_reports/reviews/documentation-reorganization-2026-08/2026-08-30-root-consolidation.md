# Cuexis Documentation Root Consolidation Report

状态：completed

快照日期：2026-08-30

本报告追加记录文档信息架构整理的根目录收敛。它不改写此前的产品状态、历史报告或验证结果。

`docs/` 根目录原有 23 个独立文件，其中 17 个是仅包含一个 canonical link 的 compatibility entry。
这些单文件 stub 已删除，旧逻辑路径收敛至 [legacy mapping](../../../legacy-paths.md)。

根目录现在只保留：`README.md`、`CURRENT_STATUS.md`、`PROJECT_GUIDE.md`、`ROADMAP.md`、
`DOCUMENTATION_POLICY.md`、`legacy-paths.md` 和机器读取的 `status_contract.json`。`check_docs.py`
现在强制该白名单，避免以后再次在根目录堆积业务文档。

本次收敛不保留旧的逐文件 URL；历史名称到 canonical 文档的映射可审计，完整预整理备份仍由
`2026-08-30-reorganization.md` 记录。
