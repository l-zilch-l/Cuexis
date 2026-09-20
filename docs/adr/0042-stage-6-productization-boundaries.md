# ADR 0042：Stage 6 产品化的实施边界与长期维护决策

状态：已接受（owner 于 2026-09-20 授权冻结关键实施决策）；尚未实现

日期：2026-09-20

适用范围：[Stage 6](../stage_plans/active/stage-06/plan.md) 的 S6-D01 至 S6-D08。
本 ADR 冻结实施方向，不代表 A1 基线、A2 合同落盘/表征或任何实现批次已经通过。
字段级 Schema、诊断表与独立 golden 由 A2 按此决策形成，不得在该过程中重新选择架构。

## 背景与优先级

当前 PlaybackSource 以文本项目文档为主；Packed candidate 校验部分位于开发工具层；
Playback/Runtime 已依赖底层 render，而正式渲染方法只在 OpenGL adapter；音频 replacement
的最终激活可能失败。把这些接线问题留给实现者临时决定，容易形成循环依赖、双解析、
伪装成 v4 的中间数据，以及没有实际原子性的“回滚”。

本 ADR 细化 ADR 0024、0027、0032、0033 与 Foundation R5 交接：

- 继续保留 v4/CXT v1/CXC v1、旧 Reader 和 FrameDigest 算法，不扩大 Packed revision 1。
- R5 的“不作为公共 SDK 能力”继续表示不发布正式 Chart v5 capability，不加入默认入口。
  本 ADR 明确允许**通用显式 entry 选择 API**在独立 experimental 构建中消费 candidate；
  不公开 CanonicalSemanticChart、candidate Requirement 或 CXT AST。
- 原计划“在 cuexis_render 中直接增加消费 Playback 的接口”被下述高层 renderer 分层取代。
  不通过前置声明或 header-only 技巧掩盖实际模块循环。
- SDK API 的 Stage 6 目标为源兼容增量 `0.7.1`，不是已经实施的版本变化；
  Stage 8 不预留 minor。若实现必须破坏现有结构或签名，须重开本 ADR，不能仍标为 patch。

## S6-D01：显式 source 与候选隔离

### 固定选择

1. 增加 `CUEXIS_ENABLE_CHART_V5_CANDIDATE`，默认 OFF。ON 构建和安装元数据必须标为
   experimental；生产与实验安装前缀禁止混用。`CuexisConfig.cmake` 在实验安装树上要求
   consumer 显式设置 `Cuexis_ALLOW_EXPERIMENTAL=ON`，否则配置失败。
   Playback binary/import library 的名称也必须区分 candidate flavor，不能仅改变 zip 文件名；
   包元数据和 Player 启动诊断显示 flavor，clean staging 验证不能交叉误加载。
2. 增加三个独立命名的公开工厂：`fromFilesystemProjectEntry`、`fromCxcFileEntry`、
   `fromCxcMemoryEntry`。输入为既有 locator/owning bytes，加显式的项目/包相对 entry path；
   资源来源沿用原项目/CXC provider。确切声明在 A2 落盘，但不得新增旧方法重载或改其参数。
   这些方法只选择已登记 metadata 描述的 entry，不依据路径后缀识别格式。
   两种构建的公开声明完全相同；开关只控制实现能力，不用条件编译改变公开类定义或布局。
3. 现有 `fromFilesystemProject`、`fromCxcFile`、`fromCxcMemory`、`fromChartText` 等默认工厂
   在两种构建中都维持原 v1-v4 语义。新方法在 OFF 构建遇到 candidate entry 稳定拒绝；
   ON 构建也必须通过显式选择才能进入 candidate path。禁止文件失败后探测其它格式。
4. CXC 沿用已登记的 `cuexis.chart-entry.v1` 表；filesystem project 在 ProjectConfig v1
   的 extensions 下登记同名候选表，复用 entry 描述和 profile 校验规则。路径相对项目根，
   复用 containment 和资源根规则。`entry.chart` 继续指向 v4 回退文档，不能改成 Packed。
   项目扩展的 Schema/Reader 与 CXC 表的共同 typed 描述在 A2 明确，不使用 Player 私有 JSON。
