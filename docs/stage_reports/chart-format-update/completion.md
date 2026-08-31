# Stage Chart Format Update Completion Report

状态：G6 complete；G5 report-SHA hosted revalidation 已通过，项目所有者已接受，CFU-G 已关闭

报告日期：2026-08-24

权威计划：[Stage Chart Format Update 实施计划](../../stage_plans/completed/chart-format-update/plan.md) §5.9、§11。

## 1. 结论与接受状态

CFU-A 至 CFU-F 的格式、迁移、Playback prepare、CXC 工具、消费者、确定性、安全和性能交付均已有
实际实现证据。CFU-G0 至 G5 已完成；G4 的最终候选 hosted 验证以及 G5 report-SHA revalidation 在
Linux、Windows MSVC 和 Windows MinGW 上全部通过。

本报告的 G5 提交 SHA 为 `6e52677be2f27f83d0b709619be7ffcc141d6f95`，其 report-SHA hosted
revalidation 已完成。项目所有者随后明确接受本报告，G6 记录了阶段封存和 Stage 4 解锁；Stage 4
仍未开始，且本阶段没有实现 AnimationSystem 或放开非空动画 Playback。

项目所有者接受记录：**已接受（2026-08-24；明确回复“接受 G5 completion report”）**。

## 2. SHA、版本与公共边界

| 项目 | 值 |
| --- | --- |
| 最终代码/格式候选 SHA | `4371fdcf04f4f89bfddf070cbb15e4c903810a53` |
| G4 文档与 hosted 验证 SHA | `12f9ef7605b501cbb602338b21bd8eac4c26617b` |
| G5 completion report 提交 SHA | `6e52677be2f27f83d0b709619be7ffcc141d6f95` |
| G6 状态提交 SHA | 由包含本次 G6 更新的 Git commit 标识，避免自引用哈希 |
| Build version | `26.08.01-1` |
| SDK API version | `0.6.0` |
| Chart format | v4 |
| CXT format | v1 |
| CXC format | v1 |
| FrameDigest | v3，公共结构未改变 |

`cuexis_cxc` 仍是内部 target，不提供 `Cuexis::Cxc` public CMake component、安装 package component
或公共 archive 类型。稳定 C ABI 仍延期到 Stage 12。

## 3. CFU-A 至 CFU-G 交付摘要

| 批次 | 实际交付与证据 |
| --- | --- |
| CFU-A | 格式更新范围、外部资产兼容窗口和产品边界盘点；见 [inventory](2026-08-10-inventory.md)。 |
| CFU-B | ADR 0038、Chart v4、CXT v1、CXC v1、参数冻结和 Template Binding 决策已接受。 |
| CFU-C0/C1 | Schema、valid/invalid/golden fixtures、typed source Reader 和内部 manifest target。 |
| CFU-C2 | canonical Writer、参数 resolution/identity、CXT import、deterministic lowering、资源闭包、capability 和 aggregate budgets。 |
| CFU-C3 | strict ZIP32 Stored Reader/Writer、manifest/project closure、owning file/memory package、package identity 和 package-backed provider。 |
| CFU-C4 | `cuexis_cxc_pack`、`cuexis_cxc_validate`、`cuexis_cxc_unpack`，原子输出、no-overwrite、exit `0/1/2` 和 binary round-trip。 |
| CFU-D | v1/v2/v3 历史 Reader、显式 v3/v4 migration、报告 golden、D3 seek/stop 等价；项目所有者记录未提供外部资产。 |
| CFU-E | SDK `0.6.0`、owning typed/CXC source factory、prepare options、capability preflight、PreparedSemanticIdentity 和 transactional reload。 |
| CFU-F | Playback-only consumers、跨平台 deterministic fingerprint、limits/overflow、sanitizer、clang-tidy、coverage 和最大内容趋势。 |
| CFU-G0/G1 | 当前状态校准、文档防回退门禁和 §11 退出条件审计。 |
| CFU-G2 | `AnimationProgramInput` typed handoff、capability、fixtures、budget、diagnostics、ownership 和残余风险。 |
| CFU-G3/G4/G5 | 最终候选本地门禁、同 SHA hosted Linux/MSVC/MinGW 验证、report-SHA revalidation 和六路 artifact parity。 |
| CFU-G6 | 项目所有者接受、格式阶段封存、Stage 4 `unblocked but not started` 状态切换和残余风险保留。 |

