# RR-F132 身份字节基线（T-10 改前）

状态：evidence

更新日期：2026-11-24

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-03-identity-baseline
status: evidence
authority: none
date: 2026-11-24
-->

## 结论

当前实现（提交 `5dc55387` 的 debug 预设）对 `FoldFixture` 的两个变体产生下列规范身份字节。T-10 改动身份字段后必须重跑 `rr_f132_identity_baseline` 并逐字段解释差异；不能以新实现生成的期望证明身份未变。

| 变体 | 字节数 | 规范字节 SHA-256 |
| --- | --- | --- |
| graded=false | 3,425 | `84efa8707f26b36a…`（完整值见文件） |
| graded=true | 3,592 | `d18e2c47e9bf9e83…`（完整值见文件） |

两个变体相差 167 字节，对应 `;grade.signed-interval.v1` 后缀及其长度前缀。

## 1. 取数方法

用例 `rr_f132_identity_baseline`（`tests/judgement/rr_f132_identity_baseline_tests.cpp`）按 `fold_recovery_seek_tests.cpp:341-383` 的构造准备 `FoldFixture`，经 `JudgementSession` 的 `configure`、`prepare` 后取 `query()->kernelView().judgementIdentity.canonicalBytes().toHex()`，用 Catch2 `INFO` 打印。

命令：

```text
out\build\debug\bin\cuexis_judgement_tests.exe rr_f132_identity_baseline -s
```

- 退出码 0，129 条断言全部通过。
- Catch2 把长字符串按 80 列折行并以 `-` 续行；`identity-baseline.txt` 已把续行拼回连续十六进制，每行给出字节数与 SHA-256。
- 原始输出保留为 `identity-capture-raw.txt`，SHA-256 `87f0e5783f15289bf222cb1c053d0b50555f6ad82c3835d59ae7b124362a2390`。
- 拼装后的基线文件 `identity-baseline.txt`，SHA-256 `873eac20ba49e8e11070dc4e806d57b12054e6d7f5ff9084bd6bb238372fb407`。

环境：MSVC 19.51、`debug` 预设、提交 `5dc55387`（工作树无未跟踪改动时采集）。

## 2. 这个基线能证明什么

- 记录的是**当前实现**产生的字节，不是历史发布值；仓库与两套外部设备包都没有 `judgementIdentity` 记录。
- 本机 `master` 副本（`aac83e5e`，2026-11-16）与 `d1951326` 源码存档的 `execution_kernel_prepare.cpp`、`gameplay_assembler_assemble.cpp`、`identity_factory.hpp`、`execution_kernel_submit.cpp` 与当前 HEAD 逐字节相同，见[旧工作副本的身份代码与发行记录](../old-checkout-identity-and-release.md)；因此这些字节同时也是该段历史实现的字节。
- 不含 graded 变体的 engine/ruleset/session 字段文本；字段结构见[RR-F132 身份映射](../rr-f132-identity-binding.md)。

## 3. 限制

- 只在 `debug` 预设计算；未在 release 或 shared 构建复算。
- 未与设备或历史发布比对；历史发布值仍无法核实。
- T-10 改动后若字节变化，须逐字段说明；不能只重跑用例。