5. 必须给出唯一 entry path，且该项 `playback=true`、文件存在、profile/revision 受支持。
   不选第一项、不按排序选项、不在多个 playback entry 之间猜测。裸 Packed 文件加载不在
   Stage 6 的用户入口范围；内部 decode 测试不受此限制。
6. Player 使用独立参数 `--candidate-entry <path>` 配合 `--project` 或 `--cxc`。
   非实验构建拒绝该参数；不保存为用户偏好，不自动接管旧项目。`--chart` 保持既有文本入口。

### 模块与拥有关系

- 公共 API 只携带 locator、entry path、拥有的 bytes 和 Result，不引入 Chart/CXC 类型。
- `cuexis_chart` 拥有 Packed profile 校验、解码、离线 assembler 与 lowering；
  `cuexis_cxc` 拥有 entry 表的共同 typed 描述/Reader，以及包存在性、包/entry 身份和与
  decoded 结果的绑定。Playback 的 filesystem 组合路径复用该表 Reader，再通过既有
  filesystem/provider 校验文件内容；Chart 不读取 ProjectConfig 或 CXC manifest。
  Playback 负责来源选择、provider 资源准备和会话事务。工具仅调用这些内部能力。
- Source state 使用显式 tagged payload，区分 legacy/project documents 与已校验 candidate；
  禁止把二进制塞进 `chartJson`、特殊 `sourceId` 前缀或 JSON extension 字符串作为私有通道。
- 校验发布一个拥有 bytes/provider 寿命和 decoded typed chart 的不可变输入结果；prepare
  复用它。不要为了类型隐藏而重新解析 JSON、重新 decode Packed 或依赖全局内容注册表。
- 对未选择的 entry 仍执行既有包合同要求的校验，不能以单次 decode 为由降低包有效性要求。
  每个需要语义校验的 entry 最多 decode 一次，选择结果复用同一次校验的产物。

拒绝：默认工厂自动升级到 v5、用 `allCapabilities()` 宣布正式 v5、直接导出内部语义树、
Playback 依赖 tools、v5 转 v4 JSON 再调用旧 Reader。

## S6-D02 / D03：身份、lowering 与 A16

### 身份的唯一来源

- 显式实体继续使用其规范 UUID 文本。generated entity 保留完整 typed tuple；
  execution ID 固定为 `v5g1:` 加完整 256-bit SHA-256 的小写 hex。
  hash 输入为域 `cuexis.entity.execution-id.v5.candidate.1`（含终止 NUL）和
  [Packed Spec §6.5](../formats/PACKED_CHART_FORMAT.md) 的完整规范 identity bytes。
  不使用 ordinal、显示名称、`std::hash`、地址、截断 hash 或拼接未经长度编码的字符串。
- 每次 prepare 建立 execution ID 到完整 typed identity 的映射，检测重复和 hash 冲突。
  两个不同 tuple 映射到同一 ID 必须拒绝，不加随机后缀。该映射贯穿 prepared/active 寿命。
  execution ID 是有明确定义的观察别名，不替代 canonical identity。
- Snapshot 和 HostOverride 对 candidate 使用该 execution ID；父引用通过同一映射建立。
  FrameDigest v1-v3 算法不改，candidate 帧使用既有字符串 ID 编码，不给旧 v4 改名。
- typed requirements 完整保存在不可变 candidate 内容中，包括实体身份、localId、Beat、
  kind、domain/action、constraints/effects。Stage 6 不发布公共 Judgement API，不把要求
  降级成渲染对象或只保存数量；Stage 7A 可以从同一内部内容取用。

### 分离内容身份与执行配置身份

