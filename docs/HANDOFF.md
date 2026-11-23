# 当前交接

状态：active

更新日期：2026-11-23

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

## 当前阶段与工作项

当前为 Stage 7-RR，状态以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准。[T-18 文档治理](stage_plans/active/stage-07-rr/tasks/T-18.md)、[T-01 基线、派单包与冷启动](stage_plans/active/stage-07-rr/tasks/T-01.md)与 [T-03 合同与诊断](stage_plans/active/stage-07-rr/tasks/T-03.md)已完成。T-01 把实施基线固定在 `a22e255f`；T-03 订正 8 篇合同、追加四个 `rr2_contract_*` 用例与两条 RPA 对照，并在 `candidate-debug` 实测通过，生产源码未改。

## 当前任务

下一次接手的首项是 [T-04 RR-3/R1 构建隔离](stage_plans/active/stage-07-rr/tasks/T-04.md)（scope `R1`）：分开默认与显式候选构建闭包，落实 F302 的 Chart 源表隔离、RR-F216 库类型初始化顺序、RR-F217 shader cache 测试接线与 RR-F434 定版本摘要拒绝，并交付安装消费者与三个闭包阴性对照。验收需要 `headless-debug`、`debug`、`candidate-headless-debug`、`candidate-shared-debug` 四个预设的 fresh configure 与定向构建，范围与耗时须先与 owner 确认。

T-01 证据在 [基线与派单报告](stage_reports/stages/stage-07-rr/readiness/2026-11-23-t-01-dispatch-baseline.md)；T-03 证据在 [合同与诊断交付](stage_reports/stages/stage-07-rr/implementation/2026-11-23-t-03-contracts-and-diagnostics.md)。派单输入的规范哈希取基线 blob，见 [hashes.txt](stage_reports/stages/stage-07-rr/evidence/t-01-baseline/hashes.txt)，独立 checkout 复算不需要 `out/`。

## 下一步

1. 与 owner 确认 T-04 的构建范围（四个预设的 fresh configure、定向构建与安装消费者），再执行；已获授权的 `candidate-debug` 只覆盖 T-03 的四个用例和两条 RPA 对照。
2. 按[唯一调度](stage_plans/active/stage-07-rr/schedule.md)：T-04 完成后波次 5 的 T-02、T-05、T-17 并行；T-06 等 T-05 与 T-17。
3. 每卡按其任务级验收取证并回写发现处置；文档变更收尾前运行文档总入口、事实、命名与生成物检查。实际发布、SDK 晋升与旧 API 退役仍各自单独取得。

## 阻塞与待决

T-04 的四个预设构建超出已授权的 `candidate-debug` 定向构建，属于需 owner 选定的构建任务；未选定前可先做 CMake 源表与门禁脚本的静态改动，但不能宣称闭包或安装证据。

F132 的旧身份字节需要 T-10 在改动前 dump；F135 的五类故障注入与安装消费者分别归 T-06、T-08。D-20 的正式发行判定语料在仓库内为空集，历史发行资料是否存在无法核实，缺口已具名交 T-09。

## 验证状态

T-01：文档总入口、事实、生成物、调度与 `git diff --check` 全部退出 0；独立代理复现调度正反例，正本哈希未变；交叉核验重算 202 个派单路径的 blob 哈希，0 不一致；冷启动 T-04/T-06 成立、T-13 补注后复验成立。

T-03：`cmake --preset candidate-debug --fresh` 与三个测试目标构建成功；`rr2_contract_packed`（#121）、`rr2_contract_beat`（#170）、`rr2_contract_entry`（#326）、`rr2_contract_input`（#488）各自 `ctest --no-tests=error` 退出 0；`cuexis_rpa_tests` 的 `B3 different original Ticks…` 与 `B3 occurrence duplicate and slot conflict…` 两条对照退出 0。F135 故障注入、安装消费者与 F132 旧回放字节未执行，不能按通过计。

CMake target 事实仍缺输入：`check_docs.py` 该项按设计显示 `NOT CHECKED`，严格入口会失败；取得 `debug` 预设的 fresh configure 后可用 `CUEXIS_TARGETS_FILE` 补验。产品行为、设备与 hosted 证据随各责任任务取得。

## 接手沿用的产品决定

- RR 完整实现并实验验证 Gameplay v3；Stage 7.5 完整验收后 Stage 8 以 Chart v5＋Gameplay v3 为正式主线，旧内容按原规则、时间和身份运行。
- v5 保持 CXT2、Clip2、Behavior Event2 和整数 render.alpha 的原设计，拒绝旧作者动画语法。
- 设备等待的计时、超时判断和处置归 Player，SDK 不实现该机制；取消/清理及终态须真实发生。
- F501 的内容有效性与独立提交门禁、F135 的分配失败/容量/观测缺失分类、F203 的整批登记验证按已接受合同执行。
- 全面结构清理及逐组 API 过渡沿[登记册裁定](stage_plans/active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)，实际过渡发布、替代稳定和消费者证据满足后才能裁定旧入口退出。
- 生产源、安装头与构建系统文件名不得带阶段/任务编号，测试与工具可以；遗留路径按 `docs/governance_rules.json` 逐路径豁免。

## 最近改动

本轮已提交：`a22e255f` 治理落盘、`3b8bf41e` T-01 派单与证据、`569253d8` T-13 复验、`03334ecd` 命名硬规则与检查器、`6c77a888` T-01 证据修正。T-03 的交付与状态更新随后提交。
