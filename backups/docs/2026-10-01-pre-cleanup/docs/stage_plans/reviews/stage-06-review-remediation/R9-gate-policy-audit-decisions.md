# R9 门禁策略审计 — 裁定与实施依据

本文件是 R9 批次**门禁策略缺陷链**的裁定记录与实施依据。它记录"做什么、为什么、以及什么证据能推翻结论"，
使实施与后续评审不必重新推导。背景与现象见
`docs/stage_reports/stages/stage-06/2026-09-28-r9-reference-host-command-loop.md` §2.1。

**状态**：裁定完成，**已实施**。基准修订 `f0837aa`。

订正 2026-09-29：本行原写"待实施"。本文件 §4 的审计项已由
[R9-gate-policy-impact-table.md](R9-gate-policy-impact-table.md) 逐项作答并落表（该表自述 closing the
audit items left open by section 4 of this file，即 D11/D14/D15），策略基线与 CI 固定亦已在
`354c801`、`99de5ae` 落地。

---

## 1. 已确证的事实

### 1.1 三处缺陷与 runner 实际报错

| # | 位置（修复前） | 代码 | runner 报错 |
|---|---|---|---|
| 1 | `VerifyReferenceHostCommands.cmake:105-109` | `string(REPLACE "\|" ";")` + `list(LENGTH)` + `if(NOT field_count EQUAL 3)` | `:108 (message): Malformed case declaration 'c01-absolute-anchor\|cmd\|'` |
| 2 | 同文件 `:299-302` | `list(GET fields 2 case_flags)` | `:302 (list): list index: 2 out of range (-2, 1)` |
| 3 | 同文件 `:463` | `if(NOT directive IN_LIST cuexis_command_known_directives)` | `:463 (if): ... "NOT" "directive" "IN_LIST" ... Unknown arguments specified` |

缺陷 2 被缺陷 1 的 `FATAL_ERROR` 遮蔽；缺陷 3 在更后面的 `.expect` 解析阶段，只有前两者修完才可达。

### 1.2 执行上下文

- 两个脚本作为 **CMake 脚本模式（`cmake -P`）** 执行：`VerifyReferenceHost.cmake` 是入口，
  `include()` 了 `VerifyReferenceHostCommands.cmake`。
- 两者**都从不调用 `cmake_minimum_required()` 或 `cmake_policy()`**，因此**脚本模式下的策略基线从未建立**。
- `.github/workflows/linux-quality.yml` 用 `runs-on: ubuntu-latest` 且**从不安装 CMake**
  （`apt-get install` 装的是 `ninja-build clang autoconf ...`）；CMake 来自 **runner 镜像**。
- Linux 日志中的 `Downloading .../v4.4.0/cmake-4.4.0-linux-x86_64.tar.gz` 是
  **vcpkg 为自己下载的私有副本**（其上一行是 `-- Running vcpkg install` /
  `A suitable version of cmake was not found (required v4.4.0).`），**不是**执行门禁的 CMake。

### 1.3 版本对照（Linux 上实测）

测试工具：`out/tools/cmake-3.28.3-linux-x86_64/`、`out/tools/cmake-4.4.0-linux-x86_64/`
（官方二进制，手工下载）。探针在 `out/cmake3-probe/`，**均未声明策略**。

| 探针 | 4.4.0 | 3.28.3 |
|---|---|---|
| 两分隔符声明的 `list(LENGTH)` | 3 | **2** |
| `list(GET fields 2 f)` | 正常 | **`list index: 2 out of range (-2, 1)`** |
| `if(NOT directive IN_LIST known)` | accepted | **`Unknown arguments specified`** |
| 3.28.3 的提示 | — | `Policy CMP0007 is not set` / `Policy CMP0057 is not set` |

同机 3.28.3 上，探针**加** `cmake_minimum_required(VERSION 3.25)` 后：
`list(LENGTH)=3`、`list(GET 2)=[]`、`IN_LIST accepted` —— **三处缺陷全部不再发生**。

**反例判据**：若在某 CMake 版本上、脚本模式下、未声明策略时，
`list(LENGTH "c01;cmd;")` 得到 3 且 `IN_LIST` 被识别，则"策略基线缺失是本链根因"不成立。

### 1.4 仓库既有模式

```
VerifyCfuF3Determinism / CfuF4Performance / ChartCapacity /
ExternalConsumer / S4GPerformance / S5HPerformance
    → cmake_minimum_required(VERSION 3.25)
VerifyReferenceHost / VerifyReferenceHostCommands   → 无（本批次新增）
VerifyPlayerDistribution                            → 无（既有）
```

