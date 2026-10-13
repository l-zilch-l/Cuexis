# Chart v5 谱面编写指南

状态：现行 v5 候选作者指南；不是正式发行 Schema 或实现完成声明。

更新日期：2026-10-14。按 owner 要求由原 v4 指南更新；
[v4 原文](../archive/CHART_V4_AUTHORING_2026-09-02.md)保留供旧格式使用。

本指南说明如何把轨道、文字、判定线和音符写进内容实体，使用包内资源及明确的编译入口。
字段决策归 [v5 工作包](../stage_plans/active/chart-format-update-for-v5/plan.md)，
当前交付状态只见 [CURRENT_STATUS](../CURRENT_STATUS.md)。

## 1. 先选当前能执行的路径

| 路径 | 当前可用范围 | 不能推断 |
| --- | --- | --- |
| v5 Foundation 静态 candidate | Canonical Entity、Transform、Renderable、Camera、资源闭包、Packed、显式 Playback entry | 不代表 v5 Behavior/Animator 已实现 |
| v5 Gameplay inline author candidate | 已有版本化 author schema、静态 `entities` 与 Gameplay V2 typed assembly | 文件 version=5 不自动选择 RPA profile 或完成设备验收 |
| CXT v2 Core candidate | 显式 invocation、有限展开；Foundation 仅支持已注册的受限字段和 integer/beat 输入 | 不代表完整 CXT v2 动画或所有长期类型可用 |
| v5 Behavior source candidate | `objects`、Behavior event v2、位置/缩放/旋转/显隐及整数 alpha；显式 source-json entry | 不支持 Template、参数、Clip/Animator、CXT import 或 Gameplay Requirement |
| 完整 v5 动态合同 | Clip v2、CXT v2 等长期语义 | 仍待对应实现门禁；不能在静态 Packed 塞未知段 |

当前 CanonicalComponent 是 Transform / Renderable / Camera。Behavior、Animator、字体布局、
四轨规则不是该模型的默认组件。不得把不支持的字段藏在 extensions，或改用 Player 每帧绘制
来声称谱面实体已经实现。实体能表达几何，与动态求值能力是否实现是两件事。

完整 v5 作者语义中的 Object/Template/Parent 与当前候选 inline 源中的 `entities` 是不同的
物理编写层。第 2 节描述静态 inline dialect；第 5 节描述独立注册的 Behavior source dialect。
两者有不同的入口编码和 profile。不能只修改 version 而沿用旧入口。候选源不是未来生产 JSON Schema 的替代权威。

## 2. 实体、身份、父子与静态组件

以下是 `entities` 的一个记录片段，不是完整可独立编译文件：

```json
{
  "identity": {
    "kind": "generated",
    "chartId": "019a0000-0000-7000-8000-000000000001",
    "bindingId": "practice.scene",
    "moduleId": "entity.practice",
    "exportId": "scene",
    "path": [{ "nodeId": "track_D_background", "iterationIndexPlusOne": 0 }]
  },
  "components": [
    {
      "kind": "transform",
      "position": [-1.5, 0.15, -0.04],
      "rotation": [0, 0, 0, 1],
      "scale": [0.85, 2.9, 1]
    },
    {
      "kind": "renderable",
      "mesh": "mesh.quad",
      "material": "material.grey",
      "alpha": 255
    }
  ],
  "requirements": []
}
```

完整源例以仓库 [inline_zero.json](../../tests/fixtures/gameplay_author/inline_zero.json) 为
空 Gameplay author envelope；测试生成器见
[make_entity_practice_chart.py](../../tools/make_entity_practice_chart.py)现在生成第 5 节的
Behavior source，而不是本节的静态 inline envelope。二者都不因 version=5 自动具有判定。

身份必须唯一。显式身份使用 `kind: explicit` 与 UUIDv7 `objectId`；生成身份保留完整
Chart/Binding/Module/Export/path，不改成临时整数或字符串 hash。
`iterationIndexPlusOne=0` 表示非 Repeat 的节点；Repeat 身份按
[CXT v2](../formats/CXT_V2_FORMAT.md)保留完整路径。

候选 `parent` 引用另一个完整实体身份，不是普通名字；引用必须存在，不能成环。
父子关系不会自动继承组件。每个实体都要明确自己的 Transform 与 Renderable。

| 组件 | 当前候选字段 | 写法 |
| --- | --- | --- |
| Transform | position、rotation、scale | Vec3、Quaternion、Vec3；位置有限，scale 正且非零 |
| Renderable | mesh、material、alpha | 注册 AssetId；alpha 必须整数 `[0,255]`，显式写出 255 |
| Camera | type、fovY、nearPlane、farPlane | perspective；`0 < fovY < 179`，`0 < nearPlane < farPlane` |

