# R3 第一批正式门测量附件（2026-11-01）

本目录保存四个组的全部字节：预注册、三个驱动器、逐进程日志与派生结果。它回答的问题只有一个——
在 `6256af8` 这一份源码上，哪些门真的被测到了、读数是什么、哪些门因为边界不可比而仍未判定。

- [返回本批报告](../2026-11-01-r3-first-gate-batch.md)

## 分组与证明范围

| 目录 | 内容 | 证明什么 | 明确不支撑 |
| --- | --- | --- | --- |
| `load/` | 9 次 `cuexis_player.exe --smoke-test` 的 stdout/stderr 与 `results.json` | 进程创建→首张有效画面 present-return 的 9 个读数；每次运行的 golden 行与唯一 `first_frame_presented` 都在日志里 | 不支撑音频设备初始化路径（smoke 场景关闭音频），也不支撑 disk-cold 说法 |
| `seek/` | 3 次 `cuexis_execution_host_tests "[seek-matrix]"` 的日志与 `results.json` | 功能矩阵 180 次 seek 的 `request_to_ready_us` / `request_to_present_us` 分布 | **不支撑**适用 Seek ≤20 ms 门：矩阵是 CPU 测试渲染器、33/129 记录、事件自带 `formal:false` |
| `seek.aborted-1/` | 第一次尝试的日志 | 记录"过滤器写到了错误的可执行文件"这次作废尝试：没有跑任何用例，也没有取到读数 | 不作任何读数使用 |
| `read/` | 9 对 A/B 进程的日志、两台驱动器的日志与 `results.json` | 预注册的配对读取比较：逐对 B/A 比值、逐窗区间与全组区间 | 只覆盖 1800 实体、`extractFrame` 读取块；不覆盖更新/提交/渲染路径 |
| `tools/` | 预注册、三个驱动器 | 判据、边界、统计口径在测量前写定；驱动器可重跑 | 不是新的接受判据 |

## 两个臂的定义（`read/`）

- **A**：`d344088`（pre-PD，2026-10-18）的独立 worktree `out/_armA-d344088`，用同一套 MSVC/Ninja 构建
  `cuexis_pd_baseline_probe`。该提交没有 `tests/pd/read_scaling_probe.hpp`，所以把 HEAD 的同一份头文件
  放进该树并在其 `measurement_method_tests.cpp` 末尾引入——头文件自己写着"common finite diagnostic
  harness, also compiled against the archived pre-PD source"。
- **B**：HEAD `6256af8` 的既有 `out/build/rpa-candidate-release` 树（第四次矩阵构建）。
- 两侧二进制哈希记录在 `read/results.json` 的 `arm_a.sha256` / `arm_b.sha256`，也在 `manifest.json` 的
  `binding.read` 里。

## 为什么"同一份工作"是可核的

每对A/B必须同时满足三条，否则该对被判无效且不参与区间：

1. 两侧打印的 `PD_FOLLOW_CONTENT.sha256`（图文本的 sha256）相同；
2. 两侧三个读取块的快照 digest 集合相同；
3. 每个读取块 `elapsed_us >= 100000`（PD 的长块校准）。

本批 9 对全部满足，`results.json` 里逐对可查。

## 读取顺序

1. `tools/preregistration.json`：先看判据、边界、分组与两次补记（seek 目标修正、读取协议补记）。
2. `load/results.json`：9 个读数、`summary` 与每次运行的日志名。
3. `seek/results.json`：三进程各自的 ready/present 统计与合并统计。
4. `read/results.json`：逐对原始中位数、逐窗区间、全组区间与 `decision`。
5. 需要复核某个读数时，打开对应的逐进程日志；`manifest.json` 给每个归档文件的源路径、字节数与两侧哈希。

## 边界

- 本次批全部为 test-only：不构成 R3 技术接受、设备验证或生产接受。
- `load/` 的读数包含进程创建到首行日志的约 40 ms（本批实测 39.5–41.1 ms），该段是进程与日志初始化，
  属于门的边界之内。
- `seek/` 是代价证据而不是门：适用 Seek 的读数需要一个产品渲染器与真实规模回放的独立装置。
- `read/` 的 A 臂是归档源码加同一份探针的适配构建；适配只增加了一个 `#include`，探针代码与 B 臂逐字相同。
- 归档文本统一为 CRLF；`manifest.json` 对每个文件同时记录 `source_sha256` 与 `sha256`。