Packed semantic identity、exact artifact hash 和 CXC package hash 保持 Foundation 原算法。
candidate PreparedSemanticIdentity 使用独立域
`cuexis.prepared-semantic.v5.candidate.1`，包含 semantic hash 与排序后的实际资源身份，
不包含路径、压缩布局、设备、窗口、clock mode 或音量。资源列表必须覆盖经校验的传递闭包，
重复身份冲突必须拒绝；音频 bytes 沿用现有 audio content identity 规则。

ResolvedSessionConfig 单独有规范 identity；应用层的 SessionExecutionIdentity 再组合
prepared content identity 与该配置 identity。不能为满足“配置可复现”而改变 v4 的
PreparedSemanticIdentity，也不能把设备校准混入 Chart semantic hash。
A2 将域、字段编码和排序写入对应 Spec，并提供手工可审查的独立预映像/golden。

### Typed lowering 与离线组合

- `cuexis_chart` 增加 candidate lowering，输出现有 ChartRuntime 和内部身份/要求关联；
  不新增第二个 RuntimeSession。静态 alpha 的 `/255` 只在该转换点执行一次。
- CXT v2 展开只是离线组合输入。chart 层的 typed assembler 接收显式 chart metadata、
  静态实体与展开结果，验证身份/父图并派生 feature 和 resource closure；
  `cuexis.gameplay.candidate.lanes4` 由被验证的 lanes4 要求确定，不由 Player 补写。
  只包含静态组件时的 feature 集合必须遵循现有 profile validator，不凭空增加要求。
- Packed Writer 仍校验调用方 typed 内容，不静默修复错误声明；Reader 仍只 decode。
  assembler 的正例必须使用实际离线工具入口验证，不能仅在测试中注入 feature。
- 离线工具显式调用 assembler -> encode -> package；运行时不执行 CXT v2，不恢复来源 AST。
  revision 1 的 Behavior/Animation/Effect 禁止清单不变。

拒绝：运行时对象名反推 tuple、要求数据丢弃、hash=0 跳过验证、以资源路径代替内容身份、
为了复用 v4 编译器而丢失 candidate 字段、第二份只供 Player 使用的 feature 推导逻辑。

## S6-D04：Renderer 放在 Playback 之上的内部层

### 固定依赖图

箭头表示直接依赖；所有新增 target 均需注册和 allowlist 验证：

```text
cuexis_playback -> cuexis_runtime / cuexis_render / 既有内部依赖
cuexis_presentation_renderer -> cuexis_playback / cuexis_render / cuexis_core
cuexis_render_opengl -> cuexis_presentation_renderer / platform_sdl / 既有 shader cache
Player 控制层 -> cuexis_playback / cuexis_presentation_renderer / 后端无关 audio
Player 装配层 -> platform_sdl / render_opengl / audio_sdl
```

`cuexis_presentation_renderer` 是新的内部静态库，不是支持安装的 SDK component。
底层 `cuexis_render` 继续拥有 ECS render component、RenderScene 和 legacy debug 值；
它与 Playback/Runtime 均不得反向依赖新库。Playback 公共 presentation 类型不搬家，
也不为本次重构增加公共类型别名或第三方类型。

### 接口与职责

- 新层拥有 `IPresentationRenderer`、move-only `PreparedPresentation`、中立 draw command/
  summary 和统一 frame submission。接口消费既有 PreparedPlayback / FrameSnapshot，
  内容和 debug pass 在同一提交中组合；不另造一棵内容场景树。
- draw 排序/分 pass/summary 构造在新层共用，OpenGL 只消费命令并实现 GPU 上传和绘制。
  测试 renderer 也消费同一命令，不复制第二套“预期”排序实现。
- prepare 返回绑定 renderer instance/generation 与 Playback token 的候选；
  renderer 同时至多一个 outstanding candidate。候选析构在 owner thread 自动 discard，
  不使用全局 handle 表；renderer 必须比候选活得更久。
