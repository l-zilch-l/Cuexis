# 外部旧设备验收包清点（D-20 语料补充）

状态：evidence

更新日期：2026-11-24

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.external-legacy-corpus
status: evidence
authority: none
date: 2026-11-24
-->

## 结论

owner 提供的两套仓库外设备验收包，可以作为 D-20 的“旧判定回归可执行输入与聚合期望”，不构成正式发行语料；不能提供 F132 的历史身份字节。

| 判断 | 结论 | 依据 |
| --- | --- | --- |
| D-20 正式发行判定语料 | 不成立 | 无 tag、发行说明、发布清单或对外交付记录；包 A 自记 `formal_acceptance: false`，包 B 自记“不预先记为验收通过” |
| D-20 可执行回归输入与聚合期望 | 部分成立 | Chart/Gameplay 成品、配置、tick 输入序列与 score/hits/misses 聚合期望齐备，期望与 `scoreRules` 算术一致 |
| F132 旧身份字节 | 不成立 | 两包无 `judgementIdentity.canonicalBytes()` 记录；只有 Player 会话配置身份 `ff578f4f…`、Chart 语义身份与 Gameplay 装配身份 |
| 与仓库 D-20 清单的关系 | 部分成立 | “仓库内正式发行组合为空集”不变；“历史资料无法核实”收窄为“仓库外有两套内部候选验收包，仍无发行身份” |
| 当前实现复现两包聚合期望 | 成立（单例路径） | R4 三例拼接 166 字节与 `verification.log` 逐字节相同；PD3c 两入口归一后与 `logicalCases` 相同，见[当前实现重放](../evidence/external-corpus/replay-current/replay-current.md) |

## 1. 两套包

| 项目 | 包 A `Cuexis-R4-P7-O2-2026-11-14` | 包 B `Cuexis-PD3c-DeviceAcceptance-2026-10-19` |
| --- | --- | --- |
| 时点 | 2026-11-14 | 2026-10-19，另有 2026-10-20 补测 |
| SDK / 构建 | `0.9.1` / `26.11.13-1` | `0.9.0` / `26.10.19-1` |
| 源码身份 | `d1951326`（trace `ce2a9a69`） | `d3440882` 加 81 项未提交输入（`59e304b8`） |
| Player SHA-256 | `03727446…`、`95e2cc20…`、`774fe3cc…` | `72594dfe…`、`147a1b6a…`、`05f12917…` |
| 性质 | 内部候选设备验收 | 内部候选设备验收 |

两包的 Player 二进制与 `D:\w1` 构建目录均已不存在；只有 `binding.json`/`verification.json` 里的哈希留存。

## 2. 期望判定值

score / hits / misses：

| 包 / 课程 | all-hit | no-input | early-hold-release |
| --- | --- | --- | --- |
| A `keyboard`（13 Requirement） | 34 / 15 / 0 | −15 / 0 / 15 | 24 / 13 / 2 |
| A `keyboard-dense-v5`（385 Requirement） | 778 / 387 / 0 | −387 / 0 / 387 | 768 / 385 / 2 |
| B `keyboard`＝`keyboard-release`（19 Requirement） | 46 / 21 / 0 | −21 / 0 / 21 | 36 / 19 / 2 |

三套值都能由配置的 `scoreRules`（tap/head +2、body +3、tail +5、Miss −1）推出，与 `course.json` 一致。它们是“公式推导 + 同库离线 helper”的期望，不是外部人工 golden，也不是从设备输出回抄。

A 的密集课程只有**合成输入**运行达到 778/387/0（`20261114-123842-155151-deep-scripted`），零输入运行达到 −387/0/387；实体运行最高 562/315/72，没有一次全命中。B 的 `pd4-20261020-211807-505` 实体运行达到 46/21/0 并完成 terminal horizon。

## 3. 证据文件

复制到本目录的关键文件（相对路径 → 字节数 → SHA-256）：

