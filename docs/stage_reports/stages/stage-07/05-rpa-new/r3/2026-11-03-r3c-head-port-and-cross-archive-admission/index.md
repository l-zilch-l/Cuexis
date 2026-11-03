# R3c 原型移植到 HEAD，并补上跨旧 archive 的字节准入（2026-11-03）

状态：candidate + local verification；2026-11-04 正式单元未形成有效样本，**不采用**；test-only 探针；
不改判据、不关闭 R3，也不替代 R4 / 设备 / 生产接受。

日期：2026-11-03　基线源码：`c085688a`（候选在独立工作树 `D:/Cuexis-r3c-v4` 上，尚未合入主树）
原型源码：`a6e3b31` + `r3c-v4-unselected.patch`（归档在
[`2026-10-30-resume-evidence/`](../2026-10-30-resume-evidence/)）

返回 [RPA 报告索引](../../README.md)。口径见
[重复性口径裁定](../2026-11-03-r3-repeatability-scope-ruling.md)。

## 0. 结论

1. **移植完成，语义/故障/字节三项都有本树读数**：原型在产品侧只改 4 个文件（`execution_kernel.cpp`
   +314 行、`shared_sequence.hpp` +42、`execution_kernel.hpp` +12、`recovery_codec.cpp` +2/−1）。
   HEAD 已把 `execution_kernel.cpp` 拆成 advance / archive / query / restore / internal / setup，
   314 行按职责落到对应拆分文件。`cuexis_judgement` 与 `cuexis_rpa_tests` 零错误、零警告编译；
   R3c 相关 **14 个用例 / 1037 条断言全绿**（Debug，无插桩）。
2. **补上了 v4 没有的跨旧 archive 字节准入**：v4 只在**抓取那一刻**用"这一个点"的大小去问
   预算，`archive()` 只是把 writer 的 4 个槽原样塞进索引。后果是 `tickPayloadBytes(archive)` 读的是
   **整个准备共用的 live 计数器**，读不到"这个 archive 到底持有什么"；而未内部封存的 archive 一旦
   从 seek 分支继承索引，还会带着别人的点。本候选增加了三件事：
   `admitTickPoints()` 在封存时按**该 archive 自己发布的那一个 budget 对象**复核每个点、
   索引记录 `ticksBytes` / `ticksRefused`、非内部封存的 archive 不再继承任何点索引。
   于是"旧 archive 持有被淘汰的点"这件事第一次变成**可读、可复算**的量，而不是共用计数器里的一个数。
3. **读数（Debug，无插桩，固定 fixture）**：单点计费
   ≈1.43 MB（1000 requirements）/ 2.00 MB（1536）/ 52.3 MB（40000）。
   1536 六代同留时 `live` 12.10 MB 而最新 archive 只 admitted 8.07 MB——**4 个槽不是进程驻留的上界**，
   多出来的 2 个点正是被旧 archive 留住的。40K 在默认 128 MiB 下只装得下 2 个点
   （103.95 MB，第三个要 155.9 MB 才进得来），256 MiB 装 4 个（207.9 MB），512 MiB 仍 4 个
   （槽上限），但 `live` 涨到 311.9 MB（多 2 个退休点）。
4. **进程内存归因（外部采样，不在测试里写测量代码）**：capture 臂与 replay-only 臂在**同一 horizon**
   上的 private bytes 差只有 −0.5…+0.5 MiB，落在采样噪声内；而计费是 12.10 MB（1536）/ 103.95 MB
   （40K）。也就是说**在必须常驻的 exact journal 还活着时，计费是保守上界，实际增量远小于它**。
   这与归档的 v4 quota frontier 一致（40K 4 个点的 `extra_requested` 只有 1.18 MB，约 0.3 MB/点）。
5. **明确不声称**：v4 归档里的观察器 hook（`r3c_cache_event` / `r3c_staged_point` /
   `r3c_floor_release` / `r3c_detach_*` / `r3c_counter_boundaries`）是研究期插桩，**没有移植**；
   依赖它们的 guard/quota 读数只有归档值，本轮不在新树重跑 40K 正式门。采用与否、quota 取多少是
   owner 决定（见 §4）。