- 合法候选 activate/discard 为无分配、无失败交换；公开可恢复输入在 prepare/validate
  阶段返回 Result。错线程/违反对象寿命等编程错误沿用仓库策略，不吞异常后继续。
- submit 和 present 在接口上分离，但每个有效帧只提交/呈现一次。零尺寸表面暂停提交，
  resize 不重建 Playback 内容；重建 renderer 使旧 renderer token 失效。
- 窗口由应用装配层拥有，adapter 可持有受控 lease；renderer close 只释放其 GPU/context
  资源，不主动关闭应用窗口。最后销毁顺序为候选、renderer、窗口、平台 runtime。
- Player 正式控制/帧循环不能 include `render_opengl` 或 SDL；具体 factory、窗口事件翻译
  和设备选择只在装配/平台适配文件中出现。架构测试按源文件职责验证，不能只验证整个 target。
- 像素探针、原生 handle 与 OpenGL 信息留在 adapter 测试/诊断，不加入中立 summary。
  旧 OpenGL/RenderBackend 入口保留为兼容包装，共用新实现，不继续维护独立算法。

拒绝：render -> Playback 成环、复制 public presentation types、`void*` 万能后端接口、
每帧 switch 后端种类、为尚未实现的 Vulkan 暴露 API 对象、测试 renderer 只返回成功。

## S6-D05：配置、设备与事务控制

### 配置与应用结构

新增内部应用库 `cuexis_player_support`，拥有 typed 配置组合和不依赖 GPU/SDL 的命令状态机，
不由 engine 或宿主 SDK 依赖；未来 Studio 可按真实复用需求提取前端，不提前建通用配置框架。
ProjectConfig v1 除 D01 的注册扩展外不增加窗口、profile 或设备字段。

UserPreferences v1 的首批字段限于窗口大小/全屏、vsync、gain 和 AudioDeviceProfile ID。
默认值只有一处；配置文件/CLI 不支持通用任意 key 覆盖。gain 范围沿用 `[0,1]`，默认 1，
通过 AudioSDL 的独立命名方法动态 apply，不扩张既有 `IAudioTransport` 虚表。其余窗口/后端
静态请求与设备/profile 变化通过明确重建生效；不做文件监视器自动应用。
缺失/坏偏好用默认值并报告诊断，未来版本原件不得覆盖；显式 profile 错误则失败。
使用同目录临时文件、验证后原子替换、进程间写锁和版本校验，不使用 last-writer-wins。
这是首个 UserPreferences 版本；不存在的旧格式不能被发明成 v0 来展示迁移成功。
v1 内保存规范化必须幂等；只迁移实际存在且有 Spec 的旧版本，其他版本按明确拒绝/回退规则
处理。框架预留显式版本分派职责即可，不建立通用 JSON 字段迁移 DSL。

ResolvedAppConfig 保存请求值与来源；EffectiveSettings 只保存实际结果；
ResolvedSessionConfig 仅保存实际消费的 clock mode 与输出校准值，不记录窗口/音量/路径。
创建前探测与 SDL subsystem 初始化可以早于设备打开，但不能早于配置文件校验。
Session 创建/prepare 前必须已冻结本次执行配置。

### AudioDeviceProfile 的保证上限

- profile 有命名的 `system-default` 路由与显式 selector 两种模式。默认路由不冒充特定设备，
  不把其当前名称持久化为物理身份。校准非零值只允许显式 selector，默认路由校准为零。
- 显式 selector 固定使用 `(SDL audio driver, exact UTF-8 device name)` 精确匹配，要求恰好
  一个输出设备；不能使用列表 ordinal、当前进程 SDL ID、模糊匹配或第一项回退。
  枚举结果只在本次打开前绑定到实例 ID，打开时复核；热拔插后必须重新匹配。
- 这是**可复现选择条件，不是硬件序列号认证**。同名替换硬件无法由该合同识别；
  文档和 UI 必须如实说明。Stage 6 不宣称跨驱动稳定物理设备身份，不用未验证的 OS 私有
  API 假装解决。将来需要硬件身份认证时另增版本化 native selector，不能改写 v1 语义。