| 文件 | 来源 | 字节 | SHA-256 |
| --- | --- | --- | --- |
| `a-binding.json` | A 根目录 | 8,898 | `1313fee7e81f9316cf87f0f039b3250211122b091a697e923b47096f66697f8e` |
| `a-dense-verification.log` | A `keyboard-dense-v5/` | 166 | `3c6723af9c8fadd292ba1ac508beb6b5ac504aa425ec9413a78f035c97dd901c` |
| `a-dense-verification-cases.json` | A `keyboard-dense-v5/` | 160,354 | `768cff0e3ec023bb52b19f9c9fc9d9184be24256230a522b0c18dee807b890e4` |
| `b-keyboard-verification.json` | B `keyboard/` | 850 | `997c097297e00f4f91a2dff62a78a0cc29649302ad60ef067a831b06fbf1529b` |
| `b-keyboard-verification-cases.json` | B `keyboard/` | 8,284 | `e068e61481a4eb24c8af083f76cacd704ef0045b2206cc00c8f2e699f0d49874` |
| `b-binding-release.json` | B 根目录 | 750 | `6ab94d9a449e26b5fa43ad4b2031f7c036e075295d0aded118546fb9528feb77` |
| `packages-hashes.txt` | 两包全量清单（2,478 个文件） | 342,931 | `354d45e12b4c0a0f7826b51c2f1225131383937fdf745b99925f6464418bfb20` |

图表与压缩成品（`practice.cxc`、`gameplay.packed`、`music.cxc` 等）体积大，未复制；其哈希在 `packages-hashes.txt` 与 `a-binding.json` 内。

## 4. 与仓库既有说法的关系

| 仓库说法 | 本清点判断 | 说明 |
| --- | --- | --- |
| 正式发行判定组合在仓库内为空集 | 成立 | 两包在仓库外，未改变仓库内清单 |
| 历史发行资料是否存在无法核实 | 部分成立（可收窄） | 找到了内部候选验收包，仍无发行身份 |
| 唯一存储态旧判定成品是 S7A-7/8 `main.cxc` | 成立（限仓库范围） | 仓库外另有 3 份可执行判定成品，哈希与 `main.cxc` 无交集 |
| 原 P7 DFJK 记录 40/19/2 | 无法核实 | 两包没有 40/19/2 运行；有完整末态的 DFJK 为 34/15/0、43/20/1、46/21/0 |
| A 键盘复现 2026-10-19 课程 | 不成立 | A 的 `authoring.log` 记录用新 13 Requirement oracle 替换旧 19 Requirement 文本 |

## 5. 可用与不可用

可用于 T-09/T-10/T-13：B 的 19 Requirement 课程作为旧构建回归输入，A 的 dense v5 作为 385 Requirement 高物量输入，两包的源码/二进制/内容哈希作为来源绑定，778/387/0 与 46/21/0 作为聚合期望。

不可用于：把任一套写成“正式发行语料通过”；把 F132 写成“已取得历史身份字节”；用合成输入运行替代设备验收；用包内 `replay=equal` 替代 replay 字节比对。

## 6. 当前实现重放（2026-11-24）

旧实现（`d1951326`）与当前实现（`3943fb13`，Debug、SDK 0.9.1）分别重放两包的单例语料，逐例结果与历史期望一致：

| 包 / 用例 | 当前实现 | 历史期望 | 旧实现 | 判定 |
| --- | --- | --- | --- | --- |
| R4 dense all-hit | 778/387/0，replay=equal | 同左 | 同左 | 成立 |
| R4 dense no-input | −387/0/387，replay=equal | 同左 | 同左 | 成立 |
| R4 dense early-hold-release | 768/385/2，replay=equal | 同左 | 同左 | 成立 |
| PD3c all-hit / no-input / early-hold-release | 46/21/0、−21/0/21、36/19/2 | 同左 | 同左 | 成立 |

R4 三行拼接 166 字节，SHA-256 `3c6723af…901c`，与 `a-dense-verification.log` 逐字节相同；PD3c 输出按 CRLF 归一到 157 字节，SHA-256 `a124c5fd…51ab`，与 `b-keyboard-verification.json` 的 `logicalCases` 相同。两侧驱动源码是同一 blob `35af0a91…`；`d1951326..3943fb13` 的 65 个提交中 `engine/judgement` 与 `engine/gameplay` 无文件差异。

原样完整 R4 用例文件（160,354 字节）超过工具内 128 KiB 解析预算，两侧都退出 2、同一条 `json.parse.size_limit`；成立范围仅限拆成单例的重放路径。聚合值相同只能说明总分、命中与失误数未变；逐音符判定窗口、跨实现 replay 字节、设备输入仍无法核实。完整命令与逐例输出见[当前实现重放证据](../evidence/external-corpus/replay-current/replay-current.md)。

## 7. 限制与未做

- 原始目录在 owner 桌面，未纳入仓库；本目录只存关键小文件与全量哈希。
- 两包都没有逐音符/Fact 期望与结算明细，只有聚合结果。
- 设备级输入时间戳、校准偏移、人工观察缺失；A 的 `human_observations` 明确未观察。
- 当前实现重放已完成（2026-11-24），但只是 Debug 构建；历史设备 Player 是 Release，逐音符层没有期望可对照。
