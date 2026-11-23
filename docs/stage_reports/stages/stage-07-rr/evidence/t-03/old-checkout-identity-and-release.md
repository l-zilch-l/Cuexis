# 旧工作副本的身份代码与发行记录

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-03-old-checkout
status: evidence
authority: none
date: 2026-11-23
-->

## 1. 旧工作副本事实

本机另有一份完整工作副本 `E:\Cuexis`，分支 `master`，HEAD `aac83e5e0ae195256531be73a44c66b8abc231c2`（2026-11-16），tree `9c7c79eaa6d2f3429d247ff706dee388148d77f1`，工作树干净；`out/build` 不存在，`out/` 下 0 个 `.exe`，无构建产物。`aac83e5e` 是当前分支 HEAD 的祖先（`git merge-base --is-ancestor` 退出 0）。

另有一份 2026-11-14 的源码存档 `out/evidence/workspace-cleanup-20261114/w1-retirement/source-d1951326.zip`（164 MB，`d1951326`）。

## 2. 身份代码逐字节一致

下列文件在 `E:\Cuexis`（`aac83e5e`）与当前 HEAD 的工作树字节相同（SHA-256）：

| 文件 | SHA-256 |
| --- | --- |
| `engine/judgement/src/execution_kernel_prepare.cpp` | `4d8051e6e7cbd9bc5fbe2fc9cedcf772f31f690f63fed002d59ed52d9a8445e5` |
| `engine/judgement/src/gameplay_assembler_assemble.cpp` | `fa21ba4ae5c299f7e5886e927215a6d4191438dfa3c98a05a9fb3b16be1c902b` |
| `engine/judgement/src/identity_factory.hpp` | `e3c902c55f36d0d8f3d28bc16e163ef823da867d78b703f16c89ff553d1589eb` |
| `engine/judgement/src/execution_kernel_submit.cpp` | `27440916732d399e`…（前 16 位） |

同一组文件从 `d1951326` 存档解出后与当前 HEAD 的 `git diff --no-index` 为空。也就是说决定判定身份的两个构造点（engine 后缀拼接与 prepared preimage 写入）在 2026-11-14 存档、2026-11-16 `master` 与当前分支上都没有变化。

结论：**旧实现的身份字节可以用当前代码复现**，不需要旧二进制。落地方式：`tests/judgement/rr_f132_identity_baseline_tests.cpp` 的 `rr_f132_identity_baseline` 用 `-s` 打印 graded=false/true 两套 `judgementIdentity.canonicalBytes().toHex()`，T-10 改动后重跑并逐字段解释差异。

这条只覆盖“本机旧副本与存档里的实现”。真实发布版在设备上产生的身份值仍无记录；仓库内没有任何 `judgementIdentity` 十六进制记录（全量检索 0 命中），也没有旧发布二进制。

## 3. 旧状态页记录的发行范围

`E:\Cuexis\docs\CURRENT_STATUS.md`（SHA-256 `3efab2847ae58cc115e5e6f3a595b2017a4b4103ccadab807c6eaf601977e327`）写：

> 本次发布仅包含关闭／Stage 7.5 规划、文档检查器及日期版本 `26.11.14-1`；没有产品源码、SDK、profile 或 caps 变化。

同页另写 `owner 未授权 merge／release`。旧副本里唯一非测试的成品谱面是 `assets/projects/demo20s.cxc`（145,816 字节，SHA-256 `dbf8fccafe73cd8258e350f63157f6f6422ed366f3e0d1f2c6258d1b08319c56`），属于演示工程，不带正式发行身份。

对 D-20 的意义：2026-11-14 的发布候选明确只含文档与版本号，未携带产品源码、SDK 或内容；这与“仓库内没有正式发行的判定语料”一致，也解释了为什么唯一留存的旧成品是 S7A-7/8 的内部交付件。更早是否对外交付过判定语料，仓库与旧副本都没有记录，仍然无法核实。

## 4. 边界

- 本页只记录本机文件事实与旧状态页原文，不证明任何外部交付。
- 未从旧副本构建、未运行任何二进制。
- D-20 的正式语料缺口仍归 T-09；身份字节回归仍归 T-10。
