# Stage 6 复核：切片与子代理原文附录

状态：historical review snapshot
快照日期：2026-09-28
更新日期：2026-09-28

本附录保留 20 个子代理（10 片 × 2 轴）的原始报告，供追溯。汇总与去重后的结论见
[Standards 记录](2026-09-28-standards.md) 与 [Spec 记录](2026-09-28-spec.md)；
范围、方法与门禁结果见[汇总](2026-09-28-summary.md)。跨轴不合并、不重排；本附录按切片罗列。

## 1. 切片与来源

| 片 | 主题 | diff path filter（基线 `13dab93...eaaf375`） | spec 来源 |
| --- | --- | --- | --- |
| S1 | Foundation 加固 R0–R5（PR #25） | `engine/chart engine/cxc tests/chart tests/cxc cmake CMakeLists.txt engine/CMakeLists.txt` | 加固计划（R0–R5）、R0–R5 报告、R5 §10、`PACKED_CHART_FORMAT.md`、handoff review |
| S2 | A2 合同 + B1 版本门禁 + 版本滚动 | `tools/check_version_gate.py tools/check_version_gate_tests.py tools/check_stage6_a2.py tests/version_gate tests/fixtures/stage6_a2 .github vcpkg.json CMakeLists.txt tools/CMakeLists.txt tests/CMakeLists.txt .gitattributes` | plan §S6-A2/§S6-B1、ADR 0042 S6-D07/D08、`VERSIONING.md`、B1 与 A2 报告、findings 问题 1 |
| S3 | C1 candidate source / CXC / typed lowering | `engine/chart engine/cxc engine/playback engine/runtime tools/cxc_common tests/playback tests/runtime tests/cxc tests/CMakeLists.txt` | plan §S6-C1、ADR 0042 S6-D01–D03、C1 实现与退出报告、R5 §10 |
| S4 | D1/D2 渲染边界与 OpenGL 迁移 | `engine/presentation_renderer engine/render_opengl engine/render tests/presentation_renderer tests/render` | plan §S6-D1/§S6-D2、ADR 0042 S6-D04、D1/D2 报告、findings 问题 2 |
| S5 | C2 配置与音频设备 | `engine/player_support schemas tests/player_support engine/audio_sdl engine/platform tests/audio_sdl` | plan §S6-C2、ADR 0024、ADR 0042 S6-D05、C2 报告、`STAGE6_CONFIG_AND_MEDIA.md` |
| S6 | C3 Player 控制与事务 | `app/player tests/player` | plan §S6-C3、ADR 0042 S6-D05、C3 退出报告、`PLAYER_APPLICATION.md` |
| S7 | E1/E2 离线媒体导入 | `tools/media_import tools/media_importer tests/media_import tests/fixtures/stage6_e vcpkg-overlays vcpkg.json THIRD_PARTY_NOTICES.md cmake/VerifyMediaImporter.cmake .gitattributes` | plan §S6-E/§S6-E1/§S6-E2、ADR 0042 S6-D06、`STAGE6_CONFIG_AND_MEDIA.md`、E1/E2 两份报告、`S6-G10`/`S6-G11` |
| S8 | E3 资源身份/缓存/原子发布 | `tools/asset_publish tests/asset_publish` | plan §S6-E3、ADR 0042 S6-D06、E3 两份报告、`S6-G12` |
| S9 | C4 参考宿主与分发 | `examples/reference_host cmake` | plan §S6-C4、§S6-A2 宿主特别要求、ADR 0042 S6-D08、C4 两份报告、`S6-G13`、`docs/api/README.md` |
| S10 | F1/F2 证据链与文档 | `docs AGENTS.md tools/check_docs.py tools/check_docs_status_contract_tests.py` | plan §3.1/§S6-F1/§S6-F2、completion.md、F1 记录、各批次退出报告、findings、`DOCUMENTATION_POLICY.md` |

说明：部分子代理运行环境没有 shell，改用直读工作区文件（`eaaf375`，与 diff 结果一致），
已在各条注明；父代理对高/中严重度发现做了独立复核，结论以两轴记录中的"父代理已核实"标记为准。

**本附录保留子代理原文，不代表最终判定。** 二轮细化后有 4 条被驳回、1 条被撤回、
10 条被改级或改状态，逐条对照见[细化记录 §4](2026-09-28-refinement.md)与
[汇总的被驳回项](2026-09-28-summary.md)；文中已加删改标记的位置以两轴记录为准。

## 2. S1 — Foundation 加固 R0–R5

### 2.1 Standards

未发现硬违规：`engine/chart/include/**`、`engine/cxc/include/**` 全 ASCII（0 处非 ASCII）；
`Result` 无被忽略；命名与命名空间合规。判断项：

1. **Duplicated Code（强）**：3 行 `error/fail(std::string,std::string)->core::Error` 工厂在 5 个文件各一份
   —— `engine/chart/src/packed_chart_primitives.cpp:20`、`packed_chart_io.cpp:35`、
   `packed_chart_tables.cpp:26`、`packed_profile.cpp:16`、`packed_semantic_identity.cpp:33`。
2. **Duplicated Code**：Foundation profile 字面量两处各一份——
   `engine/chart/src/cxt_v2_loader.cpp:784`/`1509`/`1524` 与 `packed_profile_internal.hpp:20-24`；
   生产者与校验者各自硬编码同一注册值，易漂移。
