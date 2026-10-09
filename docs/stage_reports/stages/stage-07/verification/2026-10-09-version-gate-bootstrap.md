# Version Gate bootstrap 验证、推送与保护阻断记录

证据日期：2026-10-09。用户本轮授权验证后推送 bootstrap，并取消当前 SHA CI；不修改保护、不代 SDK 审批、不合并或发行 PR #32。主线仍是现有 stage-7 / PR #32。

## 实际状态与最小反例

接手 stage-7 HEAD：`3c98463550ea736f9b07a29b526d8ca9b748beee`，工作区 clean；trusted master：`5472c463640cf3b66a03dd86fe87bd87239b2659`。
旧 SHA 的六个运行中 push/PR CI run `37881502333`、`37881502284`、`37881502277`、`37881497944`、`37881497949`、`37881497939` 均已实际取消并确认 completed/cancelled；已失败的 Version Gate `37881502289` 不伪称取消。新推送 CI 不在这次旧 SHA 取消范围内。

原五基础设施文件加日期更新组装为本地 `7da4d5b1502904a5caa0691f1cb401a6cf2093a7`。SDK `0.7.0`、日期 `26.10.09-1`，未引入 Stage 7 Gameplay 代码。直接 `git push origin HEAD:master` 被 GitHub `GH006` 拒绝：Required status check "Version advancement (pre-merge)" is expected。远程 master 未改变；未绕过保护、未提交伪造成功状态。

这是首次安装的部署顺序冲突：新 workflow 要求旧 master 提供 owners；旧 master 无 owners；master 又拒绝没有 pre-merge 成功记录的直接推送。单纯发布 SDK approval 评论或重跑旧 run 均不能消除它。

## 两步候选与合同归属

采用保持 SDK 不变的基础设施候选：先部署新版 checker、owner registry、CODEOWNERS 和日期文件，保留旧 workflow/tests；这一步由旧 master 的可信 checker/tests 验证。随后现有 PR #32 同步该已合入的 base，新 workflow 才读取 base 新 checker/registry，并运行 base 的旧兼容 tests。新 workflow/tests 继续由 PR #32 集成；不从候选树取脚本回退，不引入新的审批语义。

候选 SHA：`3076948ce403ea0da6fbe389835b6a4c9defbcd3`，已推送 `codex/version-gate-bootstrap`。相对 master 仅五文件：`.github/CODEOWNERS`、`.github/sdk-api-owners.json`、`tools/check_version_gate.py`、`cmake/CuexisVersion.cmake`、`vcpkg.json`。SDK 保持 `0.7.0`，日期由 updater 更新为 `26.10.09-1`。未创建另一个 PR，未合入 master。

所属合同为 [VERSIONING](../../../../guides/VERSIONING.md) 的可信基础设施安装与 SDK owner record；[plan-a](../../../../stage_plans/active/stage-07/plan-a.md) 记录部署顺序。C78/R78 字段语义、SDK公共 API 与 Gameplay 执行规则不变。

## 验证与原始输出

原始命令、输出、失败及候选补丁见 [证据归档](2026-10-09-version-gate-bootstrap-evidence.zip)。

| 验证 | 实际结果与边界 |
| --- | --- |
| 完整新版门禁测试 Windows | 25 tests OK，2 POSIX skip |
| 完整新版门禁测试 Linux/WSL | 25/25 OK，无 skip；实际 bootstrap shell 正反例 |
| 新 checker 配旧 tests/workflow：Linux/WSL | 19/19 OK，覆盖部署兼容性；独立只读审查同样通过 |
| 同组合 Windows 旧 tests | 19 tests，1 error：旧测试对 WSL shim 的误识别及临时目录 WinError32；原始输出保留，不能称全平台旧测试通过。新版测试已修复此环境识别，仍随 PR #32 集成 |
| 旧 trusted master checker 对真实候选 SHA | `version.gate.pass`，实际 base/candidate/UTC/date/SDK不变比较，无审批布尔值或 candidate fallback |
| SDK0.7.0 日期更新 fresh Release / clean-first | Werror ON，Playback与audio_sdl实际构建通过；121 cases / 10615 assertions；架构/安装Playback consumer 2/2 |
| docs/current/version/diff | bootstrap树 docs 278/20通过，version.current.pass sdk0.7.0 / 26.10.09-1，updater --check与diff --check通过 |

两组 bootstrap 候选的 C++/公共头/版本文件相同；兼容候选仅回留旧 Python tests/workflow，不重做没有新输入的 C++ 构建。独立审查检查了可信base执行、审批tuple、CLI不能授权、真实actor校验和部署顺序；没有代审批。

## 未退出门禁与下一步

远程 master 仍为旧基线，因此 PR #32 的 bootstrap_required 仍未解决。需要 owner 允许一个仅用于 bootstrap 的独立 PR，使候选走受保护 pre-merge 流程；这是对原“不另建PR”边界的明确例外，当前未获授权，未创建。

候选合入 master 后才同步 stage-7，同日日期升至 `26.10.09-2`（若日期变化则按实际 UTC重新决定），fresh/clean-first/consumer，最终 SHA 的 SDK owner真实评论审批，再触发 gate。不能提前沿用旧 tuple；approval评论本身不自动触发 workflow，发布后重跑该最终 SHA 的 Version Gate。

S7A-8.4 未退出：可信安装尚未完成、owner精确审批、保护/required workflow的可信来源与新SHA hosted仍待回填。GPU/window/audio/真实设备本轮未执行；不扩大生产预算、不做S7A-9关闭、不进入S7B+/S7C/Stage8正式发行或Stage7A关闭。
