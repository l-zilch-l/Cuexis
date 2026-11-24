# Chart 与 Packed 预算上限

<!-- cuexis-meta
id: GUIDE-LIMITS
status: current
authority: references
-->

状态：current（由 `tools/verification/gen_docs_limits.py` 从 `engine/chart/include/cuexis/chart/limits.hpp` 生成，勿手改）

本页是上限数值的**单一可读来源**。合同文档引用预算时应链接本页，而不是复述数字；
修改上限只能改 `limits.hpp` 后重新生成本页。

## Chart 解析/编译上限（Cuexis Chart Limit）

每个字段都是上限：调用方可以更严格，但不得放宽。

| 字段 | 值 | 源码表达式 |
| --- | --- | --- |
| `maxInputBytes` | 16,777,216（16 MiB） | `16U * 1024U * 1024U` |
| `maxNestingDepth` | 64 | `64` |
| `maxStringBytes` | 1,048,576（1 MiB） | `1024U * 1024U` |
| `maxMetadataMembers` | 1,024 | `1024` |
| `maxTemplates` | 10,000 | `10000` |
| `maxBehaviors` | 10,000 | `10000` |
| `maxObjects` | 100,000 | `100000` |
| `maxPatchesPerTemplate` | 256 | `256` |
| `maxTracksPerBehavior` | 6 | `6` |
| `maxKeysPerTrack` | 65,536 | `65536` |
| `maxTotalBehaviorKeys` | 262,144 | `262144` |
| `maxTempoEvents` | 4,096 | `4096` |
| `maxStops` | 4,096 | `4096` |
| `maxEventsPerBehavior` | 65,536 | `65536` |
| `maxTotalBehaviorEvents` | 262,144 | `262144` |
| `maxExtensions` | 256 | `256` |
| `maxDiagnostics` | 1,024 | `1024` |
| `maxIdentifierBytes` | 256（256） | `256` |
| `maxChartParameters` | 256 | `256` |
| `maxAnimationImports` | 10,000 | `10000` |
| `maxAnimationClips` | 10,000 | `10000` |
| `maxAnimationTracksPerClip` | 256 | `256` |
| `maxAnimationSegmentsOrStepsPerTrack` | 65,536 | `65536` |
| `maxTemplateBindingsPerAnimator` | 256 | `256` |
| `maxAnimationLayersPerAnimator` | 64 | `64` |
| `maxBlendGroupsPerLayer` | 64 | `64` |
| `maxClipInstancesPerBlendGroup` | 256 | `256` |
| `maxAnimationMaskEntries` | 256 | `256` |
| `maxAnimationTemplateNameBytes` | 256（256） | `256` |
| `maxAnimationTracks` | 65,536 | `65536` |
| `maxAnimationSegmentsAndSteps` | 1,048,576 | `1048576` |
| `maxGeneratedAnimationRecords` | 100,000 | `100000` |
| `maxAnimationTemplateBytes` | 4,194,304（4 MiB） | `4U * 1024U * 1024U` |
| `maxCxtV2Modules` | 10,000 | `10000` |
| `maxCxtV2Parameters` | 256 | `256` |
| `maxCxtV2SlotsPerPrototype` | 256 | `256` |
| `maxCxtV2Prototypes` | 10,000 | `10000` |
| `maxCxtV2Patterns` | 10,000 | `10000` |
| `maxCxtV2RepeatDepth` | 16 | `16` |
| `maxCxtV2Nodes` | 100,000 | `100000` |
| `maxCxtV2ExpansionEntities` | 40,000 | `40000` |
| `maxCxtV2ExpansionRequirements` | 40,000 | `40000` |
| `maxBeatNumeratorMagnitude` | 1,000,000,000,000 | `1000000000000LL` |
| `maxBeatDenominator` | 1,000,000,000 | `1000000000LL` |

## Packed 预算（Frozen Foundation）

每个字段都是天花板：`min(requested, frozen)`；16 MiB 文件门禁与 40,000 实体门禁不因宿主放宽。

| 字段 | 值 | 源码表达式 |
| --- | --- | --- |
| `maxPackedFileBytes` | 16,777,216（16 MiB） | `16U * 1024U * 1024U` |
| `maxPackedDecodedBytes` | 16,777,216（16 MiB） | `16U * 1024U * 1024U` |
| `maxPackedSectionBytes` | 16,777,216（16 MiB） | `16U * 1024U * 1024U` |
| `maxPackedStrings` | 100,000 | `100000` |
| `maxPackedReferences` | 100,000 | `100000` |
| `maxPackedEntities` | 40,000 | `40000` |
| `maxPackedRequirements` | 40,000 | `40000` |
