# Chart Format Foundation F2：CXT v2 Core 缺口闭合

状态：completed task evidence；Foundation 仍为 active

日期：2026-09-07

[2026-09-05 F2 报告](2026-09-05-f2-cxt-v2-core.md) 保留为首次落地快照，不被改写。
本报告按计划原文的 F2 完成定义闭合缺口，不改写该计划的完成栏。

## 1. 与计划完成定义的差距

计划 F2 完成定义是：integer/beat 子集、16 entity golden、40000 preflight、负例稳定。
CFF-A 完成定义还要求完整阶梯展开为 16 entities / 16 requirements，`groups=10000`
可在分配前预测 40000，lane 越界、缺 slot、动态 reference、递归和超预算稳定失败，
展开结果不含 AST/Parameter/Slot/Index。

2026-09-05 快照把 16-entity stair 和 40,000-entity preflight 归入 F4-F8，与上述完成
定义冲突。当时实现还存在：解析失败可 `continue`/`nullopt` 而不诊断；空 `components`
注入默认 Transform；integer affine 溢出可 UB；schema 为 stub；阶梯 golden 只有 4
个实体；`groups=10000` 用 `maxEntities=3` 代替 40000 预测；负例未隔离；Repeat count
允许 affine。

## 2. 本次闭合范围

独立 candidate 入口仍是 `CxtV2Loader::expand`，不改变 CXT v1、Chart v4 prepare 或默认
Playback 路径。

闭合项：

- Foundation schema 收紧为 integer/beat 子集：`schemas/cuexis.animation-template.v2.schema.json`
- fail-closed parse：未知字段、空 extensions、必填字段和畸形 JSON 均诊断且不发布 chart
- 空 `components` 不注入 Transform；仅声明的 Transform 进入展开结果
- integer affine 使用 checked multiply/add；`INT64_MIN * -1` 拒绝
- Repeat count 只接受 integer literal 或 integer parameter
- 预检按 Repeat count × body 乘积计算，超预算时写入 `counts` 后失败，不分配 40000 实体
- 16-entity 阶梯 fixture 与数组重排 identity golden
- 独立负例：export mismatch、slot type/missing、lane 4、动态 judgementDomain、parent cycle、
  未知字段、非空 extensions、number/boolean/vector 参数、不完整 interval、affine Repeat count

阶梯 fixture：

- `tests/fixtures/chart_format_foundation/valid/pattern_stair.cxt`
- `tests/fixtures/chart_format_foundation/valid/pattern_stair_reordered.cxt`

## 3. 验证证据

命令在 `mingw-debug` preset 上执行，避免当前 debug tree 的 MinGW 编译器与
`x64-windows` MSVC Catch2 ABI 不匹配。

```text
cmake --build --preset mingw-debug --target cuexis_chart_tests cuexis_json_support_tests
.\out\build\mingw-debug\bin\cuexis_chart_tests.exe "[f2]" --reporter compact
.\out\build\mingw-debug\bin\cuexis_json_support_tests.exe "[f2]" --reporter compact
.\out\build\mingw-debug\bin\cuexis_chart_tests.exe "[f1],[f2],[f3]" --reporter compact
clang-format -i engine/chart/src/cxt_v2_loader.cpp tests/chart/cxt_v2_loader_tests.cpp tests/json_support/schema_artifact_tests.cpp
git diff --check
python -B tools/check_docs.py
python -B tools/check_docs_target_contract_tests.py
```

结果：

| 命令 | 结果 |
| --- | --- |
| mingw-debug `cuexis_chart_tests` / `cuexis_json_support_tests` 构建 | 通过 |
| `cuexis_chart_tests.exe "[f2]"` | 17 cases / 88 assertions 通过 |
| `cuexis_json_support_tests.exe "[f2]"` | 1 case / 10 assertions 通过 |
| `cuexis_chart_tests.exe "[f1],[f2],[f3]"` | 27 cases / 138 assertions 通过 |
| `git diff --check` | 通过 |
| `python -B tools/check_docs.py` | 218 Markdown files 通过 |
| `python -B tools/check_docs_target_contract_tests.py` | 2 tests 通过 |

正例核对：默认 `groups=4` 展开 16 entities / 16 requirements；`n2` identity 为
`(groups,1),(n2,0)`，`startBeat=33/2`，lane=2，Transform `position.x=2`。
`groups=10000` 在 `maxCxtV2ExpansionEntities=39999` 时 `counts.entityCount=40000`，
诊断 `cxt.v2.budget.expansion`，不发布 chart。direct prototype 空 components 不带
Transform。数组重排 fixture 展开实体逐项相等。

## 4. 边界与后续

F2 现在满足计划自己的完成栏。本报告不启动 F4，也不把 Packed tables、16 MiB writer
gate、CXC candidate entry 或 40k/16 MiB 容量验收写成已完成。CXT v2 仍是 candidate：
非生产 Schema，未接入默认 Playback。