---

## 2. 裁定结论

### D1 — 共同根因，分两类记录

**未建立脚本策略基线**是共同的环境根因；**故障机制分两类记录**：
`CMP0007`（`list()` 空元素被丢弃，缺陷 1、2）与 `CMP0057`（`IN_LIST` 不被识别，缺陷 3）。
保留共同的预防点，同时给排查者两个不同的症状入口。

### D2 — 采用方案 (c)：声明策略 **且** 保留与策略无关的构造

在**入口脚本**加 `cmake_minimum_required(VERSION 3.25)`（对齐六个同级脚本），
**同时保留**三处与策略无关的构造（分隔符计数、模式提取、`list(FIND)`）。
理由：声明策略治根因；与策略无关的构造在"被别处以不同策略上下文 `include()`"时仍然稳。
`CMakeLists.txt` 的调用**不能替代**——CTest 启动的是新的 `cmake -P` 进程。

**被 `include()` 的 `VerifyReferenceHostCommands.cmake` 不重复声明策略**，依赖入口契约。

### D3 — 下界保持 3.25，本次不改 `CMakeLists.txt`

`--no-tests=error` **在 CTest 3.25 文档中已存在**（3.26 新增的是环境变量 `CTEST_NO_TESTS_ACTION`），
故 workflow 的 ctest 用法与宣称的 3.25 下界**不冲突**。
后续审计项记为"完整构建、预设及 CI 命令是否符合宣称的 3.25 下界"，而非"需升级到 3.26"。

### D4 — CI 固定工具版本

- 固定**官方二进制 + 校验和**，把同一目录的 `cmake` 与 `ctest` 放入后续步骤 `PATH`；
  配置与测试前**打印两者路径与版本**。不引入第三方安装 action。
- **故障复现版本**固定 **3.28.3**；**完整矩阵**使用固定的 3.28.3 与固定的 **4.x**。
- **下界作业**固定 **3.25.x**，先只跑**独立脚本/解析器测试**，是控制本批范围的合理选择。
- 报告须明确：**这不等于完整项目已验证支持 3.25。**
- 不得在 CI 记录版本之前，称某版本为"当时 runner 的实际版本"。

### D5 — 让成功账目在托管日志中直接可审计

在现有测试步骤中，普通套件用 `-E '^cuexis_reference_host_staging$'`，
随后（同一步或下一步）用 `-V -R '^cuexis_reference_host_staging$'` **只运行它一次**。
保留 CTest 的退出码、超时与测试属性，并把账目直接写进日志；代价是两次 CTest 启动，
需核对 `-E` 排除后**仍覆盖全套**。

**不使用 `PASS_REGULAR_EXPRESSION` 作为成功判据**：其匹配时进程退出码被忽略，
且不会让非 verbose 的成功 stdout 出现在托管日志中。

### D6 — `;` 校验前移（并改正我此前的错误理由）

**已证伪**：我原注释称"含 `;` 的值会被当作多个实参传给 `list(FIND)`"是**错的**——
实测 `list(FIND known "a;b" idx)` → `-1`，带引号的 `;` 仍是**一个**实参。

改为：在 `\;` 还原、`string(STRIP)`、空行/注释判断**之后**，
`separate_arguments()` **之前**，对整条 `line` 做**锚定匹配**：

```
^(<允许指令>|... )($|[ \t])
```

效果：`require;bad`、`"require"`、`req\ uire` **都不再被当作合法指令**；
行首空白已由 `STRIP` 消除，多空白由边界允许。之后**仍保留** `separate_arguments` 与参数数量检查。
要点是**明确禁止首 token 的引号/转义别名**，而不是用正则复刻 shell 分词
（`UNIX_COMMAND` 尊重引号并让反斜杠转义下一字符，仅按空白切首词会有歧义）。

### D7 — 允许指令集合保持单一事实来源

锚定正则**由 `cuexis_command_known_directives` 生成**，但生成前必须验证：
集合非空、各项非空、**无重复**、每项匹配严格指令名字符规则 `^[a-z][a-z0-9-]*$`；
随后 `list(JOIN ... "|" ...)` 并加首尾与空白边界。字符限制同时避免正则元字符注入。
独立测试**保留自己的允许集合**，新增指令时要求明确更新测试。

### D8 — 抽取声明为纯数据文件

