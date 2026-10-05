# S7A-3/4 Hosted Evidence and S7A-5 Handoff

状态：dated verification and planning record；3/4受限功能验收与同源SHA hosted齐全，5–9本轮仅规划

日期：2026-10-05

## 1. 新提交与既有证据

分支stage-7；代码行为提交 `a0c8b7e4f783995bc19e626f3654ab11f845a0a7`，build26.10.05-1、SDK0.7.0。
owner告知CI全绿后，本轮重新读取GitHub Actions API并核对所有run的headSha、conclusion及jobs。
[本地功能验收](2026-10-05-s7a-3-4-functional-acceptance.md)、
[E1逐行审计](2026-10-05-s7a-3-4-e1-audit.md)、
[CI优化基线与本地验证](2026-10-05-ci-runtime-optimization.md) 保留当时记录，
其“hosted尚未验证”被本报告的新证据补充，未把历史记录改写为新的运行结果。

| workflow / event | run | jobs | conclusion |
| --- | --- | --- | --- |
| Linux Quality / push | [37321966005](https://github.com/l-zilch-l/Cuexis/actions/runs/37321966005) | 14 success / 0 skipped | success |
| Windows MSVC / push | [37321966334](https://github.com/l-zilch-l/Cuexis/actions/runs/37321966334) | 2 success / 0 skipped | success |
| Windows MinGW / push | [37321966023](https://github.com/l-zilch-l/Cuexis/actions/runs/37321966023) | 2 success / 0 skipped | success |
| Linux Quality / pull_request | [37321974655](https://github.com/l-zilch-l/Cuexis/actions/runs/37321974655) | 14 success / 0 skipped | success |
| Version Gate / pull_request | [37321974886](https://github.com/l-zilch-l/Cuexis/actions/runs/37321974886) | 1 success / 2 skipped | success |
| Windows MSVC / pull_request | [37321974899](https://github.com/l-zilch-l/Cuexis/actions/runs/37321974899) | 2 success / 0 skipped | success |
| Windows MinGW / pull_request | [37321974605](https://github.com/l-zilch-l/Cuexis/actions/runs/37321974605) | 2 success / 0 skipped | success |

共7个runs、37个成功jobs；Version Gate另2个按event条件跳过的jobs，不记为实际运行通过。
Linux两次Quality各14个jobs均成功，包括现有sanitize/coverage/tidy等验证组合；
这不证明新judgement模块达到某个未登记coverage阈值或所有容量通过。
Windows两个工具链的Debug/Release、媒体组合均在现有workflow内通过。

API的source headSha均为a0c8b7e。push验证实际分支提交；pull_request在GitHub的PR/合成merge上下文执行，
不把source headSha当成PR checkout的每个GITHUB_SHA。旧6b11d10只作耗时基线，未转记为新增实现成绩。
本次未单独采集cache service命中日志或做受控性能对比，CI全绿证明配置/组合可执行，
不直接等于已证明全部缓存命中或已测得预计提速。

## 2. 下一轮实施选择

评估S7A-5至9后，选下一次完整实施S7A-5.1–5.5，首卡先完成Ruleset内存profile与字段准入。
5的Fold/Score状态闭包是6的Snapshot/Replay前置，6又是7的产品集成前置；
8需四项Stage6交接及owner-only版本门禁，9需全部对象模型、真实容量/阈值和owner最终接受。

详细交付与风险见 [五批规划评估](../../../stage_plans/active/stage-07/s7a-5-9-delivery-plan.md)，
可直接复制的下一对话指令见 [S7A-5接手文档](../../../stage_plans/active/stage-07/s7a-5-implementation-handoff.md)。
本轮只作文档、接口/生命周期定点核对和清理，没有实施5–9或冻结新Ruleset字段/数值/codec。
S7A-9预算整体继续INCOMPLETE GATE，Stage7A不关闭，不进行merge/release或SDK0.7.1放行。

实时D-9证据：stage-7有SYSTEM_BASH/视图一致性探针；origin/master的同一脚本无这些守卫。
历史“未提交工作区补丁”不再是当前事实；master落库、具名接受和SDK条件化放行仍需owner独立处置。

## 3. 临时证据归档与清理

证据ZIP位于工作区外：

    D:\Cuexis-worktree-evidence\stage-07\a0c8b7e-2026-10-05\s7a-3-4-local-evidence.zip

同目录有evidence-inventory.json（逐文件path/bytes/SHA256和清理targets）及cleanup-summary.json。
ZIP内部保留原out/...路径，额外含evidence-manifest.json；可按旧报告basename定位原始日志。
旧报告的out路径描述运行时位置，清理后以此归档为实际读取位置。

- 文件数：2618；原始逻辑字节188,361,224；ZIP字节41,207,541。
- ZIP SHA256：`bd2a8a583f4989de9e8728062bbbb38c895016386f469e8b548ec4d931da276f`。
- 归档后执行CRC和每个member SHA256校验，再以PowerShell Get-FileHash核对整体hash。
- 删除138个已验证位于D:\Cuexis-worktree\out内的本次临时targets：out/s7a-*、ci-draft、ci-tools、
  ci-media-temp、prepush-debug-temp、prepush-release-temp；删除前验证所有最终绝对路径和reparse points。
- 已确认owned s7a-*及上述目录无残留；没有使用git clean -fdx或跨shell拼接删除命令。

保留out/build的可复用配置/依赖缓存、早期s7a0/s7a1证据和非本次生成的旧日志，
以及用户backups、.buildenv.ps1、.dsh和所有tracked源文件/fixture/历史报告。
WSL的Linux依赖prefix仍是相关build配置的输入，不因清理工作区而盲删外部依赖。
归档含本次script/CLI数据和actionlint工具，下一轮需要复核时读取归档；不把vendor/cache内容提交到Git。

## 4. 本轮文档门禁与工作区

本轮为docs-only，按AGENTS执行以下检查，退出码均为0：

| 检查 | 本轮结果 |
| --- | --- |
| `python -X utf8 -B tools/check_docs.py` | 365个Markdown、20个candidate JSON/CXT通过 |
| `python -X utf8 -B tools/check_docs_status_contract_tests.py` | 4 tests通过 |
| `python -X utf8 -B tools/check_docs_target_contract_tests.py` | 2 tests通过 |
| `python -X utf8 -B tools/update_version.py --check` | 26.10.05-1一致 |
| `git diff --check` | 无空白错误 |

未重新运行C++/CTest；以上docs checks不作为产品实施证据。
文档与清理结果提交到stage-7以保持Git工作区干净；实际文档提交SHA以Git记录为准。
本报告引用的hosted证据只属于a0c8b7e，后续文档提交或下轮新代码SHA不能借用为自己的同SHA成绩。
