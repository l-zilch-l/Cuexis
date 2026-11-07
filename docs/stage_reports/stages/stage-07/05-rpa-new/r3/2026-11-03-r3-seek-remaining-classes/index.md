# 适用 Seek 余下三类：外部归档首次认证、自然 checkpoint 首次使用、无合法 checkpoint 回退（2026-11-03）

状态：local measurement；test-only 探针；只对这三类登记 20 ms 判据，**不关闭 R3**，不替代 R4 /
设备 / 生产接受。

日期：2026-11-03　源码：`c085688a` + 探针差异 `tests/pd/pd5_host_probe.cpp`（记录在
[`tools/results.json`](tools/results.json) 的 `source.dirty_diff_sha256`）

返回 [RPA 报告索引](../../README.md)。口径见
[重复性口径裁定](../2026-11-03-r3-repeatability-scope-ruling.md)；已有的两类（同档重复热使用、
冷进程）见 [Pause 预热与适用 Seek 门](../2026-11-01-r3-pause-prime-and-seek-gate.md)。

## 0. 结论

1. **门账里"适用 Seek 2/5 类"补到 5/5。** 父计划 §9 预声明的五类中，此前只有"同档重复热使用"与
   "冷进程"有 ≤20 ms 读数；本轮给余下三类各建了夹具并在同一台设备上测完：
   外部归档首次认证、本会话自然 checkpoint 首次使用、无合法 checkpoint 回退。
2. **三类全部通过，余量 ≥10×。** 每类每规模 6 个全新进程 × 16 次 seek，共 **36 进程 / 576 次
   seek**，36 个进程全部有效；最差一次 `request→present` 是 **1880 µs**，判据 20 000 µs。

   | 类 | 129 记录（6 进程最大值） | 1024 记录（6 进程最大值） | 中位 | 判定 |
   | --- | ---: | ---: | ---: | --- |
   | 外部归档首次认证 | 1880 µs | 1192 µs | 567 / 599 µs | 通过 |
   | 自然 checkpoint 首次使用 | 1041 µs | 803 µs | 530 / 539 µs | 通过 |
   | 无合法 checkpoint 回退 | 941 µs | 1335 µs | 539 / 561 µs | 通过 |

3. **每个被计时的请求都带 oracle 校验，且类本身有前置断言。** 探针在每次 seek 之后把
   `host->snapshotGameplay()->toBytes()` 与"无提示 origin 路径"的 `encodeSnapshot` 结果逐字节比较，
   576 次全部相等。计时之外，探针还用 host 实际调用的同一个 `forkForSeek` 复跑一次夹具，记录
   "证书真正恢复了多少条记录"：natural 六个进程都得到 `certified_records=1`（证书确实被消费，
   不是静默回落），external 与 fallback 六个进程都是 `0`（无原生索引、外来提示确实被拒）。所以
   这里的"快"不是把 hint 丢掉换来的。
4. **三类夹具都不是"造一个能过的场景"。** 校验点写在测量之前（[preregistration](tools/preregistration.json)）：
   自然 checkpoint 由 `JudgementSession::checkpoint` 从本会话 archive 铸出、带私有证书，首次使用；
   外部归档由 `GameplayReplay::fromBytes` 从字节重建，是无原生索引的独立 owning source；
   回退类的唯一提示来自**另一个独立 archive 的末个 target**，既不可能绑定来源、也超过几乎所有 target。
5. **不声称**：hosted 同 SHA CI、R3 退出、设备/生产接受；也不把这三类读数外推到 R4 的集成窗口。
   探针不固定适配器（与 2026-11-01 同一 `configureOpenGlContext` 路径），本轮只登记主机清单，
   见 [`tools/environment.json`](tools/environment.json)。

## 1. 三类夹具

同一探针用例 `PD5 scripted seek timing probe`（`[.][pd][pd5][seek-gate]`）新增
`CUEXIS_PD5_SEEK_CLASS`；host 的 pause → replayCut → seek → sample → present 时序与既有两类完全一致，
只有夹具不同。

| 类 | `CUEXIS_PD5_SEEK_CLASS` | 夹具 | 为什么是这个类 |
| --- | --- | --- | --- |
| 外部归档首次认证 | `external` | 把会话 archive 序列化成字节、再用 `GameplayReplay::fromBytes` 解回来，所有被计时的 seek 都用这个解码后的 owner | 解码结果是独立 owning source、没有原生 checkpoint 索引，第一次 seek 必须完整认证外部来源；不带提示 |
| 本会话自然 checkpoint 首次使用 | `natural` | 用 `JudgementSession::checkpoint` 从**本会话自己的** archive 在 horizon 7000（第一条记录的 horizon）铸一个带证书的 checkpoint，作为唯一提示；首个 ≥ 它的 target 是 8103 | 证书只在 consume 时才被验证，铸造成本在计时之外，所以请求里付的是"首次认证 + 恢复"。certified restore 要求 checkpoint 的 cut 正好落在记录边界，所以在 1103 铸一个"空"证书不能算这一类（首轮就踩到过这个坑，见 §3 末） |
| 无合法 checkpoint 回退 | `fallback` | 唯一提示是从**另一个独立解码 archive** 的末个 target（106103）铸出的证书；被 seek 的是本会话 archive | 来源身份永远不匹配，且对 15/16 个 target 还超界；合法路径只剩下 record/origin 回放 |

计时的东西严格是"请求"：夹具准备（铸 checkpoint、序列化/解码 archive）都在计时区外；计时区是
`replayCut + seek + sample + present` 直到 present 回执返回。

## 2. 协议（测量前写定）

