# S7A-3 第二半 part 1：主控独立复验的带日期证据

状态：dated evidence record（主控对实现交付的**两次独立复跑**的带日期证据；**不是**实施证据、
**不是** owner acceptance、**不是** S7A-3 完成声明、**也不是** Stage 7A 完成声明）

更新日期：2026-10-03

上级文档：[Stage 7A 实施计划](../../../../stage_plans/active/stage-07/plan.md) ·
[S7A-3 第二半 part 1 实现裁定落地记录](../decisions/2026-10-03-s7a-3-implementation-rulings.md) ·
[Gameplay V2 acceptance package](../../../../proposals/gameplay-v2-acceptance/README.md) ·
[批次门禁](../../../../proposals/gameplay-v2-acceptance/BATCH_GATES.md)

本文登记主控（控制侧）对实现代理两次交付的**独立复跑**方法与实测数字：**不复用代理自述的任何数字**，
每条门禁均在本机重新执行并记原始退出码。本文只登记证据与方法学结论，**不复制** Spec / ABI 合同正文，
不构成实施、验收或阶段完成声明。**未在本文列出的门禁一律视为未运行。**
本文数字**不能替代**实现侧报告 [S7A-3 第二半 part 1 实现裁定落地记录](../decisions/2026-10-03-s7a-3-implementation-rulings.md)
的逐条语义处置。

> **表述纪律。** 复跑通过**只**说明"在被验提交上，这些门禁确实通过"。
> 它**不**说明状态预算门禁已完整、**不**说明包含性门禁已生效、
> **不**说明预算数值已冻结，也**不**表示 S7A-3 或 Stage 7A 完成（见 §6）。
> 两次复验的**原始输出**保留在控制侧的带日期文件
> `D:\Cuexis-worktree\.dsh\ai-work-linker\archive\verify-2026-10-03-141756.txt` 与
> `D:\Cuexis-worktree\.dsh\ai-work-linker\archive\verify-2026-10-03-144031.txt`（工具侧原始件，非本仓路径）；
> 本文是其正典摘录，两者不一致时以原始件为准。

---

## 1. 复验范围与方法