## 4. §11 退出条件对照

| # | 条件 | G6 状态 | 证据 |
| ---: | --- | --- | --- |
| 1 | ADR 已接受格式身份、载体、版本、时间域、引用和扩展合同 | PASS | [ADR 0038](../../adr/0038-cxc-v1-and-chart-v4-boundary.md) |
| 2 | 规范、Schema、typed Reader、Writer 和 validator 一致 | PASS | CFU-C1/C2 reports、`schemas/`、Chart tests |
| 3 | v1/v2/v3 历史读取与显式非覆盖迁移 | PASS | [D migration](2026-08-13-d-migration.md)、[D3](2026-08-14-d3-equivalence.md) |
| 4 | 合法/非法、预算、资源、capability、未知扩展 fixtures | PASS | `tests/fixtures/chart_format_update/`、Chart/CXC/Playback tests |
| 5 | prepare/activate/reload/seek 成功与失败 headless evidence | PASS | CFU-F1/F2、Playback tests、G3 local validation |
| 6 | public Playback-only clean external consumer | PASS | CFU-F2 external consumer gates |
| 7 | round-trip、identity、FrameDigest 和顺序确定性 | PASS | CFU-F3、G4 hosted artifact fingerprints |
| 8 | CXC metadata、entry order、package bytes 跨平台 golden | PASS | G4 hosted six-way CXC SHA parity |
| 9 | 空动画 v4 成功；非空动画稳定拒绝 | PASS | CFU-E2/F1/F2、diagnostic signature |
| 10 | PlaybackSource、CXT、ParameterSet 所有权与 identity 边界 | PASS | CFU-C3/E1/E3、architecture/public-header gates |
| 11 | archive license、链接闭包、安装清单和退出路径 | PASS | dependency policy、NOTICE、external consumer gates |
| 12 | E0 API、SDK compatibility、symbol/export、consumer | PASS | CFU-E close、SDK `0.6.0` gates |
| 13 | pack/validate/unpack exit code、atomicity、no-overwrite、cleanup | PASS | CFU-C4 tools gates |
| 14 | 外部旧资产清单或“未提供外部资产”决策 | PASS | CFU-D close，兼容窗口保留 |
| 15 | 最终 SHA hosted Linux/Windows 结果和失败记录 | PASS for report SHA `6e52677...` | G5 report-SHA revalidation runs below；Linux 首次失败为 vcpkg 下载瞬态，failed-job rerun 通过 |
| 16 | completion report 已创建并由项目所有者接受 | PASS | 本报告已创建；项目所有者于 2026-08-24 明确接受 |
| 17 | Stage 4 字段、延期职责与风险已交接 | PASS | [G2 handoff](2026-08-16-g2-stage4-handoff.md) |
| 18 | 权威状态与链接一致 | PASS | `tools/check_docs.py`、各级 docs index |

当前顺序门禁为：`18 PASS / 0 PENDING / 0 PRODUCT BLOCKER`；G6 状态切换已完成。

## 5. 本地验证

G3 本地候选报告记录了以下实际结果；G4/G5/G6 文档检查在本工作树重新执行：

| 门禁 | 结果 |
| --- | --- |
| Debug fresh configure/build/CTest | passed；`378 passed / 1 skipped` |
| Release fresh configure/build/CTest | passed；`378 passed / 1 skipped` |
| Headless Release CTest | passed；`343 passed / 1 skipped` |
| External consumers | Debug/Release 各 `7/7` passed |
| Architecture | Debug、Release、Headless 各 `1/1` passed |
| Format | `cuexis_format_check` passed |
| Installed public-header ASCII | passed |
| License/NOTICE closure | passed |
| Version | `tools/update_version.py --check`，`26.08.01-1` |
| Documentation | `tools/check_docs.py` passed；150 Markdown、20 candidate JSON/CXT |
| Whitespace | `git diff --check` passed |

唯一 skip 是当前 Windows 环境无法创建 symlink 时的 physical-containment escape 测试条件，不是
失败。MinGW shared 的实验性 `.dll.a`/symbol-tool 差异不属于 hosted 支持矩阵；hosted MinGW 仅执行
静态 Debug/Release。