旋转沿用 Quaternion `[x,y,z,w]`、shortest-path slerp 和既有 Mixing 规则。
v5 **没有采用 Euler 或任意多圈角度**；不要从旧建议或旧指南的计划提醒推断这些能力。
长期 Object component 的版本化字段是 `cuexis.transform`、`cuexis.renderable`；上述 `kind`
记录是候选 inline 表示，不把两套 JSON 混进同一个数组。

## 3. 轨道、文字和音符都是内容

| 内容 | 建议实体表示 |
| --- | --- |
| 轨道底板/边线 | 矩形 Mesh 的独立实体；位置、scale 与颜色 Material 明确 |
| 判定线 | 独立细矩形实体；不由按键回调即时补画 |
| 标题、DFJK、说明 | 程序生成字形 Mesh，或包内字形 Texture + Mesh；每段文字是 Renderable 实体 |
| 短音符 | 独立实体；视觉位置与 Requirement 的目标时刻分别声明 |
| Hold | 头、体、尾实体，可共享资源；它的几何形状本身不证明保持判定 |
| Hit/Miss 反馈 | 已有 Gameplay FactBinding 与明确实体目标；不能根据按键或画面自行猜命中 |

当前没有内置 text/track/DFJK 组件。文字的字形生成属于离线内容工具；生成结果进入 CXC。
Player 只播放/呈现内容，不包含这份谱面的标题、字体或轨道布局代码。

当前生成器将短音符的下降和显隐写为 Behavior；Hold 的体段位置、伸缩与尾段也在谱面中声明。
此前 `STATIC V5` 版本是静态布局，不能用于运动验收。新版本仍是零 Requirement 的动态呈现，
DFJK 按键不会产生 Hit/Miss 或分数，不能作为 B5 判定验收证据。

## 4. 时间和相机

Gameplay 的整数 Tick、Chart 有理 Beat、音频 frame 和显示时间不能混用。
Chart 的 BPM/Tempo/Stop 决定 Beat 映射；Gameplay Requirement 使用其明确 timebase 与
normalization。不要通过每帧加一或每个键加一替代真实时钟。

Beat 写成约分的 numerator/denominator，denominator 正。生成固定四分之一/十分之一秒课程时，
先用有理数构造时刻，避免将浮点算术噪声写成超预算分母。此为作者数据处理，不是改写捕获时间。

候选默认相机片段：

```json
"defaultCamera": {
  "type": "perspective",
  "fovY": 60,
  "nearPlane": 0.1,
  "farPlane": 1000,
  "pitch": 0,
  "yaw": 0,
  "roll": 0,
  "defaultTransform": {
    "position": [0, 0, 5],
    "rotation": [0, 0, 0, 1],
    "scale": [1, 1, 1]
  }
}
```

空间为右手系，+X 右、+Y 上、默认视线 -Z。此候选 DTO 的 defaultTransform 与
旧 v4 `camera.defaultTransform` 字段不同，不能相互照搬。aspect 由实际视口提供。

## 5. alpha、Behavior、Clip 与 CXT 的 v5 版本

当前 `candidate.behavior-v2-v1` source-json profile 实现以下 Object/Behavior 子集；
Clip/CXT 的长期规则仍不属于该 profile：

- 静态 Object Renderable 的 component version 仍为 1，alpha 是整数 `[0,255]`，省略默认 255。
- 动态透明度属性名为 `render.alpha`；v5 源不得沿用旧透明度属性名或浮点作者端点。
- Chart v5 的 `behavior.event` 必须 version 2，即使没有透明度事件。
- Chart v5 / CXT v2 的 AnimationClip 必须 version 2。
- Chart v5 只导入 CXT v2；v4 只导入 CXT v1。
- Quaternion、fovY、Hermite 归一化斜率和既有 Mixer 规则沿用；不得乘斜率以进行 alpha 单位转换。

选定的 Behavior 事件片段：

```json
{
  "id": "behavior.note.fade",
  "type": "behavior.event",
  "version": 2,
  "events": [{
    "property": "render.alpha",
    "startBeat": {"numerator": 4, "denominator": 1},
    "durationBeats": {"numerator": 1, "denominator": 1},
    "startValue": 255,
    "endValue": 0,
    "startSlope": 1,
    "endSlope": 1
  }],
  "stepEvents": []
}
```

