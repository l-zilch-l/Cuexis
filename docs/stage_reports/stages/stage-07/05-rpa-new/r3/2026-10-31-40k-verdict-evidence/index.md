# 40K 吞吐与预算判定附件

状态：冻结证据索引。返回 [判定报告](../2026-10-31-r3-40k-240fps-and-budget-verdict.md)。

- [复制来源、原始字节及哈希](manifest.json)
- [身份、取舍与限制补充](identity-and-selection-supplement.json)
- [刷新测量脚本](fresh-tools/run_40k_refresh.py.txt)、[汇总脚本](fresh-tools/summarize_verdict.py.txt)、
  [串行执行脚本](fresh-tools/after_matrix.ps1.txt)
- [本轮汇总输出](fresh-tools/summarize.log.txt)（四组刷新运行的完整表格，含逐秒窗口）
- [探针源码三代](probe-lineage/head-gpu_throughput_probe.cpp.txt)

## 目录

| 目录 | 内容 | 证明什么 |
| --- | --- | --- |
| `fresh/` | 2026-10-31 23:53 起在 `466ff76` 上的 1080P / 40K 两组，各三进程、每进程 120 帧 | 当前源码差预算 12.07×（仅渲染）/ 14.72×（静态提帧） |
| `fresh-720p/` | 同一二进制、同一夹具，只把视口换成 1280×720 | 像素少 2.25 倍而成本几乎不变：仅渲染 FPS −1.53%、p99 +7.00% |
| `fresh-1800/` | 1800 实体、每条路径三进程、各 20 秒 | 两条路径在 HEAD 上都通过（p99 3.211 / 3.725 ms，最小逐秒窗口 370 / 303 帧） |
| `fresh-1300/` | 1300 实体、每条路径三进程、各 20 秒 | 两条路径在 HEAD 上都通过（p99 2.568 / 3.071 ms，最小逐秒窗口 485 / 383 帧） |
| `frozen-1080p/` | 2026-10-29 的 1080P 容量扫描：16 档曲线、`audit.json` 与三个持续子目录 | 冻结前缘 1800 / 1300，以及 1900 / 1500 / 1400 的失败读数 |
| `frozen-720p/` | 2026-10-29 的 720P / 40K 主组 | 冻结侧 720P 的两条路径与全量行为对照 |
| `probe-lineage/` | 三代探针源码：冻结 1080P、冻结 720P、HEAD | 两代之间的差异只在启动与观测，不在测法 |
| `fresh-tools/` | 刷新脚本、串行执行脚本、执行状态与汇总输出 | 四组刷新运行是同一二进制、同一测法、串行执行且无失败步骤 |

每个刷新运行目录含 `registration.json.txt`（计划、二进制 sha256、探针源码 sha256、
`head`、`dirty_files`）与 `results.json.txt`（`runs[]` 逐进程 + `groups{}` 按模式聚合）。
`fresh/` 与 `fresh-720p/` 另有六个逐进程日志 `NN-<路径>-40000.log.txt`。
归档时所有文本文件加了 `.txt` 后缀，浏览器不会把它们当成可执行内容。

## 刷新点的数字

| 分辨率 | 路径 | wall FPS（三进程） | 均值 | p99 | p99 / 4.166667 ms | 需要提速 | p99 组内极差 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1920×1080 | 仅渲染 | 22.09 / 22.54 / 22.42 | 44.611 ms | 50.282 ms | 12.07× | 10.71× | 7.64% |
| 1920×1080 | 静态提帧 | 17.52 / 17.41 / 17.51 | 57.114 ms | 61.325 ms | 14.72× | 13.71× | 2.64% |
| 1280×720 | 仅渲染 | 22.51 / 22.76 / 22.82 | 43.927 ms | 46.994 ms | 11.28× | 10.54× | 2.01% |
| 1280×720 | 静态提帧 | 17.62 / 17.39 / 17.49 | 57.164 ms | 60.485 ms | 14.52× | 13.72× | 4.88% |

均值与 p99 取三个进程的中位数；每进程 120 个测量帧、超预算帧 120/120。
按实体摊薄：1080P 仅渲染 1.115 µs（均值）/ 1.257 µs（p99），静态提帧 1.428 / 1.533 µs；
720P 仅渲染 1.098 / 1.175 µs，静态提帧 1.429 / 1.512 µs。预算等价于 **0.1042 µs/实体**。

## 持续组的数字