## 1. 移植内容

| 原型文件（a6e3b31） | +行 | HEAD 落点 |
| --- | ---: | --- |
| `engine/judgement/src/execution_kernel.cpp` | 314 | `execution_kernel_internal.hpp`（预算/租约/计数器/点类型/索引字段）、`execution_kernel_advance.cpp`（`advance` / `advanceForReplay` / `advanceImpl` / `advanceRecorded` 抓取块）、`execution_kernel_archive.cpp`（`archive()` 索引、partial-horizon 快速恢复、`advanceForReplay` 调用点）、`execution_kernel_query.cpp`（`tickPointCount` / `tickRestoreCount`）、`execution_kernel_restore.cpp` 与 `recovery_codec.cpp`（回放入口改 `advanceForReplay`）、`execution_kernel_impl_setup.cpp`（创建 budget） |
| `engine/judgement/include/cuexis/judgement/shared_sequence.hpp` | 42 | 同名文件（`visitStorageCapacity` / `visitAppendOnlyRetention` / `sharesCompletePrefixPagesWith`） |
| `engine/judgement/src/execution_kernel.hpp` | 12 | 同名文件（`advanceForReplay`、`tickPointCount`、`tickRestoreCount`、`tickPayloadBytes`、`tickPayloadLimit`、`setTickPayloadLimitForProbe`、`struct TickCapture;`） |
| `engine/judgement/src/recovery_codec.cpp` | 2 | 同名文件（`decodeReplay` 用 `advanceForReplay`） |
| `tests/rpa/rpa_b3_tests.cpp` | 409 | 拆成新文件 `tests/rpa/rpa_b3_r3c_tests.cpp`（+691 行，含本轮新增）并加入 `cuexis_rpa_tests` |

移植判据不是"看起来一样"，而是**逐行闭合**：把 `r3c-v4-unselected.patch` 的全部新增行（369 行）与
HEAD 移植后的语料对照，只剩观察器 hook 相关的 56 行缺失（即 §0.5 声明不移植的部分），其余全部命中。
原型补丁本身在 `a6e3b31` 上 `git apply` 为零错，`execution_kernel.cpp` 复算 sha256 为
`9ef90b44…`，与 `r3c-v4-selection.json` 一致；移植基线 HEAD 与该原型只差文件拆分，语义改动为零。

### 1.1 本轮新增（原型没有）

| 变更 | 位置 | 目的 |
| --- | --- | --- |
| `TickPayloadLease::charge()` / `owner()` | `execution_kernel_internal.hpp` | 让索引能读到"这个点欠了多少字节、欠的是哪个 budget" |
| `TickPoints::bytes()` | 同上 | 一组槽自己的计费，不借用全局 live |
| `admitTickPoints(source, owner, admitted)` | 同上 | 只接受"计费对象 == 本 archive 发布的 budget"的点；其余拒绝且不动 writer 的槽 |
| `NativeRecoveryIndex::ticksBytes` / `ticksRefused` | 同上 | 每个 archive 自己的 admitted 字节与被拒数 |
| `archive()` 封存时走准入；未内部封存不发布索引 | `execution_kernel_archive.cpp` | 分支继承的索引也按本代 budget 重新准入；不可信来源不再携带点 |
| `tickPayloadAdmittedBytes()` / `tickAdmissionRefused()` | 同上 + `execution_kernel.hpp` | 探针/测试可读，不新增公开 ABI |

保留 `tickPayloadBytes()` 原义（**整个准备共用的 live**），并在报告与测试注释里写清它不等于单
archive 驻留；避免把 v4 归档读数改成另一种口径后无法对照。

## 2. 语义与故障验证

`tests/rpa/rpa_b3_r3c_tests.cpp`（Debug，`cuexis_rpa_tests`）：

| tag | 用例 | 断言 |
| --- | --- | ---: |
| `[rpa][r3c][prototype]` | 密封 Tick 恢复 partial 状态（5 种前缀）、槽保留最早点与精确 replay | 625 |
| `[r3c-budget]` | 0 预算拒绝、租约跨代计费/最后持有者退款、容量访问重入、剩余预算保留旧点、fork 共享计费、append-only 页共享、原 archive 释放后 Fact floor | 129 |
| `[r3c-cross]`（本轮新增） | 逐代 own payload、多代同留仍在 limit 内、拒绝后 replay 字节不变、未封存/独立 archive 不 admitted、分支继承索引重新准入 | 283 |

