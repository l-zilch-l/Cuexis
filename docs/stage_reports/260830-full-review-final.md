# Cuexis 260829 Full Review 最终关闭报告

状态：closed；2026-08-30 最终实现 SHA 已完成本地与 hosted 门禁，项目所有者接受 Full Review 关闭

本报告是 `260829-full-review.md` 的整改关闭证据。原始审查报告是只读历史快照，未被修改；
`docs/CURRENT_STATUS.md` 仅在本报告完成后更新。本报告关闭的是 Full Review 整改工作，不改变
Stage 5 的独立状态，也不把 deferred 能力写成已实现。

## 1. 结论与接受状态

2026-08-30，项目所有者确认本轮 hosted 结果全部通过，并要求完成 Full Review 最终报告和状态页
关闭。基于最终实现 SHA `fbe118bb310fffa1446584e0a30fd46bc743413b`，144 项 finding 已完成处置：

- P0/P1 正确性、公共边界、异常转换、identity/cache、reload、音频发布、渲染热路径和文档/CI
  风险已实现修复并通过相应聚焦、全量和 hosted 门禁。
- PB-01、CX-01、PB-04、CH-03、RT-04 和 identity 迁移等决策门已按已接受合同完成，不保留旧格式
  的隐式 fallback 或双轨 identity 算法。
- 低风险搭车项和“接受现状”项已在相邻批次及最终 diff 审计中完成处置记录；需要真实规模或
  后续 SDK 版本才能安全实施的项目保持明确 deferred，不作为本轮阻断项。

Full Review 的最终状态为 **closed**。这不表示 Chart/CXC parse-once、RT-29、World/Animation
大规模优化、大包解析降本、Studio、Judgement/Replay、稳定 C ABI 或运行时脚本已经实现。

## 2. 基线、实现和范围

| 项目 | 值 |
| --- | --- |
| 原始审查报告 | [`260829-full-review.md`](260829-full-review.md) |
| 审查证据基线 | `d380fc91e59ed893a3b17128d5d6481d924711e7` |
| 实施分支 | `260829-full-review` |
| 最终实现 SHA | `fbe118bb310fffa1446584e0a30fd46bc743413b` |
| 最终工作树 | clean；与 `origin/260829-full-review` 同步 |
| finding 总数 | 144（P0: 1，P1: 15，P2: 56，P3: 72） |
| SDK API | `0.7.0` |

最终提交之前的两个 hosted 失败均属于编译警告门禁，不是行为回归：

- `bacb0f121104e3879637320e8158048aacaae834` 的 Linux GCC Shared Release 因 shared 配置下
  未使用的 `makeAllocationBoundarySession()` 触发 `-Werror=unused-function`；最终修复限定该
  helper 到 static allocation test。
- 同一 SHA 的 Windows MinGW release 因 `GpuMaterial` 新字段触发
  `-Werror=missing-field-initializers`；最终修复显式初始化新增字段。

## 3. 决策门关闭

| 决策 | 结果 | 证据 |
| --- | --- | --- |
| D1 / PB-01 | Chart v4 renderable 要求 portable `CXPRES01`；legacy payload 稳定拒绝 | [D1/D2 报告](260829-full-review-remediation-d1-d2.md)、ADR 0041 |
| D2 / CX-01 | record-level `extensions` 严格拒绝；document-level opaque extensions 保留 | [D1/D2 报告](260829-full-review-remediation-d1-d2.md)、ADR 0041 |
| D3 / PB-04 | `frame.value_invalid` 保留给 boundary/Validation Sink；extract 只发 non-finite | [D3-D6 报告](260830-full-review-remediation-d3-d6.md)、W30-W37 |
| D4 / CH-03 | 两参 TimingMap 保留 legacy 正 finite BPM；显式 limits 路径执行严格域 | [D3-D6 报告](260830-full-review-remediation-d3-d6.md) |
| D5 / RT-04 | `RenderFrame/renderFrame` 保留并标记 legacy/diagnostic-only | [D3-D6 报告](260830-full-review-remediation-d3-d6.md) |
| D6 | v1/v2/v3 PreparedSemanticIdentity 一次性迁移到完整 canonical Chart bytes | [D3-D6 报告](260830-full-review-remediation-d3-d6.md) |