- `AudioSDL` 新增独立命名的枚举、匹配结果和 `createForDevice` API，类型只包含 Cuexis 值；
  不修改 `AudioConfig`、旧 `create` 或 `IAudioTransport` 虚表。旧 API 维持默认路由。
- 输出校准保存 `[-500000,500000]` 的整数微秒，默认零；
  正值表示设备比已有时间估算更晚输出。Player 的 CuexisAudio
  clock adapter 在 RuntimeTimeline 前执行一次 `max(0, positionMs - correctionUs/1000)`；
  不修改主音乐 bytes、Chart offset 或 transport 原始 snapshot。HostClock 由宿主自行校准，
  ChartClock 不消费音频校准。Seek 的反向 source position 为
  `targetChartTimeMs + timingOffsetMs + correctionUs/1000`，超出 `[0,duration]` 拒绝，
  不 clamp 用户目标。A2 补齐零位饱和、负校准与反向换算的独立 golden。

### 单一控制与提交顺序

所有播放命令在 owner thread 串行处理，同一时刻仅一个 load/reload/rebuild。
Stage 6 不实现后台异步 prepare、取消、自动重试或自动热重载。UI 事件翻译成 typed command，
smoke 也调用同一控制器；不在测试分支里保留另一套生命周期。

`Reload` 不改变 PlaybackMode，遵循 ADR 0032；切换有/无音轨或设备需要显式 `Open/Rebuild`，
构造替代 Session bundle 后切换，不把 reload 的 content mismatch 当作模式探测。
无音频 ChartClock 不因“没有音轨长度”自动停止；有音轨 EOF 进入 Ended。Stop 对音频回到
source 0，RestartAtZero 仍表示 chart time 0，两者不能混用。

事务固定为：

```text
1. 校验命令与当前状态，冻结本次操作目标时间和配置
2. Playback prepare，准备全部 renderer candidate 与 audio replacement
3. 预检所有 owner/generation/token；此后禁止回调重入或状态变更
4. 激活音频 replacement（最后一个允许返回设备失败的步骤）
5. Playback commit 的状态交换 -> renderer 无失败 activate
6. 发布应用 active bundle、EffectiveSettings 和一次 timeline discontinuity
```

- 音频准备必须完成解码、clip/store 注册及可预检的设备/stream 准备，不把分配错误推迟到
  已关闭旧 stream 后。需要同时打开而设备不支持时，准备失败并保留旧对象；
  用户可显式 Stop/Open，不自动关闭旧设备重试。
- 第 3 步必须通过私有 transaction guard 或同等可验证的 token 检查确保第 5 步不会因
  stale candidate 失败。不能仅靠注释“这里不会失败”，也不增加虚假的 public nofail commit。
- 第 4 步物理设备故障进入应用 Failed，取消未提交 renderer/Playback 候选；保留可诊断的
  最后内容身份，不声称旧音频仍可恢复。禁止播放旧音频配新画面或反向组合。
- 软件准备失败不 reset 旧 timeline/transport，不改变内容 generation。正在播放的旧音频
  可以自然前进；“保留时间”不是要求物理时间停止。目标时间按命令捕获值确定，
  KeepChartTime 如因同步准备产生位置跳转必须发布 discontinuity，不能静默伪装连续播放。
- renderer/present 在提交后出错，内容不倒退为旧版本；进入可诊断的 suspended/failed 状态，
  只经显式 Rebuild 恢复。对称地，配置文件保存失败不谎报为运行时 apply 失败或成功。

拒绝：全局可变配置、读取 UserPreferences 改 World、模式失败后自动重试、先 commit 内容
再执行可能失败的音频激活、吞掉设备错误、为了回滚而复制整个运行中的 World。

## S6-D06：固定离线媒体栈与发布模型

### 工具和依赖