3. **possible Repeated Switches / Duplicated Code**：`packed_chart_tables.cpp:46-49`（section 注册表）
   与 `:1262-1263`（required section 清单）各自硬编码 section code；spec §5.3 要求"同一注册表判定"，
   因 required 与 registered 语义确不同，列为判断。
4. **possible Middle Man**：`packed_chart_io.cpp:220-234`（`PackedChartWriter::size`）与
   `:285-297`（`PackedChartReader::decode`）基本只转发 `packed::encode/inspect/decode`。
5. **possible Primitive Obsession**：`packed_chart_tables.hpp:34` 用 `std::array<std::uint8_t,32>`
   表示 semantic identity；`packed_chart_primitives.hpp:67-68` 的别名间接层可省。
6. **possible Speculative Generality**：`canonical_semantic_chart.hpp:58`(`HalfOpenRange`)、`:64`(`endBeat`)、
   `:87`(`effects`) 在 Foundation 全被拒绝：但 spec §8 显式要求 typed 占位并禁止 opaque JSON，
   按"仓库文档覆盖基线"压掉。
7. `TypedReference{domain,id}`（`canonical_semantic_chart.hpp:45`）用字符串承载域类别属 Primitive Obsession；
   spec §6.2/R4 明确要求文本 domain，压掉。

### 2.2 Spec

H01–H04 的修复确实落到代码与测试，不是只写报告（`packed_profile.cpp:22` 唯一判定被
`packed_chart_tables.cpp:889`/`:2031` 双侧调用；`packed_semantic_identity.cpp:31` 有 Spec §10.1 域串；
R1–R4 用例已注册）。全仓 0 个 `S6-*` 标识符。发现：

1. **(a)** 加固计划 §2.2「不增加公共 SDK 类型」——R1 向 `engine/chart/include` 新增
   `PackedSemanticIdentity`/`semanticPreimage`/`semanticIdentity`/`semanticIdentityHex`。
   （**父代理已驳回**：该目标不进入安装导出集。）
2. **(a)** 计划 §2.3「报告必须对应实际调用路径」——R1 报告 §2 把 R1.5 记在
   `tools/cxc_common/src/cxc_candidate.cpp`，该文件实为 12 行转发（`:7-9`），真正比对在
   `engine/cxc/src/cxc_candidate.cpp:375-386`。
3. **(b) 未被要求**：`engine/chart/src/candidate_lowering.cpp:133` 新增 `validateFoundationProfile` 调用，
   使 CXT→canonical lowering 也硬拒 out-of-profile；计划 §2.1 只要求 Writer/Reader/CXC 的一致拒绝，
   R2 报告 0 处提及 lowering。
4. **(c) 实现可疑**：Spec §3.3（`PACKED_CHART_FORMAT.md:146`）把 `packed.budget.section_bytes`
   定义为 section 字节预算，代码却用它报非预算的 uint32 wire-range 溢出：
   `packed_chart_tables.cpp:35`、`:989`、`:1012`。

## 3. S2 — S6-A2 / S6-B1

### 3.1 Standards

未发现硬违规（版本四处一致、SDK API 未提前升 `0.7.1`、格式符合 `yy.mm.dd-v`）。判断项：

- `.github/workflows/version-gate.yml:52-65`、`:98-111`、`:141-154` Duplicated Code
  （trusted-baseline materialize 循环 + `version.bootstrap.required` 失败块三处近乎逐字重复）；
  同处 `run: |` 内缩进不齐（54/63/66 行各多一空格）。
- `tools/check_version_gate.py:275-279` `_result_json` 冗余死代码（`asdict(result)` 已含两字段，紧接重新赋同值）。
- `tools/check_version_gate.py:88-92` 与 `tools/update_version.py:150-158` `check_current` 同形同文案。
- `tools/check_version_gate.py:97-104,232-240` Data Clumps + Primitive Obsession
  （`(trusted_utc_date, context, allow_sdk_api_change)` 结伴传递，`context` 是裸字符串 + 内联集合校验）。
- `tools/check_stage6_a2.py:245` `check_text_boundaries` 名字未揭示它还断言禁止依赖边与必需 target。
- `.gitattributes` 缺 `tests/fixtures/stage6_a2/**` 的 eol 规则（对比已有 `chart_format_update/**` 条目）。
- `tools/check_stage6_a2.py` 未接入 CI/CMake（`linux-quality.yml:218` 只跑 `check_docs.py`）；
  报告已声明它是手工门禁，故不计违规。

### 3.2 Spec

（该子代理环境无 shell，证据来自读文件；已核对用例 11 个与报告一致。）

1. **(a)** 计划 `:194-195`「两个 PR 竞争、跨日重新发布与历史复验负例/边界」：11 个用例中**没有任何并发
   PR 竞争用例**；bootstrap/缺文件路径只由字符串断言（`:216`、`:224-228`）覆盖，"缺失基线"只用全零假 SHA
   （`:168-177`）。
2. **(a)** 计划 `:140-142`「八项…形成独立 golden 和最小表征」：`check_stage6_a2.py:245-263` 仅表征
   D01/D02/D04/D06，**S6-D07/S6-D08 无表征**。
3. **(a)** 计划 `:171`「所有新增合同必须可测试」：脚本未注册进任何 CTest/CI。
4. **(c)** 计划 `:185-186`「历史 SHA 的复验使用其记录的发布上下文」：`--context`
   （`check_version_gate.py:105-106`）只做枚举校验，live/historical 走完全相同的日期规则；
   测试只断言 `result.context`（`check_version_gate_tests.py:97-104`），传当天日期同样通过。