## 4. 批次和 finding 映射

下表完整承接整改计划 §14。相同 finding 在多个阶段出现是有意的：先做契约/决策，再做实现或
taxonomy；最终状态以本报告和对应批次报告为准。

| 处置路径 | finding | 最终处置 |
| --- | --- | --- |
| 第 0 批 | CH-01、AP-01、AP-02、CM-01、AP-06、PB-08、AP-19、CM-03、RT-02（文档侧） | closed |
| 第 0.5 批 | CH-03、CH-04、CH-12、PB-05、PB-06、PB-10、PB-04（文档归属说明）、RT-05、RT-06、RT-08、RT-25、RT-33、RT-34、CM-02、CM-05、CM-08、CM-10、CM-13、CM-14、CM-20、CM-24（注释）、CM-27、CX-03、CX-04、CX-06、CX-16、CX-26、AP-03、AP-04、AP-05 | closed/contract recorded |
| 第 1 批 Lane A identity/cache | CH-02、CX-02、CX-05、CX-23 | closed |
| 第 1 批 Lane B math | CM-04、CM-06、CM-09、CM-17、RT-16 | closed |
| 第 1 批 Lane C docs/CI | AP-16、AP-17 | closed |
| 第 1 批 Lane D reload | RT-01 | closed |
| 第 2 批 render | RT-26、RT-27、RT-28、AP-08 | closed |
| 第 2 批 audio | CM-21、CM-22、CM-23、CM-24（代码侧） | closed |
| 第 2 批 resource index | PB-12 | closed |
| 第 3 批 prepare/taxonomy | PB-03、PB-04（最终 code 归属）、PB-14、PB-15 | closed |
| 第 3 批 parse-once 安全切片 | CH-05、CH-06、CX-10 | deferred by accepted scope; characterization and ownership parity retained |
| 搭车 Chart | CH-07、CH-08、CH-09、CH-10、CH-11、CH-13、CH-14、CH-15、CH-16 | reviewed; maintenance follow-up, no closure-blocking regression |
| 搭车 Playback | PB-02、PB-07、PB-09、PB-11、PB-13；PB-16 仅记录 Stage 6 设计 | reviewed; PB-16 deferred to Stage 6 |
| 搭车 CXC/assets/shader | CX-07、CX-08、CX-09、CX-11、CX-13、CX-14、CX-17、CX-18、CX-19、CX-20、CX-21、CX-22、CX-24、CX-25、CX-27 | reviewed; maintenance follow-up |
| 搭车 runtime/world/render | RT-03、RT-07、RT-09、RT-12、RT-13、RT-15、RT-21、RT-22、RT-23、RT-24、RT-30、RT-31、RT-32 | reviewed; maintenance follow-up |
| 搭车 core/media | CM-07、CM-12、CM-15、CM-16、CM-18、CM-26 | reviewed; maintenance follow-up |
| 搭车 app/tools/build/tests | AP-09、AP-12、AP-14、AP-15、AP-18、AP-21、AP-22、AP-23、AP-24 | reviewed; maintenance follow-up |
| 等触发 T1 | RT-02（代码侧）、RT-10、RT-11、RT-17、RT-18、RT-19、RT-20、CM-25 | deferred until real large-scale World/Animation workload or Studio pressure |
| 等触发 T2 | CX-12、CM-11 | deferred until real package/performance trigger and owner authorization |
| 等触发 T3 | RT-29 | deferred until 10k-scale render-state evidence or explicit owner trigger |
| 等触发 T4 | AP-07、AP-11；Chart 参数化 typed 化（无独立编号） | deferred to the corresponding Stage 6/API or new Player scenario |
| 接受现状 | CM-19、CX-15、RT-14、AP-10、AP-13、AP-20 | accepted current behavior; recorded |

