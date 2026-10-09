# RPA归入Stage 7子阶段的规划修订

状态：dated handoff；仅文档归属与导航修订

证据日期：2026-10-09（Asia/Shanghai）

## 依据与基线

owner明确要求RPA属于Stage 7子阶段，不放在future独立阶段中。
核对工作区为 `D:\Cuexis-worktree`，分支 `stage-7`，HEAD
`c4b1f68db0d809b9a9c0c27b37d53538423417ff`，沿用PR #32。
接手已有VERSIONING及审批助手未提交修改，完整保留；本次不更改产品实现或工作流。

## 唯一计划与职责

| 项目 | 修订 |
| --- | --- |
| 归属 | Stage 7内的Stage 7-RPA，与7A、7B+按分册管理 |
| 唯一正文 | [plan-rpa](../../../../stage_plans/active/stage-07/plan-rpa.md)，受[总计划](../../../../stage_plans/active/stage-07/plan.md)约束 |
| 旧路径 | `docs/stage_plans/future/realtime-playback-foundation/plan.md`只留兼容跳转，不维护第二份计划 |
| 工作编号 | RPA-A0–A7、RPA-N0–N3保持，架构、命名、实体设备与实时同步均在RPA验收 |
| 7A边界 | 7/8受限集成与原门禁保持，S7A-9按原内核/消费者/预算范围验收，不新增RPA前置 |
| 7B+与Stage 8 | 7B+高级能力线保留；Stage 8消费7A、7-RPA及选入并验收的7B+成果 |

RPA纳入active目录只表示已纳入Stage 7规划，不能解释为产品重构、命名迁移、设备验收或
阶段关闭已完成。旧设备失败及历史测量保留；不接受生产阈值，不实施S7B+/S7C或发行工作。
当前状态只在CURRENT_STATUS维护；旧报告补后续修订说明，不倒改历史证据。

## 验证与未执行项

本次为文档变更，验证命令与原始输出保存在 `out/rpa-stage7-substage/validation.log`。
结果如下，全部退出码为0：

| 命令 | 原始结果摘要 |
| --- | --- |
| `python -B tools/check_docs.py` | `Documentation checks passed: 403 Markdown files and 20 candidate JSON/CXT files validated.` |
| `python -B tools/check_docs_status_contract_tests.py` | `Ran 4 tests`，`OK` |
| `python -B tools/check_docs_section_contract_tests.py` | `Ran 5 tests`，`OK` |
| `python -B tools/check_docs_target_contract_tests.py` | `Ran 2 tests`，`OK` |
| `git diff --check` | 无空白错误；本轮修改文件已恢复工作区CRLF换行 |

实施SHA不适用；上述HEAD仅为文档修改基线，没有新的产品行为证据。
C++ Debug/Release、ON/OFF、安装消费者、MinGW/Linux、hosted与GPU/window/audio/实体设备
验证均未执行。本次不提交推送，不改版本，不关闭S7A-8.4或任一子阶段。