语义判据是**逐字节**：恢复后的 `snapshotGameplay` 与无提示 origin 路径的
`encodeSnapshot` 相等；分支再 `advance` 后 `sameReplayHistory` 且 `encodeReplay` 字节相等。
故障面覆盖：预算拒绝（`setTickPayloadLimitForProbe(0)`）、诊断控制下不落点
（`inject({})` 后 `canCaptureTicks()` 为假）、错误/独立 archive 与 host 构造的 external archive
一律 0 点 0 字节。

## 3. 字节计入读数

工具：[`tools/run_r3c_accounting.py`](tools/run_r3c_accounting.py)；读数：
[`tools/accounting-results.json`](tools/accounting-results.json)。隐藏用例
`R3c accounting diagnostic`（`[.][r3c-accounting]`）只打印计数，进程内存由**外部采样器**读取
（`K32GetProcessMemoryInfo`），测试里没有测量代码。全部行来自同一个二进制（sha256 见 JSON）。

### 3.1 单点计费与逐代 admitted（默认 128 MiB）

| fixture | 第 1 点计费 | 第 6 代 points / admitted | 第 6 代 live | 说明 |
| --- | ---: | --- | ---: | --- |
| 1000 reqs，单窗口 | 1 429 416 B | 1 / 1 429 416 B | 1 429 416 B | 只有一个窗口闭合，只落一个点 |
| 1536 reqs，wave 256 | 2 001 448 B | 4 / 8 065 096 B | 12 095 992 B | **live > admitted**：2 个被淘汰点仍被旧 archive 持有 |
| 40000 reqs，单窗口 | 52 323 608 B | 1 / 52 323 608 B | 52 323 608 B | 40K 单点约 49.9 MiB |
| 40000 reqs，wave 256 | 51 967 496 B | 2 / 103 947 896 B | 103 947 896 B | 第 3 个点要 155.9 MB，默认 limit 拒绝 |

### 3.2 40K quota frontier（同一二进制，只改 `setTickPayloadLimitForProbe`）

| limit | 第 6 代 points | admitted | live | 第三个点是否进得来 |
| ---: | ---: | ---: | ---: | --- |
| 128 MiB | 2 | 103.95 MB | 103.95 MB | 否（155.9 MB > 134.2 MB） |
| 256 MiB | 4 | 207.92 MB | 259.90 MB | 是；第 5 个点成为退休点，live 再涨 52 MB |
| 512 MiB | 4 | 207.93 MB | 311.89 MB | 是；槽上限 4 后仍有两代退休点 |

所以"把 limit 调大"买到的是**覆盖**（能留住几个 window 的点），不是"进程会多占这么多"。4 个槽的
容量上限与 limit 是两件事：512 MiB 下 live 311.89 MB 中只有 207.93 MB 属于最新 archive。

### 3.3 进程内存归因（capture vs replay-only，同一 horizon）

| fixture | capture 臂 private | replay-only 臂 private | 差 | 计费 |
| --- | ---: | ---: | ---: | ---: |
| 1536，wave 256 | 84.72→101.26 MiB | 84.96→101.13 MiB | −0.24…+0.13 MiB | 12.10 MB |
| 40000，wave 256 | 1875.37→1878.29 MiB | 1875.26→1877.82 MiB | −0.42…+0.47 MiB | 103.95 MB |

两臂都随 advance 单调增长约 16.5 MiB（1536）/ 0.4–3 MiB（40K），这是**journal 本身**的增长，与点无关；
点带来的差落在 ±0.5 MiB 噪声里。原因在设计里写着：除 append-only Fact 页以外，点持有的页都被
mandatory journal 同时持有，所以按页计费必然是上界。**反过来也说明**：只要 journal 必须常驻，
"按 128 MiB 计费只装 2 个点"并不等于"进程会多 128 MiB"。

## 4. 采用材料与需要 owner 定的问题