这个事件可放入 Behavior source 的 `behaviors` 数组，**不能加入 Packed static profile**。
完整可编译的小例见 [behavior_candidate.v5.json](../../tests/fixtures/chart_format_update/behavior_candidate.v5.json)。
顶层使用 `format: cuexis.chart`、`version: 5`、`objects` 与 `behaviors`；对象是 UUIDv7，
`cuexis.behavior` 的引用 `domain` 为 `behavior`。对象的 Renderable 片段：

```json
"cuexis.renderable": {
  "version": 1,
  "mesh": {"domain": "asset", "id": "mesh.quad"},
  "material": {"domain": "asset", "id": "material.D"},
  "alpha": 255
}
```

`parameters`、`templates`、`animationClips`、`animationTemplateImports` 必须为空数组。
未知属性、旧 Behavior version、旧透明度属性和小数 alpha 都拒绝。
`render.visible` 使用 `stepEvents`，明确声明出现前 false、出现时 true、离场时 false。
Transform 的连续事件使用有理 Beat 和归一化 Hermite 斜率；无事件时保留对象基值。
实例的 `cuexis.note` 不等于 Gameplay Requirement。

CXC factory 在选定入口时解析、校验并编译一次，保留 owning typed runtime；prepare 与每帧
update 不再解析 JSON。语义身份使用独立 domain 与 canonical typed 内容，精确文件身份
仍是原始 JSON bytes 的 SHA256。Playback 要求 `cuexis.behavior.event.v2`，显隐还要求
`cuexis.render.visibility.v1`。候选开关关闭时，显式候选 factory 继续拒绝加载。

长期 CXT v2 类型合同与当前 Foundation 实现分别见
[CXT_V2_FORMAT](../formats/CXT_V2_FORMAT.md)。当前 integer/beat 与受限 Transform 字段
不等于所有 number/vector、材质或 Animation 扩展都已实现。AssetId、旋转、alpha 与拓扑
不得通过未注册的参数槽位间接改变。

## 6. 资源闭包、Packed 和 CXC

Renderable 的 Mesh/Material 必须在 Asset Index 中注册并包含实际 bytes。Material 的 Texture
引用与 dependencies 必须一致；资源采用
[Portable Presentation](../formats/PORTABLE_PRESENTATION.md)的 CXPRES01 payload。
静态 inline 的主音乐由 `mainMusic` 声明；Behavior source 使用
`audio.mainMusic: {"domain":"asset","id":"audio.main"}`，编译器派生资源闭包。

当前测试包的**实际运行入口**是 `compiled/chart.v5.json`，声明 `encoding: source-json`
与 `compilerProfile: candidate.behavior-v2-v1`。包旁 `practice.v5.json` 与该入口内容一致。
它经显式 candidate factory 播放；默认 production Chart Reader 不自动选择这条候选路径。

CXC v1 仍保留合法的 v4 compatibility fallback 文档，以满足现有容器的默认 Project closure；
它是空场景、不是这份 v5 测试谱面。启动脚本明确选择 Behavior source entry。检查格式时先看实际
候选 entry，不要将 fallback 的版本当成实际播放格式。不能删掉 fallback 后忽略容器校验失败。

CXC 当前拒绝未列入受支持闭包的任意作者文件。测试原始 JSON 放在包旁；声明 provenance
path 不等于允许把任意 JSON 放进容器。文件清单、SHA256、compiled semantic identity、
artifact identity 与解码计数须分别核对。

## 7. 生成和实际验证

当前离线 fixture helper 调用通用 Behavior candidate 编译器，写出并重新编译入口，比较语义身份。
它只接受 testOnly 配置，先产生呈现实例，再用独立 Gameplay author 编译器附加判定载荷。
动态支持在 chart/CXC/Playback 中实现，Player 不持有这份测试谱面的布局或动画逻辑。
此工具不是通用 v5 发行编译器。

Windows / 已有 MSVC candidate 构建：

```powershell
python -B tools/build_entity_practice_compiler.py `
  --build out/build/candidate-debug --output out/entity-v5/compiler `
  --vcvars "D:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat"

python -B tools/make_entity_practice_chart.py `
  --output out/entity-v5 --compiler out/entity-v5/compiler/compiler.exe `
  --fixture out/build/candidate-debug/s7a78-public-fixture

python -B tools/make_judgement_practice.py `
  --directory out/entity-v5 --fixture out/build/candidate-debug/s7a78-public-fixture `
  --assembler out/build/candidate-debug/bin/cuexis_gameplay_assemble.exe

out/build/candidate-debug/bin/cuexis_player.exe --mode audio `
  --cxc out/entity-v5/practice.judgement.cxc --candidate-entry compiled/chart.v5.json `
  --execution-test-only --gameplay-packed out/entity-v5/gameplay.packed `
  --gameplay-configuration out/entity-v5/gameplay-configuration.json `
  --gameplay-config-budget 131072,64,8192,16384,8192 `
  --gameplay-key 7:lane.d:domain.binding.one --gameplay-key 9:lane.f:domain.binding.one `
  --gameplay-key 13:lane.j:domain.binding.one --gameplay-key 14:lane.k:domain.binding.one `
  --execution-tail-input-deadline 35100000