新增 `CUEXIS_BUILD_MEDIA_TOOLS` 与 vcpkg `media-tools` feature，默认 OFF，独立于
shader-tools。内部 `cuexis_media_import` 与命令行 `cuexis_media_importer` 拥有解码流程；
Player/Playback 不链接该库，不通过启动外部 importer 实现隐式运行时解码。

解码器选择固定如下，来自当前 baseline
`40f3c709db80acf154ac4b17a1f83c564ebd022e` 的 port 记录；版本记录不是安全审计结果：

| 输入 | 选定依赖 | baseline 版本 | 固定方向 |
| --- | --- | --- | --- |
| PNG | libpng | 1.6.58 | 使用正式 reader 与限额/allocator 接口 |
| JPEG | libjpeg-turbo | 3.2.0 | 整数准确 IDCT，固定上采样与色彩转换选项 |
| MP3 | minimp3 | 2021-11-30 | 仅 Layer III、禁用 SIMD、流式有界读取 |
| Ogg Vorbis | libvorbis / libogg | libvorbis 1.3.7#4 | 单逻辑流，不接收 chained/multiplexed stream |
| FLAC | libflac | 1.5.0 | native FLAC，整数解码 |

不使用系统安装的 ffmpeg、动态探测 codec 插件、自研 codec 或多个 decoder 的失败回退。
库只通过私有薄适配层调用，不建立只有一个实现也要动态注册的通用 codec 框架。
解码库报告的截断、hole、坏帧和校验错误不得通过补零、跳帧、重同步继续输出成功产物；
合法 metadata 的跳过按 profile 执行，不能与损坏恢复混为一谈。
许可证以固定 port 的上游文本及实际分发闭包复核，尤其不能用 manifest 单一 SPDX 标签
替代 libjpeg-turbo 的组合许可证材料。实际接入时再同步依赖政策、manifest 与 notices。

### 输出 profile 的不可变选择

- 图像输出既有 `CXPRES01` RGBA8，sRGB 标记、straight alpha、左上起点逐行排列。
  缺省无颜色 metadata 按 sRGB；非 sRGB ICC、非支持 gamma、CMYK/YCCK、APNG 拒绝。
  PNG palette/gray/tRNS 展开为 RGBA；16-bit PNG 在 v1 拒绝；JPEG 基线与渐进式均须测试。
  EXIF Orientation 1-8 在导入时归一化且只应用一次，坏/矛盾 metadata 拒绝。
  颜色管理和 HDR 不在 v1 中悄悄近似。
- 音频输出固定 RIFF/WAVE PCM S16LE，保留受支持输入采样率与 mono/stereo，不重采样、
  不混音、不归一化响度、不加 dither；只写规范 fmt/data，不写时间戳或源路径。
  MP3 使用选定库对有效 delay/padding 的单次裁剪；无 metadata 时不猜测静音；
  Vorbis 按 granule 终点截尾，FLAC 保持样本数。禁止用 Chart offset 掩盖解码起点差异。
- FLAC 高位深转 S16 使用固定整数舍入/饱和；Vorbis 浮点量化采用明确且不依赖当前
  rounding mode 的规则，NaN/Inf 拒绝。A2 将量化和边界规则写成独立 golden。
  固定 compiler FP 选项，禁 fast-math/FMA 融合等会改变结果的优化。
- 同 profile 的输出要求 Windows/MSVC、Windows/MinGW、Linux/GCC/Clang 逐字节一致。
  不允许按平台分不同 profile 来掩盖失败，不允许用音频/图像近似比较替代 canonical hash。
  无法满足时阻塞 E1/E2 并重开该 profile 决策，不能静默换库或更新 golden。

### 预算与原子发布

以下为 v1 的保守上限，A2 Spec 必须落盘并与既有资源限制取更严格者：

