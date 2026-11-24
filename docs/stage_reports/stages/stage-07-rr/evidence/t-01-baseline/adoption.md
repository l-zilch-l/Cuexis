# T-01 采用记录（Stage 7-RR 派单绑定）

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-01-adoption
status: evidence
authority: none
date: 2026-11-23
-->

本记录把 T-01–T-17 绑定到 2026-11-23 的实际上游基线。按[计划 §4 唯一派单授权入口](../../../../../stage_plans/active/stage-07-rr/plan.md#唯一派单授权入口)执行：首次接卡尚未绑定时取本次读出的完整 HEAD。绑定不改变已接受的产品规则；变更现行合同仍须 ADR/Spec 接受（D-04）。

## 基线身份

| 项 | 值 |
| --- | --- |
| 分支 | `stage-7-rr` |
| 采用基线提交（BASE_SHA） | `a22e255f6716b22f96ad8a183136eae4fb04ac2f` |
| 基线 tree | `7869115460b0c17a1f833f30700cd06f34e8e3c3` |
| 采用时工作树 | 干净（无已跟踪差异、无未跟踪文件） |
| 基线提交说明 | `docs(stage-7-rr): land documentation governance for T-18` |
| 上一基线 | `ca5cbb9121b015ae0b3b5d681610ca94dac78184`（T-18 治理输入基线，附未提交补丁与未跟踪原文） |

T-18 治理成果在本提交中落盘。治理交付封存的原始输入（`inputs.zip`、`working-tree.patch`、`migration.json`）属于冻结证据，用于追溯 `ca5cbb91` 时代的工作树，不作为本次实施基线；本次基线是干净提交，可直接 `git checkout` 复现。

## 设计基线沿革与差异核对

T-01 步骤 1 要求核对设计基线与实施工作树的差异。沿革与差异如下，逐文件清单见同目录 `history/name-status-*.txt`：

| 修订 | 时点与用途 | 到本次基线的差异 |
| --- | --- | --- |
| `9623efa3` | 2026-11-21 可实施性审查（旧研究对照） | 167 个文件：61 新增、100 修改、6 重命名 |
| `8599c83a` | 首次实施交接提交 | 168 个文件 |
| `db6042b6` | 正式实施前计划检查交接 | 155 个文件 |
| `ca5cbb91` | T-18 治理输入基线（工作树另有治理补丁） | 治理补丁已并入 `a22e255f` |

`9623efa3..a22e255f` 的 167 项变化分布在 `docs/`（150）、`tools/`（13）、`.github/`（1）与根文件 `AGENTS.md`、`cmake/CuexisVersion.cmake`、`vcpkg.json`；产品源码目录 `engine/`、`app/`、`tests/`、`schemas/` 无变化。版本文件只有构建日期从 `26.11.18-1` 变为 `26.11.22-1`（`cmake/CuexisVersion.cmake` 与 `vcpkg.json` 同步），SDK API 仍为 `0.9.1`。因此设计审查当期的产品事实未被后续文档工作改写，但旧报告内的构建字面量是历史值，实施证据必须以本次基线重取。

## 哈希口径

派单输入的规范哈希取基线提交中的 blob 字节（`git cat-file blob <BASE_SHA>:<path>`），在任意 checkout 上复现。`manifest.json` 另记工作树字节哈希；本机 Windows `core.autocrlf=true` 使 134 个输入在工作树按 CRLF 物化，行尾归一化后与 blob 相同，内容无差异。派单与复核一律以 blob 哈希为准。

## 逐卡绑定

scope code 取自各卡「目标」段；卡片与输入哈希取自 `manifest.json`、`dispatch.md`。

| 任务 | scope | 卡片 SHA-256 | 主合同/输入 | 前置 |
| --- | --- | --- | --- | --- |
| T-01 | `DOC` | 见 `baseline.md` 采用表 | `plan.md`、`acceptance.md`、`schedule.md`、`findings.md` | T-18 |
| T-02 | `DOC` | 同上 | `CURRENT_STATUS.md`、`docs/formats/README.md`、`guides/VERSIONING.md` | T-04 |
| T-03 | `RR2` | 同上 | `docs/api/*`、`docs/formats/*`、`schemas/cuexis.gameplay-diagnostics.v2.codes.json` | T-01 |
| T-04 | `R1` | 同上 | `contracts/runtime-timing.md#构建头与安装`、`contracts/runtime-timing.md#r1八个-playback-tu` | T-03 |
| T-05 | `R5` | 同上 | `contracts/render-player-writer.md#内存与布局` | T-04 |
| T-06 | `R5` | 同上 | `contracts/render-player-writer.md#提交资源与交付`、`contracts/runtime-timing.md` | T-05、T-17 |
| T-07 | `DOC` | 同上 | 三份候选合同、`docs/templates/report.md` | T-02、T-06、T-08、T-15、T-16 |
| T-08 | `RR4` | 同上 | `contracts/runtime-timing.md#rr-4-逐符号兼容迁移` | T-03、T-04、T-06 |
| T-09 | `DOC` | 同上 | `acceptance.md`、`findings.md` | T-07、T-17 |
| T-10 | `NEXT` | 同上 | `contracts/runtime-timing.md#prepared-与当次推进` | T-08 |
| T-11 | `NEXT` | 同上 | `contracts/runtime-timing.md#作者版本stop比例和最小多等级`、`formats/CXT_V2_FORMAT.md` | T-06、T-10 |
| T-12 | `NEXT` | 同上 | `contracts/runtime-timing.md#谱面时间-api` | T-10、T-11 |
| T-13 | `NEXT` | 同上 | `contracts/prepared-artifact.md` | T-12 |
| T-14 | `NEXT` | 同上 | `contracts/runtime-timing.md#playback-时钟与实时准入` | T-08、T-13、T-17 |
| T-15 | `NEXT` | 同上 | `contracts/render-player-writer.md#命令与启动` | T-11、T-14 |
| T-16 | `NEXT` | 同上 | `contracts/render-player-writer.md#p5内部-writer-与显式迁移` | T-13 |
| T-17 | 独立立项修复 | 同上 | `docs/api/playback-session.md#preparedplayback-有效性修订f501已接受未实现`、`docs/architecture/RUNTIME_SESSION.md` | T-03、T-04 |

T-17 由 owner 在 2026-11-22 单独立项，承接 RR-F501/F508/F509 与 2026-11-23 接受的 RR-F203；它不属于 DOC/RR2/R1/R5/RR4/NEXT 范围码，但与本轮同基线采用。

## 随基线生效的已接受范围

- 实施范围：T-01–T-17 统一实施，含 F302（T-04）、F310（T-08）、F313/F316（T-08 内部入口＋T-14 分层）、F207（T-06）的全面结构清理（[登记册 §24](../../../../../stage_plans/active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)）。
- 玩法：RR 完整实现并实验验证 Gameplay v3；Stage 8 以 Chart v5＋Gameplay v3 为正式主线；v5 沿用 CXT2/Clip2/Behavior Event2/整数 `render.alpha`，拒绝旧作者动画语法。
- 已接受未实现的合同：F501 有效性修订、F135 `allocation_failed` 与观测丢失分类、F203 覆盖登记前整批验证。
- 设备等待超时归 Player，SDK 不实现计时机制；SDK 提供操作状态与实际结果。

## 不随基线生效的事项

实际发布、SDK minor 晋升、公共 API 退役、跨平台/设备/hosted/容量长任务证据仍按各自门禁单独取得。采用不等于实施完成，也不等于发布。旧 API 逐组过渡窗口按 D-19 执行。
