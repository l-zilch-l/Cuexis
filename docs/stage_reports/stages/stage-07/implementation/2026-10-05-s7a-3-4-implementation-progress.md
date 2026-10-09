# S7A-3/4 implementation progress and pause point

状态：implementation progress；owner 要求当前部分收尾后暂停，整批验收未完成

日期：2026-10-05

## 1. 范围与版本

按 [A–G 交接](../../../../archive/stage-07-planning/s7a-3-4-implementation-handoff.md)
推进。分支 stage-7，基线 HEAD `6b11d102f6dcac4d9782cd03d0bae15add54af28`。
实现位于未提交工作区，基线 SHA 不代表新增行为已获得 hosted 验证。
57 个变更实现/测试/Schema/CMake 输入的 path+SHA256 manifest 指纹为
`00e8b6bda15d97cdc5e3c9b1b0e40c63eb11f69ed7bd589a0d35346ef9be621a`，
存于 `out/s7a-3-4-behavior-inputs.json`；它是工作区输入快照，不是 Git commit。
保留原有设计文档；未提交、推送、修改 CI 或升 SDK 版本。

## 2. 当前已实现的部分

| 交接卡 | 工作区实现 | 证据与尚未完成的范围 |
| --- | --- | --- |
| A | execution profile/phase targets/atom bindings/independent pair、规范 identity、Capsule revision3、诊断码 | execution prepare 负例与 rev3 round-trip 已验证；全部字段 mutation 和增量 Reader hostile 对账尚未齐全 |
| B | json_support owning source DTO、严格 payload/inline Schema、私有 cuexis_gameplay_author adapter、真实 CXT expander 与 frozen Binding、mixed-radix affine | 两路 2×3 fixture、prototype、file/memory、unused definitions、原子替换及 affine 负例通过；zero expansion、多 requirement/零 requirement 实体、源排列等完整覆盖仍待补齐 |
| C | 复用既有 Capsule/prepare 测试 | §11 八组和 §12 逐行四态审计尚未完成，不能以测试总数代替 E1 完成 |
| D | transactional ingress journal、L1 late routing、独立 L2 表模型 | 指定 late golden、signed endpoints、重复/dispatch collision、回退和原子性已有测试；完整 plan D-time/L-negative 行级闭环仍待对账 |
| E | owning configuration/prepared/query、contact、Pattern progress、K4 winner、T4 reducer、Free/Held/Terminal、runScope/reset | 主要路径及半开窗口/零 grace/owner release/no-tail/observer 已有验证；全部 P-state/H/K/C12 组合与跨实例不变量的完整验收未完成 |
| F | immutable timers/active index、独立 S1 扫描、tagged Fact origin/排序/IDs、下一 Tick signal、TickDraft seal | 每个 prefix 的结构对照、ID MAX、四个失败注入点、重复 phase/error/signal overflow 已验证；完整 normalized/admission 随机 trace、所有 ordered-list 和失败点组合仍待完成 |
| G | Debug fresh 全构建、聚焦 CTest、架构/诊断门禁、format/doc 检查 | 完整 Debug/Release CTest、Release clean-first、static/shared 外部 consumers、MinGW、最终跨工具链和 hosted 证据尚未齐全 |

源码入口：`engine/judgement/src/execution_kernel.cpp`、`execution_profile.cpp`、
`input.cpp`、`gameplay_prepare.cpp`、`engine/gameplay_packed/src/gameplay_capsule.cpp`、
`engine/json_support/src/gameplay_author.cpp`、`tools/gameplay_assembler/src/gameplay_author.cpp`。
fixture 位于 `tests/fixtures/gameplay_author/`；测试位于 `tests/judgement/execution_*`、
`tests/gameplay_author/gameplay_author_tests.cpp` 和 `tests/gameplay_packed/gameplay_capsule_tests.cpp`。

本次实际修订的合同缺口已在 [author profile](../../../../formats/gameplay-v2-author-profile.md)
记录最小反例：缺少现成 v5 inline JSON Reader，以及 Repeat 实例声明名与模板 localId 的冲突。
新增 inline grammar 只用于显式离线工具，不开启生产 Reader/Playback 或宣称 Stage 8 格式已发布。

## 3. 已运行验证

| 检查 | 当前结果 |
| --- | --- |
| MSVC cmake --preset debug --fresh；cmake --build --preset debug | exit 0，完整 Debug 构建 |
| CTest labels judgement/gameplay_packed/gameplay_author | 165/165，通过；单独 -N 核对 author=4、packed=14、judgement=147（其中含诊断码门禁） |
| 单独 execution Catch2，seed 740304 | 11,404 assertions / 23 cases，通过 |
| author Catch2，seed 740304 | 234 assertions / 4 cases，通过 |
| architecture + gameplay diagnostics CTest | 2/2，通过 |
| revision3 golden | bytes=3370，SHA256 ff1ff218925f2f8c684505651b9170c46ce2f386091fd3da34a8bdfe99e18bb4；preimage=4793，SHA256 8c5c41467cc986974d038eb66d5836198918ffe245adf51d8e575f156bf7a8ca |
| Linux GCC 本次尝试 | configure 失败：CFU-F3 requires CUEXIS_IMPLEMENTATION_SHA or a Git worktree；WSL 不识别 Windows worktree Git 路径，未获得本轮编译/测试结果 |
| Linux Clang 当前源码 | 三个聚焦 target 编译通过；judgement 147/147、gameplay_packed 14/14、gameplay_author 4/4；同一 revision3 bytes/preimage golden 通过。不是完整 Linux 矩阵 |
| cuexis_format_check | exit 0；37 个变更 C++ 文件已格式化 |
| documentation/status/target contract | check_docs 359 Markdown / 20 candidate JSON/CXT；status 4 tests / target 2 tests，全部通过 |
| whitespace/conflict scan | git diff --check 通过；另扫变更实现文件（含 untracked），0 问题 |

构建日志与聚焦 CTest 日志在 `out/s7a-3-4-current-build.log`、
`out/s7a-3-4-focused-debug.log`、`out/s7a-3-4-clang-current.log`、
`out/s7a-3-4-clang-tests.log`（本地产物，不进入版本库）。

## 4. 恢复后的工作顺序

1. 完成 B 的剩余源 fixture 和 A/D/E/F 的逐行边界验收；先补断言，再修复发现的问题。
2. 建立 C 的 Capsule §11/12 四态逐项对账，定位源码符号、fixture、测试与具体断言。
3. 完成 G 的完整 Debug/Release、static/shared external consumers、GCC/Clang/MinGW
   和相同输入 golden 矩阵；WSL 配置要显式提供真实基线 SHA，并另记工作区内容指纹。
4. 对最后工作区行为生成证据摘要，更新计划/状态；继续保留 state-budget 缺测与
   S7A-9 容量整体证明的 INCOMPLETE GATE，不能关闭 Stage 7A。

没有开展 Ruleset/Fold、Snapshot/Replay 或 Playback/Player 集成的产品实施。
本报告是暂停点，不是 S7A-3、S7A-4 或 Stage 7A 的完成报告。