| 资源 | 上限 |
| --- | --- |
| 单个 encoded source | 64 MiB |
| 图像每边 / 总像素 / RGBA8 | 8192 / 8,388,608 / 32 MiB |
| 音频采样率 / 声道 / 时长 | 8,000-192,000 Hz / 1-2 / 1,800 s |
| 最终 WAV（含 header） | 64 MiB，同时满足既有 encoded WAV 门禁 |
| 单次导入额外解码/转换工作内存 | 128 MiB，不含已计入的 source 与最终输出 |
| 导入并发 | 1，批量输入串行处理 |

计数/乘法先检查再分配，逐帧/逐行检查；不信任 metadata 总长度，不完全解压后才拒绝。
对库内部无法可靠计量的分配必须使用有硬内存限制的 worker 进程兜底；不能只在最外层
catch bad_alloc 冒充预算。硬限制的可测执行证据是 E1/E2 门禁，不把吞吐指标当安全界限。

缓存键由原始 bytes hash、版本化转换 profile、decoder 精确版本/构建选项组成；
缓存内容必须重验 identity。产物路径按内容身份寻址，已存在的内容不可就地覆盖。
项目多文件输出写入新的不可变 generation 目录；CXC 输出写到同目录临时包，完成关闭和
完整校验后只原子替换最终包。使用进程间发布锁，同目标第二个 writer 返回 busy。
禁止多文件各自 rename 后宣称项目级原子性；generation 导入结果不会自动改写用户项目入口，
显式采用该 generation 或重打包才使其生效。

原始资源保留在作者侧，provenance 记录 source/profile/output 身份；发行 CXC 只要求
运行时实际闭包，不为追溯性强迫分发原始 JPEG/MP3。打包不自动导入，Playback 不修复缓存。

## S6-D07：版本门禁

- 合并必须基于受保护 `master` 最新 SHA；要求 PR 分支保持最新并串行合并，不在 Stage 6
  同时建设第二套 merge queue 版本分配协议。docs-only 同样递增。
- 比较基线取受信任目标分支事件/历史，不接受 PR 提供的基线或发布日作为最终权威；
  检查逻辑由受保护基线版本执行。修改门禁本身需要代码所有者复核与独立负例。
  首次装配由 owner 审查 checker 与保护规则，记录 bootstrap 例外及其基线；正式生效后
  不再允许候选自行替换 checker 获得通过。未配置保护规则时只能报告“脚本完成、门禁未启用”。
- 同 UTC 日期 build 精确 +1；跨日 build=1；发布门禁校验 UTC 日期。比较器接收显式
  基线/日期以便测试，但受保护 workflow 控制这些输入。缺历史或保护权限不能降级放行。
- 合并前检查会因目标分支变化失效；跨 UTC 日需要重跑。合并动作复核检查的日期与基线；
  仅靠前一天的绿色状态不可发布。合并后审计比较事件 before/after，防止直接 push 绕过。
- 历史 SHA 复验使用已记录的基线与发布日，只证明该历史门禁，不产生新发行版本。
  日期版本、SDK API、内容版本相互独立；手动创建 tag 不能替代递增检查。

## S6-D08：SDK 版本与具名宿主

Stage 6 SDK 目标冻结为 `0.7.1`：仅允许新名字的 additive 方法/类型，不改变已安装
struct layout、已有签名、枚举值含义、虚表或旧 source 行为；不增加支持安装的 render 模块。
旧 `0.7.0` consumer 源码对 `0.7.1` 应重新构建通过，`SameMinorVersion` 规则不变。
shared 仍要求匹配构建/工具链，不承诺二进制替换。若必须 breaking change，先停止并重开
版本决策，同步 Stage 8；不能用 date version 或 experimental 标记绕过 API 版本。