5. **(c)** ADR 0042:321「修改门禁本身需要代码所有者复核与独立负例」：测试读取的是 `TRUSTED_ROOT`
   中的 workflow 副本（`check_version_gate_tests.py:19,199-229`），候选修改
   `.github/workflows/version-gate.yml` 无法被任何在库用例发现；仓库无 `CODEOWNERS`（0 命中）。
6. **(c)** `check_version_gate.py:134-137` 同日 build 上限时先抛 `version.build.exhausted`，
   抢在 `version.unchanged` 之前。
7. **(b)** `--event`（`:292`、`:331`）只用于打印，无任何规则消费。

## 4. S3 — S6-C1

### 4.1 Standards

**硬违规**

1. `engine/cxc/src/cxc_candidate.cpp:533` 用了稳定码 `cxc.budget.exceeded`，而
   `docs/formats/CHART_ENTRY_V1_FORMAT.md:122` 规定为 `cxc.candidate.budget_exceeded`
   （CODE_POLICY「可序列化诊断使用稳定 code」）；同文件 `:356` 用的是候选码。

**判断项**

- Duplicated Code：`:182-194` 与 `:485-497` 逐字相同的 11 元素 `constexpr std::array fields`。
- Duplicated Code：`engine/playback/src/playback_source.cpp:852-893` 与 `:895-936` 约 30 行同构。
- Duplicated Code（跨模块）：`candidate_lowering.hpp:42` 内联 `"candidate.static-tap-lanes4-v1"`
  重复了 `cxc_candidate.cpp:27` 的命名常量；`countRequirements`（`:109`）重复
  `candidate_lowering.cpp:122-131` 的计数循环。
- Middle Man：`tools/cxc_common/src/cxc_candidate.cpp:7-9` 纯转发。
- Speculative Generality / 死参数：`cxc_candidate.cpp:55` `readRequiredString` 第三参数无名且从未使用。
- Speculative Generality：`candidate_lowering.hpp:40-41` 的 `flags{1}`/`candidateRevision{1}` 只写不读。
- Data Clumps：`engine/cxc/include/cuexis/cxc/cxc_candidate.hpp:26-32` 六字段结伴。
- Primitive Obsession：同上 `:27-29` 用 `std::optional<std::string>` 承载 SHA-256 身份。

未发现违规：公开头全 ASCII；`Result` 均已检查；异常在工厂边界转稳定 Error；
execution ID（`v5g1:`）与 prepared identity 预映像逐字节符合 `CHART_ENTRY_V1_FORMAT.md:145-155`
与 `STAGE6_CONFIG_AND_MEDIA.md:109-121`。

### 4.2 Spec

**正面确认**：未走 v4 JSON 中转（`playback_session.cpp:1878-1891`）；默认 v4 路由未改
（`playback_source.cpp:762`、`:792`）；candidate 失败不回退读同包 v4；工具层已收为纯转发。

**二轮更正**：本节第 1 条（A16 feature/resource closure 缺生产入口，原 SPEC-09）**已被父代理撤回**——
`packed_chart_tables.cpp:2009-2027` 在 decode 侧派生闭包并在 `:2031` 校验 profile。
其余各条的判定保留，详见[细化记录 §4](2026-09-28-refinement.md)。

1. **(a)** 计划 §S6-C1 #6 + ADR 0042 S6-D03：实现只校验**已声明** feature
   （`packed_profile.cpp:34-52`），闭包原样透传（`candidate_lowering.cpp:307`）；全仓无派生
   feature/closure 生产入口，正例在测试手写 `CanonicalFeature{...}`
   （`tests/cxc/cxc_candidate_roundtrip_tests.cpp:48`、`tests/playback/playback_candidate_tests.cpp:50`）。
2. **(a)** 验收「预算边界、零限制和 stale candidate 稳定失败」：零上限与 C1 专用 stale 用例仍缺
   （exit `:61-63`）；`candidate.identity.resource_conflict` 无独立用例。
3. **(c)** exit `:62`「缺资源的 `candidate.identity.resource_conflict` 已在 prepare 中拒绝」不准确：
   缺资源走 `playback.identity.resource_missing`（`playback_session.cpp:851/861/872/889`）；
   `candidate.identity.resource_conflict` 实为「空 assetId 或 audio+presentation 混合用途」（`:681-685`）。
4. **(b)** `lowerCandidateRuntime` 额外按 execution ID 重排 `RuntimeObject`
   （`candidate_lowering.cpp:277-303`）；`candidateResourceRequirements` 额外拒绝
   audio+presentation 混合资产（`playback_session.cpp:681`）。

## 5. S4 — S6-D1/D2

### 5.1 Standards

未发现工具可强制的硬违规（`presentation_renderer` 无 GL/SDL 头、纯 ASCII、无被忽略的 `Result`）。
主要问题是重复实现与转发：

- **契约偏差（judgement，D2 已自认）**：`2026-09-22-s6-d1-renderer-contract.md:23` 声明
  `buildPresentationCommands` 是"唯一的排序、分 pass 和 summary digest 实现"，但
  `OpenGlBackend::submit` 仍走自有 `buildDraws`+`summaryDigest`
  （`open_gl_presentation.cpp:923`、`:810`），仅用 `toDrawSummary` 复制成中立类型；
  `buildPresentationCommands`（`draw_command.cpp:268`）只被测试 renderer 消费。
  `2026-09-23-s6-d2-exit.md:39,80` 已明文承认。
