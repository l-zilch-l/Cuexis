# Cuexis Chart Entry Extension v1

状态：candidate（Stage 6 候选格式合同，阶段已关闭）

更新日期：2026-10-01

Status: Stage 6 candidate contract recorded by S6-A2. The entry factories and candidate
Playback implementation are not present yet.

This document owns the field and selection contract for the registered extension
cuexis.chart-entry.v1. The JSON shape is defined by
[cuexis.chart-entry.v1.schema.json](../../schemas/cuexis.chart-entry.v1.schema.json).
The extension is used in both ProjectConfig v1 and the CXC manifest extensions object. It
does not change the existing CXC entry record, ProjectConfig entry.chart, or the v4 default
source factories.

## 1. Purpose And Boundary

The extension maps a portable relative path to a typed chart entry. It is metadata, not a
second chart format and not a public representation of the Packed semantic model.

The Stage 6 candidate path is deliberately narrow:

- Packed flags must be 1 and candidateRevision must be 1.
- The registered feature is cuexis.gameplay.candidate.lanes4, version 1.
- The requirement domain is candidate.lanes4 and the action is press.
- Each supported requirement has exactly one lane constraint in the inclusive range 0 through 3.
- Only tap or point requirements are admitted.
- Effects are empty. Hold, Release, Slide, Flick, multi-lane and unregistered sections fail.
- BEH0, BHD0, ANM0 and FXS0 are rejected even when their payload is empty.
- Runtime does not parse CXT v2. CXT expansion and A16 feature/resource derivation are offline
  typed assembler work.

This contract is candidate-only. It does not enable a default Writer, a formal Chart v5
capability, a stable C ABI, or a new SDK version.

Gameplay V2 的独立 `gameplay-graph` Playback entry 不属于本 Stage 6 extension。现有
candidate factory 只接受上文的 `packed-chart` tap profile；遇到带有 V2 Graph entry kind
的未来扩展必须稳定拒绝，不能按 Packed Chart、source entry 或空表现路径解释。该扩展只有
在 Stage 7A/Stage 8 的 manifest、Graph round-trip、prepared identity 和同一 Judgement
kernel 证据全部接受后，才可加入新的 entry contract。

## 2. Extension Shape

The value stored under extensions[cuexis.chart-entry.v1] is an object with one member:

| Field | Type | Rule |
| --- | --- | --- |
| entries | array | One or more entry records, sorted by path bytes |

Each record has the following fields. Fields marked candidate are required when playback is
true; source records may omit them.

| Field | Type | Rule |
| --- | --- | --- |
| path | portable path | Required. Project-relative or archive-relative entry path |
| kind | string | Required. Stage 6 value is chart |
| encoding | string | source-json, source-cxt, or packed-chart |
| playback | boolean | Required. Only true records can be selected by a candidate factory |
| sourcePath | portable path | Optional provenance path; not a runtime input |
| sourceSemanticIdentity | lowercase SHA-256 | Optional source/build identity observation |
| compiledSemanticIdentity | lowercase SHA-256 | Required for playback candidate records |
| artifactIdentity | lowercase SHA-256 | Required for playback candidate records; hash of exact bytes |
| compilerProfile | string | Required for playback; Stage 6 value is candidate.static-tap-lanes4-v1 |
| expandedEntityCount | unsigned integer | Required for playback; maximum 40,000 |
| expandedRequirementCount | unsigned integer | Required for playback; maximum 40,000 |

The entry schema rejects unknown fields. Packed flags, candidateRevision, section registration,
CRC, decoded byte counts and eventCount are checked from the selected Packed bytes; they are not
duplicated in this extension. A metadata count never replaces a decoded count comparison.

The existing base entry record remains only path, byteCount and sha256. Extension fields must not
be added to that record. The CXC manifest continues to contain exactly the files required by its
existing package closure plus the declared extension entries.

## 3. Path And Selection Rules

Paths are UTF-8 JSON strings containing portable ASCII segments separated by forward slashes.
They are non-empty, relative, and contain no empty segment, dot segment, dot-dot segment,
backslash, colon, NUL, control byte, trailing dot or trailing space. Windows reserved device
names and case-folding conflicts are rejected. A regular file and one of its descendants are
also a path conflict.

