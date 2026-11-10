# R3 提交路径停顿归因证据（2026-11-01）

本目录保存一次**诊断性**采样的全部字节：预注册、采样器与分析脚本、七个登记进程的原始样本流与逐帧
日志、四次栈回溯、以及 GPU 计时器那台独立仪器的原始数据。它回答的问题是——2800 静态点每帧那
5–6 ms 到底在等什么、等多久，以及这份等待是随实体数变长还是变多。它**不是**容量点、不是矩阵行、
不是性能门通过，也不构成 R3/R4、设备或生产接受。

报告的结论在 `../2026-11-01-r3-submit-stall-wait-attribution.md`；本索引只说明每句话能由哪个文件
重算，不给新的结论。

共 134 个文件：130 个复制件 + 本 `index.md` + `manifest.json` + `validation.json` + `.gitattributes`。
原始体积 473 156 453 字节（451.2 MiB），归档后约 29.8 MiB（本文件自己的字节数会随修订变化，所以确切
数字只记在 `manifest.totals` 里）：42 条样本/日志流走确定性 gzip（`mtime=0`、不含文件名，可复现），
88 条文本逐字节复制但 EOL 统一为 CRLF（原始哈希另记 `source_sha256`），0 个二进制副本。

## 文件与证明范围

| 文件 | 证明什么 |
|---|---|
| `manifest.json` | 本归档自己：每条复制文件的源路径、字节数、归档哈希、源哈希与 EOL 处理方式（文本 CRLF 归一、样本流确定性 gzip）；绑定 head、探针二进制哈希、探针源码哈希、`tabulated_sha256` 与 `tabulated_source_sha256`、逐运行仪器版本 |
| `validation.json` | 建库时的自检结果：文件集与清单一致、每条哈希对上、每个 gzip 流解压后哈希等于源哈希、每个文本副本无裸 LF |
| [PLAN.md](plan/PLAN.md) | GPU 计时器诊断的预注册（问题 P1–P4、点位、判据） |
| [PLAN-RIP.md](plan/PLAN-RIP.md) | 指令指针采样的预注册（采样方式、偏移推导、要读的量） |
| [PLAN-WAIT.md](plan/PLAN-WAIT.md) | A/B 等待成本试验的预注册：组序、判据 H5/H6、决策规则（§1.3 关于等待对象的表述有误，更正写在报告里，未改本文件） |
| `instruments/*.py` | 采样、分析、汇总与栈回溯脚本（12 个，见下节）；`manifest.json` 的 `instrument_versions` 记录每个运行实际用的是哪个版本 |
| `derived/wait-groups.tabulated.json` | 权威汇总表：逐进程等待次数、时长、占比、主线程内核调用占比，以及 H5/H6 判定；逐字节源哈希 `c59044564cdeb98ac5196312519ea538b57305e62f92d92d79406afc1a9e6c67`，归档副本（统一 CRLF）为 `5cad5a2f…`，两者都在 `manifest.binding` 里 |
| `derived/wait-groups.run.log` | 七进程组运行的标准输出，含每次采样的汇总行 |
| `derived/run_wait_group.summary.json` | 运行器写的组清单：实体数、进程数、是否标句柄、对应的 `.wait.json` 文件 |
| `runs/wait-2800-static/` | A 组：3 进程 × 20 s，2800 静态 |
| `runs/wait-1800-static/` | B 组：3 进程 × 20 s，1800 静态 |
| `runs/wait-2800-static-handles/` | 同一性运行：1 进程，开句柄标注 |
| `runs/rip-2800-static/` | 仪器沿革：RIP 采样器在 2800 静态的试点。其分析脚本已改版，`analysis-01.json` 只作沿革留存、不复原；`invalid-attempt-01/` 是它的第一次尝试（写出 32 428 字节样本即停，没有测量区间），整组保留 |
| `runs/rip-smoke/` | 仪器自检：一次短进程，证明采样器能挂上、能取样、能写句柄表 |
| `timer-instrument/runs/n{1800,2400,2800}/` | **另一台仪器**：`CUEXIS_GPU_TIMER` 打开时的 1800/2400/2800 两种模式，各 3 进程 |
| `timer-instrument/analyze.py`、`analysis.json` | P1–P4 的回答：GPU 间隔是否随实体数上升、慢帧是否带更大的 GPU 间隔 |
| `timer-instrument/run_stall_diagnosis.py` | 那台仪器的运行器；sha256 `30a34162…` 与全部十次登记的 `runner_sha256` 相同 |
| `walks/walk-{probe,readback-0,readback-1,40000}/` | 栈回溯：`NtWaitForSingleObject` 之上那一帧是谁，以及 40000 实体时还有没有等待 |
| `verify/verify_wait_evidence.py` | 归档自带校验器：只读本目录，重算每条哈希、解压每个流 |
| `index.md` | 本文件（生成，记在 `generated_files` 而不是 `copied_files`） |

