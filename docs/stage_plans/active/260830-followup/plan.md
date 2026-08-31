# 260830-followup 维护计划

状态：active；承接已关闭的 260829 Full Review，不启动 Stage 6

更新日期：2026-08-30

归档来源：[Full Review 最终关闭报告](../../../stage_reports/reviews/full-review-2026-08/2026-08-30-final.md)
与 [Full Review 整改计划](../../reviews/full-review-2026-08/remediation-plan.md)。

## 阶段目标

完成 `260829-full-review` 合并后的三个 follow-up 任务：

1. 文档整理。
2. Chart/CXC parse-once。
3. 关键模块分支覆盖率。

三项任务共同目标是降低后续维护成本：文档提供稳定事实入口，prepare/package 链路避免重复解析，
关键失败路径由有行为价值的 branch tests 固化。

本计划是分支级 follow-up，不是编号产品阶段。Stage 6 仍为 future，只有在项目所有者明确启动后
才迁入 `active/`。

## 前置条件

- Stage 5 已于 2026-08-28 关闭并合并至 `master`。
- `260829-full-review` 已于 2026-08-30 关闭并合并至 `master`。
- 整理前文档备份已创建并验证，可用于内容追溯。
- Full Review 已建立 Chart/CXC characterization、ownership/parity、diagnostics、rollback 和 coverage
  基线；本轮在该基线上继续，不重建第二套实现。

## 实施范围

### 任务 1：文档整理

- 重新检查 ADR、格式规范、架构、指南、阶段计划、阶段报告、提案、示例和归档的分类。
- 整理 `docs/` 根目录、stage plans、stage reports 和不必要的叶级 README。
- 统一 canonical 路径、文件命名、相对链接、单一 H1、状态词、日期和集中 legacy-path 映射。
- 建立中文优先、结构一致的 Playback SDK API 参考，减少为理解公共功能而反复阅读实现。
- 保持 `CURRENT_STATUS.md` 为唯一当前状态入口；历史证据只追加新报告，不改写原报告。
- 加强 `tools/check_docs.py`，防止目录、索引、API 元数据和状态合同回退。

### 任务 2：Chart/CXC parse-once

状态：completed；实施报告见[任务 2 完成报告](../../../stage_reports/reviews/260830-followup/2026-08-31-task-2-chart-cxc-parse-once.md)。

详细实施方案见 [任务 2 计划](task-2-chart-cxc-parse-once.md)。

先 characterization，后实现，不以直接删除 parse 调用作为起点。Chart prepare 链路与 CXC package
路径分别记录并关闭：

- parse count 和重复解析位置；
- owning object 的峰值数量、生命周期和 16 MiB 边界；
- diagnostic `code`、`message`、`fieldPath` 与顺序；
- canonical bytes、semantic identity、FrameDigest 和 capability parity；
- candidate/active commit、失败 rollback 和异常边界。

在证据固定后逐步移除重复 parse。不得在 `engine/animation/` 引入 JSON/CXC/CXT 解析，不得复制
第二套 Chart parser，不得以缓存绕过内容验证、identity 或事务提交。Chart v1/v2/v3 Reader 和
现有迁移入口继续保留。

### 任务 3：关键模块分支覆盖率

详细实施方案见 [任务 3 计划](task-3-critical-branch-coverage.md)。

优先增加以下行为路径的 branch tests：

- Chart v4 与 CXC 的非法输入、预算边界、重复键和异常转换；
- Playback prepare/reload 的失败提交、candidate/active rollback 和稳定 diagnostics 顺序；
- identity/cache-key 的输入差异、冲突和失败保留；
- HostClock/SDL 的线程、发布和失败边界；
- render adapter 的资源缺失、后端失败和 active state 保留；
- OOM、并发快照和所有权边界中可确定、可注入的失败路径。

新增测试必须证明可观察行为，不为提高孤立数字添加无断言价值的行覆盖。覆盖率报告用于定位缺口，
本轮不直接提高全局 engine 40% 行覆盖硬门槛。

## 顺序与门禁

1. 任务 1 先稳定文档路径、状态和检查器，避免任务 2/3 的证据写入移动中的结构。
2. 任务 2 先提交 characterization 与 parity tests，再按 Chart prepare、CXC package 两条链路分批
   实施 parse-once；每批必须保持 rollback 和 diagnostics。
3. 任务 3 可与任务 2 的 characterization 并行，但涉及同一模块的实现和测试修改必须串行整合。
4. 每个任务单独形成报告；共享构建目录、全量验证和最终 Git 操作串行执行。

## 验收标准

- `docs/README.md` 可达全部 Markdown，且根目录和叶级 README 数量符合文档政策。
- `docs/stage_plans/active/` 只包含 `260830-followup` 当前计划，不残留已关闭阶段目录。
- `CURRENT_STATUS.md`、`ROADMAP.md`、`PROJECT_GUIDE.md` 与 Git 合并顺序一致。
- API 文档格式统一、中文优先，代码标识保持原始英文。
- 历史计划和报告正文没有不可追溯的丢失；迁移路径可由集中映射审计。
- Chart prepare 与 CXC package 的 parse count 有可重复证据，已批准的重复 parse 被移除。
- parse-once 前后 canonical bytes、identity、FrameDigest、diagnostics 和 rollback 行为保持一致。
- Chart v4、CXC、Playback、identity/cache、HostClock/SDL 和 render 关键分支测试均有实质增加，
  且每项新增覆盖对应明确行为断言。
- 受影响的 Debug、Release、headless、package/consumer、sanitizer 或 hosted 门禁按改动范围通过。
- `python -B tools/check_docs.py` 与 `git diff --check` 通过。

## 明确不包含

- Stage 6 产品实现、SDK 新功能或公共 API 扩展。
- 重写历史报告以制造新的 hosted 或 owner-acceptance 证据。
- RT-29、T1 World/Animation 大规模优化、T2 大包解析降本和 T4 Stage 6/API/Player 工作。
- Studio、Judgement/Replay、稳定 C ABI 或运行时脚本。
- 以破坏 compatibility、identity、canonical order 或 rollback 为代价扩大 parse-once 范围。

## 关闭条件

三个任务均完成各自报告和所需验证后，新增 follow-up 总结报告；项目所有者接受后把本计划迁入
`completed/`。