全文见 [`tools/preregistration.json`](tools/preregistration.json)，要点：

| 项 | 值 |
| --- | --- |
| 仪器 | `out/build/rpa-candidate-release/bin/cuexis_execution_device_probe.exe`（Release / STATIC / candidate ON / metrics OFF）；由 `tests/pd/pd5_device_probe.cpp` 定义 `CUEXIS_PD5_DEVICE` 后包含同一 harness，跑真实 OpenGL 后端、隐藏窗口、vsync 关 |
| 规模 | 129 / 1024 条 replay 记录 |
| 进程 | 每类每规模 6 个全新进程（共 36），不重试、不补跑 |
| 每进程 seek | 16 次，target = `1103 + 7000 * i` |
| 判据 | 每次 `request_to_present_us ≤ 20 000`；每类的判定量是**该进程 16 次的最大值**，6 个进程全部通过才算该类通过 |
| 有效性 | exit 0；`PD5_SEEK_ID` 的 records/seeks/class 与请求一致；恰好 16 行 `PD5_SEEK` 且 horizon 序列既定；`PD5_SEEK_VERIFY` 为 `verified=16, mismatched=0`；有 `PD_ENV`；类前置成立（natural 的 `certified_records>0`，其余为 `0`） |
| 失败保留 | 任何进程失败只记日志并在该组留空位；慢值保留，不通过重跑改判 |
| 披露但不判定 | `request_to_ready_us`（replayCut+seek）逐行保留，不用它替代 present 口径 |

## 3. 读数

数据：[`tools/results.json`](tools/results.json)（每进程 16 个原始读数都在里面）；原始日志：
`runs/<class>/<records>/run-NN.log.txt`。

| 类 | 规模 | 有效进程 | seek 数 | 中位 | p99 | 最差 | 判定 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 外部归档首次认证 | 129 | 6/6 | 96 | 567.0 µs | 1880 µs | 1880 µs | 通过 |
| 外部归档首次认证 | 1024 | 6/6 | 96 | 599.0 µs | 1192 µs | 1192 µs | 通过 |
| 自然 checkpoint 首次使用 | 129 | 6/6 | 96 | 529.5 µs | 1041 µs | 1041 µs | 通过 |
| 自然 checkpoint 首次使用 | 1024 | 6/6 | 96 | 538.5 µs | 803 µs | 803 µs | 通过 |
| 无合法 checkpoint 回退 | 129 | 6/6 | 96 | 538.5 µs | 941 µs | 941 µs | 通过 |
| 无合法 checkpoint 回退 | 1024 | 6/6 | 96 | 560.5 µs | 1335 µs | 1335 µs | 通过 |

同类内 6 个进程的最大值分别是
658 / 998 / 650 / 727 / 921 / 1880 µs（external 129）、
1192 / 895 / 878 / 871 / 773 / 722 µs（external 1024）、
1041 / 831 / 697 / 656 / 863 / 788 µs（natural 129）、
754 / 734 / 761 / 740 / 803 / 705 µs（natural 1024）、
941 / 764 / 667 / 723 / 820 / 788 µs（fallback 129）、
725 / 1001 / 723 / 699 / 1335 / 829 µs（fallback 1024）。

观察：三类的中位都落在 530–600 µs，与既有 hot/cold 批次（约 540–620 µs）同量级；单进程最大值里
偶发 1.3–1.9 ms 的尾巴（external 129 run 05 为 1880 µs），都是单次请求的抖动而不是某类系统性变慢。
一次**首轮作废**保留在下面：最初把 natural 的证书铸在 horizon 1103，那里 cut.completeRecords=0，
`certified_records` 恒为 0，说明那个夹具根本没走证书路径；改成记录边界 7000 后 6/6 进程都是 1。
作废读数不删——它正是"前置断言必要"的证据。

## 4. 与既有两类合并后的覆盖表

| # | 预声明类 | 证据 | 最差 request→present |
| ---: | --- | --- | ---: |
| 1 | 本会话自然 checkpoint 首次使用 | 本轮 `natural` | 1041 µs |
| 2 | 同档重复热使用 | [2026-11-01 门](../2026-11-01-r3-pause-prime-and-seek-gate.md) 第二节 | 1012 µs |
| 3 | 外部归档首次认证 | 本轮 `external` | 1880 µs |
| 4 | 冷进程 | 同上（每进程全新） | 1012 µs |
| 5 | 无合法 checkpoint 回退 | 本轮 `fallback` | 1335 µs |

五类的判据都是"每次 ≤20 ms、按进程最大值判定"，最差读数 1880 µs，余量 ≥10×。**注意**：五类
读数来自两次批次、两个不同仪器版本（本轮探针新增了 class 开关与 oracle 校验），所以只按"每类
各自通过"登记，不把两批合成一个统计量。

## 5. 复现

```powershell
# 需要真实 GPU 与隐藏窗口；探针在 out/build/rpa-candidate-release
python -B tools/run_r3_seek_classes.py
```

脚本会重算二进制 sha256、记录 HEAD 与 dirty diff、逐进程写 `runs/<class>/<records>/run-NN.log.txt`，
并把结果写到 `tools/results.json`；判据与失败规则写在
[`tools/preregistration.json`](tools/preregistration.json)。

## 6. 未做

- hosted 同 SHA CI 与 R3 退出裁定（owner 2026-11-02 / 2026-11-03 裁定）。
- 适配器固定与设备接受：本轮沿用既有路径，只登记主机清单。
- 其余适用门（load、读取、D2/D3/Snapshot/排队）不由本轮改动；它们的状态见 R3 门账。
