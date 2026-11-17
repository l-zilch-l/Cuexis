# 文档治理重构（第一批：元数据、路由、生成事实、证据头）2026-11-17

状态：completed（本批范围内）；范围外内容见“未做与限制”

更新日期：2026-11-17

## 结论

在不改任何产品代码、不移动证据目录的前提下，完成了文档治理的机械层：
为 170 篇合同/计划补机器可读元数据，生成 `docs/MAP.md` 路由页与 `AGENTS.md` 状态块，
为 424 篇证据报告补统一头、为阶段索引标注 closed/active，并订正 7 处已证实的假陈述。
全部改动由 `check_docs.py` 与新增的 `check_docs_facts.py` 校验通过。

## 范围与基线

- 基线：分支 `stage-7.5`，HEAD `aac83e5e`，SDK API `0.9.1`，日期构建 `26.11.16-1`。
- 只动 `docs/**`、`AGENTS.md`、`tools/verification/*`、`tools/check_pre_push.ps1`，未改产品代码。
- 备份：改动前 `docs/` 已归档到 `out/backup/docs-20261117-020747.zip`（3,487 文件 / 420 MB）。

## 变更

1. **元数据**：170 篇合同与计划（adr 48、api 13、architecture 8、formats 21、guides 8、stage_plans 72）
   在 H1 后获得 `cuexis-meta` 块（`id`/`status`/`authority`），状态取自原有 `状态：` 行，未改写正文。
   首批有 28 篇的正文状态是散文（如“现行文档入口”“实现完成”），复查后显式赋值为
   `completed`/`historical`/`candidate`/`future`/`accepted`；最终 MAP 中 `unverified` 为 0。
2. **路由**：新增 `tools/verification/gen_docs_map.py`，从元数据生成 `docs/MAP.md`（227 行，
   按状态排序列出合同与计划，含“单一事实源”表与 agent 阅读规则）；`docs/README.md` 顶部指向它。
3. **AGENTS**：`AGENTS.md` 顶部加 `CUEXIS_STATE_BEGIN/END` 生成块（当前阶段、SDK/构建版本、
   文档入口、权威规则、正文可能滞后声明）；原正文未删。
4. **证据层（C+D）**：424 篇 `stage_reports` 报告获得 `status: evidence` + `authority: none` 头
   （文件名带日期者附 `date`）；`stage_reports/README.md` 增加“证据不是权威”规则段；
   `stage-07` 计划索引改为 `completed index`，`stage-07-5` 报告索引改为 `active index`。
5. **计划状态纠偏**：`stage-07` 的 README 状态行与元数据由 `active` 改为 `completed`；
   2026-11-17 追加执行**目录迁移**：`git mv docs/stage_plans/active/stage-07 docs/stage_plans/completed/stage-07`，
   同步改写 201 个文件、3,000 处 `active/stage-07` 引用（含证据层 117 个文件，owner 明确授权），
   修复迁移产生的相对链接 22 处，更新 `check_docs.py` 的 4 组硬编码清单与 `check_stage7_section_links`
   的路径基准，并在 `stage_plans/legacy-paths.md` 登记旧路径映射。
6. **生成事实上线**：新增 `docs/guides/LIMITS.md`（75 行，由 `tools/verification/gen_docs_limits.py`
   从 `limits.hpp` 的 `ChartLimits` 49 个字段与 `PackedChartLimits` 7 个字段生成）与
   `docs/guides/DIAGNOSTIC_CODES.md`（103 行，由 `tools/verification/gen_docs_codes.py` 从
   `schemas/*.codes.json` 生成：九类 + 114 条已注册码）。前者是预算数值的单一可读来源，
   后者声明“类别权威仍归 Spec §9.2”，只做可读投影；`MAP.md` 的“单一事实源”表已指向两页。
7. **假断言订正（7 处）**：
   - `CHART_FORMAT.md` 指向不存在的 `stage_plans/stage_2_implementation_plan.md` → `completed/stage-02/plan.md`；
   - `GAMEPLAY_V2_SPEC.md`、`GAMEPLAY_V2_ABI.md` 的 `tools/check_stage6_a2.py` → `tools/verification/check_stage6_a2.py`；
   - `MATERIAL_SHADER.md` 的 `stage_unsupported` 行注明“当前实现尚未产出该码”；
   - `realtime-host-boundary.md` 注明 D2–D5 名称是设计职责名、当前无同名入口；
   - `formats/README.md` 的 Packed “未实现” → “已实现但默认入口未消费”（附 RR-F201）；
   - `formats/README.md` 的 Capsule “revision 1 不变” → “代码默认 revision 2，接受 2/3/4”。