```

现有构建和 public fixture 必须已生成；compiler 继承该构建的工具链和依赖，不能跨工具链混链。
schema 校验不等于 prepare 或设备通过；真正的显示验证应检查 Runtime 解码数量、实际绘制
及窗口截图。CLI 带 --candidate-entry 才选择候选载荷；裸 --cxc 使用 compatibility fallback。

## 8. 动态实体与 B5 判定一起运行

当前样例把两个显式入口放在同一个 CXC：Behavior source 为 `compiled/chart.v5.json`，
Gameplay V2 capsule 为 `compiled/gameplay.packed`。包旁 `gameplay.packed` 是后者的逐字节副本，
由通用 `--gameplay-packed` 参数提供 owning GameplayContent，再通过 SDK 的
`prepareGameplayLoad(source, mode, content)` 附加到动态呈现源。两者必须共享 ChartId；
配置、资源、反馈目标及 prepare 校验失败就拒绝加载。没有自动执行作者生成器或脚本。

呈现入口包含 104 个 Object、22 个 Behavior、37 项资源；Gameplay 作者模型包含相同的
104 个显式实体和 19 个逻辑生成实体。当前 author dialect 用生成实体持有 Requirement，
视觉音符头是独立 Object，`course.json` 的 `visualHeadObjectId` 只记录视觉对象身份，
不能当作 Requirement owner。42 条 FactBinding 精确引用谱面的 Hit/Miss 徽章 Object。
徽章初始由 Behavior 隐藏，真正的阶段结果通过 FactBinding 控制显隐，Player 不临时绘制四轨、
文字、音符或判定徽章。

源 `gameplay.common.executionProfile` 可显式选择历史 `gameplay.execution.t4-k4.v1` 或
`gameplay.execution.t4-k4.rpa.v1`。RPA 配置必须同时使用 `fact.semantic.rpa.v1`、
`late.window.batch.v1`、显式 `cuexis.gameplay.rpa.v1` 能力和匹配的校准身份，不能只改版本号。
本例使用微秒 Tick、±120000 μs 成功窗口；18 个 Tap 和一个 K Hold 的 head/body/tail 共 21 阶段。
每个 Tap/Head Hit 为 +2，Body +3，Tail +5，各阶段 Miss 为 −1。
全命中为 46 / 21 Hit / 0 Miss；无输入为 −21 / 0 Hit / 21 Miss；
Hold Head 命中后提前松开为 36 / 19 Hit / 2 Miss。

离线 helper 的 `--verify-gameplay package.cxc gameplay.packed configuration.json cases.json`
对这些显式输入情景运行真实 RPA capture/advance，并与完整 Replay 求值比较。
它验证逻辑结果，不能代替实体键盘手感、听音、校准误差或人工同步验收。
音频真实 EOF 是 35 秒，35.1 秒输入截止之后由 tail 完成；最后 3 秒静音不是 EOF。
`--show-fps` 在此路径仍为可选参数，默认关闭。

## 9. 作者检查清单和迁移

1. 区分目标语义版本、source dialect、Packed 编码版本、candidate revision 与 SDK 版本。
2. 身份唯一、parent 完整且无环；不按输入数组顺序推断运行顺序。
3. 所有静态实体、文字网格、资源引用和 alpha 整数都在作者内容中。
4. 当前模型不支持的动画明确记为缺项，不能以 guide、extensions 或回调代替。
5. 实际启动选择正确 entry；manifest hash、semantic identity 和 round-trip 分别验证。
6. 不把零 Requirement 呈现包叫作 B5 可玩/判定测试。
7. 实体键盘、听音、同步、真实 EOF tail、预算和 owner 接受分别留证。

v4→v5 是显式迁移，不是版本号替换。身份/结构要按目标 dialect 转换；旧透明度端点按
半数远离零的 `round(value*255)` 转整数，0.5→128；斜率不乘 255。Behavior/Clip 版本和
CXT import 版本必须同时按合同处理。没有已实现的迁移入口时，不能承诺命令已支持 --target 5。

旧 v4 兼容写法继续见 [v4 原文](../archive/CHART_V4_AUTHORING_2026-09-02.md)。
本指南更新不关闭 V5-F、Stage 8、RPA 或 B5 的实现与接受门禁。