- 候选补丁：[`tools/r3c-head-candidate.patch`](tools/r3c-head-candidate.patch)（11 文件，+1120/−11），
  应用方式见 §5。工作树 `D:/Cuexis-r3c-v4` 保持在 `c085688a` + 该补丁的 dirty 状态，未提交、未合入。
- 与归档 v4 的差异只有两类：**(a)** 跨代准入与每 archive 计量（§1.1）；**(b)** 研究期观察器 hook 未移植。
  前者是本轮要求补的缺口，后者是刻意不带进产品的插桩。
- 需要 owner 决定的仍是归档里那句 `product memory quota/headroom decision remains open`：
  1. **quota 取多少**：128 MiB 在 40K 只给 2 个点（覆盖 2 个 window），256 MiB 给 4 个（槽上限），
     512 MiB 不再增加覆盖只增加退休驻留；
  2. **headroom 口径**：按 §3.3，计费是保守上界，实际增量在噪声内；是接受"保守计费决定覆盖"，
     还是要求改用更贴近实际的计量（会削弱"未知持有者一律全额计费"的安全边界）；
  3. **每进程请求数**：正式 Seek 单元登记须写死 16 次/进程（父计划未规定，见口径裁定）。
- 本候选**没有**动公开 recovery wire、没有新增公开头文件、没有改 RPA 记录集合；`tickPayloadAdmittedBytes`
  等只是内部 `ExecutionKernel` 的静态探针函数，和既有 `tickPayloadBytes` 同级。

## 5. 复现

```powershell
# 1) 候选：HEAD + 补丁
git -C D:/Cuexis-redesign worktree add D:/Cuexis-r3c-v4 c085688a --detach
git -C D:/Cuexis-r3c-v4 apply docs/stage_reports/stages/stage-07/05-rpa-new/r3/2026-11-03-r3c-head-port-and-cross-archive-admission/tools/r3c-head-candidate.patch

# 2) 构建（候选配置也编，避免拆分类错误只在 candidate 配置暴露）
cmake -S D:/Cuexis-r3c-v4 -B D:/Cuexis-r3c-v4/out/build/r3c-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug `
  -DBUILD_TESTING=ON -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON -DCUEXIS_BUILD_DEVELOPER_TOOLS=ON `
  -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build D:/Cuexis-r3c-v4/out/build/r3c-debug --target cuexis_rpa_tests

# 3) 语义/故障/准入
D:/Cuexis-r3c-v4/out/build/r3c-debug/bin/cuexis_rpa_tests.exe "[r3c],[r3c-budget],[r3c-cross]"

# 4) 字节计入（含外部进程内存采样）
python -B tools/run_r3c_accounting.py --worktree D:/Cuexis-r3c-v4 `
  --binary D:/Cuexis-r3c-v4/out/build/r3c-debug/bin/cuexis_rpa_tests.exe `
  --out tools/accounting-results.json
```

## 6. 未做

- hosted 同 SHA CI（owner 2026-11-02 裁定本轮不跑）、R3 退出裁定。
- 观察器 hook 的 guard/quota 探针在新树重跑：归档值仍按 v4 身份引用，不冒充本轮读数。
- 采用本身：本报告只提供材料，合并、quota 与 R3 退出均由 owner 决定。

## 7. 2026-11-04 正式采用单元与处置

按 2026-11-03 口径建立 3 窗 × 每窗 3 对、每进程 16 次请求的正式 A/B 单元。v1/v2 在测量前因
harness 错误停止，v3 在进程控制查询处停止；v4 的首个 baseline 进程完成预热并取得 7 个请求后，
第 8 次重复 `pause` 失败。控制器按预注册的“不重试、不替换”规则立即停止，`results.json` 为
1 行无效、0 个有效进程、三个 case 均为 `inconclusive`。

因此本候选**不采用**：`selected: false`、`product_promoted: false`。128 MiB quota 只是准备正式单元的
技术倾向，未升级为产品决定；本候选补丁仍只作为归档材料保留，主树未应用。完整失败序列、原始日志与
边界见[最终验证与 R3c 决定](../2026-11-04-r3-final-validation-and-r3c-decision.md)。

