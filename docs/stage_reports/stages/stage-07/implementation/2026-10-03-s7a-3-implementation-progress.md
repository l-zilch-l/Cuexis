# S7A-3 实现推进记录：G2-S2 / G2-T4 / R2-K4 / P1-W1 裁决后状态

日期：2026-10-03  
状态：实现推进记录；设计裁决已完成，S7A-3 尚未实现完成。

## 本批次已落地

按已记录的 `G2-S2 + P1-W1 + E1` 细化选择，完成了 G2-S2 的 typed prepare 输入和 R2
不可变资源计划的第一版：

- `GraceResolutionInputs` 增加 chart、单层 inherited 和 prepare 前 frozen default 的精确
  `RationalDuration` 来源；旧 `TickSpan` candidate 保留为兼容的 canonical-unit 输入。
- `resolvePreparedGrace` 执行显式 / 继承 / default 来源选择，拒绝缺失来源、错误继承关系、
  不允许的 chart supply、非正 quantization unit、反向范围、不可表示值和越界值。
- 精确有理量化使用 round-half-to-even；不使用浮点、钳制或隐式零值。
- `ResourceClaimResolutionInputs` 增加带 policy / stable claim key 的 typed candidate。
  `resolveResourceClaims` 生成只读计划，强制 `capacity = 1`，保留 observe 但不计为占用，
  拒绝 sticky / observe grace override、占用候选缺少 key 以及重复占用 key。
- `ResourceClaimResolution` 提供只读计数查询；运行期 owner / lease / contact 分配仍未在此
  prepare 产品中伪造。
- 增加正例、half-even、缺失来源、重复 claim key 和 resource-plan 查询测试。

## 验证

在 Visual Studio x64 Developer 环境下：

```text
cmake --build --preset debug --target cuexis_judgement_tests -j 2
out\build\debug\bin\cuexis_judgement_tests.exe
All tests passed (3519 assertions in 110 test cases)
```

## 2026-10-04 设计裁决

owner 已授权直接裁决，现已闭合完整组合 **G2-S2 + G2-T4 + R2-K4 + P1-W1 + E1**：

- G2-T4：`preparedGrace` 只形成 checked hard deadline `D = end + preparedGrace`，不扩大成功窗口，
  不产生 gap/handoff；`t=D` 先处理 timer，再处理 input。
- R2-K4：runtime 使用 `coordinator.policy.greedy_v1`；同资源 occupying candidate 的唯一竞争键为
  `(priority, tieRank)`，observe 不占 slot，capacity 固定为 1，不使用容器顺序、hash、线程顺序或
  `requirementKeyRef` fallback。
- P1-W1：Gameplay Capsule v2，`candidateRevision=2`，必需 `GPH0/GPR0/GPD0/GRC0/REF0`，
  section revision 1、`rowCodec=1`、semantic `flags=0`，REF0 kind 8--13；字段布局与拒绝矩阵见
  [裁决记录](../../../../proposals/gameplay-v2-acceptance/S7A-3_PACKED_W1_CANDIDATE.md)。
- E1：维持完整正例、负例、golden、file/memory、跨工具链与等价性验收范围。

上述是设计闭合，不是实现或门禁关闭证据。

## 当前实现工作

此前 P1-W1 的物理合同缺口现已闭合；实现仍需按裁决一次性落地：

1. candidate revision 数值；
2. 新必需 semantic section 的名称与必需性；
3. REF0 kind 的 wire 编号；
4. row / field 的确切布局；
5. 长度编码；
6. 端序；
7. 旧 Reader 遇到 revision / section / kind 的稳定拒绝行为。

这些值不是实现细节：它们决定旧 Reader 是否会静默接受新语义，也决定 file / memory、
跨工具链 golden 和 reader / writer 负例的合同。当前尚未生成 Packed bytes，也没有把现有 identity
bytes 冒充 Packed 编码。

## 2026-10-04 候选合同推进

已新增 [S7A-3 P1-W1 Packed 物理合同候选](../../../../proposals/gameplay-v2-acceptance/S7A-3_PACKED_W1_CANDIDATE.md)，内容包括：

- 复用既有 Packed 96-byte header、32-byte directory、little-endian、LEB128、CRC、canonical sort 和预算 envelope；
- 三套可比较的 section 组织方案：多 section（推荐）、单 Gameplay section、Foundation section 加最小新语义 section；
- 推荐方案的 `GPH0` / `GPR0` / `GRC0` row-table 草案，以及 REF0 kind 8--13 的**建议编号**；
- revision、未知必需 section / kind、乱序、重复、截断和溢出的兼容与稳定拒绝矩阵；
- owner 必须一次性裁定的 7 项阻塞值。

该文档现作为正式设计裁决记录；`candidateRevision = 2`、section 名称、REF0 kind 8--13、字段布局和
拒绝矩阵已接受。实现前仍不新增公共 wire enum，不把文档裁决误写为运行时代码或完成证据。

## 停止条件

下一批次应实现
typed row writer / reader、旧 revision 与未知必需 section 拒绝、REF0 正反 fixture、file /
memory 对照、跨工具链 golden，并将其接入 E1 的完整验收矩阵。

本记录没有实现 grace 消费窗口、hard-deadline 终止语义或运行时资源仲裁顺序；这些设计语义已裁决，
代码与证据仍待完成。

本记录不表示 S7A-3 或 Stage 7A 完成，不关闭 `INCOMPLETE GATE`，不包含暂存或提交操作。

## 2026-10-04 文档收口订正

本文的 `3519 assertions / 110 test cases` 是 2026-10-03 历史运行记录，
不是本轮重跑、clean-first 构建或 Capsule 新合同的验证证据。
前文的 row-table 草案、建议编号与“尚待七项裁定”描述现由
[Gameplay Capsule v2 format](../../../../formats/GAMEPLAY_CAPSULE_V2_FORMAT.md) 取代；
提案页只保存选择历史，不再拥有完整字段表。
完整 S2/T4/K4、REF0 fixed subdomains、结构 hash 与 E1 门禁已按 owner 授权写入设计。
当前源码的来源 policy 进入 judgement projection 与新来源无关合同不符，
后续实现必须修正并补来源等价测试；本轮不编辑 runtime。
本轮文档证据、验收剩余项与范围见 [设计收口报告](2026-10-04-s7a-3-design-closure.md)。
