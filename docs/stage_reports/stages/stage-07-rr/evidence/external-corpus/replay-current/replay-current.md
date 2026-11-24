# 当前实现重放：3943fb13 对两套历史设备包的判定语料

状态：evidence
日期：2026-11-24

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.external-legacy-corpus-replay-current
status: evidence
authority: none
date: 2026-11-24
-->
结论分级只用“成立 / 部分成立 / 不成立 / 无法核实”。本轮未改生产源码，未 `git add` / `commit`，只写 `replay-current/`。

## 判定

**旧语料在当前实现下的聚合判定与 replay 自洽断言复现了历史期望：成立。逐音符判定轨迹与 replay 字节层面的一致性：无法核实。**

R4-P7-O2 的 `keyboard-dense-v5` 三例在当前实现（`3943fb13`，Debug，SDK 0.9.1）下输出 778/387/0、-387/0/387、768/385/2，`replay=equal`；三行顺序拼接后 166 字节，SHA-256 `3c6723af…901c`，与包内 `verification.log` 逐字节相同，也与 `replay-old/` 记录的 d195 旧实现输出相同。这组结果通过工具内的分数期望比较与 replay `sameResult` 检查，退出码 0。

PD3c 的 `keyboard` 与 `keyboard-release` 是同一课程的两个入口，七个内容文件逐字节相同。三例在当前实现下输出 46/21/0、-21/0/21、36/19/2，`replay=equal`；把 Windows 文本模式的 CRLF 归一后为 157 字节，SHA-256 `a124c5fd…51ab`，与包内 `verification.json` 的 `logicalCases` 逐字节相同。补充用 d195 旧实现跑同一输入，stdout 与当前实现逐字节相同。

原样完整用例文件的一次运行在当前工具上仍然不成立：R4 的 `verification-cases.json` 为 160,354 字节，超过工具内 131,072 字节（128 KiB）的 `json::parse` 预算，退出码 2，stderr 为 `json.parse.size_limit: JSON input exceeds the configured byte limit`。这与 `replay-old/` 记录的原样入口失败同码同因，因此本报告的成立结论只覆盖拆成单例的重放路径；完整文件路径记为不成立，不计为重放成功。

聚合值相同只能说明三组用例的总分、命中与失误数未变。包内没有逐音符或逐 Fact 的期望，也没有 replay 字节，`replay=equal` 只证明同一构建内“归档一次会话→重放求值”得到相同结果。判定窗口、逐音符归属与跨实现 replay 字节是否完全一致，无法核实。

## 1. 环境与来源

| 项目 | 值 |
| --- | --- |
| 源码提交 | `3943fb1365853b6f5af5c8a41ef322d147863ee6`（2026-11-24 00:53:59 +0800） |
| 工作树状态 | `git status --porcelain` 为空 |
| 构建目录 | `out\build\candidate-headless-debug`（preset `candidate-headless-debug`） |
| 构建类型 | Debug；`CUEXIS_ENABLE_CHART_V5_CANDIDATE=ON`、`CUEXIS_BUILD_DEVELOPER_TOOLS=ON`、`CUEXIS_VERSION_SUFFIX=exp.candidate` |
| CMake / Ninja | 4.3.3 / 1.13.2 |
| MSVC | 19.51.36256（x64） |
| Python | 3.14.6 |
| 构建目标 | `cuexis_gameplay_assemble`（`bin\cuexis_gameplay_assemble.exe`，20,219,904 B，mtime 2026-11-24 00:57:39） |
| 重放二进制 | `compiler\compiler.exe`，20,009,984 B，SHA-256 `446dee548f1da34f38e7b38bd21ba20bbd9c74be0313ce267f2d91e08901bac1` |
| 旧实现二进制 | `E:\Cuexis-replay-d195\out\build\replay\entity-practice\compiler.exe`，20,003,328 B，SHA-256 `b65d5303…e1d8`，worktree HEAD `d1951326b0f2683801fa5fe168f09c8dac43f957`，干净 |