- Duplicated Code：`SummaryHash`/`hashCommand`/`summaryDigest` 在 `draw_command.cpp:58-242`
  与 `open_gl_presentation.cpp:810-921` 逐字重复（同域串、同常量）；
  `Point3`/`transformPoint`/`finitePoint`/`finiteMatrix` 同形重复。
- Duplicated Code：`DrawCommand`/`DrawSummary`/`PresentationPass`（`draw_command.hpp:20-53`）与
  `OpenGlDrawCommand`/`OpenGlDrawSummary`/`OpenGlPresentationPass`（`open_gl_backend.hpp:54-89`）
  逐字段相同。
- Duplicated Code：对象校验/排序循环 `draw_command.cpp:379-509` 与 `buildDraws`
  （`open_gl_presentation.cpp:941+`）同形。
- Middle Man：`toDrawCommand`/`toDrawSummary`（`open_gl_presentation.cpp:1959-2001`）纯字段转发。

### 5.2 Spec

1. **(a) 部分**：plan.md:316（§S6-D1 #5）要求「共用 draw builder 归新层」、ADR 0042:145-146
   「不复制第二套」；但 `buildPresentationCommands` 只出现于 `presentation_renderer.cpp:395` 与测试；
   `OpenGlBackend::submit`（`:2099-2124`）→ `renderPresentation`（`:1798`）→ 自带
   `buildDraws`（`:1854`）+ adapter 私有 `std::sort`（`:1100`/`:1104`）与 `summaryDigest`（`:896`）。
   已记为残余（d2-exit.md:80），但与 D1 §5 未达成一致。
2. **(a) 部分**：plan.md:319（§S6-D1 #2）要求定义线程、borrow/owning 数据寿命行为；
   接口在 hpp:5/148 有文字定义，但 `tests/presentation_renderer` 对 `thread|owner|lifetime` 零匹配；
   仅覆盖 outstanding candidate、generation、stale token、resize、rebuild、present 失败
   （`presentation_renderer_tests.cpp:127-256`）。
3. **(b)** 未见越界：`renderPresentationFrame` 保留为兼容包装，对应 §S6-D2「保留 SDK 0.7.0 兼容入口」。
4. **(c)** 双份 digest 算法（中立层 `draw_command.cpp:58` 与 adapter `open_gl_presentation.cpp:810`）
   逐字节重复，分别由 `:297`/`:1860` 计算；任一排序改动即可能使二者分叉。
5. **正面证据**：正式路径已不调用 `renderPresentationFrame`——`player_control.cpp:971/980` 经
   `IPresentationRenderer::submit/present`；仅 `player_smoke.cpp:343/350` 保留。

## 6. S5 — S6-C2

### 6.1 Standards

未发现硬违规（头全 ASCII、无 JSON DOM/nlohmann 泄漏、`Result` 均已检查、两份 schema 与
`STAGE6_CONFIG_AND_MEDIA.md` §1/§2 及 `PLAYER_APPLICATION.md:138` 命令码表一致）。判断项：

- Duplicated Code：`user_preferences.cpp:111-126` `unsupportedVersion` 与
  `audio_device_profile.cpp:10-25` `unsupportedProfileVersion` 形状相同。
- Duplicated Code：`user_preferences.cpp:320,331,341,349,355` 的 `std::filesystem::remove(temporaryPath,…)`
  清理块重复 5 次。
- Repeated Switches：`player_command.cpp:75-128` 对 Play/Pause/Stop/Seek/Reload 重复同一
  `Empty→not_loaded`、`Failed→failed` 级联。
- Duplicated Code：`config_location.cpp:29-45` `acceptableProfileId` 复刻 schema pattern
  `^[a-z][a-z0-9._-]{0,63}$`（`schemas/cuexis.player-preferences.v1.schema.json:27`）。
- Duplicated Code：`sdl_audio.cpp:69-88` `findUniqueDevice` 重写了
  `audio_device_profile.cpp:132-161` 的唯一匹配+歧义逻辑，并把 `player.audio_profile.ambiguous/unmatched`
  硬编码进后端模块。
- Data Clumps：`sdl_audio.hpp:24-34` `PlaybackDeviceRecord` 与 `PlaybackDeviceTarget` 字段完全相同。
- possible Feature Envy / Speculative Generality：`audio_device_profile.cpp:163-185` `observeOpenedFormat`
  构造临时 `AudioDeviceProfile` 只为复用 `matchAudioDevice`。
- Duplicated Code（测试）：`tests/player_support/player_support_tests.cpp:59-70` 复制
  `user_preferences.cpp:49-68` 的排他锁创建代码。
- 补充：`2026-09-23-s6-c2-player-support.md:36` 称 5 cases，现文件已有 9 个 `TEST_CASE`；
  报告标注 in_progress，仅提示其非退出证据。

### 6.2 Spec

**(a) 缺失/部分**：§C2 #3（plan.md:242）「收集创建前事实，发布不可变 ResolvedAppConfig」——
`ResolvedAppConfig` 仅 `{requested, preferencesSource, preservePreferencesFile}`
（`resolved_config.hpp:27-31`；`resolved_config.cpp:49-56`），无创建前能力事实，
且 `preferencesSource` 是单一枚举而非 ADR 0024:82 的逐字段来源；`ConfigValueSource::LaunchOption` 从不产生。

**(b) 越界**：无（`player_command.*` 与 `WindowKey` 归 C3）。

**(c) 实现有误**：