把 41 条声明从脚本内抽为**逐行数据文件**，每条仍为 `<id>|<kind>|<flags>`，
明确字符约束与注释规则。生产门禁读取后仍形成原来的 `cuexis_command_cases`，
现有"声明集合 vs 文件系统"检查**保持原逻辑**；独立驱动读取**同一数据**但用**自己的解析算法**。
**这是数据共享，不是判据共享。**

### D9 — 测试独立性采用两层

1. **手写独立样本与预期**，覆盖四种声明形状与各合法 kind（含空 flags、缺第三段、多分隔符、
   flags 原样保留、含 `;` 的输入）。
2. **独立测试驱动**读取 D8 的纯数据清单，用**不同于生产解析器**的算法检查全部真实条目。

新增 kind 时，测试的允许 kind 集合**故意失败**，要求修改者明确更新语法与样本。

### D10 — 验证器测试放在门禁之外

抽出**无宿主依赖**的"声明解析／期望行解析"逻辑，另建**独立 `cmake -P` 脚本与独立 CTest**。
测试向量与预期值写在测试驱动里。目的是避免"生产门禁用自己的成功计数证明自己的解析器正确"。
保留现有 41 例端到端。**形式意义上的完全独立不可得**，此为其可行近似。

### D11 — 空元素审计：检测器与它自身的顺序依赖

`list(FIND some_list "" empty_index) EQUAL -1` 是审计手段，但**只有在 `CMP0007=NEW` 时有效**。
已实测（3.28.3，列表 `"a;;b"` 确实含空元素）：

```
未声明策略 : LENGTH=2   FIND(empty)=-1     ← 断言"通过"，但列表是脏的
声明策略后 : LENGTH=3   FIND(empty)=1      ← 才检出
```

**审计不必先改变整个入口策略**：在每个待审计调用点**之前**取得原始列表值，
检测器内部用 `cmake_policy(PUSH)` → `cmake_policy(SET CMP0007 NEW)` → 检测 → `cmake_policy(POP)`，
**只改变检测所需的策略上下文**；命中项分别记录该调用在 OLD/NEW 下的结果或失败差异，
OLD 对照限定在支持旧行为的 3.x 上执行。

**两条必须写明的局限**：

- **检出空元素 ≠ 已证明行为有变化**；
- **未检出 ≠ 上游没丢过空元素**——因此审计检查点必须放在**第一次可能有损的读取之前**，
  不能只检查下游结果。

顺序依赖由三处分担：**入口注释**声明必须先建立策略基线；**策略影响表**记录检测器要求 `CMP0007=NEW`；
**独立测试**用 `"a;;b"` 确认检测器确实拒绝，并测试在旧策略调用者下局部隔离仍有效。

**不设"所有列表不得含空元素"的全局规则**——合法空 flags、空行等仍按各自语法处理。

### D12 — 生产路径只保留真正的合同断言

对 case ID、已验证的数字字段等"**空元素必定非法**"的边界，生产门禁应**直接失败**；
临时用于统计全部调用点的审计探针**不必永久留下**。独立测试覆盖断言及其反例。
对**合法空值**，不以"消除策略差异"为由禁止。

### D13 — 既有 Player 门禁单列，不混入本批

`VerifyPlayerDistribution.cmake` 同样缺策略基线，值得建**独立的后续审计项**；
但它已有环境性失败，本批同时改它会使**因果归属变差**。
先记录该门禁在**固定版本、固定 SHA** 下的独立基线与失败签名，再单独变更、单独比较。
其失败**不得归入 Reference Host 的验收结论**。

### D14 — 关键结论必须附反例判据

对"没有其他受影响调用点""CI 用的是某版本""改策略不改变某输入域行为"这类关键结论，报告应附：

**代码 SHA、工具实际路径与版本、输入范围、复现命令、期望输出、什么输出会推翻结论、尚未覆盖的范围。**

命令较短且长期约束运行行为的，**转成 CI 测试**；一次性调查证据**放在 R9 报告**，
**不塞进门禁注释**。

---

## 3. 策略影响审计（已完成部分）

枚举 CMake 3.28.3 `--help-policies`（156 节）中 `versionadded <= 3.25` 且涉及 `if()`／列表语义者：

