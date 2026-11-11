# `read_long` 整改：相机逆矩阵缓存（诊断轮，无正式结论）

状态：candidate diagnostic；两轮单变量诊断，未消费正式额度

日期：2026-11-11

返回 [超界处置](2026-11-07-read-long-over-budget-disposition.md)与
[owner 裁定记录](2026-11-11-owner-decisions.md) §4。

## 1. 问题

原固定 CPU 组的两个 `read_long` 单元都超出 1.10 相对门（legacy 1.140031、RPA 1.146032），
A/B 重复性本身通过，因此是真实读数成本而非噪声。历史离线复算显示每操作 cycles 约 827→948。

## 2. 两轮单变量诊断

诊断方式：同一台机器、同一 Release 普通模式、同一组 `PD_V2` 记录（每次读 1 个对象、
每块 1,048,576 次读），control 侧是改造前的 `f66c78d2`，prototype 侧是改造后的构建；
两侧交替各跑 2 个进程、每进程 3 个测量组（n=6），取每读中位数。

| 轮次 | 改动 | legacy prototype/control | RPA prototype/control |
| --- | --- | ---: | ---: |
| 1 | 对象行改为就地写入（去掉 72 B 返回拷贝） | 0.99102 | 1.02275 |
| 2 | 相机逆矩阵按逐位缓存（纯函数） | **0.80955** | **0.79090** |

第 2 轮的绝对读数：legacy control 0.267153 → prototype 0.216275 µs/read；RPA 0.251481 →
0.198896 µs/read。第 1 轮没有效果（RPA 甚至慢 2.3%），保留该提交只是因为它本来就更省一次拷贝。

**结论：固定开销的大头不是对象行，而是每帧一次的 `core::inverse(camera.matrix)`。**
在"每帧单对象、相机静止"的夹具里，4×4 求逆几乎就是全部增量。改为按矩阵逐位比较缓存上一次的
成功逆（`thread_local`，纯函数、无状态外泄），每帧退化成 16 个 double 的比较。

## 3. 正确性

- `cuexis_runtime_tests`：54 个用例、11,614 条断言全过。
- `cuexis_playback_tests`：185 个用例、34,750 条断言全过（含曾因"相机每帧清零"被我改动破坏的
  `r2_snapshot_tests` 相机比较；该改动已作为 `8d80ca8d` 撤回）。
- F1 十一行矩阵在改造候选上全部 `local_pass`（11/11，`source_unchanged: true`）：MSVC 八行
  （on/off × debug/release/shared/headless）与 MinGW 三行（on-debug、on-shared-debug、on-release）。
  首轮 `msvc-on-release` 与 `msvc-on-shared-debug` 在 `vcpkg z-applocal` 阶段被外部进程占用目标 exe
  而中断（`The process cannot access the file because it is being used by another process`），
  原样重跑后通过；两次失败记录保留在各自证据目录的第一份 manifestation 中。

## 4. 对正式门的含义（预测，不是结果）

若控制侧读数即当前候选，则改造后相对 pre-PD 的比例约为 `1.140 × 0.810 ≈ 0.92`（legacy）与
`1.146 × 0.791 ≈ 0.91`（RPA），都在 1.10 以下且有明显余量。这只是由同机 A/B 推出的估计，
**不构成正式判定**。

## 5. 正式组的前置条件

正式三窗九对组需要可验证的 A 侧。本检出中冻结的 pre-PD 基线目录
（`out/pd0-fresh-bound-baseline-20261018`）**不存在**，因此 A 必须由
`out/sources/r4/prepd-original`（`d3440882`）在同一工具链下重建并重新绑定，然后按
`owner_runner` 的 prepare/pilot/register/window 流程登记。该重建与登记属于下一次正式组的前置工作，
本次诊断不消费额度（`formal_attempts_consumed: 0`）。

## 6. 可回滚点

- 标签 `readlong-rollback-20261111` = `f66c78d2`（改造前）。
- 提交顺序：`19a9334c`（就地写行）、`20b7202f`（测试用兼容重载）、`8d80ca8d`（撤回相机清零）、
  `16ae94bb`（相机逆矩阵缓存，有效）。任何一轮都可单独 revert。