| 项 | 值 |
| --- | --- |
| 被验对象 ① | 实现代理「S7A-3 round-5 revision」的第 2 轮复审改正交付（复验时刻 2026-10-03 14:17–14:19） |
| 被验对象 ② | 同一被验面的第 3 轮改正交付（复验时刻 2026-10-03 14:40–14:42） |
| 复验者 | 控制侧主控；**不采信代理自述**，全部数字自跑 |
| 复验手段 | ① 一条命令跑十项门禁（控制侧脚本 `stage7a_verify.ps1`）；② 逐字节合规扫描（含 `git diff --check` 的盲区）；③ 相对里程碑快照（`archive\2026-10-03-stage7a-milestone-s7a3-part1\`）的逐文件 SHA256 越界比对；④ 定点用例按名复跑；⑤ 构建日志"真编译"取证 |
| 变更面判定基准 | 里程碑快照逐字副本 + `MANIFEST` 哈希表；快照可用 `git apply --check -R` 反向验证与树一致 |

**为什么必须自己复跑**：实现侧报告与代理消息都是**断言**；控制侧的职责是把断言变成**可复现的观测**。
两次复验中，代理自述与实测**逐项相符**（见 §3.5），但这条一致性本身也是被验证出来的结论，不是前提。

---

## 2. 第 2 轮复审改正的复验（14:17–14:19）

### 2.1 十项门禁

| # | 门禁 | 实测 |
| --- | --- | --- |
| 1 | `--clean-first` 构建（`cuexis_judgement` + `cuexis_judgement_tests`） | exit **0**；`: error` 0 / `错误` 0 / `: warning` 0 / `警告` 0 |
| 2 | `cuexis_judgement_tests.exe` | exit **0**；`All tests passed (3335 assertions in 104 test cases)`（基线 3277 / 102 ⇒ +58 / +2） |
| 3 | `ctest -R cuexis_architecture --no-tests=error` | exit **0**，1 / 1 Passed |
| 4 | `ctest -R cuexis_gameplay_diagnostics_codes --no-tests=error` | exit **0**，1 / 1 Passed |
| 5 | `cuexis_format_check` | exit **0**（另 clang-format 22.1.3 `--dry-run --Werror` 5 文件 exit 0） |
| 6 | `python -B tools/check_docs.py` | exit **0**；341 Markdown + 20 JSON/CXT |
| 7 | `git diff --check` | exit **0**（盲区见 §2.3） |
| 8 | 控制侧 31 条不变量 | **31 PASS / 0 FAIL**，exit 0 |
| 9 | 越界检查（逐文件 SHA256） | **恰好 7 个文件变化，零越界** |
| 10 | `git status --porcelain` | 41 条，**STAGED=0**（未 `git add`、未提交） |

### 2.2 构建"真编译"取证

为排除 `ninja: no work to do` 冒充通过：构建日志 35 行内 `no work to do` **0 次**，且含
`[1/29] Building CXX object engine\core\CMakeFiles\cuexis_core.dir\src\thread_checker.cpp.obj` …
`[27/29] Building CXX object tests\judgement\…\gameplay_pattern_compile_tests.cpp.obj`、
`[28/29] Linking CXX executable bin\cuexis_judgement_tests.exe`；测试可执行文件构建时刻落在本次运行区间内。

### 2.3 `git diff --check` 的盲区

本轮全部改动在 `git status --short` 中为 `??`（untracked），**git 不检查它们**
⇒ 该门禁对 untracked 改动**不构成**空白/行尾证据。控制侧逐字节扫描补齐：

| 文件 | bytes | CR | nonASCII | trailingWS | finalLF |
| --- | --- | --- | --- | --- | --- |
| `engine/judgement/src/gameplay_assembler.cpp` | 168848 | 0 | 0 | 0 | True |
| `engine/judgement/src/source_codes.hpp` | 37790 | 0 | 0 | 0 | True |
| `engine/judgement/src/pattern_dfa.hpp` | 45174 | 0 | 0 | 0 | True |
| `engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp` | 53787 | 0 | 0 | 0 | True |
| `tests/judgement/gameplay_pattern_compile_tests.cpp` | 61912 | 0 | 0 | 0 | True |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | 231535 | 0 | 132871 | 0 | True |
| `docs/stage_reports/stages/stage-07/2026-10-03-s7a-3-implementation-rulings.md` | 55720 | 0 | 32022 | 0 | True |

`engine/judgement/include/cuexis/judgement/*.hpp`（安装树公开头）**7 个全部 pure ASCII / non-ASCII = 0**，
符合 AGENTS.md 硬规则；两个 Markdown 的非 ASCII 为中文正文，符合预期。

### 2.4 变化面

相对里程碑快照 `…\2026-10-03-stage7a-milestone-s7a3-part1\untracked\` 恰好 7 个文件，与代理声明的 7 个完全重合：
`gameplay_assembler.hpp` 51669 → 53787、`gameplay_assembler.cpp` 162699 → 168848、
`pattern_dfa.hpp` 43131 → 45174、`source_codes.hpp` 38096 → 37790、
`gameplay_pattern_compile_tests.cpp` 48815 → 61912、`GAMEPLAY_V2_SPEC.md` 225801 → 231535、
`2026-10-03-s7a-3-implementation-rulings.md` 26827 → 55720。

`README.md` / `docs/api/**` / `docs/proposals/**` / `schemas/**` / `tools/**` / `CMakeLists.txt` /
`cmake/**` / `.gitignore` / `AGENTS.md` **零改动**，与派单禁改清单一致。

### 2.5 控制侧自身的维护者修订

实现侧报告指出控制侧此前写入索引行的符号名已失效。控制侧以代码命中数核实后更正（索引归控制侧维护，代理禁改）：
`maximumUnbounded` **0 命中**（旧名）、`maximumBoundUnavailable` **0 命中**（中间名）、
**`maximumLengthUnavailable` 12 命中**（现行名）⇒ 索引行写现行名，旧名只作历史标注。

---

## 3. 第 3 轮改正的复验（14:40–14:42）

### 3.1 十项门禁

| # | 门禁 | 实测 |
| --- | --- | --- |
| 1 | `--clean-first` 构建（含 `--clean-first`） | exit **0**；`: error` 0 / `错误` 0 / `: warning` 0 / `警告` 0 |
| 2 | `cuexis_judgement_tests.exe` | exit **0**；`All tests passed (3407 assertions in 105 test cases)`（相对第 2 轮 3335 / 104 ⇒ +72 / +1） |
| 3 | `ctest -R cuexis_architecture --no-tests=error` | exit **0**，1 / 1 Passed |
| 4 | `ctest -R cuexis_gameplay_diagnostics_codes --no-tests=error` | exit **0**，1 / 1 Passed |
| 5 | `cuexis_format_check` | exit **0** |
| 6 | `python -B tools/check_docs.py` | exit **0**；341 Markdown + 20 JSON/CXT |
| 7 | `git diff --check` | exit **0**（对 `??` 无鉴别力，见 §2.3） |
| 8 | 控制侧 31 条不变量 | **31 PASS / 0 FAIL**，exit 0 |
| 9 | 越界检查（逐文件 SHA256） | **恰好 7 个文件变化，零越界** |
| 10 | `git status --short` | 41 条，**STAGED=0**；`git diff --cached --name-only` **0** |

`HEAD = 449e864`、分支 `stage-7` ⇒ 改动**全部留在工作区、未提交**。

### 3.2 定点用例（按名复跑，不只信总数）

| 用例 | 实测 |
| --- | --- |
| 被改判：`…a state-count measurement gap refuses only a proven overrun` | exit 0；**50 断言 / 1 用例** |
| 新增：`…the state-count lower bound is the longest acceptable trace plus one` | exit 0；**35 断言 / 1 用例** |
| 新增：`…a count with no representable value is a gap, an overrun, or neither` | exit 0；**25 断言 / 1 用例** |
| 标签聚合 `[budget]` | exit 0；**255 断言 / 9 用例** |
| 全量再跑 | exit 0；**3407 断言 / 105 用例** |

### 3.3 逐字节扫描

| 文件 | bytes | CR | nonASCII | trailingWS | finalLF |
| --- | --- | --- | --- | --- | --- |
| `engine/judgement/src/gameplay_assembler.cpp` | 178141 | 0 | 0 | 0 | True |
| `engine/judgement/src/source_codes.hpp` | 38955 | 0 | 0 | 0 | True |
| `engine/judgement/src/pattern_dfa.hpp` | 46017 | 0 | 0 | 0 | True |
| `engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp` | 55119 | 0 | 0 | 0 | True |
| `tests/judgement/gameplay_pattern_compile_tests.cpp` | 71320 | 0 | 171 | 0 | True |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | 237279 | 0 | 137128 | 0 | True |
| `docs/stage_reports/stages/stage-07/2026-10-03-s7a-3-implementation-rulings.md` | 83037 | 0 | 47982 | 0 | True |

测试文件的 171 个非 ASCII 字节**全部**是被改判 / 新增用例中派单书要求写入的逐字中文改判说明（已读源码确认位置与内容）。
**7 个安装树公开头全部 non-ASCII = 0。**

### 3.4 行尾未被静默改写

四个文件的 CR 计数在快照与现行**均为 0**，LF 计数按新增内容成比例增长
（`GAMEPLAY_V2_SPEC.md` 2415 → 2508、实现裁定记录 302 → 950、测试 854 → 1207、`gameplay_assembler.cpp` 3218 → 3425）
⇒ 没有发生"全文件重写"式的行尾风格变更。

### 3.5 与代理自述的一致性

代理自述的**每一项数字**（构建 exit 0 与四个错误/警告计数全 0；3407 / 105；两道 CTest 1 / 1；
format exit 0；`check_docs.py` 341 / 20；`git diff --check` exit 0；逐字节四项；7 个公开头全 ASCII；
41 条 STAGED=0；7 个改动文件）**与实测逐项相符，无一处偏差**。其主动报备的
"format 首跑 exit 1（114 处违规）→ 修复后复跑 0"亦与控制侧在 14:32 独立观察到的
`gameplay_assembler.cpp:2194-2195` 违规互相印证。

### 3.6 静止性判定的假阳性

复验脚本的"静止守卫"报 `WARN: not quiescent`（两个会话转写文件的 mtime 触发）。控制侧判定为**假阳性**并给出判据：
① 其中一个"正在写"的会话恰是刚发出收尾消息的那一刻；② 另一个**不在**本工作区会话目录内（属控制侧自身会话）；
③ 复验**前后构建进程数均为 0**；④ 构建目录最新文件就是本次复验产生的 `.ninja_log`。⇒ 无并发 ninja，clean-first 结论有效。
**方法学教训**：静止性判据应取"构建进程数 + 构建目录最新文件"，**不要**只看会话转写的 mtime。

---

## 4. 方法学结论（两次复验共同得出，可复用）

1. **`git diff --check` 对 untracked 改动没有鉴别力**。本阶段改动多为 `??`，必须以**逐字节扫描**（CR / nonASCII / trailingWS / finalLF）补齐该项证据。
2. **`--clean-first` 是硬性要求**，不是保守：本机曾记录 MSVC 头依赖扫描缺陷（改私有头不重编 TU），`ninja: no work to do` **永不**作健康证据。
3. **构建过程要留"真编译"取证**（编译/链接步骤行 + 测试可执行文件构建时刻），否则"exit 0"可能来自空操作。
4. **按名复跑单个用例时必须避开逗号**：Catch2 的名字过滤器以逗号分隔多个 spec，含 `", "` 的用例名直接传参会 `exit=2` 并打印 `No test cases matched`，**这不是测试失败**；应改用 `*子串*` 通配符，并与全量跑交叉印证。
5. **越界判定要用带哈希的快照**：只数"改了几个文件"不足以证明未越界，需逐文件 SHA256 与声明的变更面逐项对齐。

---

## 5. 与实现侧报告的关系

| 角色 | 文档 | 负责 |
| --- | --- | --- |
| 实现 | [S7A-3 第二半 part 1 实现裁定落地记录](../decisions/2026-10-03-s7a-3-implementation-rulings.md) | 逐条语义处置、每轮裁定的落地位置与理由 |
| 复验 | 本文 | 在被验提交上**重新执行**门禁并记原始退出码、越界与逐字节证据 |

两者**不是互相替代**：实现侧报告负责"为什么这样实现"，本文负责"这些门禁在被验提交上确实通过"。

---

## 6. 本文**不**主张的事

- **不**主张 S7A-3 完成、**不**主张 Stage 7A 完成。
- **不**主张状态预算门禁已完整：当计数缺席且**声明推不出**超过已接受上限的下界时，该维度仍不 enforced、门禁仍 **incomplete**。
- **不**主张包含性门禁已生效：复验时 `checkPatternContainment` **未被装配路径消费**（全仓 `PatternArmBound` 无构造点），**以真实容量调用门禁并验证原子失败仍是进入 S7A-4 的硬前置**。
- **不**主张预算数值已冻结（S1-05 / P1-10：S7A-9 前不冻结限额）。
- **不**主张语义已被独立评审：本文只做**复跑与结构核对**，语义正确性依据实现侧报告与其裁定来源。
- 第 3 轮评审卡自身的两点局限（新建会话而非续用、未独立读工作区）**必须与结论同引**。
- 本批次相对基线 **净增 0 / 净删 2** 个 `judgement.s7a3.*` 字面量（控制侧独立复算：集合 61 → 59）。

---

## 7. 本次复验**未**覆盖的范围

- **未**跑 hosted CI（Linux Quality / Windows MSVC / Windows MinGW）、**未**跑 MSYS2、**未**跑干净 HEAD 的 Release、**未**跑 sanitizer / coverage 预设（后两者在本机 MSVC 上本就不可用）。
- **未**验证装配消费点、**未**验证 `gate_incomplete` 的原子拒绝路径（尚未实现）。
- **未**做性能 / 内存 / 并发行为评估。
- **未**审阅未列入 §2.4 / §3.3 的任何文件。