Project paths are relative to the validated project root. CXC paths are relative to the archive
root. The selected path must be an exact path match after validation; no extension, suffix,
content magic or case-folding probe is allowed.

The caller must provide one explicit entry path. A package may contain more than one valid
playback record, but no factory may choose the first record or a sorted record. The requested path
must resolve to one and only one extension record, and that record must have playback=true. An
exact duplicate or case-fold duplicate is rejected before selection.

The legacy ProjectConfig entry.chart continues to point to the v4 chart document. Existing
fromFilesystemProject, fromCxcFile, fromCxcMemory and fromChartText keep their v1-v4 behavior.
They do not probe this extension after a failure and do not silently upgrade to a candidate.
A bare Packed file is not a Stage 6 user entry.

The complete project or package contract is validated before the selected candidate is published.
The selected semantic entry is decoded at most once; the owned decoded value, owned source bytes,
provider and resource closure are passed to prepare. Playback does not reparse JSON, re-decode
Packed bytes, or depend on a tools target.

## 4. Candidate Validation Order

The order below is observable through the first stable diagnostic and is part of the contract:

1. Validate the extension object, entry array, path syntax, ordering and duplicate conflicts.
2. Validate the explicit path and require one matching playback record.
3. Validate the base package or project closure, regular file presence, byte count and exact hash.
4. Read the Packed fixed header and apply the 16 MiB file, section and decoded-byte limits.
5. Validate magic, wire version, header size, flags, candidateRevision, directory ranges and CRCs.
6. Validate registered sections, required sections, UTF-8, canonical ordering and all count limits.
7. Decode through packed::decode, then validate the candidate profile and typed requirements.
8. Recompute semantic identity and compare the extension compiledSemanticIdentity and Packed header.
9. Verify resource closure and keep the generated identity, parent relation, requirements,
   constraints and effects in the immutable typed candidate.
10. Publish no partial result on failure; only the later Playback transaction may prepare or commit it.

The minimum stable diagnostic set is:

| Code | Meaning |
| --- | --- |
| cxc.chart_entry.extension_missing | The registered extension is absent |
| cxc.chart_entry.extension_invalid | Extension shape or unknown field is invalid |
| cxc.chart_entry.path_invalid | A path is not portable or escapes its root |
| cxc.chart_entry.duplicate_path | Exact or file/descendant path conflict |
| cxc.chart_entry.casefold_conflict | Case-folded path conflict |
| cxc.chart_entry.entry_missing | The explicit path has no extension record or file |
| cxc.chart_entry.playback_not_declared | The selected record is not playback=true |
| cxc.candidate.profile_unsupported | The profile or requirement subset is outside Stage 6 |
| cxc.candidate.revision_unsupported | Flags or candidateRevision is unsupported |
| cxc.candidate.budget_exceeded | A file, section, decoded or count limit is exceeded |
| cxc.candidate.packed_invalid | Packed structure, CRC, section or typed decode failed |
| cxc.candidate.artifact_identity_mismatch | Exact entry bytes do not match artifactIdentity |
| cxc.candidate.compiled_identity_mismatch | Recomputed semantic identity does not match metadata |
| cxc.candidate.resource_closure_invalid | A required resource is missing, duplicated or mismatched |
| cxc.candidate.partial_publish_forbidden | A failure attempted to publish a partial candidate |

Profile rejection precedes semantic identity mismatch. A structurally valid but unsupported
artifact must not be made to look like a hash failure.

## 5. Typed Lowering And Identity Preservation

The S7A-8 offline `cuexis_chart_candidate` tool consumes explicit fallback CXC, canonical
Packed metadata, CXT v2 and frozen Binding/module/export inputs. Integer parameters are
explicit `--parameter id=value` inputs. `assembleCandidateChart` validates base assertions,
appends typed expansion and derives the registered Foundation lanes4 feature and resource
uses through the existing Packed codecs. It writes `compiled/chart.packed`, entry metadata
and the manifest `cuexis.candidate-closure.v1` report in one atomic CXC publication. Real explicit-entry Playback
prepare validates assets before publication; failure preserves the old package.

