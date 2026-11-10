# 源码拆分安全网独立审计（2026-11-03）

状态：archived evidence；独立复核原文与现状注记

审计对象：`562f54d3`（`Cuexis-redesign` 分支）完成的超限源文件拆分

归档日期：2026-11-05

返回 [Shared Evidence 索引](../README.md)。当前阶段状态只以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准。

## 0. 归档说明

这份审计由独立复核方在 2026-11-03 完成，不读 `out/`、不使用仓库自带脚本，目标是对抗性验证拆分后的
“安全网”（架构门、形式化门）是否真的能拦住错误。原文逐字节保存在 [AUDIT_REPORT.md](AUDIT_REPORT.md)，
本节只记录归档时对现状的核对，不回填原文结论。

原文列出的五个“真实缺口”的当前状态（核对 `cmake/VerifyArchitecture.cmake` 与
`tools/verification/check_gameplay_diagnostic_formalization.py` 的现行实现）：

| 原文缺口 | 现状 | 依据 |
| --- | --- | --- |
| ① 类型检查接受文本、不接受定义 | 已关闭 | `_cuexis_strip_inactive` 剥离注释与 `#if 0` 分支；`_cuexis_declaration_pattern` 要求 `<struct\|class\|enum class> <Name> … {` 形状，`AmountSpecification` 不再满足 `AmountSpec` |
| ② 函数检查是纯子串 | 已关闭 | 函数条目要求 `名字(` 形状，且注释已剥离 |
| ③ 无可达性检查 | 已关闭 | `_cuexis_require_partition_includes`：每个分区头必须被伞头或同分区其他头 include，否则 FATAL |
| ④ 分区被拼成一个字符串 | 仍是设计现状 | 声明检查仍在拼接后的文本上匹配，跨文件边界命中在原理上仍可能 |
| ⑤ 形式化门可被静默空转 | 已关闭 | 缺失必要输入、缺失扫描目录、`execution_kernel*.cpp` 少于两个单元均直接记失败 |

原文的“未核实”边界按原样保留：Release 全矩阵、跨平台（Linux/MinGW）与外部消费者、旧新解码器差分 fuzz、
其余拆分提交的重推。

原始日志未入库：52 份 `gate_*`、`probe_*`、`D1_*`、`C_*`、`B_*` 日志仍在工作区
`out/_audit-2026-11-03/`；原文 §Logs 写的 `out/audit2/` 是审计当时的临时路径，归档来源为其后的
`out/_audit-2026-11-03/gate-audit/`。

## 1. 归档内容

| 路径 | 内容 |
| --- | --- |
| [AUDIT_REPORT.md](AUDIT_REPORT.md) | 审计原文（英文，逐字节） |
| [tools/ast_inventory.py](tools/ast_inventory.py) | clang AST 声明清单 |
| [tools/compare_bodies.py](tools/compare_bodies.py) | 函数体 token 比对 |
| [tools/compare_split.py](tools/compare_split.py) | 拆分前后行与 token 比对 |
| [tools/stage_new_only.py](tools/stage_new_only.py) | 仅新增行分类 |
| [tools/probe_amount.cpp](tools/probe_amount.cpp) | 架构门反例探针（类型条目） |
| [tools/probe_mapping.cpp](tools/probe_mapping.cpp) | 伞头可达性反例探针 |
| [manifest.json](manifest.json) | 本目录逐文件 bytes、sha256 与 content_sha256 |

## 2. 与实施侧记录的关系

CURRENT_STATUS 的“超限源文件拆分完成（2026-11-03）”记录的是实施侧四项校验（门变异 4/4、解码器 12/12、
`GraphSink` 21/21、27 个拆分头独立编译）。本审计是独立于实施侧的第二组读数（12/12 阶段、40/40 函数体、
64/64 函数体、444/444 AST 声明），两者可交叉阅读。

两者都不构成 Release 全矩阵、跨平台或完整等价性证明；拆分只改组织方式，不改行为契约这一结论仍以
CURRENT_STATUS 与相应测试为准。