2026-09-20 版本政策补充：遵守 [SDK 版本规范](../guides/VERSIONING.md)，
SDK 与 Stage 编号脱钩，minor 可增长到 `0.10.0`、`0.11.0` 及以后。
本 ADR 原先为 Stage 8 指定的 `0.8.0` 不再作为预留目标；正式 v5 发行按届时实际基线、
Stage 7A 等前序交付及公共合同差异批准具体版本。Stage 6 的 `0.7.1` 仅在当前 `0.7.0`
基线与兼容条件成立时有效；基线或合同变化须先重开决策。Stage 12 的 `1.0.0` 取决于
稳定合同和 ABI 验收，而非 minor 数值。

具名宿主冻结为 **Cuexis Reference Host**，位于独立示例工程 `examples/reference_host/`：

- 通过 clean-staged `find_package(Cuexis ...)` 消费安装包，源码目录没有 engine 私有
  include；作为独立进程，不复用 Player 主循环、配置库或 renderer 内部库。
- 自己拥有命令循环、内存 ContentProvider 和可控 HostClock；支持 open/play/pause/
  seek/reload/quit，显示实际帧摘要与结构化错误。可以无窗口运行，不假装交付第三方引擎插件。
- 自行消费公开 manifest/portable resources 与 snapshot，使用 validation sink 证明资源
  获取和帧一致性；脚本化命令只作为测试驱动，不是内容运行时脚本。
- production 构建验证 v4；experimental 安装树使用相同公开 entry 工厂和显式 entry path
  验证 candidate，保持同一个宿主代码路径。不通过源码树内部入口替代安装消费。
- Stage 6 的声明只覆盖该具名 C++ 宿主，不能写成 Unity/Unreal/Godot adapter 已实现。

SDK、Player、media tools 分开打包；production/experimental、static/shared、
Debug/Release 分开 staging。运行目录不依赖源码 assets 或开发机 PATH，
版本元数据、运行库和实际分发依赖的许可证必须随产物安装。

## 变更控制与必须保留的证据

实现者可以调整私有函数名、文件拆分和局部容器；不得自行改变上述入口、依赖方向、
身份域、profile 输出、提交顺序、版本策略或验收范围。新证据推翻设计时，提交
“复现/影响/替代方案/兼容与迁移/测试”并请求 owner 重新裁定，不以成本或工期替代授权。

尤其禁止通过修改 golden、关闭测试、放宽预算、按平台跳过测试、移除失败回滚或扩大
public header allowlist 来获得绿色。每个例外都必须先变更本 ADR 或所属 Spec。

A2 尚须完成：字段级 Spec/Schema、API 声明草案、模块/安装图、最小编译/接口表征、
独立 identity/媒体/配置 golden 与测试清单。它验证本 ADR 能被实现，不再负责从零选择方案。
SDK/库版本本轮不改代码、不新增实际依赖；没有新实现/hosted/GPU 证据，不关闭工程问题。

## 依据

- 仓库：
  [PlaybackSource](../../engine/playback/include/cuexis/playback/playback_source.hpp)、
  [render 依赖](../../engine/playback/CMakeLists.txt)、
  [音频 replacement](../../engine/audio_sdl/src/sdl_audio.cpp)、
  [ADR 0032](0032-playback-clock-and-prepared-audio-transaction.md)、
  [ADR 0033](0033-cpp-shared-library-preview-boundary.md)、
  [Foundation R5](../stage_reports/stages/chart-format-foundation/2026-09-17-r5-regression-and-handoff.md)。
- 依赖版本核对：上述固定 vcpkg baseline 的 `ports/libpng`、`ports/libjpeg-turbo`、
  `ports/minimp3`、`ports/libvorbis`、`ports/libflac` manifest；不是当前 upstream 最新版声明。
- 上游接口与许可证核对入口：
  [SDL device name](https://wiki.libsdl.org/SDL3/SDL_GetAudioDeviceName)、
  [minimp3](https://github.com/lieff/minimp3)、
  [libjpeg-turbo licenses](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/LICENSE.md)、
  [Vorbis](https://github.com/xiph/vorbis/blob/master/COPYING)。
  实施接入必须再核对固定版本的文件，不将这些 moving reference 当作已归档合规材料。