`AG-CH-COHESION-*` 和 `AG-CH-CANONICAL-I<n>` 是无独立 finding 编号的结构性前置任务；它们已
完成职责边界和 parity 基线，但不改变上述 finding 计数。

## 5. Hosted 验证

三个最终 workflow 均以最终实现 SHA 运行并返回 success：

| Workflow | Run | 结果 |
| --- | --- | --- |
| Linux Quality | [33316147601](https://github.com/l-zilch-l/Cuexis/actions/runs/33316147601) | success |
| Windows MSVC | [33316147625](https://github.com/l-zilch-l/Cuexis/actions/runs/33316147625) | success |
| Windows MinGW | [33316147662](https://github.com/l-zilch-l/Cuexis/actions/runs/33316147662) | success |

Linux Quality 覆盖 GCC/Clang、shared/release、sanitizer、coverage、clang-tidy 和 shader-tools
相关门禁；Windows workflow 覆盖 MSVC 与 MinGW 的 debug/release 矩阵。三个 run 的 `head_sha`
均为 `fbe118bb310fffa1446584e0a30fd46bc743413b`。

首次失败 run 仅用于记录修复链：

- [Linux Quality 33313204623](https://github.com/l-zilch-l/Cuexis/actions/runs/33313204623)：`GCC Shared Release`，已由上文 helper 条件修复关闭。
- [Windows MinGW 33313204541](https://github.com/l-zilch-l/Cuexis/actions/runs/33313204541)：`release`，已由上文显式字段初始化修复关闭。

## 6. 本地验证

最终 SHA 的本地证据包括：

- MSVC Shared Release 全量 CTest `539/539`；静态 Release 编译通过。
- GCC 15.2 WSL Shared Release 编译通过。
- 静态 PB-08 allocation gate `257 assertions` 通过。
- `cuexis_format_check`、`python -B tools/check_docs.py`、`python -B tools/update_version.py --check`
  和 `git diff --check` 全部通过。
- architecture、installed Playback leak、static/shared external-consumer 门禁已通过。

WSL 直接运行 OpenGL 测试因无显示环境失败；这是本地环境限制，不是产品失败。对应 Linux Quality
hosted run 已在同一最终 SHA 成功，因此不阻断关闭。

## 7. 保留合同与残余边界

FrameDigest v1-v3、canonical bytes/order、合法输入 identity（D6 约定的 legacy 一次性迁移除外）、
capability 默认集合、Playback 公共观察面、candidate/active rollback、owner-thread 合同、公共头
ASCII、architecture allowlist、Playback-only consumer 边界和异常模块边界均保持。

以下内容明确不属于本轮实现：

- 完整 Chart/CXC parse-once；本轮只完成 characterization、typed projection 和 ownership/parity
  安全切片，后续必须独立证明 diagnostics、canonical bytes、16 MiB ownership 和性能收益。
- RT-29 不透明 render pass 状态排序；必须先有万级对象证据，并同步 validation sink、summary/digest
  和 golden。
- T1 World/Animation 大规模热帧优化、T2 大包 JSON/CXC 降本、T4 Stage 6 API/Player 剧本。
- Runtime script、逐帧脚本回调、Studio、Judgement/Replay、稳定 C ABI。

这些 deferred 项目已按接受策略记录，不是未识别的阻断项；重新启动时必须重新开任务卡并运行相应
probe、decision 和 hosted gate。

## 8. 关闭记录

- 原始历史报告保持只读。
- 本报告新增后更新 `docs/stage_reports/README.md`。
- 本报告完成后更新 `docs/CURRENT_STATUS.md`，将 Full Review 标为 closed；Stage 5 仍保持其
  独立的 `S5-H local checkpoint; hosted/owner acceptance pending` 状态。
- 关闭提交仅在本地创建，不执行 push。