入口是 `tools/practice/compile_entity_practice.cpp`（工作证据：tools/practice/compile_entity_practice.cpp） 的 `--verify-gameplay` 分支。该文件在 `d1951326` 与当前 HEAD 是同一 blob（`35af0a9126afbf196bc50fc6f8fe1f84453632bc`），所以两侧的驱动代码相同，重放差异只可能来自链接的引擎库。`git rev-list --count d1951326..HEAD` 为 65；`engine/judgement` 与 `engine/gameplay` 无文件差异，`engine/playback` 有 11 个文件变动（+204/−114，准备路径拆分），`engine/chart` 与 `engine/cxc` 有读取与链接改动。本轮实际检验的是当前播放/准备/装配链路加载旧成品后的判定输出。

工具内的判定顺序：读取配置、packed 与用例；按 tick 投递输入并在 `end` 推进；查询 `score/hits/misses` 并与用例 JSON 内嵌期望比较；归档会话、求值 replay 并比较 `sameResult`；打印 `name: score=… hits=… misses=… replay=equal`。退出码 0 表示两项比较都通过。

## 2. R4-P7-O2 `keyboard-dense-v5`

四个输入与包根 `binding.json` 的 `keyboard-dense-v5` 记录逐项一致：

| 文件 | 字节 | SHA-256 | binding 一致 |
| --- | ---: | --- | --- |
| `practice.cxc` | 10,294,067 | `366ebff81233056c39bc9940b23d8a21e5a5d330ade5ec9bc02de77b6db86dd1` | 是 |
| `gameplay.packed` | 309,961 | `acfd31eef366f5cc9aa6e4d3559cb2ec6cc981d4f35f43840f2f8e886a671f16` | 是 |
| `gameplay-configuration.json` | 30,413 | `03cf0745b2651502b5e5b613ae45e352a3a549a91cedeeea0ab9033f4de146e3` | 是 |
| `verification-cases.json` | 160,354 | `768cff0e3ec023bb52b19f9c9fc9d9184be24256230a522b0c18dee807b890e4` | 是 |
| `verification.log` | 166 | `3c6723af9c8fadd292ba1ac508beb6b5ac504aa425ec9413a78f035c97dd901c` | 是 |

包内期望：

```text
all-hit: score=778 hits=387 misses=0 replay=equal
no-input: score=-387 hits=0 misses=387 replay=equal
early-hold-release: score=768 hits=385 misses=2 replay=equal
```

单例文件 `cases/all-hit.json`、`cases/no-input.json`、`cases/early-hold-release.json` 分别为 46,948、94、46,959 字节，各含一个用例。脚本 `scripts/check_inputs.py`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/scripts/check_inputs.py） 用 Python 对象比较确认，三者分别与包内完整文件对应条目相等（770、0、770 条输入，horizon 45,000,000）；文件体积与 `replay-old/` 的拆分文件不同，只是 JSON 排版差异。

| 用例 | 退出码 | 耗时 | stderr | stdout |
| --- | ---: | ---: | --- | --- |
| all-hit | 0 | 20.24 s | 空 | `all-hit: score=778 hits=387 misses=0 replay=equal` |
| no-input | 0 | 7.62 s | 空 | `no-input: score=-387 hits=0 misses=387 replay=equal` |
| early-hold-release | 0 | 20.18 s | 空 | `early-hold-release: score=768 hits=385 misses=2 replay=equal` |

三份 stdout 按包内顺序拼接为 166 字节，SHA-256 `3c6723af9c8fadd292ba1ac508beb6b5ac504aa425ec9413a78f035c97dd901c`，与 `verification.log` 的字节与哈希都相同；`replay-old/outputs/split-*.stdout.txt` 拼接后也是同一哈希。完整记录见 `outputs/hash-check.json`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/hash-check.json）。

完整用例文件一次运行：