| 实体 | 路径 | wall FPS（三进程） | 中位 p99 | 最小逐秒窗口 | 判定 |
| ---: | --- | --- | ---: | ---: | --- |
| 1800 | 仅渲染 | 383.77 / 387.81 / 426.37 | 3.211 ms | 374 / 370 / 418 | 通过 |
| 1800 | 静态提帧 | 314.85 / 342.57 / 338.04 | 3.725 ms | 303 / 336 / 312 | 通过（冻结未测） |
| 1300 | 仅渲染 | 498.15 / 500.24 / 573.98 | 2.568 ms | 485 / 485 / 559 | 通过（冻结未测） |
| 1300 | 静态提帧 | 435.78 / 418.10 / 411.17 | 3.071 ms | 402 / 398 / 383 | 通过 |

判据是 p99 ≤ 4.166667 ms **且** 每个完整 1 秒窗口 ≥ 240 帧。1300 仅渲染组的组内相对极差
15.16%（FPS）/ 15.04%（p99）越过 15% 屏，因此该组只用于支持通过/不通过，不取其 FPS 中位数
当速度读数；原因（第三个进程 20 秒内 11,480 帧 vs 9,964 / 10,005）未查清，见补充。

## 冻结侧

`frozen-1080p/measurements.csv` 是 108 行原始测量（UTF-8 BOM，第一列名为 `phase`），
`audit.json` 复核 107 个有效进程、150,171 个测量帧，`confirmed[]` 只有 1800（仅渲染）与
1300（静态提帧），并且 `formal_acceptance` 为 false。三个持续子目录各自保留
`registration.json` 与 `results.json`，所以 1800 / 1900 / 1500 / 1400 / 1300 的每条 p99
都可以从本附件重读，不需要回到原始目录。

冻结侧 20 秒进程的逐帧日志（每个 1.3–1.9 MB，共 15 个）与刷新侧同类日志不复制，
只在 `manifest.json` 的 `excluded[]` 里记录路径、字节数与 sha256；它们支持的逐秒窗口
与分位点已经在同目录的 `results.json` 里。

## 二进制绑定

四个刷新运行的 `registration.json` 记录同一个 `binary_sha256`：
`91ed0d303c09ef92e6abab9e30b63c87da77322d1dc7132c6b45204c220578b9`。
该可执行文件是构建产物，未复制进附件；`after-matrix-status.json` 记录了同一哈希，
`manifest.json` 的 `excluded[]` 以 `sha256`（登记值）与 `sha256_on_disk`（当前字节）
两个字段并列，因为之后的全量矩阵会用 `--clean-first` 重建同一路径。读数以登记值为准。

## 读取顺序

1. `fresh-tools/summarize.log.txt`：四组刷新的全部表格，先看这里。
2. `fresh/results.json.txt` 与 `fresh-720p/results.json.txt`：1080P 与 720P 的逐进程记录。
3. `fresh-1800/results.json.txt`、`fresh-1300/results.json.txt`：逐秒窗口与尾部。
4. `frozen-1080p/audit.json`、`measurements.csv`、三个持续子目录：冻结侧的全部读数。
5. `manifest.json`：每个复制文件的原始路径、字节数与 sha256（含源哈希与归档哈希）。
6. `identity-and-selection-supplement.json`：绑定、取舍、两代不混用的规则与未决项。

## 复现

刷新脚本默认拒绝脏工作区（`--allow-dirty` 可显式放行）：

```powershell
$env:PATH = 'D:\Cuexis-redesign\out\build\rpa-candidate-release\bin;' + $env:PATH
python -B fresh-tools/run_40k_refresh.py.txt --exe <probe.exe> --out <new-1080p-dir>
python -B fresh-tools/run_40k_refresh.py.txt --exe <probe.exe> --out <new-720p-dir> --width 1280 --height 720
python -B fresh-tools/run_40k_refresh.py.txt --exe <probe.exe> --out <new-1800-dir> --entities 1800 --seconds 20
python -B fresh-tools/run_40k_refresh.py.txt --exe <probe.exe> --out <new-1300-dir> --entities 1300 --seconds 20
python -B fresh-tools/summarize_verdict.py.txt --run1080 <new-1080p-dir> --run720 <new-720p-dir> `
    --sustain 1800:<new-1800-dir> 1300:<new-1300-dir>
```

重新构建探针时必须用同一个源码状态（`466ff76`）、同一 Release 配置和同一 CPU 集合，
并用 `registration.json` 里的 `binary_sha256` 核对是否是同一个可执行文件；哈希不同就意味着
构建环境不同，那次读数不能与本附件并列。

文本证据在归档时统一为 CRLF，使新检出得到相同字节；二进制按字节原样保存。两者在
`manifest.json` 里分别记录 `source_sha256` 与 `sha256`，`eol` 字段标明处理方式。