This versioned Foundation profile does not lower Gameplay v2/Capsule revision 3 or infer
missing Gameplay v2 authored declarations. That author adapter retains its strict
declared/derived contract. Judgement/Playback integration remains S7A-7 and production budget
acceptance remains S7A-9; neither is certified by this tool.

Candidate presets enable the existing build switch. Installed public library binary names
gain `-candidate`; target names and public layouts remain invariant. Package metadata exposes
`Cuexis_BUILD_FLAVOR` and `Cuexis_EXPERIMENTAL`. Experimental packages reject imports unless
the consumer sets `Cuexis_ALLOW_EXPERIMENTAL=ON`; separate install prefixes are required,
with a flavor stamp rejecting mixed installs before copying package files. Consumer permission
cannot enable candidate decoding in production binaries.

Player `--candidate-entry path` requires exactly one `--project` or `--cxc` locator; OFF parsing
rejects it before opening input. Reference Host uses the same explicit Entry factories and
reports package flavor. Legacy factories never select candidates implicitly. Consumers of
the released Entry contract request SDK API `0.7.1` or newer in the same minor.

The offline typed assembler supplies chart metadata, expanded entities, parent relations,
requirements and resource references. It derives the lanes4 feature and closure from validated
typed requirements; Player cannot add a feature after decode. The resulting candidate keeps:

- the complete explicit or generated identity tuple;
- the parent identity, with cycle and missing-parent rejection;
- every Requirement tuple, including localId, Beat/interval, kind, domain, action,
  constraints and effects;
- the declared and derived resource closure; and
- the mapping from generated identity to its full typed identity for snapshot and host override use.

Generated execution IDs use the separate domain
cuexis.entity.execution-id.v5.candidate.1 followed by a NUL byte and the complete Packed
identity bytes defined by Packed Chart section 6.5. The result is v5g1: followed by all 64
lowercase SHA-256 hex characters. Ordinals, display names, addresses, std::hash and truncated
hashes are not identity inputs.

Prepared semantic identity uses the separate domain
cuexis.prepared-semantic.v5.candidate.1 followed by a NUL byte. It contains the compiled
semantic identity and the sorted actual resource identities, but not paths, archive layout,
device selection, window settings, clock mode or gain. Execution configuration and the composite
session identity are defined in the Stage 6 configuration contract.

The existing v4 identity and FrameDigest v1-v3 algorithms remain unchanged. Candidate identity
does not rename or rewrite old v4 golden values.

## 6. Build And User Entry Boundary

The candidate build switch is CUEXIS_ENABLE_CHART_V5_CANDIDATE and its default is OFF. An ON
installation is marked experimental, uses a separate staging prefix and flavor metadata, and
requires Cuexis_ALLOW_EXPERIMENTAL=ON from the consumer. Production and experimental trees are
not mixed.

The candidate API has three new names only:

- fromFilesystemProjectEntry
- fromCxcFileEntry
- fromCxcMemoryEntry

Both OFF and ON builds expose the same declarations and class layout. OFF returns a stable
candidate-disabled error when these factories are called. ON still requires the explicit entry
path. The public API carries only locators, an owning entry path, owning CXC bytes and Result;
it does not expose Packed, CXC, CanonicalSemanticChart, Requirement or CXT AST types.

## 7. Support Matrix And Evidence State

| Capability | A2 contract | Current implementation state | Verification owner |
| --- | --- | --- | --- |
| Project and CXC extension shape | Recorded and schema-linked | Local production bridge exists; C1 exit is open | C1 |
| Explicit candidate factory names | Recorded in API draft | Public declarations exist; experimental install gate remains C4 | C1/C4 |
| v4 legacy factory behavior | Preserved by scope | Local MSVC playback tests still pass | C1/F1 |
| R5 candidate profile | Repeated here as a hard boundary | Playback uses the production bridge; hosted exit is open | C1 |
| Typed lowering and A16 | Field retention and ownership recorded | Local lowering and prepare path exist; C1 exit is open | C1 |
| Experimental install separation | Consumer and staging contract recorded | Version/package gate not implemented | B1/C4 |
| Official v5/default Writer | Explicitly excluded | Not supported | Stage 8 |