- 「EffectiveSettings | … | Actual negotiated values」（`STAGE6_CONFIG_AND_MEDIA.md:21`；ADR 0042:183）：
  `logEffectiveWindow` 的 appliedVsync/appliedFullscreen/appliedGain 全取自 requested
  （`player_assembly.cpp:308-316`；`player_app.cpp:153` 传 `requested.vsync`），无协商回读
  （`open_gl_backend.cpp:449` 仅 `SetSwapInterval`）。
- C2 退出报告 §2.6 称热拔插导致的缺失、歧义和格式变化都失败，证据实为注入设备列表的单测
  （`tests/player_support/player_support_tests.cpp:423-459`）；completion.md:135 已如实记为
  「仅在 fake seat 验证」，但退出报告本身未标注。逻辑亦重复：`observeOpenedFormat`
  （`audio_device_profile.cpp:163-185`）与 `recheckBoundDevice`（`sdl_audio.cpp:540-573`）。

**点名复核项**：`WindowKey` 上报确实存在（`sdl_window.cpp:37-56,163-167`；消费于
`player_assembly.cpp:18-38`），属 C3；枚举初始化与打开分离符合（`sdl_audio.cpp:113-121` vs `470-500`），
无伪 preflight；零/多匹配皆失败（`audio_device_profile.cpp:142-158`），schema 无 ordinal/handle 字段，
校准不与 latency/Chart offset 叠加（`resolved_config.cpp:103-115`）。

## 7. S6 — S6-C3

### 7.1 Standards

**已记录整改未落实（二轮判定：本轮范围外，见 STD-04）**：`player_app.cpp:27-43` `audioStateName` 与 `frame_diagnostics.cpp:19-35` `stateName`
逐字相同的 `audio::PlaybackState` switch；仓库文档明确要求合并——
`docs/stage_reports/reviews/full-review-2026-08/2026-08-29-review.md:1039-1042`（AP-09）与
`docs/stage_plans/reviews/full-review-2026-08/remediation-plan.md:1240`。

**判断项**

- Duplicated Code / Repeated Switches：同上 switch 形状两份。
- Duplicated Code：`player_smoke.cpp:229-238` 与 `:280-310` 同形"prepareReload → renderer.prepare
  候选 → 断言被拒 → discard"，仅断言不同。
- Speculative Generality：`player_control.cpp:64-66` `NullJudgeSystem` 空实现，仅 `:786`/`:926` 调用，
  对应 Stage 7A 尚未存在的 Judgement。
- Speculative Generality（死代码）：`PlayerController::timingOffsetMs()`（`player_control.hpp:104`/
  `player_control.cpp:254`）全仓无调用者。
- Primitive Obsession：控制层遍布裸 `double` 毫秒。
- Mysterious Name（轻微）：`player_assembly.cpp:18-38` `inputActionFor` 覆盖全部枚举后仍有
  `return std::nullopt;`。

**已确认未违反**：ASCII 规则只约束安装公共头（`main.cpp`、`player_app.hpp` 的 CJK 注释不在安装树）；
`Result` 未忽略；拆分后职责单一，未见半迁移残留。

### 7.2 Spec

（子代理无 shell，结论来自读工作区文件。）

1. **(a)** decoder 阶段失败注入未被执行：spec plan.md:280 要求按阶段注入
   decoder/resource/shader/audio/renderer/commit 失败；fixture 定义了 `failClipPreparation`
   （`tests/player/player_control_tests.cpp:262,353`，`test.clip.decode_failed`），
   但全部 TEST_CASE 均未将它置 true，实际只注入 resource（`missingResourceChart`）、
   renderer（`markDeviceLost`）、precheck、audio、commit。
2. **(a)** 「v4 与 candidate 共用命令表」无证据：plan.md:279；命令表按设计格式无关
   （`player_command.hpp:15` 仅依赖 core），但 `tests/player` 无任何 candidate 源经 `PlayerController`
   的用例（`projectSource` 只读 `assets/projects`）。
3. **(a)** 「超出可播放范围」仅对音频定义：plan.md:268；`runSeek` 以 `validateSeekTargetMs(targetMs, -1.0)`
   调用（`player_control.cpp:550`），时长未知即跳过上界；ChartClock 会话无范围检查，
   `seek_outside` 仅在音频经 transport 触发（`tests/player:867` `audio.transport.seek_range`）。
4. **(b)** 未发现明显越界：`Rebuild`、`RestartAtZero` 被 plan.md:269/272 与 ADR 0042:213 覆盖；
   `player_smoke.cpp:233,259,284,303` 的负例探针符合 plan.md:267。
5. **(c)** Load 在 mode 失败后自动重试：`player_control.cpp:403-421` 在
   `playback.mode.content_mismatch` 且来源为配置来源时重新读来源并再 prepare 一次；
   ADR 0042:242 明确「拒绝：…模式失败后自动重试…」，且该路径对同一内容 decode 两次。
   `PLAYER_APPLICATION.md:152` 记为有意行为，但未在 spec 中据 ADR 重开授权。

## 8. S7 — S6-E1/E2

### 8.1 Standards

**硬违规（子代理判定，父代理已驳回第 1 条）**

1. ~~预算上限超出契约~~（**父代理已驳回**：512 MiB 是 `cuexis.media-cache` 记录上限，
   `publish.cpp:84` 的 1 GiB 是读取既有发布的比较上界，均非导入 profile 预算）。