8. **门禁**：新增 `tools/verification/check_docs_facts.py`（生成物新鲜度 + 版本字面量漂移），
   接入 `tools/check_pre_push.ps1` 的 quick checks；`check_docs.py` 允许根级 `MAP.md`；
   `DOCUMENTATION_POLICY.md` 增补元数据块规范与生成物/事实检查说明。
9. **AGENTS 精简**：`AGENTS.md` 由 357 行压到 50 行，正文只保留路由、权威顺序、硬约束与
   “不要”清单；阶段史、构建命令、版本格式改为指向 `docs/MAP.md` 与 `docs/guides/*`。
   同步更新 `docs/status_contract.json` 的 AGENTS 必需片段（11 条 Stage 4/5 事实 → 2 条文档指针）、
   `check_docs.py` 的 `check_agents_guide` 必需片段（删掉 `SDK API 0.6.0` 等过期事实，改为 5 条规则指针），
   以及生成块措辞（明确“正文只写路由与硬约束”）。
10. **ADR 增补**：`docs/adr/0027-playback-sdk-product-boundary.md` 的「依赖方向」新增
   “Judgement 是内部 STATIC 模块，只链接 core，不安装头文件，不进入公共 SDK。”
   依据：`engine/judgement/CMakeLists.txt:1`（STATIC）、`:63-66`（PUBLIC 仅 `cuexis::core`）、
   `:51-52`（无 FILE_SET HEADERS / 无 install），且不在 `CUEXIS_PUBLIC_EXPORT_TARGETS`。
11. **AGENTS 二轮修订**（依据无上下文子代理的冷启动路由测试）：
   - `docs/CURRENT_STATUS.md` 元数据新增 `current_item`，生成器校验路径存在后在 AGENTS 生成块与
     MAP 当前阶段行输出“当前工作项”，堵住“新 agent 不知道下一步做什么”的硬洞；
   - MAP 新增静态路由表「常见变更的阅读路径」（公共 API / 格式字段 / 依赖 / bug 四行）；
   - AGENTS 正文去掉阶段专属的 `stage-07-rr/findings.md` 硬编码路径，改为指向生成块的当前工作项；
     补“生成块是唯一例外”与“状态以 CURRENT_STATUS、路由以 MAP 为准”的仲裁句；
     “1–3 篇”改为“1–3 篇，复杂任务最多 5 篇”。
12. **元数据落位缺陷修正**：子代理发现 `CHART_V4_FORMAT.md` 的 `cuexis-meta` 块被插进状态句中间
   （状态句被劈成两段）。全量扫描后同类共 **22 个文件**（ADR 3、formats 6、guides 1、计划 12），
   已统一把块移到状态段之后；根因是迁移脚本按“首个 `状态：` 行”插入，未识别跨行状态段。
13. **生成块路由修复**（对抗性代理发现，真实缺陷）：AGENTS 生成块原先用 docs 相对路径拼链接，
   写进仓库根后成为死链，且两道门禁都不校验 AGENTS 链接。修复：AGENTS 链接加 `docs/` 前缀；
   `CURRENT_STATUS` 元数据新增 `current_register`，生成块与 MAP 分列“当前工作项/当前登记册”；
   `check_docs.py` 新增 `check_agents_links()`（仓库根相对解析），并用“注入坏链 → 报错 → 还原”
   做过阴性对照。同时 MAP 增加 authority 列、`schemas/*.schema.json` 路由行与 `examples/` 入口，
   `DOCUMENTATION_POLICY` 的权威顺序补“代码与 schema 最高”。
14. **外部审查交叉核对**：归档外部 59 条审查报告
   （[2026-11-17-external-findings-review.md](2026-11-17-external-findings-review.md)），
   独立复核其 7 条 S1 全部属实，补登记 RR-F501–F510（见 findings §8），并列出 12 条与既有登记册的对应关系
   及 20 余条未复核项。