## 6. Hosted workflow 证据

G5 report SHA `6e52677be2f27f83d0b709619be7ffcc141d6f95` 的三套 workflow 均已完成并成功：

| Workflow | Run | Jobs |
| --- | --- | --- |
| Linux Quality | [32710728943](https://github.com/l-zilch-l/Cuexis/actions/runs/32710728943) | GCC Coverage; GCC Shared Release; GCC Release; Clang Shared Debug; Clang ASan + UBSan; clang-tidy |
| Windows MSVC | [32710729026](https://github.com/l-zilch-l/Cuexis/actions/runs/32710729026) | debug; release |
| Windows MinGW | [32710728985](https://github.com/l-zilch-l/Cuexis/actions/runs/32710728985) | debug; release |

每个列出的 job conclusion 均为 `success`。Linux 首次失败的 GCC Release external consumer 配置
阶段是 vcpkg `libmount` 下载瞬态，failed-job rerun 通过，未修改源码或构建逻辑。

## 7. Deterministic artifact 与 F4 evidence

六份 F3 artifact（Linux Release、Linux Clang Shared Debug、MSVC Debug/Release、MinGW Debug/Release）
的关键字段一致：

```text
CXC bytes                         6905
CXC SHA-256                       1cb2fcbf7a852a4db2ed9119359c68ff1cc4a06d1d41d18018fff89cf737d723
Prepared semantic identity        6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5
Stop FrameDigest v3               11596562486377158370
Migration Chart SHA-256           1ca9f60feee215fdc4eca1f7cafbbea8704976eca18ed40e51333b6b2e7a5385
Migration report SHA-256          81df14e422603ae411ea8a70f5de89fb49ae2a34b804af71146a8be3075824e0
Diagnostics fingerprint SHA-256   9f7f98fc3bedf81588e62011fd3a49587014b3390aae3906404498e7b60176a0
```

Stable non-empty animation rejection signature:

```text
playback.capability.unsupported@$/animationClips#cuexis.animation.clip.v1|playback.capability.unsupported@$/objects#cuexis.animation.layers.v1
```

F4 hosted evidence includes ASan/UBSan focused `9/9`, clang-tidy `68/68` build steps, combined coverage
of lines `80.4%`, functions `96.8%`, branches `44.8%`, and a 67,113,275-byte maximum-content trend probe.
Performance remains trend-only; no runner-specific hard threshold was introduced.

## 8. Stage 4 handoff and retained exclusions

Stage 4 consumes owning typed `AnimationProgramInput` data, including Clip, Object, Layer, BlendGroup,
ClipInstance, mask, priority, weight, fill and iterations. It must not parse JSON, CXC or CXT in
`engine/animation/`.

The following remain outside CFU-G and the completed format-stage scope:

- `AnimationSystem`, sampling, blending, `PropertyResolver`, OverrideToken and World write-back;
- non-empty Chart v4 animation Playback;
- public `Cuexis::Cxc` package API or component;
- runtime scripts, per-frame callbacks, expressions or bytecode;
- FrameDigest version/FrameSnapshot structure changes;
- stable C ABI and language bindings.

## 9. Residual risks

- Stage 4 must preserve typed-input ownership and avoid hot-path allocations.
- Seek/reload reconstruction semantics and long-term stability of the public typed Chart handoff remain
  Stage 4 review items.
- Windows symlink capability is unavailable in the current local environment; the hosted matrix remains
  the cross-platform evidence source.
- Performance figures are trend observations, not universal limits.

## 10. Acceptance and next action

G6 status is **complete**. The report-SHA hosted revalidation results are recorded above, and the project
owner explicitly accepted this report on 2026-08-24. The G6 state transaction:

1. marked Stage Chart Format Update completed and CFU-G closed;
2. changed Stage 4 to `unblocked but not started`;
3. updated the authoritative status/index documents and acceptance record; and
4. will be finalized by merging the branch into `stage-ChartFormatUpdate` after the G6 workflow gate.

`.codex/` is intentionally excluded from the G5/G6 submissions and must not be reintroduced in later commits.
