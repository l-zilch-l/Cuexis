# T-03 逐项 finding 处置表

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-03-disposition
status: evidence
authority: none
date: 2026-11-23
-->

| ID | 原记录要点 | 本卡做了什么 | 本卡没做什么 | 移交/理由 |
| --- | --- | --- | --- | --- |
| RR-F111 | 旧文档写“完整 Header 置零”算 CRC，与 Reader/Writer 前 92 字节不一致 | 复核 Writer `:557-561` 与 inspect `:73`；补 `rr2_contract_packed`，把旧 96 字节算法值写入 CRC 字段后必须报 `packed.header.crc`；该用例上一版已由主代理在 candidate-debug 实测通过 | 未改算法、未改现有产物、未验证外部旧文件互操作 | T-09 按证据核退出；外部迁移按 D-25 保留说明 |
| RR-F112 | Writer 硬写 descriptor 0；TIME mode 1 报 `packed.time.invalid`、REQ0 报 `packed.requirements.codec`；GridDelta 仅测试调用 | 合同 §4.2 补 GridDelta 测试-only 地位；补 TIME/REQ0 mode 1 对照（envelope 与 section CRC 保持，语义层拒绝）；随 `rr2_contract_packed` 上一版实测通过 | 未实现 GridDelta wire，未删现有 primitives 测试，未改 Writer 选择 | 保留测试覆盖即满足本卡；实现扩展需另立合同 |
| RR-F113 | v1/v2/v3 Reader 额外拒绝分子/分母上限，合同未用具名常量 | 合同改为 `ChartLimits::maxBeatNumeratorMagnitude` / `maxBeatDenominator`，写明约分后判定与分子符号两侧；`rr2_contract_beat` 上一版已实测 v1–v3 边界，本轮补 `2*max/2` 与 `2*(max+1)/2` 约分对照，**待重跑** | 未改 `limits.hpp`，未改生成页 `guides/LIMITS.md`（已由生成器覆盖） | T-09 核退出；约分新用例需在下次构建确认 |
| RR-F114 | 合同对 candidate playback encoding 范围不一致，已按 profile 对齐 | 复核 `cxc_candidate.cpp:430-444` 与默认 CXC 入口 `:498-519`；`rr2_contract_entry` 上一版已实测两 profile 通过、交换 encoding 拒绝 | 未重跑候选播放验收，未改 `cxc_candidate.cpp` | T-09/T-13 按候选播放证据另验；编码门是该函数内 `entry_invalid` 的唯一产生点，未发现同码不同因 |
| RR-F115 | `cxtImports/cxtInstances` 0 命中，§6 曾过度声明 | 全仓复核实现/测试 0 命中；确认 `CxtV2Loader::expand` 由 `tools/chart/chart_candidate` 以显式参数调用；§6 补该入口句 | 未新增 CXT 展开能力，未改 `cxt_v2_loader` 或工具行为 | 文档状态由 T-07 承接，后继能力由 T-11/T-13 另验 |
| RR-F121 | 公开 API 不收版本，但文档残余“指定版本 digest parity”；测试重复编译 `frame_digest_version.cpp` | 文档拆成宿主可观察默认版本摘要（`computeFrameDigest` + `algorithmVersion`）与内部定版本测试（`playback::detail::computeFrameDigestVersion`，不安装） | 未改公开头，未运行 parity，未处理测试重复编译 | 测试重复编译归 T-04/RR-F214；本卡只核文档与可达性 |
| RR-F122 | `materializePresentation` 无候选宏门，默认安装头暴露 RPA candidate 入口与 `host.rpa.*` | 固定源码/安装证据：无条件 `PUBLIC FILE_SET`、声明在宏块外、实现无条件编入并返回 `host.rpa.*`；形成 RR-4 安装/宏头 diff 清单 | 未改公共头/CMake/安装面，未做安装消费者构建 | 窗口与逐符号迁移归 T-08（RR-4），生产面归后续卡；安装面/ABI 不由本卡裁定 |
| RR-F131 | 原记录称 same-tick 与 duplicate queue 两个稳定码被互换，且被既有测试固化 | 分别取证普通规范化、旧执行 §8、RPA occurrence；合同写明三层入口首错并保留旧 profile 次序与 Replay 行为；`rr2_contract_input` 上一版已实测；RPA 由既有 B3 覆盖 `judgement.late.duplicate_queue_entry`、`judgement.input.occurrence_conflict` 与转发顺序，不在本用例内；本轮补强 input 断言待重跑 | 未改 `input.cpp`/`execution_kernel_submit.cpp`；未把普通入口的规则写进旧 profile | 集成者需在 findings 追加分层纠正并保留原记录；生产输入逻辑不变 |
| RR-F132 | 原记录称 `gameplay.fold.finite.v1` 应进 ruleset/session identity，grade 写作 `signed-interval/1` | 逐字段对照 `execution_kernel_prepare.cpp:58-77` 与 preimage 写入顺序；三份文档补“engine 算法标记 vs ruleset/session 实际配置”和实际后缀；给出 T-10 取旧字节命令 | 未搬移字段、未改后缀、未取得旧 hex 身份字节（四个合同用例不覆盖身份字节）；未以新实现生成的期望证明身份未变 | T-10 在改动前 dump `judgementIdentity.canonicalBytes()` 并比较旧回放；差异按 D-04 留裁定 |
| RR-F135 | 三种互斥处置并存：`bad_alloc` 冒充 `host.rpa.capacity_exceeded` 或被吞成成功 | 在 `realtime-host-boundary.md` F135 小节与本报告写同一张“旧实现/已接受目标/未执行验收”表；逐项链接 T-06 `rr_frame_observation_errors` 场景；指出 `fault_` 无写入点、预建错误同码 | 未改观测桥、未做故障注入、未新增错误框架；不重开已接受裁定 | 生产修复与五类故障注入归 T-06；本卡只做基线/合同，未执行 |
| RR-F142 | 该 `error()` 辅助的 9 类失败统一为 `player.execution.test_only`；文件另有 7 个 `player.execution.*` 码 | 复核 9 类失败触发点与 `:632` 透传；纠正“7 个码”实为 `logger.info` 通道（`:263/338/407/493/543/580/599`） | 未改 Player 执行码，未登记新诊断，未运行 Player 场景 | T-15 承接；新增稳定码须先走 D-04 |

## 共同边界

- 上一版四个 `rr2_contract_*` 用例与两条 B3 RPA 对照已由主代理在 MSVC 19.51 + `candidate-debug` 实测通过
  （各 1/1）。本轮新增的 Beat 约分与 input 显式断言待重建重跑。CTest 枚举编号随每次 discovery 重写而
  漂移，不作为身份；引用一律用用例名。F135 五类故障注入、安装消费者/ABI、F132 旧身份字节仍未执行；
  本表不把合同接受、源码对照或未跑场景写成运行通过。
- 逐条回查诊断产生点后，未发现四例靠“另一个原因产生同码”通过：`entry_invalid` 在
  `validateCandidateChartBytes` 内只由编码门产生，`chart.beat.limit_exceeded` 与三个 `packed.*` 码也
  各自唯一。仍偏松的前置条件在 `rr2_contract_input`：复用序号未断言 `wasForwarded==false`，跨批结算
  只断言 `receipts.size()==1`；加强建议见 `report.md` 的「“为错误原因通过”的风险」，本轮没有放宽断言。
- `docs/stage_plans/**` 与 `docs/stage_reports/**` 未改；登记册写回由集成者按本表执行。
- 生产源码 `engine/`、`app/` 未改；测试只追加到卡面点名的既有源文件。
