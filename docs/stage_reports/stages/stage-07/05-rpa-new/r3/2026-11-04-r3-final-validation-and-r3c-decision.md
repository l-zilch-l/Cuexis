# R3 最终本地验证与 R3c 采用决定（2026-11-04）

状态：local verification + candidate disposition；test-only；**不是** R3 退出、hosted 验证、设备、
生产或 owner 接受。

日期：2026-11-04

返回 [RPA 报告索引](../README.md)。当前门账与阶段状态只以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准。

## 0. 结论

1. 磁盘事故后的源码已完成 Debug、Release、candidate Debug 与 candidate SHARED 四套本地构建/CTest
   复核；计数分别为 **1211/1211、1211/1211、73/73、1401/1401**，均为 0 失败。最终可审计的
   11 行同源矩阵在本报告“最终矩阵”一节追加。
2. R3c 正式 A/B 单元按预注册规则执行，但四次启动尝试均未产生完整有效单元；最后一次 v4 的首个
   baseline 进程完成 30 秒预热并取得 7 个有效请求后，在重复 `pause` 处失败。控制器按
   `no_retry_or_replacement` 停止，未补样、未重跑求通过。
3. 因正式判据要求 **3 窗 × 每窗 3 对** 全部有效，实际有效进程为 **0**，R3c 的比值区间无法形成。
   R3c 因此登记为 **不采用**：`selected: false`、`product_promoted: false`，128 MiB 也未被提升为
   产品准入决定；候选补丁继续只作为归档材料保留。
4. 这不把“单元无法完成”误写成“R3c 性能失败”，也不允许从 7 个非正式请求外推收益或保护结论。
   R3b 可信恢复边界不因本记录重开。

## 1. 四套本地验证

| 配置 | 构建/测试 | 结果 | 说明 |
| --- | --- | ---: | --- |
| Debug | `cmake --preset debug --fresh` + build + `ctest --preset debug --no-tests=error` | 1211/1211 | 0 失败 |
| Release | `cmake --preset release --fresh` + `--clean-first` + `ctest --preset release --no-tests=error` | 1211/1211 | 0 失败 |
| candidate Debug | `candidate-debug` 定向相关 CTest | 73/73 | 0 失败 |
| candidate SHARED Debug | `cmake --preset candidate-shared-debug -B out/build/rpa-candidate-shared-debug --fresh` 后全量 CTest | 1401/1401 | 磁盘事故后先从已知良好 candidate Debug 的 `vcpkg_installed` 补齐 1174 个缺失文件，再重新配置 |

这四套是冻结前的快速闭包。最终报告的权威矩阵证据必须来自下一节登记的 11 行全新
configure/build/CTest，且运行期间 `source_unchanged=true`。

## 2. R3c 正式 A/B 预注册与尝试序列

正式设计固定在
[`2026-11-04-r3c-formal-adoption-v4/registration.json`](2026-11-04-r3c-formal-adoption-v4/registration.json)：
3 个 case（1K H902、40K H5、40K H902）、3 窗口 × 每窗 3 对、每进程 16 次 Seek、每进程 30 秒机器预热；
A/B 顺序按 `AB/BA/AB` 轮换；Student-t 95% 对数比值区间上界必须 `<= 1.10`，候选每次请求还必须
`<= 20,000 µs`；不重试、不替换。

| 尝试 | 结果 | 保留位置 |
| --- | --- | --- |
| v1 | `check_no_build` harness 逻辑错误；无测量进程 | [`2026-11-04-r3c-formal-adoption/`](2026-11-04-r3c-formal-adoption/) |
| v2 | 将 `Path` 作为 subprocess stdout；无测量进程 | [`2026-11-04-r3c-formal-adoption-v2/`](2026-11-04-r3c-formal-adoption-v2/) |
| v3 | 完成 1 个 1K/A 预热和测量进程后，`GetProcessInformation` 查询失败 | [`2026-11-04-r3c-formal-adoption-v3/`](2026-11-04-r3c-formal-adoption-v3/) |
| v4 | 修复日志根目录并去掉非必要 power-state 查询；首个 baseline 进程在第 8 次 `pause` 失败，控制器立即停止 | [`2026-11-04-r3c-formal-adoption-v4/`](2026-11-04-r3c-formal-adoption-v4/) |

v4 的 `registration.json`、`results.json`、控制器和两份原始日志均原样保留。`results.json` 记录
1 行、`valid=false`、`exit=42`、0 个有效进程；三个 case 的区间均为 `null`，decision 为
`inconclusive`。失败日志在
[`logs/00-1k-h902-w0-p0-A.log.txt`](2026-11-04-r3c-formal-adoption-v4/logs/00-1k-h902-w0-p0-A.log.txt)，
30 秒预热日志在
[`logs/00-1k-h902-w0-p0-A-warmup.log.txt`](2026-11-04-r3c-formal-adoption-v4/logs/00-1k-h902-w0-p0-A-warmup.log.txt)。

## 3. R3c 处置

- **不采用 R3c 产品补丁**；主树没有应用
  [`r3c-head-candidate.patch`](2026-11-03-r3c-head-port-and-cross-archive-admission/tools/r3c-head-candidate.patch)。
- **不采用 128 MiB quota 作为产品决定**。此前“128 MiB 保守覆盖”只是准备提交正式单元的技术倾向，
  正式单元未通过执行有效性，不能升级为采用结论。
- 候选补丁、跨代准入材料、8 组 accounting 读数和四次正式尝试全部保留；后续如重开，必须作为新的
  预注册单元，不得把本记录描述为性能失败或成功。
- R3c 补丁实际统计为 **11 文件、+1120/−11**；新增测试文件为 **+691**。原报告中的
  `+1090/−11` 与 `+661` 是错误统计，现修正。

## 4. Seek 门账与边界

适用 Seek 余下三类的独立复核见
[`2026-11-04-r3-seek-recheck/`](2026-11-04-r3-seek-recheck/index.md)：36/36 进程有效、576 次请求的
oracle 全部一致，最差 1,249 µs。结合已有两类，五类适用 Seek 均有独立本地通过读数，R3 门账由此从
**14 项通过 + 1 项部分**收口为 **15 项通过 + 0 项部分**。

仍未由本报告关闭的项目：

- 最终 11 行同源验收矩阵：待源码与文档冻结后运行；结果在本报告后续章节追加。
- hosted 同 SHA：owner 已裁定本轮不运行，仍未验证；不标记通过或豁免。
- owner 对 R3 退出的裁定：只由 owner 作出。
- 40K @ 240 FPS、R3b 可信恢复、旧 128 MiB 绝对内存上限：保持既有裁定，不重开。

## 5. 最终矩阵

待追加。