| 项目 | 值 |
| --- | --- |
| 输入 | `verification-cases.json`，160,354 字节 |
| 退出码 | 2 |
| stdout | 空 |
| stderr | `json.parse.size_limit: JSON input exceeds the configured byte limit`（69 字节） |
| 耗时 | 1.06 s |
| 工具预算 | `json::parse(read(argv[5]), {131072, 32, 8192})`，即 128 KiB |
| 旧实现同路径 | `replay-old/outputs/direct-fullfile.*`：exit 2，同一条错误 |

`replay-old` 用放宽预算的本地驱动在 43.72 s 内一次跑完同一文件并复现三行；本轮没有复制该驱动，也没有改当前工具预算。按任务口径，这条路径不计为当前实现的重放成功。

## 3. Cuexis-PD3c `keyboard` / `keyboard-release`

两个入口各含 7 个课程数据文件与 2 个设置文件。`course.json`、`entity-index.json`、`practice.cxc`、`gameplay.packed`、`gameplay-configuration.json`、`verification-cases.json`、`verification.json` 在两目录间逐字节相同，属同一课程的两个启动入口，不构成两份独立语料。包内没有 `verification.log`，历史期望写在 `verification.json` 的 `logicalCases` 字段。

| 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| `practice.cxc` | 4,085,436 | `4d4a6bfd70bd288f3a6bc9a31fdac912b8d88c200847ee6a7eabcea705fb0bd3` |
| `gameplay.packed` | 21,633 | `928b2730bb15d972ef8bd579fddde7d98dda8a867a2de46c7abcfeab479e76d4` |
| `gameplay-configuration.json` | 33,221 | `96a1e75a69d8a8f43e3da08da35390bb96180784fec343a920cc481d48f506d9` |
| `verification-cases.json` | 8,284 | `e068e61481a4eb24c8af083f76cacd704ef0045b2206cc00c8f2e699f0d49874` |
| `verification.json` | 850 | `997c097297e00f4f91a2dff62a78a0cc29649302ad60ef067a831b06fbf1529b` |

这些哈希与包根 `binding.json` 的 `keyboard/*` 项一致。`verification.json` 记录 `phases: 21`、`testOnly: true`、`physicalKeyboardAccepted: false`，`logicalCases` 为：

```text
all-hit: score=46 hits=21 misses=0 replay=equal
no-input: score=-21 hits=0 misses=21 replay=equal
early-hold-release: score=36 hits=19 misses=2 replay=equal
```

用例文件 8,284 字节，未触及 128 KiB 上限，因此直接运行包内完整文件；三例的输入条数为 38、0、38，horizon 35,000,000。当前实现在两个入口上各跑一次：

| 入口 | 退出码 | 耗时 | stderr | stdout |
| --- | ---: | ---: | --- | --- |
| `keyboard` | 0 | 2.67 s | 空 | 三行，见下 |
| `keyboard-release` | 0 | 2.55 s | 空 | 与 `keyboard` 逐字节相同 |

```text
all-hit: score=46 hits=21 misses=0 replay=equal
no-input: score=-21 hits=0 misses=21 replay=equal
early-hold-release: score=36 hits=19 misses=2 replay=equal
```

捕获文件为 160 字节，行尾是 Windows 文本模式写出的 CRLF；`logicalCases` 在 JSON 中存的是 LF。按行归一后为 157 字节，SHA-256 `a124c5fdf4724c95b559b5f34fb6b469fea7fb4ae291302e8482b41f905751ab`，与 `logicalCases` 的 157 字节及哈希都相同。`replay-old/` 未包含 PD3c，本轮补充用 d195 旧实现对 `keyboard` 跑同一命令：

| 实现 | 退出码 | 耗时 | stdout |
| --- | ---: | ---: | --- |
| 当前 `3943fb13` | 0 | 2.67 s | 三行，160 字节 CRLF，SHA-256 `3fa393e6…70dd` |
| 旧 `d1951326` | 0 | 2.64 s | 与当前逐字节相同 |

PD3c 历史期望来自 `d3440882`（SDK 0.9.0，构建 26.10.19-1）加上 81 项未提交输入生成的二进制；该二进制已丢失。当前实现与 d195 都复现了它的聚合期望，但两者都不是原始二进制。