2. `.gitattributes` 遗漏新 fixture：本 diff 只加了 `vcpkg-overlays/** text eol=lf`，
   `tests/fixtures/stage6_e/media/**` 下 60+ 个二进制与 `golden/*.json` 未声明 `binary`/`eol=lf`。

**判断项**

- Duplicated Code（且已漂移）：解码器字面量重复在 `media_identity.cpp:329-336`、`image_import.cpp:24-25`、
  `audio_import.cpp:20-22`；JPEG 两处不一致——profile 钉的是
  `libjpeg-turbo-3.2.0-idct-islow-fancy-upsample`，而 `media_importing`/provenance 报告的是
  `libjpeg-turbo-3.2.0`。
- Duplicated Code：`core::Error{std::move(code), std::move(msg)}` 包装器四处重复——
  `media_internal.cpp:36 mediaError`、`publish.cpp:20 ioError`、`worker_process.cpp:16 workerError`、
  `media_json_internal.cpp:19 invalid`。
- Duplicated Code：`publish.cpp:75 publishImmutable` 与 `:164 promoteTemporary` 同形。
- Primitive Obsession：四个身份全用 `std::string`（`media_provenance.hpp:51-64`），而文档反复要求
  它们"must not conflate"。

**符合项**：`CMakeLists.txt:896-915` allowlist 与链接一致；`vcpkg-overlays/libvorbis` 用
`REF v1.3.7`+`SHA512` 固定、五个 patch 齐全、`vcpkg.json`/`DEPENDENCY_POLICY.md`/
`THIRD_PARTY_NOTICES.md` 三处版本同步；`cuexis_playback`/`cuexis_player` 未进入媒体链接闭包。

### 8.2 Spec

**结论**：E1/E2 主体与 spec 对齐，但有两处"看似实现、实为漏判/静默近似"，两处负例覆盖缺口。

1. **(a)** JPEG "损坏 marker" 负例缺失：plan.md:371；`tests/fixtures/stage6_e/media/image/` 只有
   `corrupt_chunk.png`，无损坏 marker 的 JPEG fixture/断言；退出报告 §2.1 把该行记为已覆盖。
2. **(a)** 交错 PNG 未冻结：plan.md:363「不支持的变体必须明确拒绝，不静默近似转换」；
   `image_import.cpp:492` 对 interlaced PNG 走 `png_set_interlace_handling` 直接接受，
   无 fixture/golden。
3. **(b)** E1/E2 的库/CLI 在同一 diff 内还实现 E3 面（`media_cache`、`media_provenance`、
   `--cache-dir/--generation-dir`）；核实测试文件现为 23 个 TEST_CASE（E1/E2 18 + E3 5）、golden 18 个；
   归 E3 且已文档化，非无归属扩张。
4. **(c)** FLAC 伪造时长被接受而非拒绝：plan.md:383；`generate_negative_fixtures.py:63` 把 STREAMINFO
   total 改为 `2^36-1`，但 `audio_import.cpp:596` 的 `declaredSamples` 比较被包在
   `!processed||!verified||failed`（`:588`）分支内；MD5 仍通过故被跳过，`importAudio` 成功，
   `media_import_tests.cpp:511-516` 正断言二者输出相等——负例被写成正例/漏判。
5. **(c)** 线性 gamma 被当 sRGB 静默输出：ADR:274、:277；`image_import.cpp:353` 允许
   `gAMA=100000`（线性），却无任何 gamma 转换，线性输入直接按 sRGB 编进 `CXPRES01`。

**符合项**：`media-tools` 默认 OFF（`CMakeLists.txt:41`、`vcpkg.json:48`）；`engine/`、`app/` 对
`media_import` 引用 0 命中；四处同步齐备（`vcpkg-configuration.json` 基线 `40f3c709…`）；
`6e3a3c6` 用 `vcpkg-overlays/libvorbis` 单值 `M_PI` 统一四平台，未按平台拆 profile；
`chained.ogg` 由 `generate_negative_fixtures.py:60` 真拼接，在 `audio_import.cpp:272-285` 拒绝；
预算为 checked arithmetic 且循环内 `AudioAccumulator::append` 提前停止。

## 9. S8 — S6-E3

### 9.1 Standards

未发现硬性文档违规（子代理无 shell，直读工作区文件）。判断项：

- **Duplicated Code（事务形状三处重复）**：`写独占临时文件 → rename 覆盖目标 → 失败恢复` 在
  `src/asset_publish.cpp:826-837`（`adoptGeneration`）、`:917-980`（`commitPackage`）、
  `:992-1003`（`rollbackPackage`）各写一遍（近 Shotgun Surgery）。
- **Duplicated Code（父目录校验）**：`isDirectory(parent)` + `invalid_request` 在 `:903-907`、
  `:1013-1017`、`:1046-1050` 三处重复，message/context 不一致。
- **Primitive Obsession / 所有权不可由类型判断**：`include/.../asset_publish.hpp:84 void* handle_`
  同时承载 `HANDLE` 与 `reinterpret_cast<void*>((intptr_t)fd)`（`src/asset_publish.cpp:539`）；
  CODE_POLICY 要求公共 API 必须能从类型或文档判断所有权。
- **可移植性**：`readEnvValue` 已按 `_MSC_VER` 修正（`publish_fs_internal.cpp:38-55`），
  但 `readFileBytes` 仍按 `_WIN32` 选 `_wfopen_s`（`:198-199`）；非 UCRT MinGW 会复现 `0753e6a`
  的失败模式。
