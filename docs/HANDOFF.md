# 当前交接

状态：active

更新日期：2026-11-23

<!-- cuexis-meta
id: STATUS-HANDOFF
status: active
authority: owns
-->

## 当前阶段与工作项

当前为 Stage 7-RR，状态以 [CURRENT_STATUS](CURRENT_STATUS.md) 为准。[T-18 文档治理](stage_plans/active/stage-07-rr/tasks/T-18.md)与 [T-01 基线、派单包与冷启动](stage_plans/active/stage-07-rr/tasks/T-01.md)已完成。T-01 把实施基线固定在提交 `a22e255f6716b22f96ad8a183136eae4fb04ac2f`（tree `7869115460b0c17a1f833f30700cd06f34e8e3c3`，工作树干净），交付派单清单、调度正反例、独立冷启动和 D-20 旧内容清单，产品实现尚未开始。

## 当前任务

下一次接手的首项是 [T-03 RR-2 既有合同与诊断](stage_plans/active/stage-07-rr/tasks/T-03.md)（scope `RR2`）：按[采用记录](stage_reports/stages/stage-07-rr/readiness/2026-11-23-t-01-dispatch-baseline.md)在基线上逐项交付既有合同的对照、修订和回归记录；改变 wire、接受集合或稳定码语义前仍须对应 ADR/Spec 接受（D-04）。取证口径、派单输入哈希与回滚基线见同报告的附件。

T-01 的完整证据在 `out/review/stage-07-rr/T-01/`；提交内固定了收据、哈希表、语料清单与旧成品夹具。独立 checkout 核对派单时按 [hashes.txt](stage_reports/stages/stage-07-rr/evidence/t-01-baseline/hashes.txt) 的 blob 口径复算，不依赖 `out/`。

## 下一步

1. 执行 T-03，完成后按[唯一调度](stage_plans/active/stage-07-rr/schedule.md)进入 T-04；波次 5 的 T-02、T-05、T-17 在 T-04 完成后并行。
2. T-04 起需要真实 configure 与构建隔离证据。完整构建、容量/内存、hosted 与设备长任务仍须先说明范围、可行性与预计耗时，待 owner 选定后执行；在取得真实 CMake target 事实前，文档严格入口会因缺输入失败，普通入口按设计显示 `NOT CHECKED`。
3. T-13 的冷启动补注已在任务卡内完成，复验回执随修订提交登记；T-10、T-13 的 D-20 条目已指向[旧谱兼容语料清单](stage_reports/stages/stage-07-rr/readiness/2026-11-23-legacy-corpus-inventory.md)。
4. 每卡完成时按其任务级验收取证，更新发现处置与交接；文档变更完成前运行文档总入口、事实与生成物检查。实际发布、SDK 晋升与旧 API 退役仍各自单独取得。

## 阻塞与待决

当前无阻止 T-03 开始的待决设计。构建类长任务的执行范围与时机等待 owner 选择，这不阻止先做静态与短检查。

D-20 的正式发行判定语料在仓库内为空集，历史发行资料是否存在无法核实；该缺口已具名交 T-09，不得用内部候选 L1–L4 替代正式发行语料，也不得据此新增正式支持承诺。

既有受限退出、采样额度、设备/P8 台账与 testOnly 限制不因本轮基线或治理改变；出现新产品合同、兼容承诺冲突或缺证据退出的需求时，只暂停受影响切片并交 owner 裁定。

## 验证状态

T-01 已在本基线实际运行：`check_docs.py`、`check_docs_facts.py`、`check_plan_schedule.py`、`test_check_plan_schedule.py`（27 项）与 `git diff --check` 全部退出 0；独立代理在阶段目录副本上复现调度正反例，正本 `schedule.md` 哈希前后一致；独立交叉核验复算了派单清单（178/178 路径、5104 文件 checkout），并把规范哈希改为基线 blob 口径以消除行尾差异。冷启动对 T-04、T-06 判成立，对 T-13 判部分成立，四项派单缺口已补注。CMake target 事实仍缺输入，`check_docs.py` 该项标 `NOT CHECKED`，严格入口会失败；本轮未 configure、未构建、未运行任何产品测试或设备任务，产品验收不能沿用旧报告。

## 接手沿用的产品决定

- RR 完整实现并实验验证 Gameplay v3；Stage 7.5 完整验收后 Stage 8 以 Chart v5＋Gameplay v3 为正式主线，旧内容按原规则、时间和身份运行。
- v5 保持 CXT2、Clip2、Behavior Event2 和整数 render.alpha 的原设计，拒绝旧作者动画语法。
- 设备等待的计时、超时判断和处置归 Player，SDK 不实现该机制；取消/清理及终态须真实发生。
- F501 的内容有效性与独立提交门禁、F135 的分配失败/容量/观测缺失分类、F203 的整批登记验证按已接受合同执行。
- 全面结构清理及逐组 API 过渡沿[登记册裁定](stage_plans/active/stage-07-rr/findings.md#24-统一实施前设计裁定2026-11-23)，实际过渡发布、替代稳定和消费者证据满足后才能裁定旧入口退出。

## 最近改动

T-18 治理成果已作为 `docs(stage-7-rr): land documentation governance for T-18` 提交；T-01 在其上绑定基线并交付派单证据，随后进入 T-03。
