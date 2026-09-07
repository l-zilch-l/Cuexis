# Chart Format Foundation F1/F3：语义模型与 Packed 编码原语

状态：completed task evidence；Foundation 仍为 active

日期：2026-09-05

## 范围

本报告记录 Foundation F1 和 F3 的完成证据。两项均建立独立 candidate 基础设施，
不改变 Chart v4/CXT v1/CXC v1 的默认 Reader、Writer、Playback 或 FrameDigest 路径。

## F1：Canonical Semantic Chart typed model

新增 `engine/chart/include/cuexis/chart/canonical_semantic_chart.hpp`，表达：

- explicit 与 generated concrete entity identity，后者保留冻结的 binding/module/export 与
  finite expansion path；
- typed parent identity、Component（Transform、Renderable、Camera）和 Packed candidate
  component mask；
- typed Requirement、point/half-open interval 形状、judgement domain、action、lane constraint
  和显式 effect reference vector；
- typed resource closure、Chart timing、camera、features 和 main music。

该模型不保存 JSON DOM、CXT AST、Parameter、Slot、Pattern 或运行时表达式。它没有接线到
现有 v4 prepare/Playback，F2/F4 将分别负责 CXT 展开和 Packed table bridge。

Focused tests：`tests/chart/canonical_semantic_chart_tests.cpp` 覆盖 parent identity、
generated path、tap/point/lane requirement、resource closure 和 component mask。

## F3：Packed codec primitives

新增 `engine/chart/include/cuexis/chart/packed_chart_primitives.hpp` 与
`engine/chart/src/packed_chart_primitives.cpp`，提供：

- little-endian fixed-width byte reader/writer 和 pre-allocation bounds check；
- unsigned shortest LEB128 与 signed ZigZag，稳定拒绝 truncated、overflow 和 non-minimal input；
- CRC-32/ISO-HDLC（`123456789` -> `0xcbf43926`）；
- Rational Beat atom、GridDelta descriptor/atom、canonical rational rejection 和 checked signed
  arithmetic。

F3 不实现 Header、Directory、section registry、STR0/REF0/IDN0/ARCH/ENT0，或完整 Packed
Reader/Writer；这些仍属于 F4-F6。

Focused tests：`tests/chart/packed_chart_primitives_tests.cpp` 覆盖 fixed-width endian、ZigZag
signed boundary、truncation/overflow/non-minimal LEB128、CRC vector、Rational Beat canonical
round-trip、GridDelta exact accumulation 与 overflow/off-grid rejection。

## 验证

通过：

- `cmake --build --preset debug --target cuexis_chart -j 2`
- `cmake --build --preset debug --target cuexis_format_check -j 2`
- `clang-format --dry-run --Werror`（新增 F1/F3 源文件和测试）
- `git diff --check`
- `python -B tools/check_docs.py`
- `python -B tools/check_docs_target_contract_tests.py`

尝试 `cmake --build --preset debug --target cuexis_chart_tests -j 2` 时，所有新增和既有
Chart test object 已成功编译，但最终链接被当前 debug tree 的 MinGW 编译器与
`x64-windows` MSVC-built Catch2 libraries 的 ABI 不匹配阻断。典型错误是 Catch2/MSVC
符号 unresolved；这不来自 F1/F3 源码。须在一致的 MSVC Developer shell 或一致的
MinGW triplet 环境重新 configure 后运行 Catch2 与 CTest。

## 后续交接

F2 以 Canonical Semantic Chart 为唯一展开产物；F4 以同一 model 为 Packed table/stream
输入，并只消费 F3 原语。F5/F6 必须在 writer sizing、atomic publication、reader budget
与 semantic parity 前，不把 candidate 接入 Playback。