- **Mysterious Name / 魔法字面量**：role 常量已具名（`:46-47`），唯独 `rollbackPackage` 传裸串
  `"rollback"`（`:992`）。
- **Message Chains（轻）**：`loaded.package->identity().hex()` 在 `:893` 与 `:931` 重复。
- **Divergent Change（轻）**：`asset_publish.cpp` 一文件承担 generation 事务、adopt、package/pair 发布
  （约 1100 行）。
- **重启恢复缺口（轻）**：`recoverPublicationStaging` 只清 `staging/` 与 `generations/` 下的 `.tmp.`，
  不覆盖包发布落在任意父目录的 `.cuexis-publish.tmp.`（`:588-617`）。

### 9.2 Spec

**(a)** plan.md:400 要求验收「磁盘满/权限/替换失败和并发发布」；磁盘满与只读权限**未测**
（退出报告 §7 自认留 C4），只有「父目录不存在 / 被同名普通文件挡住」
（`asset_publish_tests.cpp:618-653`）。#1/#2 的四类身份与缓存键实现在 `tools/media_import`，
不在本 diff，本切片无法核验；只能确认 provenance 不进运行时闭包（`asset_publish.cpp:411-412`）。

**(b)** 未见明显越界；`adoptGeneration` 属 ADR 0042:310-311「显式采用该 generation」允许。
唯一：provenance 被并入 generation identity（`asset_publish.cpp:173`），spec 只要求它不进闭包。

**(c)**

1. ADR 0042:308-309「完成关闭和完整校验后只原子替换最终包」：`commitPackage` 做两次 rename
   （`target→backup` 再 `temp→target`，`:946-957`）；窗口内 target 缺失；崩溃后
   该 `.cuexis-backup.tmp.*` 是上一有效包唯一副本，却被 `recoverPublicationStaging` 当临时文件删除
   （`:596-599`）→ 违反 #4「失败不覆盖上一有效包」。pair 已单独捕获 `previousV4`（`:1075-1082`），
   backup 冗余且有害。
2. #3「进程间发布锁拒绝并发 writer」：pair 只锁 `v4Target.parent_path()`（`:1067`），
   candidateTarget 在另一目录时不受该锁保护。
3. 幂等重发把 `closureBytes` 置 0（`:694`），与首次发布不一致。

## 10. S9 — S6-C4

### 10.1 Standards

**公共边界：合规**——宿主只 include `<cuexis/playback/*.hpp>`（`host_runner.cpp:3-5`、
`host_content.hpp:8-9`），只用 `find_package(Cuexis ... COMPONENTS Playback)` + `Cuexis::Playback`
（`CMakeLists.txt:23,39`），无私有 include、不链 Player 实现。

**门禁脚本：合规**——`execute_process` 均带 `RESULT_VARIABLE` 并显式判 0
（`VerifyReferenceHost.cmake:41-68`、`:276-292`）；负例不用 `WILL_FAIL` 而自判并校验期望文本
（`:53-68`）；测试级 `TIMEOUT 900`/`RESOURCE_LOCK`（`CMakeLists.txt:406-410`）；
MinGW 运行库分支到位（`:238-248`）；`cmake/` 无新增安装项（`install()` 仍只导出
Config/Targets/Version，`CMakeLists.txt:610-626`）。

**硬违反**：报告 §2（`2026-09-27-s6-c4-…md:28`）宣称交付 `examples/reference_host/README.md`，
但该目录下无任何 `.md`；`BUILDING.md:468` 亦将其当作已文档化入口。

**Smell（judgement）**

- Speculative Generality：`HostReport::steps()`（`host_report.cpp:45`）、
  `HostFileProvider::readCount()`（`host_content.cpp:90`）无调用者；
  `HostContent::providerRootId{"main"}`（`host_content.hpp:46`）从未被读，
  `readBlob` 反而硬编码 `"main"`（`host_content.cpp:102`）。
- Duplicated Code：`parseUnsigned`/`parseDigest` 重复数字累加循环（`main.cpp:32-45` 与 `:60-68`）；
  门禁正/负例 configure 参数重复（`VerifyReferenceHost.cmake:170-205` 与 `:361-376`，
  `vcpkg_arguments:196-200,203-204` 又抄 `configure_arguments`）。
- Duplicated Code（跨新脚本）：`cuexis_host_run_checked`（`:41-51`）与
  `cuexis_dist_run_checked`（`VerifyPlayerDistribution.cmake:35-45`）同形。
- **Minor**：`ENV{PATH}` 于 `:270` 设定、`:328` 才恢复；中途 `FATAL_ERROR` 会跳过恢复。

### 10.2 Spec

1. **(a)** #4「验证符号」未落地：plan.md:298；exit §7:255-256 自认"宿主二进制的符号级检查
   （导入表/依赖清单白名单）还不是门禁…属于 F1"。
2. **(a)** #4「错误 SDK minor 拒绝」非门禁：plan.md:299；报告 §3.4:116-117 自认"该用例目前是手动核对，
   尚未注册为禁用例"；`VerifyReferenceHost.cmake` 无对应负例；exit §2:122 用 `find_package(0.7.0)`
   正例冒充。
3. **(a)** #4「错误工具链拒绝」仅 shared：`VerifyReferenceHost.cmake:378-381` static 明确跳过。
4. **(a)** #5「验证配置迁移不损坏旧文件」未实现：plan.md:300；`VERSIONING.md:236-239` 自认
   「都只有 v1，实现不发明 v0 迁移」；升级示例仅重建。