| 策略 | 摘要 | 结论 |
|---|---|---|
| **CMP0054** (v3.1) | `if()` 仅对未加引号实参做变量/关键字解释 | 已按 D15 过近似审计，结论见[影响表](R9-gate-policy-impact-table.md) |
| **CMP0053** (v3.1) | 简化变量引用与转义求值 | 已逐点核对，结论见[影响表](R9-gate-policy-impact-table.md) |
| CMP0007 (v2.8) | `list()` 不再忽略空元素 | 缺陷 1、2；按 D11 审计 |
| CMP0057 (v3.3) | `IN_LIST` 运算符 | 缺陷 3 |
| CMP0121 (v3.21) | `list()` 检测非法索引 | 已核对，结论见[影响表](R9-gate-policy-impact-table.md) |
| CMP0124 / CMP0130 / CMP0139 / CMP0064 | foreach 作用域 / while / PATH_EQUAL / TEST | 已核对，结论见[影响表](R9-gate-policy-impact-table.md) |

`list()` 子命令实际用法（对照 `CMP0007` 受影响集合：

`LENGTH GET FIND INSERT JOIN REMOVE_ITEM REMOVE_AT REMOVE_DUPLICATES TRANSFORM SUBLIST SORT POP_FRONT/BACK REVERSE FILTER`；
`APPEND`/`PREPEND` 走另一条路径）：

```
VerifyReferenceHost.cmake         : APPEND                    (不受该路径影响)
VerifyReferenceHostCommands.cmake : APPEND, FIND, GET, LENGTH,
                                    REMOVE_DUPLICATES, SORT    ← 后两者此前未审计
```

### D15 — `CMP0054` 的审计形态

`"整条条件是一个带引号变量"` 为 0 处**只是必要而非充分**。
`CMP0054` 在 OLD 下会把**任何位置**的带引号或括号实参继续当作可能的变量名或关键字解释，风险包含
`if("${left}" STREQUAL "${right}")` 的**两侧**、`MATCHES` 的**左侧**，以及展开后碰巧成为
`NOT`／`AND` 等关键字的实参；未加引号实参不属此项差异。

机械检查应**保守列出两个文件中所有 `if()`／`elseif()`／`while()` 内的带引号/括号实参**，
再按输入域判断其是否可能与已定义变量名或关键字相撞。
最低可信审计是保留这份**站点清单、碰撞假设与针对性 OLD/NEW 探针**。

---

## 4. 审计项（已逐项作答并落表）

1. **逐点追踪**喂给 `FIND`／`REMOVE_DUPLICATES`／`SORT`／`GET`／`LENGTH` 的列表来源，
   记录**输入约束**，按 D11 在首次可能有损读取**之前**设检查点。
2. 按 D15 产出 `CMP0054` **站点清单**（保守过近似）。
3. 核对 `CMP0053`／`CMP0121`／`CMP0124`／`CMP0130`／`CMP0139`／`CMP0064` 在两张文件中的命中点。
4. 产出**策略影响表**并随代码提交：**入口、策略号、调用点、输入约束、探针、预期差异**。
   修改相关脚本或升级测试用 CMake 时由改动者更新，评审者核对。

**完成记录（2026-09-29）**：本节四项已由 [R9-gate-policy-impact-table.md](R9-gate-policy-impact-table.md)
逐项作答并落表；该表覆盖 `CMP0054`／`CMP0053`／`CMP0007`／`CMP0057`／`CMP0121`／`CMP0124`／`CMP0130`／
`CMP0139`／`CMP0064` 九个策略并各给结论与理由。本节文字保留为实现时的原始清单，不改写。

---

## 5. 范围边界

**本批做**：`VerifyReferenceHost.cmake`、`VerifyReferenceHostCommands.cmake`、新增独立解析器测试、
workflow 的版本固定与账目可见性、R9 报告的策略审计章节与措辞修正。

**本批不做**：Reference Host C++ 实现、SDK 公共 API、ADR 0042 冻结正文、既有 golden、
Stage 7A/8 交付物、根工程构建 target、`CMakeLists.txt` 最低版本、
`VerifyPlayerDistribution.cmake`（按 D13 单列）。

---

## 6. 修订记录

- `f0837aa` 为本文档的基准修订。三处构造修复已存在；本文件记录的是**策略基线与审计**部分的裁定。
- 本文档同时**更正**两处已提交内容的错误：
  1. `VerifyReferenceHostCommands.cmake` 中"含 `;` 会被拆成多个 `list(FIND)` 实参"的注释（按 D6 改正）；
  2. R9 报告 §2.1"Linux 作业安装了发行版 CMake"的措辞（实为 **runner 镜像提供**）。