## 四条仪器，不得混用

同一批帧被四台仪器各自量过，它们的数字**不能相加、不能相减、不能互相替代**：

1. **冻结容量阶梯**（`16a3f57a…` 运行器）不在这里，也没有重跑。它是唯一登记过的容量来源。
2. **GPU 计时器**（`timer-instrument/`，运行器 `30a34162…`）：在 `backend.submit` 前后各写一个 GL
   时间戳。它多一次同步，因此系统性地更快（2800 静态 205.549 FPS，而同点阶梯是 195.912）。
3. **等待采样器**（`runs/`）：每次采样都要挂起/恢复渲染线程，因此系统性地更慢（A 组 165.268 FPS
   对阶梯 195.912）。它的 FPS 与 p99 只用于 A/B 之间的同仪器比值。
4. **栈回溯**（`walks/`）：每次回溯要停线程，只用于读调用链，不产出任何时间统计。

扰动量级已登记：不带句柄（`slow_share 0.1316`）< 带 `--handles`（`0.1459`）< `sample_rip.py` 试点
（`0.2347`）。`--stack-bytes 512` 会把吞吐砍半，所以 A/B 全部用 `--stack-bytes 0`。

## 每个进程目录里有什么

`registration.json`（head、探针源码哈希、采样器/分析器/驱动哈希、二进制哈希）+
`results-NN-<mode>-<N>.json`（逐帧结果、每秒窗口、预算判定、身份字段）+ 以下六件：

| 文件 | 内容 |
|---|---|
| `NN-<mode>-<N>.samples.txt.gz` | 采样器原始输出，**逐字节** gzip（`mtime=0`、无文件名，可复现）；`derive_all.py` 的输入 |
| `NN-<mode>-<N>.log.txt.gz` | 探针原始逐帧日志，同样逐字节 gzip |
| `NN-<mode>-<N>.wait.json` | `analyze_wait.py` 归约出的线程/模块/内核调用/句柄表 |
| `NN-<mode>-<N>.runs.json` | `derive_all.py` 重推导出的逐帧等待片段（每次等待的起止与时长） |
| `NN-<mode>-<N>.modules.txt` | 采样时进程内的模块映射（地址区间与路径） |
| `NN-<mode>-<N>.sampler.txt` | 采样器自报：采样次数、挂起失败数、掩码与优先级 |

`runs/rip-2800-static/` 与 `runs/rip-smoke/` 没有 `.wait.json`，`derive_all.py` 因此跳过它们（见下节
`skipped: 2`）。

## 重推导

`derive_all.py` 只在**它自己所在目录**下一层找 `results-*.json`，所以重建时必须按原始的
`<组>/<文件>` 布局摆放（不是摊平），样本流先解压、文本副本先还原成原始 EOL。
`verify/verify_wait_evidence.py --rebuild` 在临时目录里自动做完这些，然后跑：

```powershell
python -B derive_all.py --check
python -B tabulate_wait.py --root <临时目录> --out rebuilt.tabulated.json
```

还原这一步不是猜的：文本副本在归档里统一是 CRLF，校验器对每条副本试两种读法（原样、把 `\r\n`
改回 `\n`），要求其中一种的 sha256 等于 `manifest.json` 记的 `source_sha256`，对不上就中止。实测
`52 text copies: 42 inverted to LF, 10 already CRLF, all matched source_sha256`（十个本来就是 CRLF 的
是 `.sampler.txt`）。第一条对每个有 `.wait.json` 的流重算 `.runs.json` 并逐字节比较，实测
`streams: 9  byte-identical: 7  changed: 0  skipped: 2`。第二条重建汇总表，其 sha256 必须仍是
`c5904456…`（`tabulated_source_sha256`；比对的是逐字节源哈希，不是归档里那份 CRLF 副本）。所以报告
引用的每一个等待数字都能从本目录的原始样本流重新算出来，而不是只存在于一张表里。

重推导还依赖与当年同版的 ntdll：样本存的是绝对地址，脚本要用 `ntdll.dll` 的 syscall stub 表把地址
翻回系统调用号。本机 ntdll sha256 是 `a74f7482085eab125ccc09152ab7e0b5994bcb13e1a7b29880bdbb24179ecb8b`，
记在 `manifest.json` 的 `environment` 里；换一台 ntdll 不同的机器，`--rebuild` 会拒绝重推导，而不是
给出一个看似合理的结果。