5. **(a)** A2「必须有自己的交互命令循环」部分：plan.md:164；`main.cpp` 仅解析 argv 后跑固定脚本
   （`host_runner.cpp:132-157`）。
6. **(a)** 已知缺口核实仍在：C4 分发门禁仅 Windows（exit §7:253）。
7. **(b)** scope creep：`VerifyReferenceHost.cmake:238-248` 复制 MinGW 编译器运行时 DLL 入宿主构建；
   `:151-168` 把父构建的 sanitizer/coverage 插桩选项转发给外部宿主工程；
   `PackagePlayer.cmake:86-98` 除解析依赖外 GLOB 复制目录内**全部** `*.dll`。
8. **(c)** #2 candidate 隔离非门禁：plan.md:292；实现只是本地手动 `out/build/c4-candidate`（报告 §5.1）
   ＋安装树文本扫描，无注册测试。**#3「不依赖源码 assets」**：宿主门禁从
   `CUEXIS_SOURCE_DIR/tests/fixtures/...` 取内容（`VerifyReferenceHost.cmake:140-143`），
   运行期虽 `cd` 源树外，但 fixture 仍源自源码树。

## 11. S10 — F1/F2 证据链与文档

### 11.1 Standards

**硬违规（文档标准）**

- `docs/stage_plans/completed/stage-06/plan.md:50-51`「E1/E2 与 F1 尚未退出」与同文件
  `:3`（completed）、`:103`（F1 completed）自相矛盾；`:103` 末句「`S6-F2` 未开始」vs `:104`
  F2 completed；`:106`「Stage 6 的 active 状态」——违反 `DOCUMENTATION_POLICY.md:32` 状态字段一致性。
- `completion.md:46` F2 行「报告已形成；owner 接受未记录」vs `:15`/`:129`「明确接受…（已记录）」。
- `docs/ROADMAP.md:35-36、107、113` 仍称 Stage 6「恢复 active」/「实施」，与 `:46` 冲突。
- `docs/guides/VERSIONING.md:175` 标题「Stage 6 已冻结的版本方向（尚未实施）」与 `:190` 及阶段关闭冲突。

**Judgement（索引/日期漂移）**

- `ROADMAP.md:5`（2026-09-17）、`docs/README.md:5`（2026-09-16）、`VERSIONING.md:5`（2026-09-20）
  均含 2026-09-27 内容却未同步 `更新日期`。
- `plan.md:98` C2「新增测试的 hosted 复验尚未发生」vs `completion.md:132`「已被覆盖」。

**基线 smell（判断）**

- Duplicated Code / Shotgun Surgery：阶段状态与结论在 `CURRENT_STATUS.md:21-39`、`AGENTS.md:10-78`、
  `stage_plans/README.md:57-91`、`ROADMAP.md:30-56` 各抄一份；一次关闭迫使四处同步——
  正是上述 active/日期残留的根因；与 policy「摘要不得复制…当前阶段结论」有张力。
- Mysterious Name：`completion.md:46` 状态格「报告已形成；owner 接受未记录」混合报告状态与接受状态。

未发现历史报告被改写为新的测试结果。

### 11.2 Spec（证据链）

**范围声明**：该子代理环境无 shell/git/gh，无法执行 diff、无法核对 SHA 对象是否存在于区间、
无法复跑 hosted；涉 commit 内容与 run 结论均标「未核对」，以下为文本交叉核对。

**(c) 看似实现但错**

1. **`S6-G13` 的 SHA↔run 错配**：`completion.md:71` 列 SHA `716b697`/`2c8211e`/`cb56e62` +
   runs `36298996619/…`；但 C4 exit（`2026-09-27-s6-c4-exit.md:268-299`）明确将这些 run
   （`36298996619/623/643/943/948/952/954`）归于 **`d7980bd`**，`36301616745` 归 `2c8211e`，
   而 `cb56e62` 的 run（`36304732xxx/36304734xxx`）未被列出。实际承载 run 的 `d7980bd` 在 G13 行缺失，
   `716b697` 无任何对应 run。spec：plan.md:441。

**(a) 缺失/部分**

2. **F2 关闭自相矛盾**：plan.md:433「记录 owner 对交接清单和残余的明确接受后才关闭/归档」；
   `completion.md:46` 的 F2 行仍写「owner 接受未记录」，却与 `:15`、`:129`「已记录」冲突。
3. **两份交接清单已交付**（非「计划已写明」）：plan.md:428-430 要求的 Stage 7A/Stage 8 清单确实存在于
   `completion.md` §6/§7，逐项挂接 C1/C2/C3/E3 证据，符合 plan.md:520。

**(b) 未要求的改动**：无法核对（不能执行 diff）。

**已核对一致**

- **排除论证站得住**：plan.md:415；F1 report §2/§5（`:29`、`:69`）明示 `headless-debug` 640 会少注册、
  不作覆盖，覆盖由 734/737/776 与 hosted 插桩承担；与 `tests/cxc/CMakeLists.txt:23`、
  `CMakePresets.json:102` 一致。
- `c80a5b5`/`95bfa70` 各 7 run（completion.md:73 vs F1 report §7/§10）文本完全一致。
- `findings.md:16-19` 三问题 `closed`、plan.md:3 `completed` 归档（文件确在 `completed/stage-06/`），
  内容自洽。

**Minor**：plan.md:103「7 个 preset」与 completion.md:72/85「8 个 preset」计数不一致。
