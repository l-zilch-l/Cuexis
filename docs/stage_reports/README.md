# Stage Report Index

状态：现行阶段报告索引

更新日期：2026-08-29

报告是带日期的证据快照。报告中的“下一步”只表示报告生成时的交接，不自动代表当前任务。

## 完成报告

| 报告 | 状态 |
| --- | --- |
| [Stage 0](stage_0_completion_report.md) | completed |
| [Stage 1A](stage_1a_completion_report.md) | completed |
| [Stage 1B](stage_1b_completion_report.md) | completed |
| [Stage 1C](stage_1c_completion_report.md) | completed |
| [Stage 1D](stage_1d_completion_report.md) | completed |
| [Stage 1E](stage_1e_completion_report.md) | completed |
| [Stage 2](stage_2_completion_report.md) | completed |
| [Stage 3](stage_3_completion_report.md) | completed |
| [Stage 4](stage_4_completion_report.md) | completed |

## Review、盘点和历史验证

| 报告 | 角色 |
| --- | --- |
| [260722 Stage 1C 全量审查](260722-1c-review.md) | review evidence, closed |
| [260806 Stage 2 review](260806-stage-2-review.md) | historical review snapshot |
| [260808 Stage 3 review](260808-stage-3-review.md) | historical review snapshot |
| [260810 Chart Format Update inventory](260810-chart-format-update-inventory.md) | CFU-A inventory |
| [260811 Chart Format Update CFU-C0 baseline](260811-chart-format-update-c0-baseline.md) | accepted implementation baseline and dependency decision |
| [260811 Chart Format Update CFU-C1 Reader](260811-chart-format-update-c1-reader.md) | Schema, fixture and typed source Reader checkpoint |
| [260811 Chart Format Update CFU-C2 lowering](260811-chart-format-update-c2-lowering.md) | canonical Writer, parameter resolution, CXT import and deterministic lowering checkpoint |
| [260812 Chart Format Update CFU-C3 CXC](260812-chart-format-update-c3-cxc.md) | strict ZIP32 envelope, package closure, owning loaders and package-backed content domains |
| [260813 Chart Format Update CFU-C4 tools](260813-chart-format-update-c4-tools.md) | completed developer pack/validate/unpack tools, atomic staging, exit-code, round-trip and hosted cross-platform gates |
| [260813 Chart Format Update CFU-D1/D2 migration](260813-chart-format-update-d-migration.md) | closed D1/D2 explicit v1/v2/v3 → v4 JSON lift and CLI `--target` after local Debug verify |
| [260814 Chart Format Update CFU-E0 API](260814-chart-format-update-e0-api.md) | accepted Playback public API and SDK `0.6.0` implementation target; E0 closed, E1 next; no installed-header or version implementation yet |
| [260814 Chart Format Update CFU-E1 source](260814-chart-format-update-e1-source.md) | closed unified PlaybackSource state, typed/CXC factories, SDK `0.6.0`, static/shared consumers and local Debug/Release gates; E2 next |
| [260814 Chart Format Update CFU-E2 prepare](260814-chart-format-update-e2-prepare.md) | closed Chart v4 prepare, public parameter conversion, format capability preflight, static Runtime projection and pre-Stage-4 animation rejection; E3 next |
| [260814 Chart Format Update CFU-E3 identity](260814-chart-format-update-e3-identity.md) | closed PreparedSemanticIdentity combiner, cross-source identity parity and transactional reload observation; E4 next |
| [260814 Chart Format Update CFU-E4 gates](260814-chart-format-update-e4-gates.md) | closed local Debug/Release/shared E-batch gates, consumer identity observation and shared export/import |
| [260814 Chart Format Update CFU-E close](260814-chart-format-update-e-close.md) | owner-accepted CFU-E close after hosted Linux/Windows/MinGW success on `2dc5f6c` |
| [260814 Chart Format Update CFU-D3 equivalence](260814-chart-format-update-d3-equivalence.md) | closed Playback FrameSnapshot / FrameDigest v3 / seek-stop equivalence for lifted empty-animation v4 |
| [260814 Chart Format Update CFU-D close](260814-chart-format-update-d-close.md) | owner-accepted CFU-D close after “no external assets”; compatibility window retained |
| [260815 Chart Format Update CFU-F1 headless](260815-chart-format-update-f1-headless.md) | locally completed public-Playback headless reference, cross-source identity/frame parity and atomic failure paths; F2 next |
| [260815 Chart Format Update CFU-F2 package consumers](260815-chart-format-update-f2-package-consumers.md) | locally completed static/shared Playback-only external consumers, installed-header isolation and InternalCxc link closure; F3 next |
| [260815 Chart Format Update CFU-F3 determinism](260815-chart-format-update-f3-determinism.md) | locally completed canonical CXC/migration/identity/diagnostic fingerprint and final-SHA hosted evidence wiring; F4 next |
| [260815 Chart Format Update CFU-F4 safety and performance](260815-chart-format-update-f4-safety-performance.md) | locally completed limits/overflow/diagnostic/allocation gates plus SHA-bound sanitizer, analysis, coverage and maximum-content performance evidence wiring; closed by the CFU-F close report |
| [260816 Chart Format Update CFU-F close](260816-chart-format-update-f-close.md) | owner-directed CFU-F close after final-SHA Linux/MSVC/MinGW success, six-way deterministic parity and F4 sanitizer/analysis/coverage/performance evidence |
| [260816 Chart Format Update CFU-G1 exit audit](260816-chart-format-update-g1-exit-audit.md) | completed §11 evidence audit: 15 PASS, 1 final-SHA RERUN, 2 documentation/owner gates, no product-code blocker; G2 is recorded by the following handoff report |
| [260816 Chart Format Update CFU-G2 Stage 4 handoff](260816-chart-format-update-g2-stage4-handoff.md) | completed typed AnimationProgramInput/capability/fixture/budget/diagnostic/ownership/risk handoff; followed by the G3 validation report below |
| [260819 Chart Format Update CFU-G3 validation](260819-chart-format-update-g3-validation.md) | local candidate gates passed; candidate publication and same-SHA hosted Linux/MSVC/MinGW evidence blocked by the execution environment; CFU-G remains active |
| [260820 Chart Format Update CFU-G4 closure readiness](260820-chart-format-update-g4-closure-readiness.md) | historical offline readiness snapshot; superseded for hosted status by the 2026-08-24 verification report |
| [260824 Chart Format Update CFU-G4 hosted verification](260824-chart-format-update-g4-hosted.md) | historical G4 same-SHA Linux Quality, Windows MSVC and Windows MinGW validation; superseded for final status by the completion report |
| [Stage Chart Format Update completion report](stage_chart_format_update_completion_report.md) | G5 report-SHA hosted revalidation passed; G6 owner acceptance recorded 2026-08-24; CFU-G closed and Stage 4 unblocked but not started |
| [260824 Stage 4 S4-A module wiring](260824-stage-4-s4-a-module-wiring.md) | S4-A local checkpoint: STATIC `cuexis_animation`, typed input header, compile contract and diagnostic freeze; nonempty animation Playback still rejected |
| [260825 Stage 4 S4-B absolute sampling](260825-stage-4-s4-b-absolute-sampling.md) | S4-B local checkpoint: seekable local Beat sampling, fill/iterations, continuous/step tracks; nonempty animation Playback still rejected |
| [260826 Stage 4 S4-C layer mixing](260826-stage-4-s4-c-layer-mixing.md) | S4-C local checkpoint: Override/Additive Layer mixing into PropertyWriteBuffer; nonempty animation Playback still rejected |
| [260827 Stage 4 S4-D PropertyResolver](260827-stage-4-s4-d-property-resolver.md) | S4-D local checkpoint: unified PropertyResolver, OverrideToken, BasePropertyCommand and Runtime orchestration; nonempty animation Playback still rejected |
| [260827 Stage 4 S4-E Playback ownership](260827-stage-4-s4-e-playback-ownership.md) | S4-E local checkpoint: Playback compile/own/commit of animation state under opt-in capability; default Session still rejects nonempty animation |
| [260827 Stage 4 S4-F consumer capability](260827-stage-4-s4-f-consumer-capability.md) | S4-F local checkpoint: public animation capability constants in default `allCapabilities()`; consumer/FrameDigest v3 parity; trimmed Session still rejects |
| [260827 Stage 4 S4-G safety allocation](260827-stage-4-s4-g-safety-allocation.md) | S4-G local checkpoint: ChartLimits/write budget, warmed zero or bounded allocation, F4-style performance probe |
| [260827 Stage 4 S4-H local validation](260827-stage-4-s4-h-local-validation.md) | historical S4-H local Debug/Release snapshot before final SHA publication |
| [Stage 4 completion report](stage_4_completion_report.md) | S4-H hosted Linux/MSVC/MinGW success on `3df9274`; owner acceptance recorded 2026-08-27; Stage 4 closed and Stage 5 unblocked but not started |
| [260828 Stage 5 S5-A contracts](260828-stage-5-s5-a-contracts.md) | S5-A contract freeze: ADR 0040, Material/Shader spec, refined S5-B..H; no production code |
| [260828 Stage 5 S5-B shader tools](260828-stage-5-s5-b-shader-tools.md) | S5-B local checkpoint: optional cuexis_shader, vcpkg shader-tools, importer --help; no kind 4/5 parse |
| [260828 Stage 5 S5-C material/unlit](260828-stage-5-s5-c-material-unlit.md) | S5-C local checkpoint: kind 4/5 parse/identity, Asset Index v3 shader, SDK 0.7.0; default Session still Unlit-only |
| [260828 Stage 5 S5-D shader compile](260828-stage-5-s5-d-shader-compile.md) | S5-D local checkpoint: GLSL 450 → SPIR-V → GLSL 330/ES 300 and reflection behind shader-tools; no CXSCCH01 |
| [260828 Stage 5 S5-E profile/capability](260828-stage-5-s5-e-profile-capability.md) | S5-E local checkpoint: toolchain profile IDs, PresentationCapabilities version 2, parse → playback → presentation preflight; no CXSCCH01 |
| [260828 Stage 5 S5-F cache](260828-stage-5-s5-f-cache.md) | S5-F local checkpoint: CXSCCH01 Writer/Reader, canonical cache key, importer write, failed hot-reload keeps active; OpenGL consumption not started |
| [260828 Stage 5 S5-G consume](260828-stage-5-s5-g-consume.md) | S5-G local checkpoint: OpenGL/Player/Validation Sink consume CXSCCH01; default allCapabilities includes shader/parameterized; trimmed Session still rejects; Unlit golden unchanged |
| [260828 Stage 5 S5-H safety](260828-stage-5-s5-h-safety.md) | S5-H local checkpoint: spec budgets, warmed allocation contract, performance probe, Linux sanitizer shader-tools; hosted and owner acceptance pending; Stage 5 not completed |
| [260828 CFU and Stage 4 review](260828-stage-CFU-4-review.md) | open two-axis review snapshot of CFU through Stage 4 close; does not redefine CURRENT_STATUS |
| [260829 full review](260829-full-review.md) | open full-repository review snapshot (engine, app, tools, build, docs alignment); 144 findings with per-finding index (1 P0 / 15 P1 / 56 P2 / 72 P3), 11 prioritized improvement directions with implementation assessment and full finding-to-batch remediation mapping; does not redefine CURRENT_STATUS |
| [260829 Full Review 整改第 0 批](260829-full-review-remediation-b0.md) | completed local B0 remediation: CH-01, PB-08, CM-03, AP-01/02/06/19, CM-01, and RT-02 documentation correction |
| [260829 Full Review 整改第 0.5 批](260829-full-review-remediation-b05.md) | completed local B05 documentation and contract-comment remediation: CM core/audio/platform, CX-03/04/06/16/26, and AP-03/04/05; decision-gated findings remain unresolved |
| [260829 Full Review 整改 W06/W07](260829-full-review-remediation-w06-w07.md) | completed Playback and Runtime/OpenGL contract documentation; PB-04 and RT-25 remain decision-gated and unresolved |
| [260829 Full Review 整改 W08/W09](260829-full-review-remediation-w08-w09.md) | completed Chart reader characterization and controlled internal responsibility split; decision-gated findings remain unresolved |
| [260829 Full Review 整改 W10](260829-full-review-remediation-w10.md) | completed AP-16 machine-readable status contract and checker regressions; AP-17 Linux Quality CI wiring remains pending C3 |
| [260829 Full Review 整改 W11](260829-full-review-remediation-w11.md) | partial W11 math API verification; Lane A identity migration remains blocked by unresolved D6 |
| [260829 Full Review 整改 W12/W13](260829-full-review-remediation-w12-w13.md) | completed Lane B math/core remediation and Lane D Runtime reload transaction fix; Lane A remains blocked by unresolved D6 |
| [260829 Full Review 整改 C2/C3](260829-full-review-remediation-c2-c3.md) | completed target-fact and SDK-minor documentation contracts plus Linux Quality workflow wiring; hosted Linux evidence remains pending |
| [260829 Full Review 整改 W14](260829-full-review-remediation-w14.md) | completed A3 pipeline characterization review and revalidated the C1 status-contract checker; A3 production integration remains deferred to W15 |
| [260829 Full Review 整改 W15/W16](260829-full-review-remediation-w15-w16.md) | completed A3 ShaderPipelineCache integration, corrected Player adapter-failure smoke coverage, and revalidated C2 target export plus D1/D2 reload gates |
| [260829 Full Review 整改 W20-W26](260829-full-review-remediation-w20-w26.md) | local completed render hot-path, HostClock/SDL publication, Player scratch reuse, and transparent Presentation resource lookup remediation |
| [260829 Full Review 整改 W30-W37](260829-full-review-remediation-w30-w37.md) | local completed Playback prepare staging, PB-03 diagnostics, taxonomy, ownership, and final Debug/Shared/Release gates; full Chart/CXC parse-once remains separately bounded |
| [260829 Full Review 整改 D1/D2](260829-full-review-remediation-d1-d2.md) | completed accepted legacy-format exit for Chart v4 presentation and Asset Index record-level extensions; hosted revalidation remains pending |
| [SDK transition verification](sdk_transition_verification.md) | historical verification snapshot |
