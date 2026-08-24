# Stage Chart Format Update CFU-G4 Closure Readiness

状态：CFU-G4 complete；离线关闭准备包已冻结，CFU-G 仍为 active

执行日期：2026-08-20

本地基线：`5964f4bf779a413e650f77d9e8e7a5cc98e98f59`（`Prepare CFU-G local
candidate`）。该提交只存在于本地 detached HEAD，没有推送，也没有同 SHA hosted workflow。

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)
§5.9 与 §11。

## 1. G4 结论

G4 把最终关闭所需的证据、缺口、记录格式和状态切换动作整理成可直接执行的本地关闭准备包。
它没有绕过 G3 的 hosted 门禁，也没有创建
`stage_chart_format_update_completion_report.md`、记录项目所有者接受、关闭 CFU-G 或解锁 Stage 4。

完成内容：

- 冻结 18 项退出条件的最新状态；
- 建立本地与历史 hosted 证据索引；
- 冻结最终 SHA hosted run 的记录字段和失败记录规则；
- 冻结 completion report 的最小实证内容；
- 冻结项目所有者接受入口与 Stage 4 状态切换清单；
- 明确网络恢复后的唯一执行顺序，不安排动画运行时实现。

## 2. 退出条件台账

| # | 退出条件组 | 状态 | G4 结论 |
| ---: | --- | --- | --- |
| 1-14 | 格式、迁移、Playback、consumer、确定性、CXC、license、API、工具、外部资产 | PASS | G1 审计与 C-F 报告已有实现和证据；G3 本地候选未发现回归 |
| 15 | 最终实现 SHA 的 hosted Linux/Windows | PENDING | 本地提交尚未推送；不得复用 `913639c` 或 CFU-F runs |
| 16 | completion report 与项目所有者接受 | PENDING | 必须等待条件 15；G4 只冻结报告内容和接受入口 |
| 17 | Stage 4 typed handoff | PASS | G2 已冻结 typed input、capability、fixture、预算、diagnostics、所有权和风险 |
| 18 | 权威状态与链接一致 | PASS | G0-G4 状态由 docs checker 防回退；Stage 4 仍 blocked / not started |

当前汇总为 `16 PASS / 2 PENDING / 0 PRODUCT BLOCKER`。两个 pending 都是关闭顺序门禁，不是
C++、Schema、fixture、package 或工具缺陷。

## 3. 证据索引

| 范围 | 权威证据 |
| --- | --- |
| C1 Reader/Schema | [CFU-C1](260811-chart-format-update-c1-reader.md) |
| C2 Writer/lowering | [CFU-C2](260811-chart-format-update-c2-lowering.md) |
| C3 CXC package | [CFU-C3](260812-chart-format-update-c3-cxc.md) |
| C4 tools | [CFU-C4](260813-chart-format-update-c4-tools.md) |
| D migration/equivalence | [CFU-D close](260814-chart-format-update-d-close.md) |
| E Playback/API/identity | [CFU-E close](260814-chart-format-update-e-close.md) |
| F hosted determinism/safety | [CFU-F close](260816-chart-format-update-f-close.md) |
| G exit audit | [CFU-G1](260816-chart-format-update-g1-exit-audit.md) |
| Stage 4 handoff | [CFU-G2](260816-chart-format-update-g2-stage4-handoff.md) |
| local candidate validation | [CFU-G3](260819-chart-format-update-g3-validation.md) |

G3 本地结果：Debug 与 Release 各 `378 passed / 1 skipped`，headless Release
`343 passed / 1 skipped`，external/package Debug 与 Release 各 `7/7`，format、architecture、
public-header ASCII、license/NOTICE、version、documentation 与 `git diff --check` 通过。唯一 skip 是
Windows symlink capability 条件。

## 4. 最终 hosted 记录合同

网络恢复并明确允许推送后，最终候选必须包含 G0-G4 文档与检查器状态。三个 workflow 对同一
`headSha` 运行，逐项记录：

| 字段 | 要求 |
| --- | --- |
| workflow | `Linux Quality`、`Windows MSVC`、`Windows MinGW` |
| run URL / database ID | 记录实际 run，不写计划链接或旧 run |
| branch / headSha | 三套结果必须指向同一最终提交 |
| jobs | 记录每个 job 的 conclusion，不能只写 workflow 总结 |
| first failure | 若失败，记录第一个失败 step、exit code 和稳定错误摘要 |
| rerun | 区分 failed-job rerun 与新提交；新提交使旧候选证据失效 |
| artifacts | 记录 determinism、sanitizer、coverage、analysis 或 package artifact 名称 |

旧 `913639c` 三套成功 run 只证明 CFU-F 后基线，不包含 G0-G4，因此只能作为历史参考。

## 5. Completion Report 输入

条件 15 完成后，`stage_chart_format_update_completion_report.md` 至少记录：

- 最终 commit SHA、版本 `26.08.01-1`、SDK API `0.6.0` 和 FrameDigest v3；
- 本地 Debug/Release/headless、format、architecture、ASCII、license、version 与 docs 结果；
- 同 SHA Linux Quality、Windows MSVC、Windows MinGW run URL、job 结论和失败/rerun 历史；
- CXC v1、Chart v4、CXT v1、迁移、Playback prepare/identity、tools 和 compatibility 的实际交付；
- 明确未交付公共 CXC package API、非空 v4 动画 Playback、AnimationSystem、脚本或稳定 C ABI；
- 残余风险、MinGW shared experimental 边界和 Windows symlink skip；
- 项目所有者的明确接受记录。

报告不得复制完整格式合同，不得把 G4 模板文字当作实际 hosted 结果，也不得在接受前写成阶段已关闭。

## 6. Owner Acceptance 与 Stage 4 切换

项目所有者接受前的状态固定为：

```text
Stage Chart Format Update  active
CFU-G                     active
G3 hosted verification   pending
G4 closure readiness     complete
Stage 4                   blocked / not started
```

只有 completion report 已绑定最终 SHA 且由项目所有者明确接受后，才执行一次状态事务：

1. `CURRENT_STATUS` 将格式阶段改为 completed；
2. ROADMAP、计划索引和格式索引同步 completed 状态；
3. Stage 4 改为 `unblocked but not started`；
4. completion report 与项目所有者接受记录进入报告索引；
5. 文档检查器从 G4 active 防回退切换为 completed 防回退。

该事务不创建 `cuexis_animation` target，不实现 AnimationSystem，也不放开非空动画 capability。

## 7. 网络恢复后的执行顺序

1. 确认本地 detached HEAD 和目标分支关系，保留 G0-G4 提交；
2. 经用户允许后推送，不做 force push；
3. 等待同 SHA 三套 hosted workflow 完成并收集证据；
4. 如有产品或 workflow 修复，重新冻结最终候选并重跑全部三套 workflow；
5. 全部通过后创建 completion report；
6. 等待项目所有者明确接受，再关闭 CFU-G 与格式阶段。

G4 到此完成。网络不可用期间没有进一步可合法关闭的 CFU-G 门禁。