This file and the linked schema are A2 contract artifacts. They do not certify a Playback
implementation or a release.

## S7A-7/8 filesystem 多产物发布首用（2026-10-07）

R78-09a B 沿既有 `cuexis.generation` / `cuexis.adopted` 格式，不修改 Foundation entry 的字段解释。
Graph、Packed、closure report、project metadata 及必需资源必须属于同一个内容寻址的不可变 generation。
离线工具先完成 typed 语义、闭包、真实 prepare 验证，再调用 `publishAndAdoptGeneration`。
该事务持有整个 root 的既有进程共享 publication lock；generation 全量写入、重读和 hash 校验后，
`cuexis.adopted` 的一次原子替换是 reader 可消费的唯一选择点。reader 先捕获一次该 manifest，
只在被捕获的 generation 中解析后续相对路径；不能每读取一个产物重新取 adopted。
旧 generation 不删除，旧 reader 在新发布后仍可读取完整旧集；未被 adopted 的完整 generation
可以留存，不能被 reader 自动发现或作为隐式 fallback。并发 writer 命中既有 `asset.publish.busy`。

原子替换前的任一错误保留旧 adopted 及完整旧输出；替换后的目录同步失败表示 durability 未确认，
不能谎称旧 manifest 未改变，也不能执行第二次回滚替换。返回错误必须带 `commitVisible=true`。
内容 identity 沿既有 generation 格式派生，包含排序后的 path / byteCount / exact SHA256 和 provenance；
不接受新的物理或生产预算数值。单 CXC 沿原 `publishPackage` 路径，不改成目录 generation。
本节仅定义首用发布事务，不能代替 Graph/Packed 七项 metadata 或生产 assembler 的验收。

## S7A-7/8 Gameplay Entry v1 首用（2026-10-07）

Gameplay 入口使用独立 `cuexis.gameplay-entry` / version 1 metadata 文档；不改变
`cuexis.chart-entry.v1`、Foundation profile 或 CXC v1 ZIP32 Stored 的解释。
该 metadata 存于 project optional `cuexis.gameplay-entry.v1` extension 的严格 `{entries:[metadata,...]}` 目录，
只有显式 Gameplay factory 选择后才准入；旧 factory 不自动发现、编译或回退。
一个文档只描述一个显式 portable `path`，`playback=true`；闭集 `entryKind` 为
`packed-chart` / `gameplay-graph`，其 encoding 分别为 `capsule.3` / `graph.1`。
`author-source` 非播放入口，不能作为上述 encoding 的别名。

根对象所有字段均必需、禁止未知/重复键：format、version、path、playback、entryKind、encoding、
compilerProfile、expandedEntityCount、expandedRequirementCount，以及以下七项中的后六项
（entryKind 为第一项）：compiledSemanticIdentity、artifactIdentity、rulesetBinding、
capabilityClosure、resourcePresentationClosure、sourceOf。
compiledSemanticIdentity 是沿 Capsule 3 规范 preimage 的 SHA256；artifactIdentity 是 exact entry
bytes SHA256，均为 lowercase 64 hex。先核验物理 metadata、路径和 exact hash，再解码所选载荷一次，
重建并比较 compiled identity、计数及完整闭包；任一失败不发布 source。
compilerProfile 是 canonical sourceClosure 的实际 token，不能由读取方补默认值。

rulesetBinding 是完整 `cuexis.gameplay-configuration` version 1 envelope，精确比较调用方显式
配置；该物理记录包含实际 Ruleset module/build/order、mapping/timebase/session 声明及表现绑定，
并不使其中纯表现字段进入 Judgement 四分量。configuration 的语义仍由实际 prepare 校验。
capabilityClosure 包含 canonical declared、derived 和 requiredFeatures 三数组：capability 行为
`[id,revision]`，feature 行为 string；逐项比较 typed graph，禁止伪造 feature/proof 或补齐缺项。
resourcePresentationClosure 包含 gameplayResources（canonical resource IDs）、chartResources
（`[assetId,use]`，use 沿现有 CanonicalResourceUseKind 整数）、assets（按 id 排序的完整 typed asset
描述及按原顺序的 dependencies）和 bindings（configuration 中的完整 bindings array）。
所有资源还须经当前 source/provider 的真实 prepare 完整性检查，GameplayOnly 不能豁免坏 hash。
sourceOf 首版明确为 `{kind:"not-packaged",documentIds:[canonical source document IDs]}`；源未随包
携带时不得虚构 sourcePath 或接受不可验证的源 byte hash；离线 source bytes 与 provenance 留在
assembler evidence。新 packaged 变体须另行冻结，当前稳定拒绝。