## 4. 逐例对照

表中“旧实现输出”一列：R4 取自 `replay-old/report.md`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-old/report.md） 记录的 d195 结果，本轮复算其 stdout 拼接哈希一致；PD3c 取自本轮补充的 d195 运行。`replay` 均为 `equal`。

| 包 / 用例 | 当前实现（3943fb13） | 历史期望 | 旧实现输出（d1951326） | 当前退出码 | 判定 |
| --- | --- | --- | --- | ---: | --- |
| R4 dense all-hit | score=778 hits=387 misses=0 replay=equal | 同左（`verification.log`） | 同左 | 0 | 成立 |
| R4 dense no-input | score=-387 hits=0 misses=387 replay=equal | 同左 | 同左 | 0 | 成立 |
| R4 dense early-hold-release | score=768 hits=385 misses=2 replay=equal | 同左 | 同左 | 0 | 成立 |
| PD3c all-hit | score=46 hits=21 misses=0 replay=equal | 同左（`verification.json.logicalCases`） | 同左 | 0 | 成立 |
| PD3c no-input | score=-21 hits=0 misses=21 replay=equal | 同左 | 同左 | 0 | 成立 |
| PD3c early-hold-release | score=36 hits=19 misses=2 replay=equal | 同左 | 同左 | 0 | 成立 |
| R4 dense 完整用例文件一次运行 | 无法运行（exit 2，`json.parse.size_limit`） | 三行期望同上 | exit 2，同一条错误 | 2 | 不成立 |

## 5. 差异逐项

1. R4 与 PD3c 的六个单例结果在 score、hits、misses 与 `replay=equal` 上都没有差异；R4 输出与历史日志逐字节相同，PD3c 输出与历史 `logicalCases` 在换行归一后逐字节相同。
2. 完整 R4 用例文件在当前工具上仍然失败，错误码与旧入口相同（`json.parse.size_limit`，exit 2）。这是工具解析预算的限制，不是判定结果差异；160,354 字节超过 128 KiB。
3. 当前输出与历史文本的换行形式不同：程序经 Windows 文本模式写出 CRLF；R4 的 `verification.log` 本身是 CRLF，因此逐字节相同；PD3c 的 `logicalCases` 存在 JSON 字符串里，是 LF，需要归一后比较。内容逐行相同。
4. 拆分用例文件体积与 `replay-old/` 不同（当前 46,948 / 94 / 46,959 字节，旧记录 62,371 / 113 / 62,382 字节），属于 JSON 排版差异；脚本确认当前拆分对象与包内完整文件条目逐个字段相等。
5. 实现侧有 65 个提交的差异，集中在 `engine/playback` 的准备路径与 chart/cxc 读取；`engine/judgement` 与 `engine/gameplay` 无差异。聚合输出未观察到变化，逐音符轨迹是否相同没有数据可判。
6. PD3c 的 `keyboard` 与 `keyboard-release` 七个内容文件相同，两次运行不构成独立重复；真正独立的对照是“当前实现 vs d195 补充运行”。
7. PD3c 的历史期望由 `d3440882` 构建产生，旧实现对照用 `d1951326`，当前用 `3943fb13`。三者不是同一二进制，只能合成聚合层对照。

## 6. 分项判定

