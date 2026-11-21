# Stage 7-RR：两名 Astra 独立执行者接卡复审

状态：evidence

更新日期：2026-11-22

<!-- cuexis-meta
id: EVIDENCE-STAGE-07-RR-2026-11-22-ASTRA-EXECUTOR-REVIEW
status: evidence
authority: none
date: 2026-11-22
-->

## 结论

**部分成立。** 两人均能定位 16 张卡的显式前置，但尚不能全部按卡交付。主代理复核、去重后保留 [9 项待收敛问题](#结果与证据)：4 项 P1 涉及构建、身份校验和跨卡前置；5 项 P2 涉及数据表达、测试边界和工具归属。生产未实施。

## 范围与方法

两名 Astra 各自以无父上下文方式读取同一工作树的计划、三份合同、16 张卡及对应源码、schema、CMake。A 按调度波次推演交付；B 从 Player、SDK、工具的可观察结果反推输入。独立审查期间，主代理分别核对两人的阶段发现，并向提出该项的原审查者追问。两份终稿完成后，主代理去重汇总。原稿保留各自判断：`out/review/stage-07-rr/astra-independent-A-20261122.md`、`out/review/stage-07-rr/astra-independent-B-20261122.md`。

## 执行记录

- 审查起点：HEAD `9623efa321fe488e9c6b8549f7059283020c1861`。主代理逐项比对 `out/review/stage-07-rr/T-01/final-20261122/manifest.json` 的 206 项输入，SHA-256 全部一致；此记录绑定复审起点，早于本次报告和路由更新。
- 两份原稿记录调度检查识别 16 卡、检查器 17 项测试通过，并区分了静态反例和实际运行结果。上一轮组合文档检查与最终封存见[当期报告](2026-11-22-design-readiness-review.md#最终输入封存的读取口径)。

## 结果与证据

以下 `tasks/`、`contracts/`、`schedule.md` 相对 `docs/stage_plans/active/stage-07-rr/`，其余路径相对仓库根。最小修正列用于后续收敛，九项均未修。

| 编号 / 优先级 | 对应卡（来源） | 精确证据 | 触发与待确定的行为 | 最小修正 |
| --- | --- | --- | --- | --- |
| AX-01 / P1 | T-06（B） | `tasks/T-06.md:28,33`；`contracts/render-player-writer.md:65,107,117,152`；`app/player/CMakeLists.txt:16,51`；`cmake/CuexisLibraryType.cmake:8–11` | 默认 `shared-debug` 的独立 Player 打开旧谱也要走新帧路径；Playback 新自由函数仅由实验库导出。 | 明定默认构建的调用边界、导出或兼容路径；用默认及实验 shared 配置实际链接 Player，以同一旧谱核对帧与导出。 |
| AX-02 / P1 | T-13（B；T-12 交付源数据） | `contracts/prepared-artifact.md:48,78,88,210,306`；`docs/formats/GAMEPLAY_CAPSULE_V2_FORMAT.md:175–181,486`；`tests/fixtures/gameplay_author/inline_prototype.json:4,47,49,264` | 裸读可无外部 expected digest；唯一 K4 及镜像一致改为 B 并重算自摘要时，Reader 无独立参照可证明作者原值为 A。Chart UUID 与逻辑 entry ID 分属不同字段。 | 源编译校验作者 common；裸读只承诺内部一致性、owner、来源闭包和已提供的外部摘要。若须独立比对逻辑 ID，则明确其成品字段及摘要覆盖。 |
| AX-03 / P1 | T-10（A2） | `tasks/T-10.md:23,27,33`；`tests/rpa/CMakeLists.txt:19`；`tests/rpa/rpa_b3_fixture.hpp:12`；`tests/judgement/gameplay_test_fixture.hpp:20,21,65,72`；`CMakeLists.txt:1385–1390` | 拆出作者头和实现后，现有 `cuexis_rpa_tests` 的夹具仍构造作者对象；只给测试补链接又不在原 `ALLOWED` 内。 | 把 RPA 测试 CMake、私有 `cuexis::judgement_compile` 边及对应允许项纳入 T-10；默认与实验启测配置均定向构建。 |
| AX-04 / P1 | T-14、T-15（A3） | `schedule.md:28,29`；`tasks/T-14.md:36`；`tasks/T-15.md:23,27`；`contracts/render-player-writer.md:176,334`；`engine/player_support/include/cuexis/player_support/player_command.hpp:31` | T-14 要生产 owner 完成 Retry 加两个 Pause 的三个终态，Retry 命令枚举却排在下一波 T-15。 | 把生产 Retry 枚举、命令消费与事务验收归 T-14；键位、SDL 转译和 CLI 留 T-15。 |
| AX-05 / P2 | T-03（A1、B） | `tasks/T-03.md:23,28,37`；`tests/judgement/execution_profile_tests.cpp:264–289`；`engine/judgement/src/execution_kernel_submit.cpp:91,127,133`；`docs/formats/rpa-execution-contract.md:341` | tick −10、不同序号的 press/release 可在 ingress 获准；旧 submit 在同 dispatch tick 仍可原子拒绝，RPA T2 另有完整批规则。 | 按 ingress、旧 SDK dispatch、T2 分列预期；保留旧 pending 重投对照，另以真实 forwarded 项验迟到队列重复。 |
| AX-06 / P2 | T-11、T-12（A6） | `contracts/runtime-timing.md:360,383,389,428`；`contracts/prepared-artifact.md:306`；`engine/judgement/src/gameplay_prepare.cpp:75–80`；`tasks/T-11.md:27`、`tasks/T-12.md:29,35` | 声明 `tap/1` 的成功窗误写 `tap/2`；窄 `BeatWindow` 只传 kind，两份输入到编译器时相同。 | typed 窗保留完整 Phase 至校验，或明确 T-11 所有源入口先比对 ordinal 再降为 kind；以 `tap/1` 和 `tap/2` 成对验收。 |
| AX-07 / P2 | T-13（A5、B） | `contracts/prepared-artifact.md:90,100,243,346`；`schemas/cuexis.gameplay-graph.v1.schema.json:397,575,579`；`engine/gameplay_packed/src/gameplay_graph_writer_internal.hpp:363` | PA-02 非默认相机按合同写 `staticChart.camera`；复用的封闭 Graph schema 必须使用 `defaultCamera`，两个键同时写也会拒绝。 | 统一成 `staticChart.defaultCamera`；正例保留 SCN1 与实际投影对照，另验 `camera` 为未知键。 |
| AX-08 / P2 | T-16（B） | `tasks/T-16.md:31,33`；`contracts/render-player-writer.md:431,447`；`CMakePresets.json:102,218–222`；`tools/CMakeLists.txt:7,15` | 在干净的 `headless-debug` 验 `--help` 或发布失败恢复，该配置关闭 developer tools，没有实际迁移进程。 | 库向量留 headless；P5-06 改由 tools 开启的配置构建 `cuexis_chart_migrator`，检查进程输出、退出码及双目标文件。 |
| AX-09 / P2 | T-13、T-16（B） | `contracts/render-player-writer.md:368,431`；`tasks/T-13.md:23,29`、`tasks/T-16.md:23,33`；`tools/cxc/cxc_pack/main.cpp:15–18,43–48` | 离线工具承诺帮助退出 0；`cuexis_cxc_pack --help` 依现有解析会向 stderr 写 usage 并退出 2，迁移器以外的入口没有承接卡。 | 给受影响工具指定责任卡和实际命令验收，或明确登记为后继，限定本批帮助承诺。 |

**撤回项（不计入 AX）。** A4：`tasks/T-13.md:30,36` 和 `tasks/T-15.md:27,33` 已将容器/SDK 验收与 Player CLI 分开，无需提前在 T-13 验 CLI。B 的混合 CXC：`contracts/prepared-artifact.md:223,231,235` 与 `contracts/render-player-writer.md:196` 区分旧 bootstrap 校验和后继容器准入；含 v2 必需扩展的容器未因此成为 D-18 所指的受支持正式输入。

## 限制与未做

本轮只整合接卡证据。实际 shared 链接、Player/SDK/CLI 运行、ABI、GPU、设备及 hosted 结果均未在本轮验证。文档门禁不能证明这些生产路径通过。

## 对登记册与合同的影响

[登记册 §15](../../../stage_plans/active/stage-07-rr/findings.md#15-astra-执行者复审2026-11-22) 追加 AX-01 至 AX-09 的未修索引。后续先收敛四项 P1，再逐项对齐五项 P2 的输入、责任卡与验收。本轮未修改技术方案、合同或任务卡；D-05 范围与采用条件保持原状。