`verify/verify_wait_evidence.py`（不带参数）做的是另一件事：不重算分析，只核对**字节**——逐条比对
`manifest.json` 里的 `sha256` 与 `source_sha256`，解压每个 gzip，检查每个文本副本无裸 LF，并确认没有
清单之外的文件。

## 仪器版本漂移

`run_sample.py` 与 `analyze_rip.py` 在试点、自检、A/B 之间被就地改过，旧版本没有留存。`manifest.json`
的 `instrument_versions` 逐运行记录登记里的哈希、以及它是否等于归档里的文件：

| 运行 | 采样器 | 分析器 | 驱动 `run_sample.py` |
|---|---|---|---|
| A / B / 同一性 | `af4bd451…` 一致 | `79a4707d…` 一致 | `79b362f3…` 一致 |
| `rip-2800-static` | `70cbd75e…` 一致 | `326bc477…` **不一致**（归档为 `62bf05ac…`） | `07dd1ccf…` 不一致 |
| `rip-smoke` | `af4bd451…` 一致 | `62bf05ac…` 一致 | `998c5665…` 不一致 |

两个不一致的运行都是仪器沿革，报告不引用它们的任何数字，所以「不可复原」不影响任何结论；但它被写进
清单，而不是被悄悄抹平。A/B/同一性这三组（报告的全部等待数字来源）与归档文件逐字节对齐。

## 作废的尝试

`runs/rip-2800-static/invalid-attempt-01/` 是 RIP 采样器在 2800 静态的第一次尝试：采样器只写出
32 428 字节样本进程就停了，没有形成任何测量区间。它整组保留，`manifest.json` 的
`superseded_attempts` 记录它的状态与原因，报告不引用它的任何数字。

除此之外没有任何运行被丢弃。十个 `*.ready` 握手标记不归档：它们是采样器启动前的一次性标记文件，
不含可证的测量内容，`manifest.json` 的 `excluded_files` 逐个列出它们的路径。

## 主张与支撑

- 「等待次数不随实体数增长」只由 `derived/wait-groups.tabulated.json` 的 `comparison` 与
  `decision_rule` 支撑：`ratio_B_over_A = 0.8034730`（= B-2 的 `1.0049473` ÷ A-1 的 `1.2507544`），
  判据是不低于 `0.75`。
- 「等待时长随负载变长、次数不变」中的时长一半是描述量：`marginal_wait_us_per_entity = 1.6914`。
  报告同时写明这条被 40000 实体的回溯**否证**（那里一次等待都没有），两处都不得只引一半。
- 回读开销（`+47.4%` 吞吐、`−1.596 ms/帧`）只由 `walks/walk-readback-0` 与 `walks/walk-readback-1`
  这一对同二进制、同实体数、只差 `CUEXIS_GPU_PIXEL_READBACK` 的运行支撑。
- 40000 实体的成本结构（`18.2 FPS`、`54.95 ms/帧`、`exe 57.5%`、`ntdll 25.0%`、
  `NtWaitForSingleObject` 命中 0 次）只由 `walks/walk-40000/walk.json` 与 `walk.txt` 支撑。
- 主线程内核调用占比只由 `derived/wait-groups.tabulated.json` 的 `main_kernel_call_share` 支撑。

## 读取顺序

1. `manifest.json` 的 `binding` 与 `totals`：绑定的是哪个二进制、哪次 head、归档多大。
2. [PLAN-WAIT.md](plan/PLAN-WAIT.md)：先看判据，再看数字（并记住 §1.3 的等待对象写错了，更正在报告里）。
3. `derived/wait-groups.tabulated.json` 的 `decision_rule`、`comparison`、`summary`。
4. 需要复核某个进程时，读它的 `registration.json`、`results-*.json` 与 `.wait.json`。
5. 要确认等待对象时读 `runs/wait-2800-static-handles/*.handles.txt`（表头 `# value type name`）。
6. 要确认调用链时读 `walks/walk-probe/walk.txt`；要确认 40000 实体没有等待时读
   `walks/walk-40000/`。
7. 想独立验证时跑 `verify/verify_wait_evidence.py`。

## 边界

- 诊断证据：不改冻结容量下界（仅渲染 1800、静态 1300），不提升任何门限，不构成 R3/R4 或生产接受。
- A/B 每组三个新进程、每个二十秒，足够支撑「每帧约一次等待、次数不随实体数变」的方向性结论，
  不足以支撑等待时长的显著性结论。
- B 组的 `control_ok` 为 `false` 是预期：`run_sample.py` 要求慢帧占比不低于 1% 才算控制组通过，
  而 1800 静态本来就没有慢帧。
- 只在 1080p、该探针二进制、`fixture_revision=3`、NVIDIA 616.64 上成立；换分辨率、夹具或构建
  即属另一次测量。
