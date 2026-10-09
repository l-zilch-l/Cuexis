# CRLF approval repair and device acceptance fixture

状态：实施与受限验证报告；设备观察、可信部署与阶段关闭独立保留

证据日期：2026-10-09

## 基线与范围

接手实际 stage-7 HEAD `c110c500e1e7987fe19a47ba7141d0787e5ce4b8`，已有两个未提交
CRLF 修复文件保留继续完善；不 reset，不新建 PR，不写 master 或修改保护，不代发审批。
SDK API 保持 `0.7.1`，Python 验收工具与注释合同不改变 production public API。
日期构建通过 updater 升为 `26.10.09-3`；新候选的 approval/hosted 不沿用旧 HEAD。

旧 HEAD 的真实 owner LF 评论 `6075108949` 已通过 Version Gate run `37886565979`
attempt 4/job `113689277132`；原 CRLF 评论 `6074639562` 也具有匹配 tuple，但旧检查器未识别。
本修复同时处理评论筛选、最新记录解析与 validator；原始 8192 字符限制、actor、未编辑、
日期、精确字段/类型、SHA/tree/base 与最新记录撤销逻辑保持。
所属合同为 VERSIONING §S7A-8.4，计划 §3.4.14；不是代替 owner 的放行。

## 回归与原始证据

`python -B tools/check_version_gate_tests.py`：26 项通过，Windows 两项 POSIX 专项跳过。
`wsl --cd /mnt/d/Cuexis-worktree python3 -B tools/check_version_gate_tests.py`：26 项全通过。
新增正负例覆盖 CRLF actual Git/API 筛选、squash/fetch、错误 tuple、非 owner、编辑、
原始长度超界，以及新 invalid CRLF 记录不能回退旧有效批准。初次修复前两条回归均失败。
读取真实 GitHub comment 的旧检查器失败/本地修复通过输出在
`out/version-gate-crlf-trusted-current.log`、`out/version-gate-crlf-local-crlf-fix.log`。
这些是本地真实 API 验证，不声称 hosted 使用了尚未进入 trusted master 的新 checker。

日期构建 26.10.09-3 的 MSVC candidate Debug/Release 均实际 fresh configure、clean-first
构建 Player/assembler/playback；每轴 Gameplay 25 cases/1467 assertions，通过架构、安装
Playback consumer 与 Gameplay installed gate 3/3。完整命令/exit 保存于
`out/crlf-device-candidate-debug-build.log`、`out/crlf-device-s7a78-candidate-release-build.log`；
Windows/Linux Python 回归原始输出为 `out/crlf-device-windows-tests.log`、
`out/crlf-device-linux-tests.log`。本轮没有重跑新日期的 MinGW/shared/sanitizer 全矩阵，
旧 c110c50 hosted 成功不移用到本轮新 HEAD。

## 测试谱面

[操作与预期](../../../../examples/s7a78-device-acceptance.md) 是唯一设备操作说明。
`make_device_acceptance_chart.py` 生成键盘/音乐两套 author、configuration、CXC、
装配日志和 manifest，`actualPrepareValidated=true`、`productionBudgetAccepted=false`。
合同：Gameplay author profile v1、执行 T4/K4 v1、Capsule revision 3、Graph JSON v1、
CXC v1、GameplayConfiguration v1、SDK preview 0.7.1；不改变 C78/R78 的执行语义。

音频为本工具合成 60 秒 PCM16/48 kHz 单声道 120 BPM，四拍高音重拍；纹理自行生成，
quad/material 来自仓库 demo20s，无网络素材。四条 requirement、六 phase、六方块，
D/F Tap、J 故意 Miss、K Hold head/body/tail。
SDL 本地头核对 D/F/J/K scancode 分别为 7/9/13/14；这些键未占用默认快捷键。
启动前 Enter 确认准备，随后 Player 自动播放；不会错把 Space 当必需启动键。
H/T 每 admitted frame +1；不是音乐时间、设备时钟或校准结果。

人工 golden：基础操作 `score=13 combo=3 hits=5 misses=1`，零输入
`score=-6 combo=0 hits=0 misses=6`。`check_device_acceptance_chart.py` 不调用生产推进
函数计算预期；独立 zipfile/hash/wave 完整验证 manifest/artifact/PCM。
四个公共 Host 进程覆盖 Graph/Packed ×输入/零输入，核对上述人工结果，且公共路径
完整 Replay 比较为 same。两次音乐版 ChartClock 拒绝是稳定负例，不能说成音频播放通过。
装配命令与实际输出：`out/device-acceptance/*-assemble.log`，Host 命令/exit/原始输出：
同目录 `check-01.log` 至 `check-06.log`，输入为 `hit-observations.txt` 与 `commands.txt`。
按 D/F/J/K 再生成一次，两个 CXC 的完整 SHA256 均相等；输出 `repeat-identity.json`。
启动脚本 PowerShell AST 解析通过；docs、status/section 合同、version 一致性与 diff 检查通过。

[本轮原始输出包](2026-10-09-crlf-and-device-fixture-evidence.zip) 保存构建、回归、
装配、Host 正负例、实际 source/configuration/package 字节与 hash。

启动脚本记录本次 executable/package SHA256、电脑名、参数、时间、exit/stdout/stderr，
人工观察保持 pending。无输入注入；按键与反馈、输出设备、discontinuity 等实际观察另填。
初始 fixture 被 Reader 拒绝的 integer/timing 输入已按既有合同修正，没有放宽读取或预算。

## 限制与交接

真实键盘、听音、设备丢失、Gameplay 正反馈像素、校准/同步本轮未作为通过证据登记。
已有 GPU smoke 不代替新谱面设备观察。S7A-8.4 保护来源及新 HEAD owner/hosted 保留；
仅按 I78-6 累计输入，不接受生产阈值、不做 S7A-9 最终关闭、不合并或发行。
新 CRLF 代码进入 PR 后仍需 owner 合并/可信部署，当前 master 旧 checker 不因 PR 推送而更新。