15. **登记册全量核查（2026-11-17）**：6 个无上下文子代理分批核查 91 条未决项（内部 58 + 外部 33），   结果写回 findings，全册 112 条现在都有结论列、“未复核”归零：
   - A 批（内部文档类 18 条）：成立 13 / 部分成立 5；
   - B 批（内部运行时类 19 条）：成立 9 / 部分成立 10；
   - C 批（内部架构类 9 条）：成立 3 / 部分成立 6；
   - D1 批（外部 D/DOC 类 13 条）：成立 10 / 部分成立 3；
   - D2 批（外部 P/T/A 类 20 条）：成立 11 / 部分成立 7 / 不成立 1 / 无法核实 1；
   - E 批（内部 §7 尾部 12 条）：成立 10 / 部分成立 2。
   合计成立 56 / 部分成立 33 / 不成立 1 / 无法核实 1。净产出是 **18 处断言被收窄或推翻**，
   其中影响修复决策的包括：RR-F125 的“专属码零产出”不成立、RR-F143 的“docs 零引用”不成立、
   RR-F202 的“timeDiscontinuityId 不存在”不成立、RR-F206 的“非异常安全”不成立、
   RR-F315 的“content 是门面”不成立、T-02 的“tests/pd 只在 Windows 跑”不成立、
   D-25/P-03 的“容量探针未覆盖 CXT 展开”不成立。另修正 3 处文档反向过时
   （`ROADMAP.md:48`、`GAMEPLAY_V2_SPEC.md:2595,2597` 仍称“AGENTS 写 Stage 12”）。
16. **B+E 收尾（2026-11-17）**：
   - B 文档修复：`PLAYER_APPLICATION.md` 选项表由 6 项补到实现支持的 19 项（含候选门控与互斥规则）、
     启动链改为 `preparePlayerLoadBundle` → `adoptStartupLoad`、R 键改为 `RestartAtZero`、
     “所有控制面都走 apply”限定为用户命令；`MODULE_BOUNDARIES.md` 增加 `cuexis_execution_host`
     行与应用层依赖原则、更新日期。
   - E 修复清单：`findings.md` 新增 §9，把 RR-F427–F438 与 S1 项分成 P0/P1/P2 三批，
     每项给出触发条件、修复动作、验证方式与“是否需构建”。

## 证据

```text
python -B tools/verification/gen_docs_map.py --check    -> 0 (728 documents)
python -B tools/verification/gen_docs_limits.py --check -> 0 (docs/guides/LIMITS.md is current)
python -B tools/verification/gen_docs_codes.py --check  -> 0 (docs/guides/DIAGNOSTIC_CODES.md is current)
python -B tools/verification/check_docs_facts.py        -> 0 (SDK 0.9.1, build 26.11.16-1; 35 条历史版本警告)
python -B tools/check_docs.py                           -> 0 (729 Markdown files, 20 candidate JSON/CXT)
git diff --check                                        -> 0
```

规模：714 个文件变更（含计划目录迁移、AGENTS 重写、外部报告归档与 91 条核查写回），
新增 MAP、LIMITS、DIAGNOSTIC_CODES、3 个生成/检查脚本、外部审查报告与 stage-07-rr 证据目录。

## 未做与限制

- **合同语义未重写**：`RUNTIME_SESSION`、`TIMING_MODEL`、`MATERIAL_SHADER` 等与 R3/R4/R5 相关的正文
  等架构决策，本批只做事实订正。
- **去重有限**：Packed 预算 63 份、诊断码表 21 份的重复绝大多数在证据层（已冻结，owner 唯一读者），
  合同层实测仅 8 处 SDK 版本字面量、52 处 FrameDigest 语义引用，不构成应删的重复表。
- **36 条版本漂移为警告**：均为 ADR/计划中记录决策当时版本（含 `stages/stage-01` 的 `07.18.18-1`），
  属历史叙述，未逐一改写；如需严格模式，可收紧豁免标记。
- **未提交**：改动全部留在工作树，未 commit、未推送。
- **目录迁移已完成**（2026-11-17）：`active/stage-07` → `completed/stage-07`，证据层引用一并更新；
  `stage_plans/README.md` 的“当前主路线”已重排为“当前 → 未启动 → 已关闭背景”，Stage 6/7 明确标注 `[已关闭]`。
- **元数据已覆盖**：额外给 `proposals/`（47 篇）、`examples/`、`archive/`（合 70 篇）补了
  `candidate`/`historical` 元数据；未改正文。

## 下一步（可选）

1. 让 `check_docs_facts.py` 校验合同正文里引用的预算/版本数字是否与生成页一致
   （现在只保证生成页本身新鲜，不保证正文没写错）。
2. 为 `FrameDigest` 与阶段序列补生成页或单一引用规则；它们目前只有 MAP 的权威指针。
3. 按 `findings.md` §9 的三批修复清单开工；P0 批次需要构建与 CTest。