| 断言 | 判定 | 依据 |
| --- | --- | --- |
| 当前实现（3943fb13）单例重放 R4 dense v5 三例，复现 `verification.log` | 成立 | 六个字段逐一相同；拼接 stdout 166 B，哈希与日志相同 |
| 当前实现重放 PD3c `keyboard` / `keyboard-release` 三例，复现 `verification.json.logicalCases` | 成立 | 两入口 exit 0；归一后 157 B 哈希相同 |
| 当前实现与 d195 旧实现在这批语料上的聚合输出一致 | 成立 | R4 与 replay-old 记录相同；PD3c stdout 逐字节相同 |
| 当前工具用原样完整 R4 用例文件一次跑完三例 | 不成立 | 160,354 B > 131,072 B 预算；exit 2，`json.parse.size_limit` |
| 当前实现单例重放路径可用（拆分不改变用例内容） | 成立 | 拆分对象与包内条目相等；六例 exit 0 |
| 逐音符判定窗口与 Fact 归属在两版实现间完全一致 | 无法核实 | 包内无逐音符/逐 Fact 期望与事件轨迹 |
| replay 字节跨实现一致 | 无法核实 | 包内无 replay 字节；`replay=equal` 只是同会话归档→求值自洽 |
| 设备输入与实体运行可复现 | 无法核实 | 本轮只跑离线合成输入；包内无设备时间戳/采样 |

## 7. 限制

- 构建是 Debug，历史设备 Player 是 Release。判定与 replay 比较是整数逻辑，但两者不是同一构建配置。
- 只有聚合期望。无法据此判断命中窗口、连击、逐 Fact 结算与事件顺序是否完全一致。
- `replay=equal` 由当前会话自己归档再求值，验证的是同一构建内的 replay 路径，不验证历史 replay 字节。
- 当前工具仍保留 128 KiB 用例解析上限；本轮没有修改它，也没有放宽预算重跑。完整文件路径维持“不成立”。
- 两包的历史期望来自公式推导加同库离线 helper 校验，不是外部人工 golden；这一点沿用 `T-01 报告`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/report.md） 与 `reproducibility.md`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/reproducibility.md） 的清点。
- PD3c 原始构建二进制（`d3440882` + 81 项未提交输入）不可得；d195 与当前实现都不是原始二进制。
- 未运行任何设备包启动器，未修改桌面目录；两包只读。
- 未修改生产源码、未提交；工作树保持干净。

## 8. 证据文件

| 路径 | 内容 |
| --- | --- |
| `commands.log`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/commands.log） | 本轮全部命令、退出码与耗时 |
| `outputs/build-assemble.log`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/build-assemble.log） | 第二次 assemble 调用：`ninja: no work to do`，exit 0 |
| `outputs/build-assemble-first-failure.log`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/build-assemble-first-failure.log） | 更早一次调用：unknown target |
| `outputs/build-compiler.log`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/build-compiler.log） | helper 构建输出（compiler.exe 路径） |
| `compiler/build.cmd`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/compiler/build.cmd）、`compiler/build.log`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/compiler/build.log） | 借用链接命令的脚本与编译输出 |
| `outputs/r4-all-hit.stdout.txt`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/r4-all-hit.stdout.txt） 等三对 | R4 单例 stdout/stderr |
| `outputs/r4-full-cases.stdout.txt`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/r4-full-cases.stdout.txt）、`outputs/r4-full-cases.stderr.txt`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/r4-full-cases.stderr.txt） | 完整文件失败输出 |
| `outputs/pd3c-keyboard.stdout.txt`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/pd3c-keyboard.stdout.txt） 等 | PD3c 两入口的 stdout/stderr |
| `outputs/pd3c-keyboard.old-d195.stdout.txt`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/pd3c-keyboard.old-d195.stdout.txt）、`outputs/pd3c-keyboard.old-d195.stderr.txt`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/pd3c-keyboard.old-d195.stderr.txt） | d195 补充对照 |
| `cases/`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/cases） | R4 三例拆分为单元素 JSON |
| `scripts/check_inputs.py`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/scripts/check_inputs.py） | 只读输入/哈希/输出比对脚本 |
| `outputs/hash-check.json`（工作证据：out/review/stage-07-rr/T-01/legacy-corpus-external/replay-current/outputs/hash-check.json） | 全部输入、输出与二进制的字节数与 SHA-256，以及相等性检查结果 |

构建遗留的 `compiler.obj`、`compiler.pdb`、`compiler.ilk`、`link.pdb` 已清理；保留 `compiler.exe`、运行所需的 `nlohmann_json_schema_validator.dll` 与构建日志。