metadata JSON 使用调用方显式正值 testOnly decode budget（bytes/depth/string/values/elements），
不采纳生产数值；count 使用 u64/i64 checked integer token，不经 f64，count 不用于未核验 reserve。
形状、未知版本和 hash/闭包不一致均经现有公共 `playback.gameplay.invalid`，category 分别为
invalid_relation / identity_closure_incomplete；预算为 capability.budget_insufficient / budget_exceeded，
severity=error、faulted=false。CXC loader 仍先完成既有 package 全闭包校验；metadata 不替代容器验证。

Gameplay project extension 的容器为严格 `{entries:[metadata,...]}`，而每行仍是上述独立
`cuexis.gameplay-entry` version 1 文档；数组非空、path 唯一，数量沿既有 CXC/project 物理限额。
这是 Graph 与 Packed 同批多产物的显式目录，不以文件后缀或字段存在猜版本。
每行完整验证、全部路径/hash 纳入容器闭包；显式 factory 按 entryPath 选择恰好一行，
不隐式选择首行，重复 path 稳定拒绝。sourceOf 的 not-packaged 形状不变。

### S7A-7/8 离线 Gameplay assembler CLI 首用（2026-10-08）

`cuexis_gameplay_assemble` 仅 candidate ON developer tools 构建，生产工具入口复用现有
Gameplay author typed compiler，编译一次后由同一 canonical artifact 写 Graph 1 与 Capsule 3。
显式参数为 --base CXC、--foundation Foundation Packed、--source、--source-kind inline/cxt、
--entry-id、--configuration（上述完整 configuration JSON）、--output、--publish cxc/filesystem、
--test-only true、--configuration-budget 五项正整数、--graph-budget 七项正整数。
CXT 另需 --binding/--module/--export，可重复 --parameter id=i64，参数必须checked整数。
实际工具 profile 为 `gameplay.author.t4-k4.v1`，identity/configuration 必须显式，能力来自 SDK
静态 registry，author declared/derived feature 和 uniqueness proof 由原 compiler 校验，
不提供任意 feature/proof 注入选项。所有新数值仅 caller test-only 编解码预算，content生产界仍pending。

完整 fallback CXC 资源/index/project 保留，两个 compiled artifacts 的 metadata 都加入 project
Gameplay entries 目录；CXC manifest 的 optional `cuexis.gameplay-closure.v1` 保存两个完整 metadata
及原 compiler 计数、source exact SHA、prepared 验证结果和 budget acceptance=false，
不把 source path/hash 当作 packaged sourceOf。先以两载荷分别构建实际 source、
Presentation/GameplayOnly prepare+commit，任何失败不发布。CXC使用原publishPackage；filesystem
把同一已验证闭包放入不可变 generation，closure report作为 provenance sidecar，
publishAndAdoptGeneration一次切换，reader只捕获一次adopted。pack/unpack没有隐式author编译。

CLI 首用拓扑按原 P78-05 A：`cuexis_chart_candidate --gameplay` 为既有 CLI 的显式 dispatch，
原 Foundation 参数路径保持；`cuexis_gameplay_assemble` 仅为相同 dispatch 的兼容执行名，
两者调用同一实现/编译器/发布路径，不能形成第二编译或判定语义。
发布前验证 mode 沿现有 Playback 内容合同：有 mainMusic 使用 HostClock，没有时使用 ChartClock；
该选择只用于无设备的静态 prepare 验证，不自动打开音频设备，不接受生产时钟/校准承诺。
