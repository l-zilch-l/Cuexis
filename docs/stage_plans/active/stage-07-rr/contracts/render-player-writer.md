# Stage 7-RR 候选合同：渲染、Player 与写回

状态：candidate

更新日期：2026-11-22

实现状态：未实施。

<!-- cuexis-meta
id: PLAN-active.stage-07-rr.contract-render-player-writer
status: candidate
authority: owns
-->

代码核对基线：`9623efa3`。本页冻结 P2/P4/P5 的单一技术推荐，供审阅与拆卡；实施、发布和删除分别过门禁。[计划 D-05](../plan.md#4-已确认方向与决策台账) 的 RR-3 只含 R1/R5/R6。本页 P2 的旧内容布局、桥接和替代探针归 R5；新 Chart 作者数据、实验安装与保护符号退出分别过格式、SDK 与版本门禁。P4 和 P5 单列实施授权。已定行为见[架构设计附页](../architecture-design.md)。

## P2：同提交布局与呈现

源码对照：`playback_session.cpp::PresentationAccess` 有提交版本检查；`presentation_materializer.cpp::fillCamera` 使用 viewport 宽高；`PreparedPlayback::presentationCandidateToken()` 有准备身份，`FrameSnapshot` 无该身份。`app/execution_host/frame_observation.hpp` 的包只拥有快照、资源与成员，`presentOwnedFrame` 走旧 `submit`；`open_gl_presentation_frame.cpp` 用当前 active set 绘制，窗口提交与交换分离。

### 作者画幅

[D-23](../architecture-design.md#d-23-谱面画面比例与宿主尺寸) 的 Chart v5 根字段 `framings` 可省略；声明时有 1–16 项，数组顺序有意义，首项为作者默认。各项共享现有活动相机的位姿、运动和 `camera.fovY` 采样，共用场景、玩法与时钟。下列片段用于启用该字段的候选 Chart v5 schema，旧 Reader 和尚未启用该字段的候选路径保持原接受集。

```json
{
  "framings": {
    "type": "array", "minItems": 1, "maxItems": 16,
    "items": {
      "type": "object", "additionalProperties": false,
      "required": ["width", "height"],
      "properties": {
        "width": {"type": "integer", "minimum": 1, "maximum": 2147483647},
        "height": {"type": "integer", "minimum": 1, "maximum": 2147483647},
        "verticalScale": {"type": "number", "minimum": 0.125, "maximum": 8, "default": 1}
      }
    }
  }
}
```

`verticalScale` 是无单位的竖向半幅倍率：同一相机在同一观察距离，1 为共同 FOV 对应半幅，2 把半幅加倍。宽高要求精确 JSON 整数字面 token，拒绝小数点和指数写法。Reader 按原 JSON 有限数字规则解码为 binary64，保留原作者 pair、scale 是否显式填写及数组顺序；禁止 `null`、未知键、非有限值及越界，JSON/重复键错误仍走 Reader 原诊断。源 prepare 以 GCD 对 pair 约分，按作者顺序生成 `std::vector<FrameFraming>`；同一约分比只能出现一次，即使 scale 不同。形状/值错误报 `chart.framing.invalid`，重复报 `chart.framing.duplicate_aspect`，定位 `$/framings` 或首个出错元素/键；重复定位后出现的元素。未声明为 prepared 空向量，旧谱保持随 viewport 变化，不反推相机或读取 metadata。`FrameFraming` 只含已约分 pair 和已填默认的 scale；各承载层字段统一叫 `framings`，同字段一对一接线。新成品的 wire、错误及身份由 [prepared-artifact](prepared-artifact.md) 承接；旧成品不补，旧载体拒写有画幅的新内容。

### 内存与布局

头的唯一物理路径为 `engine/playback/experimental_include/cuexis/playback/experimental/frame_layout.hpp`，include 拼写 `cuexis/playback/experimental/frame_layout.hpp`，命名空间 `cuexis::playback::experimental`：

```cpp
struct AspectRatio final { std::uint32_t width, height; };
struct FrameFraming final { AspectRatio aspect; double verticalScale{1}; };
enum class FrameRotation : std::uint16_t { Deg0 = 0, Deg90 = 90, Deg180 = 180, Deg270 = 270 };
enum class FramingSelection : std::uint8_t { Default, BestFit, Exact };
struct FramingRequest final {
    FramingSelection selection{FramingSelection::Default};
    std::optional<AspectRatio> exact;
};
struct PixelRect final { std::uint32_t x, y, width, height; };
struct FrameTarget final { FrameViewport extent; PixelRect host; FrameRotation rotation{}; };
struct FrameLayout final { FrameTarget target; PixelRect content; };
struct SelectedFrameLayout final { FrameLayout layout; std::optional<FrameFraming> framing; };

auto computeContentRect(std::optional<AspectRatio>, FrameTarget)
    -> core::Result<std::optional<FrameLayout>>;
auto computeFrameLayout(std::span<const FrameFraming>, FrameTarget, FramingRequest = {})
    -> core::Result<std::optional<SelectedFrameLayout>>;
auto preparedFramings(const PreparedPlayback&) -> core::Result<std::vector<FrameFraming>>;
auto extractLaidOutFrame(const PlaybackSession&, FrameTarget,
                         std::optional<std::uint64_t> exactResultVersion = {},
                         FramingRequest = {})
    -> core::Result<std::optional<LaidOutFrame>>;
auto relayoutRetainedFrame(const LaidOutFrame&, FrameTarget)
    -> core::Result<std::optional<LaidOutFrame>>;
```

`LaidOutFrame` 可移动、不可复制、无公开构造。`snapshot()`、`layout()`、`source()`、`resourceToken()` 分别返回 `const FrameSnapshot&`、`const FrameLayout&`、`const PresentationSourceInfo&`、`const PresentationCandidateToken&`；`framing()` 返回 `std::optional<FrameFraming>` 值（未旋转的选中记录）。它拥有上述值及私有规约画幅表（`std::array<FrameFraming,16>` 加有效项数，固定开销进入 `sizeof(FramePayload)`）、规约选择请求和未缩放的本次相机 FOV，不持有资源字节、Playback、Runtime 或设备引用。`relayoutRetainedFrame` 沿现有 `rematerializeRetainedSnapshot` 的拥有型重投影逻辑，在私有 materializer 按原请求重选（仅 BestFit 随尺寸变），使用原相机样本重新物化矩阵，保留来源与 token，不授予正式帧租约。

`preparedFramings` 使用已有 Prepared 的 owner/有效性检查，返回 owning 拷贝。`prepareLaidOutOwned` 只接规约画幅表（0–16 项，已约分、去重、scale 在域内）。Renderer prepare 对非空表要求 `PresentationCapabilities::hostExtensionIds` 含 `cuexis.presentation.frame-layout.v1`；Playback 的 `validatePresentation` 检查能力，拥有型路径在物化前同样检查并传播诊断。该能力要求适配器消费同帧布局及已旋转投影并裁剪；仅运行旧 `submit` 的适配器不可声明。OpenGL 只在具备新桥的构建声明，缺能力即拒绝 prepare，不动 A。画幅归 `playback_session_internal.hpp` 中 `PreparedPlayback::State` 与会话提交描述的纯值 `framings`，默认旧谱为空；`playback_session_prepared.cpp` 的读取沿已有 owner 门。从 `playback_candidate_internal.hpp`（引入 `chart/candidate_lowering.hpp`）分离对该纯值的访问，仅在 Playback 库内复用已有 friend，不导出 shim，不向默认目标引入 Gameplay/作者头或链接边。R5 帧测试经库内测试接线 在新建、未物化的测试 Prepared 上附已校验的内存表，随后只走真实提交、提帧与 renderer 路径，无测试 CLI 或安装写入口。T-11/T-13 在正常 source prepare/成品 decode 数据流中填充。

选定候选的 renderer 句柄 `accepts`、其 token 与待提交 Playback 的 `presentationCandidateToken()` 匹配后，才允许 commit 再无失败 activate。成功 commit 把 token 和画幅表写到 Playback 私有提交描述；Update/Seek/Restore 续用，卸载清除。提取经 `detail::PresentationAccess` 同一 owner/重入门和同一次 committed descriptor/Runtime read：先查提交及精确版本，再选画幅及算布局，用该读生成投影、快照、来源和 token。旧 extract 可仅观察新谱默认作者投影（0°，无栅格修正）；内置旧 `submit` 对有画幅的 active set 拒绝。旧谱原入口保持现有投影与行为。

矩形左上原点、半开。`host` 为目标绝对坐标，`content` 为 host 相对坐标。`computeContentRect` 校验比例域（接受域内未约分 pair）、rotation，再用 `uint64_t` 校验 `host.x+host.width <= extent.width` 与纵轴同式，包括零宽/高；不得裁短。合法空目标/host 或取整结果为空时返回成功空值。旋转表示从屏幕看顺时针转显示，90°/270° 将显示比交换为 `(height,width)`，0°/180° 不交换；Exact 查找仍用未旋转作者 pair。非空 host `(W,H)` 和显示比 `(a,b)`：若 `W*b <= H*a`，`(w,h)=(W,floor(W*b/a))`；否则 `(w,h)=(floor(H*a/b),H)`。乘积用 `uint64_t`。`content=(floor((W-w)/2),floor((H-h)/2),w,h)`，余数留右/下。旧谱无比例 `content=(0,0,W,H)`。

`computeFrameLayout` 复用 `computeContentRect`；提取、retained 重排与 Player 初始窗口同用这一选择函数。先验全表、请求与 target，再判空。`Default` 选首项；`BestFit` 用旋转后的各候选算 content，最大化实际可绘物理像素面积 `uint64_t(w)*h`，平手取最早声明（默认优先），所有项面积为 0 则跳帧。`Exact` 只用约分后等于声明的项，未匹配（含空表）拒绝 `playback.presentation.layout.aspect_unavailable`，带请求比；`Default`/`BestFit` 配 `exact`、`Exact` 缺 `exact` 或未知枚举报 `playback.presentation.layout.invalid_selection`。Player 在 `validate` 阶段以合法空 target 调此函数预检 Exact（匹配时成功空值），在实际选幅时传有效物理区。旧谱空表对 Default/BestFit 均用无比例路径。

投影用列向量。取 Runtime 的 `sampledFovY`（度，现行有效域 `(0,179)`）、同次 pose、near/far；作者比例 `ar=width/height`。设 `t=verticalScale*tan(sampledFovY*pi/360)`，`effectiveFovY=2*atan(t)`（弧度），调用现有 `core::makePerspective` 的 `(0,pi)` 域，不对派生值套作者 179° 上限或钳位；等价 `P[0][0]=1/(t*ar), P[1][1]=1/t`。比例变更只改变取景；view、裁剪面、时间采样不变。旧谱在偶数四分转用 `ar=w/h`，奇数四分转用 `ar=h/w`，沿现行 `fillCamera` 透视路径。`snapshot.camera.fovY` 保存本次有效竖向角（度），viewport 保存实际 `(w,h)`。换算或写入 float 矩阵非有限/退化时沿原 camera materializer 错误，不交付帧。 未选中相机时按既有 inactive camera 路径，不制造投影或可反算的射线；选中但不存在沿 `playback.snapshot.camera_missing`，换算错误沿 `playback.snapshot.camera_invalid` 并保留具体 cause。

在齐次 clip 坐标应用 `Pout = Q*R*P`，保持 z/w 不变。`P` 只指上述未乘旋转和栅格修正的基础透视矩阵；交付的 `snapshot.camera.projectionMatrix` 唯一保存最终 `Pout`，不保存 `P`。`R` 的 `(x,y)` 分别为 0° `(x,y)`、90° `(y,-x)`、180° `(-x,-y)`、270° `(-y,x)`；应用在透视之后，不滚转相机。为消除整数 viewport 舍入造成的微量不等比，有作者画幅时令旋转后精确显示比为 `r`，`k=max(w/r,h)`，`Q=diag(k*r/w,k/h,1,1)`：以等比像素密度填满内容矩形，舍掉栅格窄轴不足一像素的边缘，视觉中心不动。旧谱四种旋转均 `Q=I`：0°/180° 基础 `P` 使用 `w/h`，90°/270° 使用 `h/w`，随后同样应用对应 `R` 一次；交换 aspect 不能替代旋转。旧谱 0° 直接保存原 `P`，不另乘单位矩阵，保持原值与摘要；无 rotation 参数的旧 extract 仍为原 0° 路径。Renderer 只用同帧 `projectionMatrix * viewMatrix`，不重算 FOV/aspect 或再次乘 Q/R。2D 窗口 UI/占位不转，谱内几何及谱内 debug 几何使用该投影。

T-05 在非安装 `engine/playback/src/frame_layout_internal.hpp` 定义纯值 inline helper `detail::composeFrameProjection(const core::Mat4& baseProjection, const experimental::SelectedFrameLayout&) -> core::Result<core::Mat4>`，两种 flavor 和私有测试直接复用这一实现，不导出。它只接已通过布局校验的非空值，依本节计算 R/Q/Pout；非有限/退化沿原 `playback.snapshot.camera_invalid`。T-06 的 materializer 调此同一 helper 并写入快照，retained 重排从原相机样本重建 P 后再调，不能把旧 Pout 当 P 累乘。此 helper 不增加安装签名、公开帧字段或 Gameplay 依赖。

未来位置输入边界（本批无位置 API）：宿主先把平台逻辑坐标换成绝对目标像素（像素索引使用中心 `i+0.5`）。以本次布局的绝对 content 半开矩形判内外，空、留边、右/下边界、非有限坐标均无命中；坐标减内容原点后，`ndc=(2*x/w-1,1-2*y/h)`。直接用同帧 `(projectionMatrix * viewMatrix)^-1 = (Pout * V)^-1` 对近/远 clip 点反投影并各自除 w，得到射线；此前不得再乘 Q 或 R 的逆。测试若要对照作者 NDC，可单独计算 `R^-1 * Q^-1 * ndc`，但这一对照结果不得再送入 Pout 的逆；分步反算只允许接未修正的 P 的逆。不得用当前窗口、相邻帧或作者默认代替。键盘逻辑通道完全不经过该变换。

初始窗口另算：已知首个候选画幅后用其选中显示比（Default 默认项；BestFit 以显示器可用物理区为 host；Exact 指定项）。短边 `S=720` 物理像素，目标 `(S*a/min(a,b), S*b/min(a,b))`，按 `s=min(1,availableW/targetW,availableH/targetH)` 等比缩两轴，最后各向下取整至至少 1；用检查过的宽乘与有理比较，不先舍入原目标。逻辑可用区乘窗口所在显示器每轴 DPI 缩放后向下取整得物理预算；换回逻辑尺寸时除每轴缩放向下取整，非零目标不足一逻辑单位取 1。Player 先创建可关闭外壳，首个成功 Load（含首次失败后成功 Reload）才应用一次窗口大小，后续 Reload 不改窗口大小。建窗后和 resize 读回 `drawableSize`；显示器可用区或 DPI 查询失败报告平台诊断并保留外壳尺寸，读不到有效 drawable 时跳过帧。旧谱沿用现有窗口宽高设置，90°/270° 仅交换初始设置宽高；初始大小不约束以后 SDK 布局。

### 提交、资源与交付

头 `cuexis/render_opengl/experimental/frame_layout.hpp`，命名空间 `cuexis::render_opengl::experimental`：

```cpp
struct BorrowedGlFramebuffer final { std::uint32_t name; playback::FrameViewport extent; };
auto prepareLaidOutOwned(OpenGlBackend&, const playback::PresentationCandidateInput&,
                         std::span<const playback::experimental::FrameFraming>)
    -> core::Result<presentation_renderer::PreparedPresentation>;
auto submitLaidOut(OpenGlBackend&, const playback::experimental::LaidOutFrame&,
                  const playback::experimental::FrameTarget& currentTarget,
                  const render::RenderScene* debug = nullptr,
                  const BorrowedGlFramebuffer* borrowed = nullptr)
    -> core::Result<presentation_renderer::DrawSummary>;
```

**R5 的链接与安装。** 两种 flavor 的 `cuexis_playback` 编入同一实现。两个 shared 库均以 `CUEXIS_PLAYBACK_API` 逐函数导出 `computeContentRect`、`computeFrameLayout`、`preparedFramings`、`extractLaidOutFrame`、`relayoutRetainedFrame`。`LaidOutFrame` 的析构、移动构造、移动赋值及 `snapshot() const`、`layout() const`、`source() const`、`resourceToken() const`、`framing() const` 均采用非内联定义并按成员导出（移动/析构 `noexcept`）；私有构造和访问 shim 不导出，复制删除。`VerifySharedExports.cmake` 在两种 shared 配置按限定名、签名及必要的 ABI 析构/构造变体逐项要求并放行；不放行整类或整个命名空间。T-05 先交两个几何函数，T-06 补齐整表。仓内 Player 与实验 SDK 直接调用同一符号，不增 `detail` 转发桥、第二套 DTO、已装旧成员或虚表。

默认构建只给实际消费头的 Playback、Player、GL、宿主及相应测试目标 `PRIVATE` 加 `engine/playback/experimental_include`；普通 `add_subdirectory` 消费者不会继承此根。仅实验 flavor 将同一根/头加入 `Cuexis::Playback` 的 `PUBLIC FILE_SET HEADERS`，装入独立实验前缀，沿用 opt-in。默认安装头不包含或引用该头；默认 DLL 的具名 R5 符号可供同版仓内 Player 链接，不据此承诺默认 SDK 源接口。GL 头归源码目标，不安装 GL/SDL；两种 Player 共用原命令校验与 draw。

会话 owner 从同一个 `PreparedPlayback` 复制画幅表，随 owning candidate 及其 token 在 `execution_host_support` 私有候选消息内传值。GPU owner 走 `prepareLaidOutOwned`，核验目标适配器的实际能力与输入一致，复用 `prepareOwned` 资源准备及原 `accepts`/`activate`，私有 pending/active resource set 拥有画幅表。非 owning `OpenGlBackend::prepare(PreparedPlayback&,...)` 直接取同一表。旧 owning renderer 入口只处理空表的旧语义内容。`submitLaidOut` 验证 `framing()` 恰等于 active 表中的一项（空值与 active 表为空等价），仍必须验证 token；旧 `submit` 对非空表拒绝。Rebuild 重用原 owning token、表和资源，只更新 renderer 身份。

OpenGL 先过 owner、设备健康、未提交检查；逐字段比较 `currentTarget` 与帧的 `target`（extent 两轴、host 四字段、rotation），再验证帧 token 等于 active set 身份、每个 `PresentationResourceRef` 全值匹配。`currentTarget` 来自调用 owner 此刻的目标，不能从待交帧拷出冒充当前值。窗口路径按既有 owner 激活窗口 context，绘制到默认 FBO，extent 还须等于实际 drawable。借用要求非零 FBO 名、声明 extent 相同，调用期有效；适配器核对当前 context 就是其活动 context，不替宿主换 context。GL 整型范围超限在修改状态前拒绝。

借用 FBO 首版明确支持单样本、非 layered 的两个 renderbuffer 附件：`GL_COLOR_ATTACHMENT0` 为 `GL_RGBA8`，`GL_DEPTH_ATTACHMENT` 为 `GL_DEPTH_COMPONENT24` 或 `GL_DEPTH_COMPONENT32F`；两者实际宽高均等于 extent。拒绝 texture 附件、其它颜色附件、stencil/packed depth-stencil、多样本、尺寸/格式不符或不完整 FBO，返回 `render.opengl.presentation.target_mismatch`，`reason=attachment_layout|framebuffer_incomplete`，带实际附件/格式/尺寸；在 clear/draw 前完成验证。复用现有 GL 绑定查询核验 renderbuffer，不新增目标注册系统。保存该借用 FBO 的全部 `GL_DRAW_BUFFERi` 路由，再显式 `glDrawBuffer(GL_COLOR_ATTACHMENT0)`；不能以 complete 为由沿用 `GL_NONE`。返回前在该 FBO 仍绑定时用 `glDrawBuffers` 恢复整组路由（包括原 `GL_NONE`），然后恢复调用者的 read/draw FBO。该路由归还在 draw 失败时同样执行。

绝对内容为 `(host.x+content.x,host.y+content.y,w,h)`；对 host 和合成后内容分别用 `(x,extent.height-(y+h),w,h)` 转成 GL 矩形。先 host scissor，开启颜色/深度写入、清至快照清屏色和深度 1；再 content viewport+scissor，debug 也裁剪。借用绘制显式关 stencil test、rasterizer discard、color logic op 和 framebuffer sRGB，polygon mode 设 fill、depth range 设 `[0,1]`，其余深度/混合/剔除沿既有 draw 路径设置。探针用绝对 GL 内容中心。所有退出点归还触及状态：读写 FBO、该借用 FBO 的 draw buffer 全路由、查询所用 renderbuffer 绑定、viewport、scissor 及 enable、清值和写掩码、深度/混合/剔除（含 front face）状态、上述 enable/polygon/depth range、program/VAO/array buffer/UBO（含槽 0）、active texture 与触及单元的 2D 绑定；触及 pack/read buffer 状态也归还。窗口成功置待 present 标记；借用成功只绘制，不 swap 或置窗口标记。失败不发布摘要/probe/新标记；验证通过后 draw 失败可能已改变 host 内像素，状态归还不承诺像素回滚。

Player assembly 在 `player_app.cpp` / `player_execution.cpp` 完成 backend holder 的最后一次移动后，用该 **同一** `OpenGlBackend&` 创建私有 `FrameLayoutRendererBinding`。binding 按值持有一个非 owning `IPresentationRenderer*`（由该 backend 上转型）、同对象的非 owning `void* object` 及两个无分配函数指针：`prepareOwned(void*, const PresentationCandidateInput&, std::span<const FrameFraming>) -> Result<PreparedPresentation>` 与 `submit(void*, const LaidOutFrame&, const FrameTarget&, const RenderScene*) -> Result<DrawSummary>`。`player_assembly.cpp` 工厂将 opaque 指针转回原 `OpenGlBackend*`，调用本节两个 GL 自由函数；`present`、`identity`、`accepts`、`activate` 仍用 binding 中同对象的 `IPresentationRenderer`。普通 loop 的输入和 `ExecutionHost::create` 注入此 binding；现有旧语义测试可仅注入 renderer，遇布局帧或非空画幅表 prepare 则拒绝。binding 构造后 backend 不移动，owner 上关闭 host/loop 后才销毁 backend；无 RTTI/downcast 已装接口，不增安装虚表，`execution_host_support` 不链接 OpenGL。

普通 `player_control_frame_loop.cpp` 每轮读到 drawable 后，在同 owner 以物理 extent 和满区 `host={0,0,width,height}` 及 rotation/selection 即取即交（R5 使用缺省值或测试注入，T-15 接启动选择）。跨线程 `ExecutionHost::trySample` 在 owner 发 `SessionRequestFrame` 前，从最近一次已应用的 drawable/host（含当轮 resize）生成同形 target 并与启动选择请求一起锁入该次请求；嵌入宿主在其 owner 直接提供自己的 extent/host。worker 只用请求值，不能读取窗口或把尺寸变化覆写到既有帧；目标变化后的旧 attempt 按下一段在提交前完整比较并放弃，重新请求；GL 再核对实际 drawable。窗口缩为零仍传空布局目标，不能退回上次非空尺寸。

正式帧的当前权由外层核对。跨线程从原 `FrameRequest` 将不可变 deadline 存到每次 `FrameEnvelope` 的 dispatch 元数据，不存到可别名的 payload；内部 `RendererConsumerGate` 消耗 attempt，按现有 context、projection scope、renderer 实例/generation、帧 ID 和 dispatch/attempt lease 核对。将 FPS/debug scene 在 submit 调用前建好；随后 `presentOwnedFrame` 在调用任一 submit 前用当前 gate 校验同一已消费 attempt 的实时路由、租约及 `now < 原请求 deadlineMonotonicUs`，不重复 `begin`。采样复用请求所用的注入单调源（缺省 `nowUs`），不能在 take 或 gate.begin 时仅检查一次，也不重开时限。到界使 gate 结束该 attempt、清 `awaitingSuccess` 且保持旧 retained，归还 attempt；host 记录 `expiredRequest` 和 `host.rpa.context_stale`（`reason=frame_deadline`），不改 worker 已转交的历史。零 submit、零 present、零成功 receipt/retained serial，不触发 renderer 故障。`submitLaidOut` 验证资源、传入的完整当前 target 及实际 drawable/FBO；旧帧与 active token 相同也不能跳过外层门禁。

`presentOwnedFrame` 在现有参数中追加 `std::optional<FrameTarget> currentTarget`（按值，无默认值）；legacy 调用传空，布局调用必传当前 owner 值，旧入口模式不符仍拒绝。`ExecutionHost` 的 GPU owner 在 overlay/smoke hook 返回后、调用该函数的最后一刻取已应用的 drawable/host/rotation；嵌入宿主直接从自己的当前目标状态取同结构。外层 gate 在同一 owner、无回调/泵消息的区间内逐字段比较 `currentTarget` 与已消费 attempt 的 `frame.layoutView()->target`，然后核对 deadline/路由并交给 binding。ordinary loop 同样在 hook 后重取和比较，再把同一值传 `submitLaidOut`。不能只比 extent、payload fingerprint 或 active token。任一字段不同（包括 host 同尺寸移位）结束该 attempt、清 `awaitingSuccess`、归还 attempt，报原 `host.rpa.context_stale`（`reason=frame_target_changed`，带请求和当前完整 target），保持旧 retained 和 serial，零 clear/draw/submit/present/成功 receipt；这属于非故障弃帧，不调用 renderer-failure 或设备暂停路径。随后按新 target 请求，空目标走成功空布局。`currentTarget` 通过原调用参数传递，不增 generation 总线。

正式冻结重绘仅在同局、同内容/资源及同 renderer generation 的新合规 attempt 内使用 active set。宿主主动留存旧帧时，在 Prepared 有效且 owner 可访问时从 `presentationManifest()` 枚举并用 `acquirePresentationResource(ref)` 持有需要的 `PortableResourcePtr`；用自有适配器按引用全值验证、导入自有 GPU 资源并重排后绘制，不回查当前会话或 mint 当前 receipt。内置桥不持退役资源，不接收覆盖资源集。

`FramePayload` 保留首成员 `StorageReservation`、资源和 membership，用 `std::variant<FrameSnapshot, LaidOutFrame> frame` 替 `snapshot`。`FrameEnvelope::snapshotView()` 取 variant 的快照；新增 `layoutView() -> const FrameLayout*`、`laidOutView() -> const LaidOutFrame*`，传统 fallback 返回空；布局帧的协议 `source` 从同一帧拷出。`FrameRequest` 与 fingerprint 加 `optional<FrameTarget> target` 与 `FramingRequest framing`；有 target 时完全忽略旧 viewport（包括 admission 检查），extent、host、rotation 及规范化选择参与布局和复用身份；旧请求必须用缺省选择。固定开销以 `sizeof(FramePayload)` 入原 budget，动态开销沿旧规则；别名只记 attempt。空布局完成请求为 `DeliveryStatus::skippedEmptyLayout`，归还 reservation，无帧/呈现成功回执。窗口 submit 成功才 present；实际成功才记录 receipt 及 mint retained；嵌入由宿主实际呈现后走原 receipt 路径。

**T-06 的实际接线。** 在 `app/execution_host/frame_observation.hpp` 给 payload 增仅返回引用的仓内 `snapshotView()`，统一由 variant 的 `FrameSnapshot` 分支或 `LaidOutFrame::snapshot()` 取得；envelope 仍保留无 payload 时的旧 fallback。`session_worker_owner.cpp::SessionOwner::requestFrame` 在原 reservation 内，用 `emplace<LaidOutFrame>(std::move(frame))` 放提取/retained 重排结果，legacy 分支放 `FrameSnapshot`，不复制 LaidOutFrame 或额外存一份快照；布局空值走上述 skipped 终态。`frame_delivery.cpp::payloadBytes` 改读这一 payload view，计当前分支的对象容量、对象字符串、资源指针及 membership，固定大小只计一次；`frameByteBound` 使用新的 `sizeof(FramePayload)`，保留溢出检查。画幅定长数组、source/token 及 variant 自身已在固定大小内，不重复加收；retained 别名只新增 attempt，不重收 payload。

`presentOwnedFrame(..., Renderer& renderer, MonotonicNow&& now, std::optional<FrameTarget> currentTarget)` 在完成外层门禁后，布局分支调用 `renderer.submitLaidOut(*frame.laidOutView(), *currentTarget)`，legacy 分支调用 `renderer.submit(frame.snapshotView())`；用模板能力检测让仅有旧 submit 的测试 adapter 仍可编译，遇布局帧则报 `playback.presentation.layout.unsupported` 且零 submit/present。布局缺 target 或 legacy 配 target 在调用前拒绝。`app/player/src/execution_host_frame.cpp` 的局部 `OverlayRenderer` 保留 identity/present 到原 renderer，新增上述 `submitLaidOut` 方法，借用 assembly 注入的 `FrameLayoutRendererBinding` 并调用 `binding.submit(binding.object, laidOut, currentTarget, scene)`；`object` 是 binding 的同 backend opaque 指针。其 legacy submit 仍调用原 renderer。scene 在 gate 最后核验前由同帧 `snapshotView()` 建好，wrapper 只借用该调用期有效的 scene 指针，不能在 submit 内再次运行 overlay/hook 或采窗口。ordinary loop 同样借用这组 frame/scene/target 调 binding；FPS 的 `snapshot_scene.cpp::appendFpsOverlay` 沿现有完整 `(Pout*V)^-1` 反投影，使屏幕文字不随谱旋转，不额外逆 Q/R。缺 binding 不能退回 snapshot submit。无新虚表、第二 DTO 或执行宿主到 OpenGL 的链接边。

冻结请求在 `SessionOwner::requestFrame` 的 retained 分支根据 `request.target.has_value()` 与 `retained.frame()->laidOutView()!=nullptr` 校验模式，包括旧 envelope fallback。身份、请求顺序和 fingerprint 沿原门禁；有 retained 但模式不符（或布局请求的规范化选择与 retained 不同）时，`FrameDelivery::admit` 通过原请求记录保存终态 `noRetainedFrame`，在分配帧 ID、attempt 或 payload 前返回，避免 `takeDelivery` 后的 `pollDelivery` 把终态读成 `historyUnavailable`。该请求不撤销或改写旧 handle、资源、membership、来源，无 `presentationSource`/Runtime/Fact 重采样、重排、submit、present 或 receipt；重复同请求返回同一终态。两向都拒绝：legacy retained 不能补出 token/布局，layout retained 不剥布局回退。模式每请求核验，切换或重建不改变旧 handle 的模式；只有新模式下实际呈现成功的帧才能提供对应的新 handle。模式相同且现有 retained 路由验证通过时，layout 同 target（含 rotation） 可别名旧 payload，任一项变化用 `relayoutRetainedFrame`；legacy 继续以 viewport 比对并用 `rematerializeRetainedSnapshot`。两条重排都只使用 retained 自身的值、资源和 membership。

| 首个失败点 | 诊断 |
| --- | --- |
| 比例域/规约不符、目标越界或未知 rotation | `playback.presentation.layout.invalid_aspect`、`playback.presentation.layout.invalid_target`，带比例、host/extent 或 rotation |
| 内存 scale 非有限/越界、画幅重复或超容量 | `playback.presentation.layout.invalid_framing`，带首个出错元素索引（超容量带项数） |
| 选择对象非法、指定比例未声明 | `playback.presentation.layout.invalid_selection`、`playback.presentation.layout.aspect_unavailable`，带选择或比例 |
| 无提交、owner/重入、版本错 | 原 `PresentationAccess` 错误，包括 `playback.session.empty`、`playback.presentation.version_unavailable` |
| 缺布局能力/桥 | `playback.presentation.layout.unsupported` |
| 实际 target/FBO/GL 整型范围错 | `render.opengl.presentation.target_mismatch`，带期望/实际尺寸或 FBO 原因 |
| 非 active token 或资源错 | `playback.presentation.frame.resource_mismatch`；设备/draw/present 继续透传原始码 |
| 过期/错路由 | 原 `host.rpa.context_stale` 或 deadline/lease 拒绝，无 submit |

这些是各模块 `core::Error` 或 `Diagnostic` 字符串。实施同批分别更新[帧呈现合同](../../../../api/presentation-and-capabilities.md)、[Player 参数](../../../../guides/PLAYER_OPTIONS.md)及[控制合同](../../../../architecture/PLAYER_APPLICATION.md)、[Chart 写回与迁移合同](../../../../formats/CHART_FORMAT.md)及 [v4 合同](../../../../formats/CHART_V4_FORMAT.md)；按[诊断码指南](../../../../guides/DIAGNOSTIC_CODES.md)检查 producer 和覆盖规则。它们不属于 `judgement` stable registry，不新增全仓注册框架。`skippedEmptyLayout` 是内部交付状态。`FrameRequest`、fingerprint、payload、私有候选消息和相关 reply 译单元由非安装 `cuexis_execution_host_support` 同批重编，default/experimental 源码组合的布局不做宏分叉；不改已装 `HostContext`、`PresentationCandidateInput`、`FrameSnapshot` 等值的 ABI 来接桥或测试。

### 兼容目标及向量

按 [D-19 分组迁移共识](../architecture-design.md#d-19-sdk-示例与宿主试用)和 [P1 安装窗口](runtime-timing.md#安装与窗口)逐组安排。兼容新增阶段保留旧签名，交付替代与迁移说明；`RenderFrame/renderFrame` 等保护入口至少经历一个实际发布的过渡版本，并持续保留至对应替代入口稳定、适用证据通过后，再裁定删除版本。T-05/T-06 只提供 R5 证据；RR-4/T-08 只迁自身列明的实验入口，不授权删除渲染符号或 `PreparedPlayback::materializePresentation`。各组分别判断稳定性和批准退出，不以其他组的发布或删除代替本组裁定。

实施核对历史 tag 的导出、实际安装/分发工件和当时合同；曾承诺安装消费则用具名仓外宿主提供 hosted 迁移证据，只承诺源码消费则留安装门禁不适用凭据并验证独立源码消费者。未核实历史、替代尚未稳定，或 [ADR 0041](../../../../adr/0041-legacy-format-exit-policy.md) 的 characterization、诊断、identity/digest/canonical parity、Debug/Shared/Release 未通过，均保留旧入口。`renderPresentationFrame` 经核查证明纯仓内且不影响公开类或导出兼容后，可在调用迁移和验证完成后清理；否则同样执行保护窗口。共享类型待仍受支持的组不再依赖后清理，具体版本随发布裁定。

`player_app.cpp` 的 `RenderFrame` 启动占位迁到窗口外壳清屏/交换，无快照或正式 receipt。

**Smoke 借用探针。** 在私有 `PlayerHooks` 增 `beforeLaidOutSubmit(const LaidOutFrame&, const FrameTarget& currentTarget, std::uint32_t renderedFrames) -> core::Result<void>`。仅 `PlayerSmokeBinding::hooks()` 的 `--smoke-test` 绑定该 hook，旧 `beforeSubmit(FrameSnapshot,...)` 留给计时。`player_control_frame_loop.cpp` 在非空布局帧及 debug/FPS scene 就绪后、正式窗口 submit 前调用；传入正式 submit 将借用的同一个 `LaidOutFrame` 实例（现行无固定比例的 smoke fixture 也走布局路径），同时传 owner 本次完整 currentTarget，同步借用、不复制、重采或延长生命周期。每轮清空上一轮借用结果；保留原正式 pixel readback 启用路径，仍只在 `renderedFrames == 1` 运行双探针。仅在双探针均成功时保存本轮按值结果，`notePresented` 在同轮正式交换成功后比较三个摘要并消费，`validatePresented` 继续从正式帧取 digest 和正式 pixel probe。

源码私有头 `cuexis_internal/pixel_probe.hpp` 中 `OpenGlDiagnosticsAccess::probeBorrowedFramePair(OpenGlBackend&, const LaidOutFrame&, const FrameTarget& currentTarget) -> core::Result<BorrowedFrameProbePair>` 提供具体入口。`BorrowedFrameProbePair` 按值持有 `omitted`、`empty`，每项为 `{DrawSummary summary; std::array<std::uint8_t,4> rgba;}`。在 backend 的 GL owner 上沿窗口路径确保其 context 已激活，按 owner 当前 target 的 extent 为本次调用创建一套临时完整 FBO（单样本 RGBA8 颜色和 DEPTH_COMPONENT24 深度 renderbuffer），使用同一非零 `BorrowedGlFramebuffer` 先 `submitLaidOut(...,currentTarget,nullptr,&fbo)`，再 `submitLaidOut(...,currentTarget,&emptyScene,&fbo)`。每次只在当次借用 submit 成功后、下一次绘制前，从此 FBO 的 `GL_COLOR_ATTACHMENT0` 读取一个 `GL_RGBA/GL_UNSIGNED_BYTE` 像素：绝对 GL 内容矩形 `(x,y,w,h)` 的 `(x+floor(w/2),y+floor(h/2))`。用 CPU 4-byte buffer，读回前 pixel pack buffer 绑定为 0，`GL_PACK_ALIGNMENT=1`，`GL_PACK_ROW_LENGTH`、`GL_PACK_SKIP_ROWS`、`GL_PACK_SKIP_PIXELS` 均为 0；作用域归还 read FBO、read buffer 和原 pack 设置。FBO 与附件均在同一 context 创建和释放，各退出点恢复所触及绑定。借用绘制和读取均不更新 `lastPixelProbe()` 或正式摘要，不设置窗口 pending present，不交换、不产出 receipt。FBO 创建、任一 draw 或任一读回失败即返回原诊断带探针阶段，不发布部分 pair、不用旧 RGBA，不运行本轮正式 submit。新增类型和方法不安装，不改 GL 自由函数返回值或 renderer 虚表。

| 向量 | 精确断言 |
| --- | --- |
| R5-01 | 21:9 host `1280×720` → `(0,86,1280,548)`；高 721 → 上 86、下 87。9:16 host `720×1280` 满区，`720×720` → `(157,0,405,720)`。 |
| R5-02 | extent `1600×1000`、host `(100,50,1280,721)` → content 相对 `(0,86,1280,548)`、绝对 `(100,136,1280,548)`；GL host `(100,229,1280,721)`、GL content `(100,316,1280,548)`。FBO 外区洋红 sentinel 不变，留边为 clear 色，正常与 draw failure 都还原预设的非默认状态。 |
| R5-03 | 21:9、短边 720、可用 `2560×1440`、2× DPI → 初始物理 `1680×720`、请求逻辑 `840×360`；实际 drawable `1660×718` → `(0,3,1660,711)`，基础 P 的作者比仍为 `21/9`，输出投影另含 Q。旧谱同目标基础 P 用 `1660/718` 且 Q=I。 |
| R5-04 | extent `10×10`、host `(10,0,0,10)` 与 21:9 host `1×1` 均成功空值；`host.x=UINT32_MAX,width=1` 且 `extent.width=UINT32_MAX` 越界；`0:1` 失败。空帧不留 payload、不提交/呈现。 |
| R5-05 | A token 1/version 5：提 version 4 失败。B token 2 激活后内置拒交 A；宿主自有 adapter + A 原资源可重排绘制，换 B 同名异 identity 拒绝，无当前 receipt。B 缺能力则 prepare 拒绝、A active 不变。冻帧异原点同尺寸重新布局，无 Runtime 或新 Fact 读取。分别取同局同路由的真实 legacy 和 layout retained 各一枚：legacy handle + 有 target、layout handle + `nullopt` 两向均返回 `noRetainedFrame`，take 无帧、poll 和同请求重投仍同状态，帧 ID 为 0，新增 reservation、Runtime/Fact/重排、submit/present/receipt 均为 0；旧 handle 与 serial 不变，随后以匹配模式的新请求仍可重用原 payload。 |
| R5-06 | v5 `[{"width":42,"height":18},{"width":9,"height":16,"verticalScale":2}]` 保留作者 pair 与缺省标记；Prepared/commit 为 `[(7:3,1),(9:16,2)]`。拒绝重复 `21:9`（定位后项）、空数组、17 项、0/溢出/非整数宽、0.124/8.001/非数 scale、`centerX` 或 `camera` 键。缺省根字段为 legacy 空表。R5 纯布局 `extent=2147483647×1` 满区 `2147483647:1` 成功；`2147483648:1` 报 `invalid_aspect`。内存 owning prepare 用未约分 `42:18` 拒绝，scale=0 或重复规约比拒绝，均不替换 active。新 wire 的规约和拒绝向量见 [prepared-artifact](prepared-artifact.md)。 |
| R5-07 | 启动普通和 worker 流，用 binding 探针核对 prepare/submit/present 均落到 assembly 的同一 backend；ordinary 每轮、worker 新请求各取当前物理 target。请求 deadline=100：take@80、gate.begin@90，overlay 后 submit 前时钟=99 则提交和 swap 各 1，=100 则两者均 0（`frame_deadline`，无新 retained，旧 retained 仍有效）；同 payload 的新 attempt 使用自身 deadline。transfer 后 resize 或最小化，旧帧不交，下一请求用新尺寸/成功空值。另在 extent 固定 `1600×1000`、host 大小固定 `1280×721` 时，已取 attempt 的 host.x=100，overlay 后 owner host.x 改为 200（仍合法）；提交前全 target 比较报 `frame_target_changed`，零 clear/submit/present/成功 receipt，awaitingSuccess 清掉且旧 retained/serial 保持、无 renderer fault/暂停。新 target 请求可重排并呈现；同 extent 只改 host.y/宽高/rotation 也各拒旧 attempt。仅传旧 renderer 的对照拒交布局帧，旧调用和安装虚表不变。 |
| R5-08 | 在 GL owner 的候选 smoke 中记录两个借用 submit 和正式 submit 的帧地址，三者同值，提帧次数为 1；借用 pair 的 omitted/empty 摘要相等，并按现有 smoke 摘要规则对照正式帧。读回夹具 `extent=8×8,host=(1,2,6,4),aspect=1:1`，空 scene、青色清屏；绝对 GL content 为 `(2,2,4,4)`，两次均从本次 FBO `(4,4)` 取 `(0,255,255,255)`。借用前置不同的旧正式 probe sentinel，双探针之后正式 probe 和 pending present 不变、零 swap/receipt；正式帧单独 submit/present，以禁用 hook 的相同输入对照正式 RGBA 和 digest，交换仅一次。再于已有成功样本后的下一次探针注入第二次 draw 失败和第二次读回失败（各一次独立用例），均返回 cause 且不交付 pair、不复用旧值、零正式提交，所有 FBO 附件及借用/readback 状态已收回。 |
| R5-09 | 内存表 `[(2:1,1),(1:2,2),(16:9,1)]`：正方形中 `2:1` 和 `1:2` 都得 5000 像素（host `100×100`），仅前两项时 BestFit 取 `2:1`；加入 `16:9` 后得 5600 并胜。BestFit 在 `200×100` 选 `2:1`，旋转 90° 后选 `1:2`。Default 两种 rotation 均选首项；Exact `4:2` 归 `2:1`，Exact `3:2` 或 legacy 空表 Exact 失败、零绘制；Exact 首项在 `100×200`、90° 时满区，不能误选作者 `1:2`。 |
| R5-10 | 共同 FOV 先 90° 后 `2*atan(0.5)`：基础 P（尚未乘 Q/R）倍率为 1 的 `Pyy` 为 1→2，倍率为 2 的为 0.5→1（误差 ≤1e−6），同 tick 两画幅 view、near/far 及相机选择相同。`2:1` 和 host `100×200` 正好适配 90°，作者 NDC `(0.5,0)` 映到内容像素 `(50,150)`；在 0°/180° host `200×100` 映到 `(150,50)`/`(50,50)`，在 270° host `100×200` 映到 `(50,50)`，四者逆算回同一作者 NDC。R5-02 的绝对内容是 `[100,1380)×[136,684)`；`(99.5,136.5)`、`(1380,400)`、`(740,684)` 均不命中；绝对左上内点用完整 `(Pout*V)^-1` 一次反投影；作者 NDC 的 `R^-1*Q^-1` 只作独立对照，不再接 Pout 的逆。21:9 在 `1280×721` 得 `Qxx=1,Qyy=(1280*9/21)/548`，等比校正后的两轴像素密度相同。未知 rotation=45 拒绝；legacy 0° 与旧帧摘要相同；legacy 90°/270° 均用 reciprocal aspect 再应用 R、Q=I，同一基础相机 NDC `(0.5,0)` 在 `100×200` 分别到 `(50,150)`/`(50,50)`，仅交换 aspect 的阴性到 `(75,100)`。BestFit retained 在 resize 切换比例时保持原 camera sample/token，无运行时重读或累积缩放。 |
| R5-11 | 合法借用 FBO 的 color0 为 RGBA8、depth 为 DEPTH_COMPONENT24、同 extent 单样本 renderbuffer，初始所有 draw buffer 为 GL_NONE 且 complete。借用 submit 必须显式路由到 color0，读回本次清屏/几何像素，返回后全部 draw routes 精确恢复为 GL_NONE，原 read/draw FBO、renderbuffer 及其它触及状态恢复。独立改为只有 color1、缺 depth、texture 附件、depth-stencil、多样本或一附件尺寸不符，即使 completeness 仍成功也在 clear/draw 前报 attachment_layout，所有像素/摘要/probe/pending 标记不变；不完整 FBO 报 framebuffer_incomplete。有效附件但 draw 失败时仍恢复 draw routes 和绑定，允许 host 区像素已有变化。 |

R5-10 的非单位 Q＋旋转对照：作者 21:9、host `721×1280`、90°，content=`(86,0,548,1280)`，`Qxx=960/959,Qyy=1`；基础 P 的作者 NDC `(1/4,-1/2)` 映到 content 像素 `(958/7,800)`、host 像素 `(1560/7,800)`。以完整最终矩阵反投影还原同一世界点/射线，误差 ≤1e−6；先逆 Q/R 再逆最终矩阵的对照必须偏离。T-05 用纯值 helper 算，T-06 从实际快照取同一 Pout 并检查 renderer 消费及 retained 重排，不能只验各自重建的公式。

## P4：Player 生命周期

基线：`player_assembly.cpp` 的 R 是 Reload，`player_command.hpp` 无 Retry；`player_app.cpp` 无源回退默认工程；`player_control_transaction.cpp` 的 Rebuild 会重读及处理音频。现有 `ExecutionHost::pause` 不合并并发暂停。以下闭合 D-07、D-13、D-15、D-16、D-22 与 D-23 的接线，R=Retry、D-09/D-12/D-14 的控制约束与 D-15 的结果顺序保留。

### 命令与启动

T-14 在 `player_support` 定义 `PlayerCommandKind::Retry=8`，由生产 owner 消费并在无窗事务中验收。T-15 接外壳：`WindowKey` 追加 `F5=8`，SDL 映射 `SDL_SCANCODE_F5`；`PlayerInputAction` 追加 `Retry=7` 并转译到上述生产命令，保留其余枚举值。按 SDL 物理 scancode 识别控制键（R/F5/B/S/Escape/Space/Left/Right），非重复、无修饰键 key-down：R → Retry，F5 → Reload，B → 纯画面 Rebuild，S → Stop，Esc/关窗 → Quit。Space 在 Playing 提交 Pause，其他可 Play 状态提交 Play（暂停时明确 Resume）；左右仍走现有 ±5 秒 Seek。控制键的按下与配对释放均不进实时判定；准备若发现宿主配置将上述保留键用作玩法扫描码，报 `player.judge.binding_conflict`（`profile,scancode,domain,channel`），未开局。

具体落点为 `engine/platform/src/sdl_window.cpp::namedKey`：参数从 `SDL_Keycode` 改成 `SDL_Scancode`，全部分支改为 `SDL_SCANCODE_SPACE/LEFT/RIGHT/R/S/B/ESCAPE/F5`，`pollEvents` 传 `event.key.scancode`，不再用 `event.key.key` 或仅追加 `SDLK_F5`。有 Shift/Ctrl/Alt/GUI 的按下不生成 named command（`WindowKey::Unknown`），锁定键 Caps/Num 状态不算上述修饰；仍原样携带 scanCode/pressed/timestamp 给玩法转换，保留重复按下过滤和事件序。`WindowKeyEvent` 不需新增字段。`player_assembly.cpp::SdlPlayerSurface::pollInput` 在送 gameplay keys 前先按物理 scanCode 排除全部保留控制键的按下和释放，即使该次 named key 为 Unknown 也不漏入玩法；其它键不因修饰键吞释放。普通/执行两入口共用此转换，SDK 不见 SDL 类型。

```text
cuexis_player [--chart PATH | --project PATH [--entry PATH] | --cxc PATH [--entry PATH]]
              [--judge [--keymap PATH]] [--aspect auto|W:H] [--rotate 0|90|180|270]
```

`--judge` 为精确拼写、无值、至多一次的布尔 flag，位置任意；`--judge=true`、`--judge false` 或重复按 `player.arguments.unknown` 拒绝。显式判定须恰有一个来源，缺来源为 `player.arguments.source_required`。缺值、多来源等语法错误无窗口，stderr 输出诊断并非零退出；`--help` 正常输出帮助并以 0 退出。语法正确且拼写受支持的请求若发行未编入正式判定入口，保留 `player.candidate.disabled`，走下述可关闭错误窗口路径，不当未知参数。正式路径无 `--judge` 不创建判定。`--chart` 接唯一源谱，`--project` 接目录或 `cuexis.project.json`，`--cxc` 接包路径。外部行为依据 [D-16 已接受共识](../architecture-design.md#外部行为2026-11-21-owner-已接受拟实施)，仍待实施授权。

`--rotate` 为启动时单值，精确接受 `0`/`90`/`180`/`270`，省略为 0；无来源时仍允许，窗口占位不转。`--aspect` 需要来源，省略选择作者默认；`auto` 选择当前 drawable 像素面积最大的已声明比例；`W:H` 是未旋转的作者比，两段仅十进制 ASCII 数字、各 1–10 字符、数值在 `1..2147483647`，允许前导零并约分。Exact 不匹配在内容验证阶段报 `playback.presentation.layout.aspect_unavailable`，经原 stage/cause 进入错误窗口；Reload 同一请求不重新选用户偏好。`--aspect`/`--rotate`/`--keymap` 各至多一处，均使用独立 argv 单值，缺值、`=` 形态、重复、无来源使用画幅或未请求 `--judge` 使用 `--keymap` 均为 `player.arguments.unknown`。现有仅观察参数仍可组合。R5 接布局但不注册这些选项；T-15 接入窗口策略和 CLI。

`--entry PATH` 是工程/CXC 启动时的单值；语法上可省略，语义选择依下表。按现有 portable relative entry path 大小写精确匹配，不接受绝对路径、越出根、空值、重复选项、`--chart` 搭配或 `--entry=...`（`player.arguments.unknown`）。先用载体所属 Reader 校验，再查该载体实际声明的条目及绑定；不把任意存在的文件当 entry，也不在不同目录/编码之间 fallback。未声明的显式 path 报 `player.source.entry_not_found`；无显式 path 且该行要求显式时报 `player.source.entry_required`。重复路径、悬挂 bootstrap、旧格式缺必需 `entry.chart` 均保留原 Reader 错误，不投影成可自动选择的目录。

| 载体及目录生产者 | 省略 `--entry` / 显式选择 | 实际工厂及 prepare 路径 |
| --- | --- | --- |
| 旧松散工程 ProjectConfig v1；Project Reader 生产已校验 `entry.chart` bootstrap | 省略只用 bootstrap；显式同 bootstrap 可用。不枚举磁盘 JSON 形成多谱目录。 | `ProjectLoader::load` → `PlaybackSource::fromFilesystemProject`。bootstrap 的 root/path 先按原 asset root 规则合成 project-relative path。 |
| 旧普通 CXC；现有 CXC Reader 生产 bootstrap | 省略只用 bootstrap；显式同 bootstrap 可用。 | `CxcPackageLoader::loadFile` → `PlaybackSource::fromCxcFile`，沿原包校验。 |
| 旧 `cuexis.chart-entry.v1` 声明（工程或 CXC） | 非 bootstrap 条目必须显式。省略不因只有一项就选 candidate。 | 工程用 `fromFilesystemProjectEntry`，CXC 用 `fromCxcFileEntry`，只接其当前已支持的 Chart-entry 元数据/编码。 |
| 旧 `cuexis.gameplay-entry.v1` 声明（工程或 CXC） | 显式、既有具名开发请求；正式 `--judge` 不借旧配置补 v3 规则。 | 原 `fromFilesystemGameplayEntry` / `fromCxcFileGameplayEntry`（或 RR-4 同名实验 facade），保留原 `GameplayConfiguration` 参数及接受集。 |
| 后继松散工程：ProjectConfig v1 的 `extensions["cuexis.gameplay-entry.v2"]`，形状与 producer 见下段 | 每次首次选择必须显式 path，即使仅一项或等于 bootstrap；否则 `entry_required`。 | `ProjectLoader::load` + T-13 v2 directory Reader → 验证实际 bytes/资产/引用 → `source-json` 调 `compilePreparedArtifact`，`capsule.5` / `graph.2` 调 `decodeCompiledCapsule` / `decodeCompiledGraph`，移入同一 Playback candidate。 |
| 后继 CXC：B 的 v2 directory 与 requiredExtension，由候选 CXC Reader 生产已校验目录 | 始终显式 path；单项、bootstrap 指成品、普通模式也一样。 | T-13 候选 CXC loader → v2 元数据及实际 bytes 校验 → 成品调用上述 decode；参考 Player 选 `source-json` 报 `player.source.encoding_unsupported`。不走 `fromCxcFileEntry` 或旧 Gameplay v1 工厂猜测解码。 |
| 裸 `--chart` v5/gameplay v3 源 | 唯一输入，无 `--entry` | 文件有界读取、T-11 源 Reader → T-12/T-13 的 `compilePreparedArtifact` → 同一 Playback candidate；旧裸谱仍用 `fromChartText`。 |

后继松散工程的目录来自作者显式写入 ProjectConfig extension 的 v2 `entries`，不从目录扫描、文件顺序、旧 `entry.chart` 或旁置报告推导。源条目各自填 B 定义的 `source-json` 行及实际 JSON bytes SHA-256；bootstrap 仍必填且存在，按候选容器同规则验证其类型。工程没有 manifest，实际 root/path 安全、文件 SHA、CXT 闭包及 Asset Index 由 T-13 的 filesystem adapter 使用同一 v2 directory Reader 和有界文件读取验证。离线 `cuexis_gameplay_assemble beat-us --publish filesystem` 生产 `cuexis.gameplay-entry.v2.json` 目录对象与成品，见 [B 的工具合同](prepared-artifact.md#schema工具与接线)；作者/测试夹具把该对象显式置入上述 extension，Player 不自动发现/合并 sidecar。没有 v2 extension 的松散 v3 多谱工程不能宣称有可选目录；作者补目录后用 `--entry`，或直接用 `--chart` 选单个源。

`player_control.cpp::openConfiguredPlaybackSource` 按上述已经验证的 carrier/version 分派；v2 使用 T-13 的候选 source adapter，把 `compilePreparedArtifact` / `decodeCompiled*` 的拥有型结果移入 `PlaybackSource` 私有状态并由现有 prepare/commit 接收。这个 adapter 的唯一路径是 build-only `playback::detail::CompiledSourceAccess::fromArtifact(std::string sourceId, std::string entryPath, gameplay_packed::CompiledCapsule artifact, std::vector<PlaybackAssetDescriptor> assets, std::shared_ptr<content::IContentProvider> provider, GameplayPrepareIntent intent) -> core::Result<PlaybackSource>`，位于 `engine/playback/internal/cuexis/playback/compiled_source_access.hpp`，实现放既有 `playback_source_gameplay.cpp`，`PlaybackSource` 无条件 friend 该私有 access，公开类布局/旧工厂签名不变。入口不再编译/解码，校验 owning artifact 与 provider/assets 闭包一致后移动；失败不创建候选。源/成品策略、限额和身份参数均来自 T-13 定义的 context，Player 不构造旧分数配置。T-13 实施该 adapter，T-15 只接表内分派；与既有仓内 access 同样使用实验 Playback 的导出标注及精确 shared 白名单，头不安装、默认库不提供，同版消费者重编，不新增框架或 SDK 承诺。

只有发布矩阵已验证的 entry/profile 才准入；显式内容缺判定绑定时 `--judge` 报 `player.judge.profile_unsupported`，不转选其它难度。Retry 和 Reload 固定第一次解析出的 exact path；Reload 把该已锁 path 作为显式选择重读同一载体，不随 bootstrap 后来改动重新选，path 被删除即失败。旧 schema 的缺 bootstrap 阴性照常执行；不再用假设允许缺省 default 的选择器替代实际 Reader 测试。

日志、FPS、性能统计等仅观察功能可与普通播放和正式 `--judge` 组合，不再与全部开发参数一概互斥。手工时间、输入注入、玩法覆盖只在显式测试入口接受。旧 `--candidate-entry`、`--mode`、手填配置/预算/步长/按键、`--gameplay-packed`、`--execution-tail-input-deadline` 和 smoke 等按各自用途保留原解析及不相容测试请求的拒绝，不自动放行所有旧开关。`--execution-test-only` 的诊断参数适配到同一 `PlayerController`/会话 owner；旧 `runExecutionPlayer` 只过渡转发，D-11 行为证据齐备后撤掉重复入口实现。开发 profile 可以驱动与正式 `--judge` 相同的回放、控制、判定和结果路径，允许额外故障注入/记录，不绕过运行准入。正式时钟由已准备的主音乐声明选择：有主音乐用音频、没有用 Chart 单调时钟；已声明却无法打开/解码或接入就失败。音频掉线不换成无音轨路径。

**正式判定规则来源。** 源 Reader 和成品 Reader 各自验证并准备同一作者模式，以 [runtime/timing 的作者与计分合同](runtime-timing.md#归一化计分与-prepared-记录)、[当次推进](runtime-timing.md#prepared-与当次推进)与 [prepared-artifact](prepared-artifact.md) 为准。Player 从选中条目读取准备结果，沿 SDK 事件及结算投影显示；不补默认等级、Exact、分数、玩法规则、窗口或输入误差。旧 inline 引用、旧 CXC 绑定及旧配置 codec 留在原具名开发入口；正式 `--judge` 遇缺少这套作者能力的合法内容拒绝，保留合法旧谱普通播放路径。

CXC v1 及当前具名组合的既有 Reader 保留。新后继 CXC 源包的合法性由 [prepared-artifact](prepared-artifact.md) 规定；本参考 Player 对纯源的后继 CXC 条目没有运行准入行（普通或 `--judge` 均拒绝），报 `player.source.encoding_unsupported`、带条目/请求编码及发行 ID；不称包损坏、不编译、不回退至另一条目或编码。未来选择支持须依 D-17 另登记具名兼容行。错误顺序为原包校验、条目选择、编码准入、内容/身份校验、模式/设备准入，任一步失败保留具体原因。

Player 根据已校验内容描述精确匹配随发行编入的支持表：`(源种类及编码版本, 作者 profile, 玩法版本及 executionProfile, 系统音频后端能力, audio/chart 时钟)`。源种类及版本从 Chart 根版本或已校验成品 header/metadata 取；作者 profile 从 `gameplay.authorProfile` 或匹配源 closure 的 `compilerProfile` 取，执行 profile 从同一内容的 `common.executionProfile` 或等价 graph 字段取；不能从 `compilerProfile`、`rulesetRef` 或设备名推断执行 profile。先核验内容/身份；不合法保留原 Reader/绑定错误，合法但缺支持行或该输入语义不支持报 `player.judge.profile_unsupported`（携上述键和条目），无普通 fallback。表行只提供具名 host profile、证据标识、键盘输入语义及最大逻辑通道数（不得大于键位文件的可用物理键数）、设备/校准约束、队列/结果和 decode 容量预算及验证过的时钟/晚到适用域；它拒绝不适用的内容值，不重写作者规则、latePolicy、计分、窗口或等级。不从旧 `GameplayConfigurationDecodeBudget` 取得生产限额。本设计定义了行匹配，正式表的准入由发行实测门禁控制，门禁通过前表为空。测试目标注入具名 fixture 行 `rr.keyboard.fixture.v1`（7 条逻辑通道容量），走与正式表同一个匹配和校验函数，不能被 CLI 揭开或链接进正式包。发行采用须同一变更登记真实支持键、确证限额和源/成品/时钟/设备输入证据，正式构建用实际表跑正反两面，并证明测试行 ID 及替代注入路径未进入发行目标。

**启动键位配置。** `--keymap PATH` 仅用于正式 `--judge`，指定普通宿主文件（路径相对工作目录；不从 Chart/CXC 内读取、不搜索默认文件）。上限 65536 字节；用仓内有界 JSON Reader（UTF-8、拒绝重复键、未知字段及错误类型），不用 SDL 或 DOM 穿过 SDK。候选 schema 文件 `schemas/candidate/cuexis.player-keymap.v1.schema.json` 的根形状如下；整数版本须用精确 JSON 整数 token：

```json
{
  "type": "object", "additionalProperties": false,
  "required": ["format", "version", "bindings"],
  "properties": {
    "format": {"const": "cuexis.player-keymap"},
    "version": {"const": 1},
    "bindings": {
"type": "array", "minItems": 1, "maxItems": 68,
      "items": {
        "type": "object", "additionalProperties": false,
        "required": ["key", "domain", "channel"],
        "properties": {
"key": {"type": "string", "pattern": "^(Key[A-Z]|Digit[0-9]|Backquote|Minus|Equal|BracketLeft|BracketRight|Backslash|Semicolon|Quote|Comma|Period|Slash|Space|Enter|Tab|Backspace|Escape|Left|Right|Up|Down|F[1-9]|F1[0-2])$"},
          "domain": {"type": "string", "pattern": "^[A-Za-z_][A-Za-z0-9_.-]{0,63}$"},
          "channel": {"type": "string", "pattern": "^[A-Za-z_][A-Za-z0-9_.-]{0,63}$"}
        }
      }
    }
  }
}
```

**物理键 token 表（v1）。** 下表是完整、大小写敏感的有限集合；Player 将 token 精确映射到现有 SDL scancode，不用键盘布局文字或 SDL 数值作文件合同，不新增输入框架。

| token（同组顺序用于八键及以上默认映射） | SDL scancode（同序，省略前缀 `SDL_SCANCODE_`） |
| --- | --- |
| `KeyA`…`KeyZ` | `A`…`Z` |
| `Digit0`…`Digit9` | `0`…`9`（主键区） |
| `Backquote,Minus,Equal,BracketLeft,BracketRight,Backslash,Semicolon,Quote,Comma,Period,Slash` | `GRAVE,MINUS,EQUALS,LEFTBRACKET,RIGHTBRACKET,BACKSLASH,SEMICOLON,APOSTROPHE,COMMA,PERIOD,SLASH` |
| `Space,Enter,Tab,Backspace,Escape` | `SPACE,RETURN,TAB,BACKSPACE,ESCAPE` |
| `Left,Right,Up,Down` | `LEFT,RIGHT,UP,DOWN` |
| `F1`…`F12` | `F1`…`F12` |

上表共 68 个 token，schema 数组容量与表大小一并检查。沿用命令键保留策略：`KeyR`、`KeyS`、`KeyB`、`Space`、`Left`、`Right`、`Escape`、`F5` 可解析但禁绑，按冲突诊断。有效容量是发行行的已测通道数与表中未保留 token 数的较小者。从准备后的逻辑输入约定提取舱内所有有效的键盘 `(domain, channelToken)` 对，以两个 token 的 ASCII 字节序排序、去重。同一对需要 Press 和 Release 时仍只列一次。host profile 只承接已支持的 keyboard 离散 Press/Release，不校正、不映射轴、鼠标、触摸、旋钮输入；任一必需输入语义不支持、通道数超过该发行行确认容量或逻辑 token 超出本配置域，报 `player.judge.profile_unsupported`。SDK 接收原 domain、channel、作者要求的 keyboard sourceClass 与标准 Press/Release；不接物理键字符串或 SDL 类型，也不按文件顺序或整数 lane 重新编号。

文件必须覆盖这组逻辑对恰好一次，且每个物理键只能绑定一个逻辑对；首个重复或保留键报 `player.judge.binding_conflict`，无一对多、多对一、和弦或隐式补齐。验证顺序：读文件/JSON 形状→检查已准备内容的输入能力/容量→按文件序检查保留键及重复物理键/逻辑对→检查未知对（首条绑定位置）→检查缺失对（按序首个）。读失败/超字节报 `player.judge.keymap_unreadable`；形状/版本/未知或遗漏逻辑对报 `player.judge.keymap_invalid`，带文件路径与首处 JSON 路径或缺失对，校验失败保持关窗可选的错误窗口、零开局。只有完整通过上述全部步骤（含已准备内容能力/容量、未知逻辑对及缺失逻辑对检查）的配置值才锁定到进程退出；此后 Retry 不读，Reload 用锁定值重验所选条目的通道集合，运行中无改键，修改已接受的文件须重启。首次加载或 F5 尚未通过完整校验时不保存有效配置，下一次 F5 必须重读同一路径；JSON 合法但逻辑对未知/遗漏同样属于未锁定。锁定后若后续资源或设备接入失败，仍保留已验证值。无 `--keymap` 时依上述排序分配：1–4 通道取 D/F/J/K 前缀；5–6 取 A/D/F/J/K/L 前缀；7 用 A/D/F/G/J/K/L；8 及以上（若发行行支持）按 token 表从上至下、组内从左至右，跳过保留键。正式路径无临时键位覆盖。在同一发行行内用同一算法处理 4/6/7 通道，行不按通道数或测试模式名硬分叉。

每轮按 SDL 事件顺序处理，忽略重复键按下和未映射事件，只在物理状态真实切换时传一次对应 SDK transition；游玩键不受修饰键并按影响，避免吞已接受按下的对应释放。控制键的 release 也不传给 SDK。新局开启 capture 前将当前仍按住的游玩键置为阻挡态；旧按下对应的释放只解除阻挡，须有新按下才发送 Press。暂停保留同局输入事实，Resume 用既有 capture reconciliation 校验冻结/恢复交界；无法证明持续按键与边界时保持暂停并依 D-12 处理，不伪造转移。时钟、接收序及 epoch 由既有 capture/Playback 路径标记，见[收尾规则](#所有权暂停与收尾)。

观察和呈现步进用真实 owner 采样/处理时序上的可靠 frontier，沿 P1/P3 的容量与时钟门禁；手工 `hStep/tStep` 保留于显式开发输入。普通设备偏好、音量或现有受校验的设备校正配置可使用，仍需符合宿主行的边界。

启动来源、选定条目标识、用途、启动偏好锁定到进程结束。显式 path 解析 CLI 后即绑定；自动选择在第一次成功解析到条目标识时绑定，包括随后设备/准备失败的情形；若首次打开失败尚无条目标识，F5 再按启动参数解析一次。`Load` 仅入场一次；外壳没有选谱/热换用途。无来源正式普通启动：Empty、可关闭窗口「无资源」，测试专用缺省样例另留。正式来源形状有效时先有可关闭外壳，Load 成功为 Loaded，再沿当前启动行为发一次 Play；有来源但文件缺失、坏谱、缺资源、模式校验、准备或接入失败进入 Failed，窗口显示简短实际原因，详细诊断带来源和底层原因写 stderr，窗口保留可 F5 或 Quit，不显示「无资源」或悄悄降级。Reload 重读**同一配置来源和条目**、重新按主音乐声明选择新内容的时钟并核验同一用途，成功仍零点暂停、静音等待 Play；Retry 保持 A 的已准备配置，不走 I/O。CLI 语法正确但不受支持的内容同样保留错误窗口并停播；沿用 `player.session.prepare_failed` 的阶段/cause 或已有具体能力诊断，不把能力不足报为未知参数或包损坏，不降级。

关闭时仍有未恢复加载或运行故障则非零退出，即使曾成功加载过；发生过错误但已恢复后正常关闭为 0。无资源窗口正常关闭，以及 Stop、自然结束后的正常关闭均为 0；Stop 和自然结束不强制关闭。普通被拒控制命令不自动计作未恢复运行故障。语法错误和致命外壳错误非零退出；沿用现有错误出口，不新建退出码框架。

### 完整准入矩阵

所有格为**成功后**状态或稳定拒绝。`—` 表示保持状态；`N=player.command.not_loaded`、`F=player.command.failed`、`D=player.command.duplicate`、`P=player.command.not_playing`、`L=player.command.load_initial_only`。所有状态支持 Quit（先停推进及输出、关闭，无额外正常结果）；未知枚举 `player.command.unknown`。`Load` 只认内部初装带来源标记，失败后只走 Reload。Empty 若异常持有 A，除 Quit 外一律 `player.command.state_inconsistent`，保 A 停播并置 Failed。

| 当前 | Load | Play | Pause | Stop | Seek（普通） | Retry | Reload | Rebuild |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Empty | 首装→Loaded；无来源拒绝 | N | N | N | N | N | N | N |
| Loaded | L | Playing | P | Stopped | — | Retry@0 | Paused@0 | — |
| Playing | L | D | Paused | Stopped | — | Retry@0 | Paused@0 | — 或按画面中断暂停 |
| Paused | L | 解除玩家原因，按条件→Playing | 按来源幂等，维持 Paused | Stopped | — | Retry@0 | Paused@0 | — 或按 D-14 恢复 |
| Stopped | L | 新局→Playing | P | D | — | Retry@0 | Paused@0 | — |
| Failed | L | F | F | F | F | 有 A→Retry@0；无 A→N | 有来源→Paused@0 | 有 A 只修画面；无 A→N |

`Retry@0` 默认 `Playing@0`；提交前已受理独立新 Pause 时，Pause 优先，直接静音提交 `Paused@0`，中间无 Play 或输出。Seek 使用现有有限、非负与音频可达性校验，实时游玩拒绝 `player.command.seek_gameplay_unsupported`。普通 Stop 重置待播位置至 0；Stopped 后普通 Seek 只改下一次 Play 起点，Retry 总从零。有 A 的 Failed Retry 需先恢复必要设备再开始；Failed Rebuild 只清画面原因，局已 Faulted 或仍有非画面故障则保持 Failed，不能借其续局。无配置的 Reload 一律 `player.command.source_unavailable`（先于表中 Reload 状态判断）。单一事务期间仅 Quit、独立持久 Pause 登记及当前事务所需回执可进入；重叠 Retry/Reload/Rebuild/Seek/Load/Play/Stop 拒绝 `player.command.transaction_in_progress`，不排队重做。收尾正在运行时 Stop 与 EOF 共用该次收尾，重复 Stop 不再结算。

**准入纯值与提交事实（T-14）。** 基线 `engine/player_support/include/cuexis/player_support/player_command.hpp` 的 Context 只有 `state/transactionInProgress`，Decision 只有 `state/duplicate`；不足以直接实施上表。该库为仓内静态支持库，T-14 在同一非安装头追加以下值，不改已装 Playback 类型。Context 保留原成员，追加 `bool hasActiveContent, hasConfiguredSource, initialLoad, realtimeGameplay`（均缺省 false）及 `PlayerPauseAdmission pauseAdmission{None}`；该枚举为 `None, CurrentRun, PendingNewRun, AlreadyHeld`。唯一 owner 在受理命令时，从当前 A、锁定来源、首装是否尚未尝试、用途和原暂停原因/事务记录构造值；Pause 的 context/operation 和非零来源先过原门禁，再区分本局新来源、新局事务待定来源或已登记来源。`initialLoad` 在第一次 Load 受理时耗尽，失败后也不恢复；PendingNewRun 仅用于已受理的 Retry/Reload 事务；其中重复的 pending 来源仍归 PendingNewRun，共享该事务终态。AlreadyHeld 只表示本局已完成冻结确认的既有原因，不能提前确认尚未提交的新局。不能据调用方自填值绕过 owner 校验。

Decision 保留原成员，追加 `PlayerCommandEffect effect{Execute}`（`Execute, RegisterPause, AlreadySatisfied`）和 `PlayerSuccessStatePolicy successPolicy{Fixed}`（`Fixed, RetryAfterDevices, ReloadPaused, ResumeAfterReasons, RebuildAfterFacts`）。`evaluatePlayerCommand` 仍为纯函数，返回准入/成功策略或已有稳定拒绝，不读设备、不登记暂停、不发布状态。先拒未知 kind 和 Empty 持 A 的不一致，再检查 Reload 有来源，之后处理事务互斥：合格 Pause 返回 RegisterPause 或 AlreadySatisfied，不能先被 busy 分支拒绝；其余重叠命令拒绝。非事务按上表查 A/来源/首装/用途。Paused 的同源 Pause 成功返回 AlreadySatisfied、`duplicate=true`，不重复冻结；Playing 的 Play、Stopped 的 Stop 仍是表中 duplicate 错误，正在收尾的重复 Stop 由 owner 合流。Quit 沿既有关闭请求，不塞入这八命令枚举。

`decision.state` 仅在 Fixed 且实际工作全部成功后可发布；其余策略交同一个 owner 在提交点读取最新事实：RetryAfterDevices 在必要设备确认后按 pending Pause 集合决定 Playing@0 或静音 Paused@0；ReloadPaused 始终静音 Paused@0，并绑定新 pending 来源；ResumeAfterReasons 只清请求者原因，剩余原因或未恢复设备继续暂停，不能证明安全恢复则按 D-12 故障；RebuildAfterFacts 只清画面原因，仍有 run fault/非画面故障保持 Failed，自动恢复限 D-14 的普通播放条件，实时仍等显式 Resume。Load 返回 Fixed/Loaded；Play 从 Paused 用 ResumeAfterReasons，其余合法 Play 用 Fixed/Playing；Stop 用 Fixed/Stopped，Seek 用 Fixed/当前态。Pause 的 RegisterPause 先登记/合流，实际冻结回执和来源登记完成后才 Paused；PendingNewRun 等待同一 Retry/Reload 成败，不立即改应用状态。设备失败沿原阶段/cause 返回失败而不发布预期成功态。`player_control_transaction.cpp::apply` 必须撤销末尾无条件 `state_=decision->state`；它只送原 `OwnerChannel` 请求和读取 terminal，唯一 owner 消费 effect/策略，在原事务记录保存待定 Pause 与设备事实并按原 ACK 回收。普通、headless、窗口调用共用该消费点，不新增控制器、管理器、消息总线或第二份状态真相。

### 所有权、暂停与收尾

唯一控制者用现有 ExecutionHost 控制路径去除测试外壳后接入 Player；窗口/capture/GL 沿主线程 owner，音频沿 audio worker，Playback 与可选 Judgement 同一个 session worker 串行执行。I/O 与解码留在现有准备路径，准备工作可阻塞其 owner，但不得阻塞音频 callback 或改旧局。`PlayerController` 的纯转移和请求映射保留，取消其与 ExecutionHost 重复的状态/事务决策。D-11 的 deadline、receipt、时钟失效/过期等行为测试走替代路径后才撤旧控制权；同一局不运行两套控制。先用原 `OwnerChannel`、`SessionCommand`/`SessionReply` 和 `HostContext`/`StepKey`，不新建消息总线或安装 SDK 控制 API。

内部提交沿 `PlayerController::apply(PlayerCommand) -> core::Result<void>`；该同步适配器送请求给唯一控制者并读终态，不自行裁决。请求值为 `PlayerCommand`、`HostContext`、`uint64_t requestSequence` 及 `uint64_t pauseSource`（仅 Pause/Resume 使用，0 无来源，1 窗口玩家，设备/画面故障仍走参与方事实）；headless 测试和其他调用方可在线程安全的既有 `OwnerChannel` 递交相同请求，使用原非阻塞 poll 接口，避免同步窗口等待影响其他来源合流。控制者按入队顺序串行受理，用 `HostContext/StepKey` 核验会话、局及 operation。面向 `PlayerController` 的命令终态为每请求一个拥有型 `Result<void>`；使用既有 channel terminal/ack 回收命令槽，满则不改状态。长事务前先保留 reply 槽；旧/重复回执只核验原请求或丢弃，不成为新意图。申请者线程不等待音频 callback，不在其上调用同步入口。

**实时结算接线。** v3 严格采用 [runtime/timing 的实时调度单次回复](runtime-timing.md#实时调度的封定与单次回复)定义的 `ScheduledSettlement`、`SessionSettlementCommand`、`SessionSettlementReply` 和 `SessionSettlementStatus`，不复制另一套事件或成绩结构。启动的 `SessionRealtimeStart` 经原 terminal 交付唯一 P 槽 `GameplaySettlementBuffer`；控制者持有它并在每个可能 seal 的现有 session job 前移动到封套，完成后取回。`postSettlement` 拒绝时 buffer 仍归调用方，不能消费输入或尝试无槽推进。Waiting 返回空 outcome 和可复用的 buffer，原 scheduler waiting record 不持槽引用。

| 实际调用点 | T-14 必接的输出及消费顺序 |
| --- | --- |
| `execution_host_frame.cpp::ExecutionHost::advance` → `SessionCoherentAdvance` → `requestGameplayAdvance` → scheduler `request/tryStart` | 请求当场执行时回复带原推进身份和 outcome。Waiting 仅保存现有 waiting ID。`SessionQueryAdvance` 及同 ID 重投只读旧 decision，不能从 query 补事件或成绩。保留已装 `GameplayAdvanceDecision` ABI。 |
| `ExecutionHost::flush` → `SessionCaptureDelivery` → `deliverGameplayCapture` → scheduler `deliver/tryStart` | 后续真实 prefix/完整批片使 Waiting 可执行时，在这个 capture job 的实际 seal 现场写槽，回复 origin 指原 advance context/requestId/commandOrdinal/decisionOrdinal。`flush` 必须消费此输出后才 ACK capture delivery；不能把 capture 回复仍按 `Result<void>` 忽略，也不能等 query 再取结果。 |
| `SessionPause` → `HostCoordinationAccess::applyPause` → `pauseExecutionStep/pauseRealtimeGameplay` → scheduler `pause` | 沿同一 buffer 透传实际 drain 的 outcome，origin 为原 `StepKey`。Pause 合流先执行一次 drain，所有命令终态可共享决定，事件只消费一次。control 提交或 tail pause 失败仍先交已封事件，再报告故障；不产生正常成绩。 |
| `SessionResume` → `applyResume` → `resumeExecutionStep/resumeRealtimeGameplay` → scheduler `resume/controlRpa` | 暂停期间真实 release 经 reconciliation 在 cut 封定 Hold 中断时也走同一槽、StepKey 和回复；不丢 Resume seal 事件，不重发同 cut 旧 fact。成功普通 Resume 无新 fact 时零写入。 |
| Stop 原会话 owner 命令 → `detail::advanceStopCut` | 原 StepKey 绑定截止及接收前缀；正常和 faulted 前缀都沿同一回复投影，成功才采用其 score。已经正常结清的局直接用已保存终态，不再 seal。 |

`session_worker_dispatch.cpp` 为上述 v3 jobs 使用单次拥有型回复；`SessionOwner::coherent` 只保留旧 request/decision 身份，不能另缓存、排队或复制 settlement。`SessionWorker::poll` 按 A 的 `OwnerChannel::pollProjected` 取出 payload，槽内留下 `SessionSettlementStatus`；重读只有状态。`execution_host.cpp::awaitSession` 识别这两种新 alternatives：首次 reply 先调用私有 `consumeSettlementReply`，按 origin 检查 active run、封定 segment 及已受理 advance/StepKey（Resume 按该 StepKey 的旧 context 和目标 nextContext 核验，不能只比切换后的 segment），再按 `[0,written)` 顺序按值交付事件、取完整 score、取回 buffer；随后保存 `decision` 的成功/故障，沿原 `acknowledge` 回收 session slot，再向命令调用方返回/传播该决定；capture job 的交付证明 ACK 仍由 `flush` 在消费成功后完成。`decision` 失败不能使代码先 throw 而遗漏已封前缀或跳过原槽回收。`SessionSettlementStatus` 只确认原请求已取，零事件/成绩；完成的 advance 仍沿 `SessionRetireAdvance` 回收旧决定，retire 失败按原错误处理，不为补结果重投。控制者未取回 buffer 时不启动下一个可能 seal 的 job。

事件消费复用 controller/host ports 的装配位置，增加仓内同步端口 `onSettlement(GameplayRunKey, GameplaySettlementEvent, const GameplaySettlementDictionary&) noexcept`：每条事件按值，字典仅在调用期借用；无观察方时控制者仍消费并回收，不阻塞推进。需要保存解释的 headless/Player 接收方在开局复制 owning 字典，callback 不分配、不重入会话。该端口不安装、不建事件总线/队列。错误身份回复只归还原 slot/buffer，不向当前局发送；旧 run 的 buffer 不可绑定到 Retry 后的新局。取值在 ACK 前完成，事件值和已保存的 score 不随 ACK、下一次槽复用或卸载失效；正常终局仍仅调用原 `onRunFinished` 一次。

**NEXT Resume 身份接线。** 使用 [runtime/timing 的同局 Resume](runtime-timing.md#playback-时钟与实时准入)；prepare 后把其已验证的 `ResumeEpochPolicy` 交给仓内 HostCoordinator/CaptureRouteOwner，外部命令不能选择策略。`execution_host_frame.cpp::ExecutionHost::resume` 在 `keepRun` 分支保持 `sessionScope/runEpoch`，只增加 `clockSegment` 和 `controlSequence`；原 `operationId/pauseEpoch/controlOrdinal/reconciliationOrdinal` 继续按各自实际事务推进，不新增 control epoch 字段。legacy 分支继续增加 runEpoch 并检查溢出，两支都检查 segment/控制序溢出。相同策略同时接到 `host_protocol_helpers.hpp::validNextContext`、`host_protocol.cpp` 的 preflight/accept、`capture_route.cpp::resumeRoute`、Playback 和 scheduler Resume 守卫；不能只改 host 让下游继续拒绝同 epoch。

原 `StepKey`、当前/目标完整 context、capture route receipt、cut、pauseEpoch、finalTraceCut/digest 和 reconciliation 必须全部对应同一次恢复，成功后才 `enableOrdinary(proof)`。`RunKey` 用于本局计分；segment/operation 等控制身份隔离旧回执，不能只按 RunKey 放行。同局切段只重置 delivery/local route 计数和已确认水位，不重置 canonical ingress、advanceId/commandOrdinal、Fold/Hold 或已交付标记；真实新段 prefix 才能重新授权 seal。旧操作重读只取旧 terminal，不能改变新 segment 或再次发事件；错/缺证明保持暂停或按原不可恢复故障处理。Retry/Reload 换局仍增加 runEpoch；legacy Resume/Seek/replay 保留原规则和 characterization。

同局 Pause 合流：第一请求启动一次冻结，在会话串行边界锁定位置和可处理输入前缀；并发新来源在操作完成前合并，分别登记来源，各请求收同一成功或同一首故障。所有必要参与方确认后才成功。已暂停且已确认时同一来源幂等，不重复冻结；新来源加独立持久原因。Resume 只解除自己的原因；其他原因未清除时保持暂停。恢复后旧操作回执不改变当前状态，新的 Pause request 仍可暂停。D-14 的纯 Rebuild 不触碰音频，不读来源或 `prepareReload`，复用 A 已存 owning input/设置走 `prepareOwned/accepts/activate`；只接受本局画面已真实呈现的确认。仅普通播放、之前 Playing、确因画面中断冻结、同局且原因全清时自动续播；实时游玩先显示冻结画面，再显式 Resume。未实际暂停时无 Pause/Resume 或重定位。

音频 owner 只报告 seat/clock/position 事实；controller 裁定可否继续。位置或判定边界不明时冻结推进与输出，不以推测时钟补 Miss。已确认同局、音频对齐冻结位置且已结算输入边界可核实，才允许手动 Resume；设备恢复只清自己原因。无法证明时保留可复用内容，报告故障，要求 Retry/退出。Retry/Reload 共享新局初始化；先停旧输出/推进；Retry 检查并复用 A、健康的 decoded clip 与 seat，只在必要时按原设备配置重新接入。Retry 不读取、解析、解码健康资源，不交付被放弃旧局的正常结束结果。Reload 使用同 Load 的 prepare 管线，A 继续持有；B 从独立解析、能力校验、资源准备、音频解码、判定初始化到设备接入确认，全部成功才切换，成功后零点暂停且静音，等待 Play。失败包装 `player.session.prepare_failed`，`stage` 依次为 `source`、`parse`、`validate`、`resources`、`audio_decode`、`judgement_init`、`device_attach`、`commit`，并保留 cause；提交前失败保留 A，停播置 Failed，仅清 B；提交后报告当前 B，不假装 A 可回滚。无 A 首次 Failed 的 F5 同流程，成功 Paused@0，失败仍 Failed，绝不回退样例。设备已被 B 尝试影响时 A 仅保内容/健康音频资源，不假定 seat 仍对齐。迟到回执按原 context/operation 丢弃。

Retry/Reload 受理时锁定事务和未来 epoch；旧局已有的玩家 Pause 原因随旧局退役。控制者对提交前已串行受理的、新的独立 Pause 请求（须匹配事务受理时的 active context，不是旧操作的回执）登记 pending 来源；提交把它们一次绑定到新 `RunKey`，扣住启动，直到解除全部独立原因。发布 `Paused@0`、参与方保持冻结/静音后，Retry/Reload 和合流的 Pause 分别走原命令槽返回成功。事务失败或取消时，待定请求走同一事务失败/取消 cause，不能报告冻结了不存在的新局；仍在阻止运行的设备事实沿 D-12 保留，不能被事务清掉。提交之后才受理的命令按新局状态和 context 处理。无 pending Pause 且必要设备已确认时，Retry 才按矩阵立即 Play；不先播再停。

**自然结束与 Stop。** 不加结束管理器、公开/API 命令、固定等待、作者字段。普通有音轨以该局有效 EOF 完成；有音轨游玩须既收到有效 EOF，又已实际推进/结算所有本局判定义务。若仍有尾段，Player 复用 `ExecutionHost::beginTail` → 既有 `SessionTailStart` → `PlaybackSession::beginRealtimeTail`；确认旧 segment 的 capture 前缀并换新 `clockSegment`，保持 `RunKey`。后继路径按 [runtime/timing 的 EOF 尾段](runtime-timing.md#playback-时钟与实时准入) 适配 `RealtimeClock::startTail/sampleTail`：普通 `sampleCompiledClock` 对设备 EOF/paused 仍返回冻结位置；Playback 从有效 EOF 的一次校正逻辑锚走独立 tail 分支，沿真实活动单调时间按 1:1 采样，设备 correction/Chart offset 不再应用，输入和显示偏移各在分支转换时应用一次。首样本不倒退，Pause 冻结，Resume 换 segment 且从冻结点接续。用已准备义务与获准的时钟/输入边界产生原 tail 参数，不从工具取得 test-only deadline。无可信 EOF、过期或倒退时走 D-12 故障冻结，零推测推进。控制者只合并同局事实：义务先清等有效 EOF，EOF 先到等尾段实际结清；齐备即结束，不因时钟数字或空队列直接完局，不人为增加画面等待。

**无音轨终点。** 在 prepare/commit 时从已绑定到该场景的表现程序计算内容尾，取 0 与有效有限记录末点的最大值：连续行为取 end，离散步进取最后事件时刻，有限动画实例（含已展开模板）取 `start + duration × durationScale × iterations` 的已编译末点（没有 scale/iterations 的记录按 1）。使用各版本既有时间域及编译映射，含实际 Stop 积分。有效记录来自已编译的实际绑定，至少有一个解析后可写且未被掩码排除的属性或步进。静态属性、空实例、终值保持、未绑定声明和编译期可证明始终零权重的记录不延长。包含有效连续轨或离散步进的无限绑定没有有限尾，须显式 Stop；不按逐帧可见性或实时权重猜测已结束。空/纯静态内容尾为 0，非法边界传播原 prepare/编译错误。Playing 后首次有效推进达到或跨过有限尾，普通无音轨即锁定结束事实；实时无音轨还须同局实际 seal 到所有必要判定义务并无待处理前缀。两事实可先后成立；不得由时间数值或空队列推断结算完成。到达尾的最后正常 Playback 帧沿既有呈现门禁；未取得有效呈现租约的末帧可放弃，不为等待画面延长运行。无音轨使用 Chart 单调时钟；终点仅用于会话结束，不限制已接受的普通 Seek。

**Stop 截止。** 控制者按串行次序只受理一次本局收尾，使用当时已核验的逻辑 cutoff C（已暂停则用已确认的冻结位置）。实时 Stop 走既有会话 owner 任务，由 Playback 调 [runtime/timing 的 `detail::advanceStopCut`](runtime-timing.md#prepared-与当次推进) 作为唯一 Stop 结算入口。owner 按下述接线先收齐完整 capture 前缀并完成 canonical 接收编号，再读取上界、锁定并关闭 kernel 接收，结算到含端 C；关闭后到达的输入不获编号，已接受但未调度、且原规则生效 tick ≤ C 的输入仍结算，未到期音符/Hold 不补 Miss。不能直接 `advance(C)` 或伪造 `C+closeThreshold`。截止/接收前缀不可证明或结算故障时停播并保留已封事实和诊断，不交付正常结果。自然结束沿普通推进完成，共用后续已有清理；重复 Stop/EOF 不再进结算。

Stop 复用现有 capture epoch/fence。T-14 的 Playing 路径先预检槽/时钟并 `acceptControl` 阻止普通 seal，再关闭本局捕获路由、收齐最后完整批片及 epoch end。`ExecutionScheduler::deliver` 沿 T-10 的 `detail::receiveCanonicalBatch` 完成接收，回填的真实序号随 admissions/归档保留；`canonicalOccurrence` 只提供 key。随后在同一串行 session owner 事务调用 `detail::acceptedIngressThrough(session, run)`，把真实上界与 C 交给 `advanceStopCut`；它核对上界全等后才关闭 kernel 接收并 seal，query 与锁定之间不处理其它输入命令。错误不能折算为空前缀，`nullopt` 与接收序号 0 区分；不从 `confirmedSequence()`、delivery 序号或输入个数推算，不增 host DTO。Paused 使用已确认冻结位置和闭合前缀，读取同一 kernel 游标，不重开或重复结束 capture epoch。自然结束、Retry、Reload、Quit 或故障也关闭旧局接收路由；Retry/Reload 提交时换 `RunKey`，新局首个 Play 才激活新输入 epoch。Pause/Resume 与 EOF tail 可换输入 epoch/clock segment，保持同一 `RunKey`。迟到的旧 route/capture/operation 消息只归还原槽，不能进入新 epoch 或重开旧 ingress。

**计分与事件投影。** Player 按 [runtime/timing 的作者、计分与 Exact 合同](runtime-timing.md#归一化计分与-prepared-记录)以及 [当次推进](runtime-timing.md#prepared-与当次推进)直通事件、成绩与错误。Stop 使用该局已准备的整谱基数。结算后表现、关闭或清理失败时，已封的事件前缀仍一次交付；结算为 `faulted` 时不交付正常成绩。

**结果交付。** [runtime/timing 的终态投影](runtime-timing.md#prepared-与当次推进)定义 v3 成绩的唯一来源：首次正常终止（自然结清或 Stop）的 `playback::GameplayAdvanceOutcome::score` 中完整的 `playback::GameplayNormalizedScoreResult`，Stop 使用同一 DTO 投影。会话 owner 从上述首次 `SessionSettlementReply.outcome` 的同次值按 `RunKey` 取值，即使分数为 0 也保留全部累加器、分母与连击字段；不调用 `queryGameplay`、`SessionGameplayResult` 或旧 Fold 补取 v3 结果，也不从事件重算。旧具名玩法沿用 `SessionGameplayResult` 的 `playback::GameplayResult` 拥有型回复。

自然结清先于音频 EOF 或无音轨表现尾时，控制者在同局终止状态内留存这一个已投影值，等外层完成事实；不创建跨局结果队列。后续 EOF/表现尾读取同值；若此时先受理用户 Stop，只收束尚在运行的音频/表现并移交该已完成成绩，不再次推进已关闭的判定，也不伪造一条 StopCut 归档。正常活跃局的 Stop 仍按上述 cutoff 结算并按实录保存。若合法的首次 v3 终态无分值载荷，报 `player.session.result_unavailable`（`reason=missing_score`，带当前 `RunKey`），不调用 port 或退回旧路径；迟到或错局回复按原身份门禁丢弃。

`CompletedRun` 是仓内拥有型值：`struct CompletedRun final { judgement::RunKey run; std::variant<playback::GameplayResult, playback::GameplayNormalizedScoreResult> result; };`。`RunKey` 来自开局 `HostContext` 的 `(sessionScope,runEpoch)`；Pause/Resume、EOF 换 segment、同局画面恢复保持 key，Retry/Reload 递增 epoch 并丢弃未完成旧局的待交付值。停输出后、清局前，只对实际开玩且同局正常结束的游玩局，在交付当次 SettlementEvent 后设置 `resultDelivered`，再同步调用既有 `onRunFinished(CompletedRun&&) noexcept` 一次。旧回复槽沿原 terminal/ack 管理；归一化成绩不另设 ACK、公开接口或独立队列。Player 外壳与 headless 测试各绑定同一端口，离开 Playback 后所收值仍有效。清理失败不影响已交付值；普通播放与未 Play 即 Stop 无成绩。

**Stop 回放与清理。** 清理前由同 owner 的诊断测试端口读取 runtime 合同定义的成功 Stop 内存归档；用末尾 StopCut 记录重放，核对 live 的输入前缀、结算 fact、frontier、Fold 和终态。回放只验证，不调用 `onRunFinished`、不向宿主重发 SettlementEvent、不创建第二局，也不作为正常成绩交付的前置条件。失败 Stop 不读取成功归档、不生成持久 codec 数据；终态不允许恢复/seek。已交付结果拥有独立值；归档与输入 slot 可随局清理，测试先取得归档副本。

清理顺序：封住接收与普通推进，检查并消费末次 run 绑定结果，设置一局一次的交付标记并移交成绩（如适用），然后撤旧 capture/advance/receipt 路由，归还其剩余队列与实例状态，只保留可 Retry 的 A/健康音频及最小的同局终止标志。已授权的末帧交付完成或被外层门禁放弃后归还资源，不让末帧阻止关闭。清理失败锁住旧路由，记录阶段/cause，允许同一 owner 重试未完成步骤；不再封边界、回放计分或重发成绩。必要清理未完成前 Retry/Reload 不得提交新局；Quit 尽力释放 owner 资源并保留失败退出。旧请求可完成原槽，但局和 operation 不匹配时不更改当前状态。

| 向量 | 精确断言 |
| --- | --- |
| P4-01 | 逐跑 6×8 格（Failed 无/有 A、设备不可用两支），断言状态和码；R、F5 各非重复按下只生一命令。无参 Empty 可关闭且退出 0，无默认工程读或判定；`--help` 帮助退出 0；缺值、多来源、重复/带值 `--judge` 无窗口、stderr 诊断、非零退出。语法正确且拼写受支持的正式判定请求，仅在构建未编入正式判定入口时保留错误窗口及 `player.candidate.disabled`；其他能力不足保持原有 profile 等具体诊断，不报未知参数。日志/FPS/性能统计等仅观察功能分别与普通播放、正式判定组合；手工时间、输入注入、玩法覆盖只在显式测试入口且不绕准入，原不相容测试请求仍拒绝。曾成功→后加载/运行失败未恢复→关闭非零；失败→Reload 恢复且零点暂停→关闭 0。Stop/自然结束不关窗，正常关闭 0；普通拒绝命令不自动使退出失败。注入已认证测试行，`--chart X --judge` 与具名工程/CXC `--entry PATH --judge` 无需额外测试参数；同内容有/无主音乐分别请求 audio/chart；缺匹配行或绑定报 `player.judge.profile_unsupported`，控制键冲突报 `player.judge.binding_conflict`，均零开局。测试行不进正式包。 |
| P4-02 | A 正播，在磁盘改 B，R 复用 A，读/解析/解码计数增量 0，runEpoch 加一且从 0 播；F5 每次重读一次，B 静音 Paused@0。按阶段注入失败、重叠 F5、设备破坏、迟到 A 回执，检查保留和停播。首次 Load 缺文件、坏谱、缺资源及合法不支持内容分别保留可关闭错误窗口，简短实际原因与 stderr 详细诊断一致，缺能力不报包损坏或降级；修复为有效 B 后 F5 从无 A→Paused@0，关闭 0；仍失败则关闭非零。A 成功后 Reload 失败也停播，未恢复关闭非零。B 键 A 重建时磁盘 B 的打开计数为零。 |
| P4-03 | 同局 Pause 来源 11、12 在音频 fence 回前到达，只冻结一次，两个 terminal 同结果；清 11 后仍停，清 12 才许恢复。单参与方注入失败，两回执同故障。恢复后重放旧 token 不暂停，下一请求可暂停。画面恢复不解除手动/音频原因；位置未知 500 ms 内零推进、零新增 Miss（该 500 ms 是观测窗口，不是判定界线，见附页 D-09 的等待态说明）。 |
| P4-04 | 对照 `CLK-03`：EOF raw=`523000us`、correction=`3000us`、offset=`20000us`；设备观察冻结 `500000`，独立 tail 活动 `100000us` 到 `600000`，停 `250000us` 仍为 `600000`；Resume 后再活动 `100000us` 到 `700000`，尚未封 `D=700000`（`close=15`），再活动 `15us` 实际结算才结束。run 不变，进入 tail 与 Resume 各换 segment。反序等 EOF；stale/倒退 EOF 零新增 Miss。无音轨有限表现尾@300、判定尾实际 seal@500：普通@300、实时@500；空/静态@首个 0 帧，活动无限循环只 Stop。 |
| P4-05 | 对照 `CLK-02` 分层构造：`C=100`、`closeThreshold=15`。105/90 的未来 pending 向量仅由 T-10 纯内核入口验证；T-14 真实 capture 先送六条无匹配义务的合法 release@10/20/30/40/50/60，再送 release@89、匹配 press@90，各批 observed=原 tick+1，最后为 91，均不越过 C。沿生产 receive 实际生成 ingress 0–7，插入合法 prefix 使 capture delivery 上界不同于 7，Stop 经生产 query 取得上界 7，不手填；缺批片不编号、不成功 Stop，相同 delivery 重投不增加编号。Stop 仅结算 press 的 Hit@90 和独立 phase 的 Miss@100；@110 未到音符及未到期 Hold 无 Miss。锁后到达的 tick 95 无编号、无归档、不结算；Stop 实封 100，独立普通 `advance(100)` 对照只封 85，不放宽未来证明或 epoch-end 守卫。EOF 同轮只交付一次；从试验注入同 epoch 异 scope、同 scope 旧 epoch 的 result reply，均不交付，segment 变化本身不换局。Reload 后未 Play 的 Stop 无结果；实际开局零分仍交付完整 `GameplayNormalizedScoreResult`，等于首次终态 `GameplayAdvanceOutcome::score`，旧 query/旧 Fold 调用计数为 0，注入清理失败仍交付一次，清理重试只执行未完成步骤，Retry/卸载后持有结果可读。Stop 正常及已 Pause 两支按 `CLK-02` 检查内存 archive 终止行和 replay，失败分支无成功归档；回放/重复通知均不重发。增加已结清、尚未到 EOF 时显式 Stop：同局完整成绩只交一次，不再调用 StopCut、不追加 Stop 终止行；漏掉首次正常终态的 score 载荷注入 `result_unavailable`，零成绩回调。Retry 时模拟键仍按住、送旧 epoch 释放与 EOF，当前局无输入/结束；当前 epoch 释放只解除阻挡，下一按下才结算。 |
| P4-06 | 实际旧工程/普通 CXC 以 bootstrap easy 且原 Reader 合法时，省略选 easy，显式同 path 也可；删除必需 `entry.chart` 即原 Reader 错误。分别用后继 filesystem extension 与 CXC v2 声明 easy/hard：省略一律 `entry_required`，只有 hard 一行且 bootstrap=hard 也拒绝省略；显式 hard 成品只选 hard，未知 path `entry_not_found`，显式后继 CXC source 条目 `encoding_unsupported`。松散 source 行 hard 经同一 compile 可准入匹配测试行；仅放 sidecar 而没有 extension 时不能选到 hard。旧 Chart-entry/Gameplay-entry v1 各用实际工厂保持原开发接受集，不能交给 v2 decoder。初装 hard 后改 bootstrap 并删 hard，Retry 缓存 hard，Reload 报 missing、保 A 停播。`--chart` 搭配 `--entry`、重复和 `../` 拒绝。开发 wrapper 与正式路径注入同内容/host facts，Pause、EOF、Stop 及结果 trace 相同。 |
| P4-07 | A=`(scope=4,epoch=7)` Playing，受理 Retry 后挂住设备回执；于 commit 前受理来源 11、12 的 Pause，再放行设备成功。新 `(4,8)` 直接 Paused@0，三命令终态成功，无 Play/audio start、无旧成绩。旧 epoch 音频/暂停回执忽略；只解除 11 仍暂停，解除 12 后才从 0 开播一次。无新 Pause 的控制组仍立即 Playing@0；提交失败时 Retry 及两 Pause 同 cause、无新局。 |
| P4-08 | 复用 [runtime/timing 的 GP-01 输入向量](runtime-timing.md#验收向量与任务前置)，输出以 [D-22 的新 SDK 计分与 Exact 投影](runtime-timing.md#归一化计分与-prepared-记录)为准。在宣布支持的每个载体，用同一获准测试映射，于逻辑 tick `1000000` 送相同逻辑 Press；Player 事件中的等级、实际误差 0、Exact，以及本次取得的成绩分值和 combo，分别与直接 SDK 对照逐字段相等。分别重跑误差边界与无实际输入的 Miss，后者误差/方向为空。去掉必需作者模式或篡改成品身份按对应合同拒绝；旧合法内容缺正式能力或 execution profile 无匹配行时 `profile_unsupported`，零开局。合法后继 CXC 纯源包在参考 Player 报 `player.source.encoding_unsupported`，无运行解码、切换或 fallback；仅对声明支持该形态的其他 Player 执行源条目播放对照。 |
| P4-09 | 用 `rr.keyboard.fixture.v1` 准备 4/6/7 通道，对按序逻辑对的默认键实测 `D,F,J,K` / `A,D,F,J,K,L` / `A,D,F,G,J,K,L`；每键 Press、重复 Press、Release 仅交付原 `(domain,channel)` 的一对 transition。自定义四键用 `Digit2,Semicolon,Up,F2`；改变 JSON 项序不影响映射，切换键盘布局时以同一物理 scancode 仍投递同一对。完整验证成功后改文件，Retry/Reload 仍用旧映射，重启生效。另测首次配置形状合法但含未知 pair：Load 失败、配置未锁；修正文件后 F5 确实重读并通过全覆盖验证，Paused@0；此后再改文件 F5 不重读配置，仍验证已锁映射。重复物理键、重复通道、`KeyR`/`Space`/`F5`、未知 token、陌生或缺失逻辑对、8 通道超过测试行容量、未知字段、版本 2、65537 字节、必需语义非 keyboard 均按对应诊断失败，零开局且不补默认键。 |
| P4-10 | 省略 `--rotate` 与显式 `0` 同；旋转 `90/180/270`、省略 `--aspect`、`auto` 和声明匹配的 `42:18` 按 R5-09/10。`--rotate 45`、两个 `--aspect`、`--aspect 1:0`、`--keymap` 无 `--judge` 为语法错误且零窗口。合法但未声明的 `3:2` 或旧谱 Exact 进入错误窗；改为声明所选比例的同条目后 F5 恢复并停在零点。再次 Reload 保当前窗口尺寸，resize 后 auto 按新物理区重选。 |
| P4-11 | 用同一真实 v3 kernel/worker/host，`close=15,H=115`：先送有效输入@90 的完整 batch，受理 advance #20 返回 Waiting；再送真实 prefix=100 仍 Waiting，prefix=101 的 capture job 才 seal，交付 Hit@90 与 timer Miss@100 各一次。逐字段等于同步内核对照的事件/完整终态值，origin 为 advance #20；capture ACK 在消费后。query、同 ID 重投、terminal 重读、retire 各零新增事件/成绩；原 reply 第二次 poll 只得 taken status。用先具备 prefix=101 再 request 的对照验证立即执行同投影；不能调用 queryGameplay 补分。P−1/错 run 槽或 post 满在输入/记录变更前拒绝且保有 buffer；注入实际 seal 后 Fold 故障，仍从触发的 capture 回复得到已封前缀、faulted 和空 score。全过程只有一个 P 槽 buffer，重用前未丢未读输出。 |
| P4-12 | 真实 Waiting 先受理 Pause，取消 waiting 后关闭完整 capture 前缀，cut=100 的 drain 实际封 Hit@90/Miss@100；仅 SessionPause 的 StepKey 回复各交一次，Pause 合流/重复/step query 无重发，score 为空。注入 drain 后 controlRpa 或 tail pause 失败，先得到相同已封事件，再错误终态且不交成绩。另用已按住 Hold 的暂停局，trace 中实际 release 经合法 Resume 在 cut 封定中断 Miss：SessionResume 回复含一次新事件，无档/方向时保持空；先前同 cut 事件不重发，Fold 故障保留此新事件但不授权新路由。Stop 在 Playing/Paused 两支走 CLK-02 的真实 cutoff，各自先交事件后移交完整成绩；回放、重复 Stop/EOF、清理失败重试仍零重发。 |
| P4-13 | NEXT 局 `RunKey=(4,7),clockSegment=3` 在 cut=100 Pause；合法新 operation/controlSequence、完整 capture trace 和匹配 route receipt 恢复到 `(scope=4,epoch=7,segment=4)`，已得分/连击/Hold 和 canonical ingress 序号连续、无新局初始化。错误 next epoch=8、同 segment=3、错 scope、旧 operation/pauseEpoch/finalTraceCut/digest 分别拒绝且不启普通 capture。恢复后注入旧 segment=3 的 deliverySequence=0、旧 advance 及旧 Pause/Resume receipt，均不推进/发事件/改新状态；新 segment=4 的真实 prefix 才可 seal。没有 release 的恢复零新增事件；有 release 依 P4-12。legacy 对照仍要求 epoch 增加，原 Resume/replay golden 不变。随后 Retry 才到 epoch=8；停止后的 CompletedRun.run 始终是本局 `(4,7)`。 |

## P5：内部 Writer 与显式迁移

`cuexis/chart/chart_writer.hpp`、命名空间 `cuexis::chart`，继续是 `cuexis_chart` 的非安装头。`ChartDocument` 与 `ChartV4SourceDocument` 用确定重载，不加 variant、共同基类或隐式版本转换：

```cpp
enum class ChartWriteLossKind : std::uint8_t { TargetUnrepresentable = 1, WriterNotFaithful = 2 };
struct ChartWriteLoss final {
    std::string objectOrPath, field;
    ChartWriteLossKind kind;
    std::string detail;
};
struct ChartWriteResult final {
    std::uint32_t targetVersion;
    std::string text;
    std::vector<ChartWriteLoss> losses;
};
[[nodiscard]] static auto write(const ChartDocument&, std::uint32_t targetVersion,
                                const ChartLimits& = {}) -> core::Result<ChartWriteResult>;
[[nodiscard]] static auto write(const ChartV4SourceDocument&, std::uint32_t targetVersion,
                                const ChartLimits& = {}) -> core::Result<ChartWriteResult>;
```

两个重载分别只接受 v3→3、v4→4；version 不一致或其他目标返回 `chart.writer.target_unsupported`（`source_version,target_version`）。v4 的源版本看 `legacyProjection.version` 和有效 v4 `canonicalSource`，二者必须是 4。Writer 返回 owning 文本，不 I/O、不修改源、不决定另存/覆盖。成功必须可由目标 Reader 按同 `ChartLimits` 重读；已知语义损失可返回合法文本和完整列表，未定位差异、无法生成合法文本或超过诊断预算则失败、无文本。保留原 `chart.writer.source_invalid` / `opaque_json_invalid` / `number_non_finite` 等原因；补 `chart.writer.output_invalid`（带 Reader 首错码和路径）、`chart.writer.fidelity_unknown`（带首个未能解释的路径）、`chart.writer.allocation_failed`（`bad_alloc`）、`chart.writer.limit_exceeded`（检查出的限额及 `length_error`）、`chart.writer.unexpected_failure`（其他异常）。固定比较语义以 Reader 值和引用为准，忽略无语义空白、默认表示、合法的有理数等值及既有规范排序；保留真正有序的事件与 patch 语义，不能把不透明数据当 typed 数字或 Beat 归约。

**P5-03/04 的私有测试接线（T-16）。** 基线静态 `ChartWriter` 没有 injector；不假设可继承替换 Writer，也不改上述签名。T-16 在 `engine/chart/src/chart_writer_internal.hpp` 增非安装 `detail::WriterTestHooks`：`void* context`、`void (*afterEncode)(void*, json::Value&)`、`void (*checkpoint)(void*, WriterCheckpoint)`；后者枚举为 `BeforeEncode, BeforeSerialize, BeforeCompare`。两个 write 重载在 `chart_writer.cpp` 共用各自类型的私有 checked-write 实现，生产调用 hooks=nullptr；该实现完成版本/源校验后，在 try/catch 内经过三个 checkpoint，并在 typed→输出 DOM 完成、正规化和重读比较之前调用 afterEncode。hooks 不得替换源、limits、Reader 或损失比较。只有 `CUEXIS_BUILD_TESTS=ON` 时，为 `cuexis_chart` 和 `cuexis_chart_tests` PRIVATE 定义 `CUEXIS_CHART_WRITER_TEST_ACCESS`，编入同 TU 的 `detail::WriterTestAccess::write(document,targetVersion,limits,const WriterTestHooks&)` 两个测试重载，转发同一 checked-write 实现。它们不安装、不加入 shared 导出表；无新增生产 CLI/env 开关或 Writer 虚表。

`tests/chart/chart_writer_tests.cpp` 在单个用例栈上创建 context/hooks，以显式参数注册到这一次调用，同步返回后销毁；无全局 setter、静态注册表或跨调用保留。P5-03 的 afterEncode 按已知对象 ID 从输出 DOM 删除 name，只改变产物，原输入仍为 `.name="edited"`；正常重读和字段比较必须返回该损失。P5-04 在各 checkpoint 分别一次抛 `std::bad_alloc`、`std::length_error`、其他异常，校验相应三个码、无 text，接着无 hook 重跑应成功。真实限额另用合法 v3 typed 内存输入与 `maxInputBytes=1`，由 checked-write 的输出字节预算先报 limit_exceeded；以两个 name 缺失和 `maxDiagnostics=1` 验完整损失超过预算时硬失败，不能返回截断成功列表。测试接线证明异常转换及清理，不宣称系统完全耗尽内存时仍能分配诊断。`tests/chart/CMakeLists.txt` 在现有 `cuexis_chart_tests` 注册具名 `rr_chart_write` 并仅 PRIVATE 暴露上述 src 头；不另建一套 Writer 实现。真实 migrator 的损失拒交用 P5-04 的实际有损旧源，不能用进程内 hook 冒充 CLI 已触发。

### 工具外部行为

按 [D-16 已接受共识](../architecture-design.md#外部行为2026-11-21-owner-已接受拟实施)，离线迁移、编译、打包工具一次命令完成后退出，明确输入/输出，成功 0、失败非零；沿用各工具现有诊断码、路径及各自已有错误分类。迁移工具保留现有具体 1/2 分类，不将这些数字推广到编译/打包工具。本轮具名四个入口：T-13 的 `cuexis_gameplay_assemble`、`cuexis_chart_candidate`、`cuexis_cxc_pack`，T-16 的 `cuexis_chart_migrator`。只有单独传 `--help` 才走帮助分支：在打开输入/输出前向 stdout 输出帮助、stderr 为空、退出 0，无文件副作用；混用及其它坏参数沿每个工具既有分类。迁移工具现有单独 `--help` 被当作参数错误返回 2，后续须更新；主体参数保留单输入及显式 `--input`、`--output`、`--report`（各一次）与可选 `--target 3|4` 默认 3。迁移单独 `--help` 到 stdout、退出 0，无文件操作；混用 `--help` 与迁移选项仍是参数错误，输出 usage 到 stderr、返回 2。

迁移发现损失即在 stderr 报具体码与路径并失败，不交付有损成品，也不写失败报告文件。进入提交前已有输出及报告字节不变；两个目标与输入须按现有 `samePath` 校验互异。进入写盘沿用现有同目录两个临时文件都写好、备份已存在目标、依次 rename 新输出及新报告的顺序。失败时撤已发布的新文件、尝试从各自备份恢复原目标、清本次临时件；回滚操作失败时保留未恢复的备份，沿 `chart.migration.output_commit_failed` 写 stderr，附失败阶段（`stage=write_temporary|backup_output|backup_report|publish_output|publish_report|restore_output|restore_report|remove_new_output`）、目标/备份路径及系统错误；仍返回 2。恢复失败时不宣称文件已回到原状。保留 [D-19 保存职责](../architecture-design.md#d-19-sdk-示例与宿主试用)：Writer 报告损失，Studio/编辑器决定确认覆盖、另存或取消。编译、打包由各自任务完成外部行为，本节不扩 T-16。

### 写入规则

**v3：** 现 `chartDocumentValue` 把 tempo/stop/templates 写空，且 direct object 路径抹去 `sourceTemplate`。新编码器写完整 typed v3 值：音频版本/asset、metadata、camera、extensions、tempo segment 六字段、Stop 的 `beat,durationMs`、Behavior events 与 stepEvents、对象及模板全量。模板根用 `prototype.components`；继承模板编码 `extends` 和从父模板解析组件到本模板 `prototype` 的差异 `patch`；实例保留 `template` 并用相同组件差异形成 `overrides`，直写对象才用 `components`。每个 `cuexis.*` 组件按名称字节序比较：新增 `add`、缺失 `remove`、改变 `replace`，使用已允许的整组件路径，`value` 为完整组件；等值不生成，引用/依赖错误拒绝。重读后比对 typed 值与模板关系，不要求复造旧 patch 字面。Reader 不装载成已知 v3 字段的非默认手填值如 `CameraData::defaultTransform.scale != (1,1,1)`，若代码证实目标 schema 没有该位置，用目标可表示的合法文本并逐字段 `TargetUnrepresentable`；不能借这个分支跳过目标可表达的字段。`ChartWriter::write` 不执行旧迁移允许的模板展开。

**v4：** 用 `ChartV4Loader::load(canonicalSource)` 建 baseline typed 值并保留原 DOM；将传入 typed 值与 baseline 逐字段比较，只编辑变化处。`legacyProjection` 负责基础 camera/audio/timing/metadata、对象、模板和 Behavior；top-level `chartId`、`extensions` 以 v4 同名字段为准，若其镜像同时被改且不一致则 `chart.writer.source_invalid`。用 stable ID 对齐模板、对象、Behavior、参数、import、clip 和 required-extension；typed 增删按对应 ID 更新，改 ID 按删旧加新，引用需调用方一起改。v4 专用字段由参数声明、import、clip、Animator 和 required-extension typed 值反向编码。

Animator 以 `(ownerKind,ownerId,baseline fieldPath)` 定位直接 component、prototype 或整组件 `add`/`replace` 源值。原位修改只改值；缺失该记录删除对应节点/patch。新记录用已有目标 owner 和空 `fieldPath`，同次写入同一 owner 只容许一条新增；直接对象/根模板写组件，且不得与仍存在的直接组件重复；继承模板/模板实例在原列表末尾追加整组件 patch。`fieldPath` 非空必须精确匹配该 owner 的 baseline，重绑用删除加新增。`parameterUses` 与已有 `fieldPath` 均为 Loader 派生值，直接修改或记录重复报 `chart.writer.source_invalid`；输出重读重建索引。

Animator patch 采用 Resolver 的存在性语义：先按编辑后的模板拓扑遍历，再处理实例；对各 owner，从编辑后父模板/目标模板的有效 Animator 存在性起步，按原顺序模拟留存 patch 和最后的新增记录。新增语句对存在目标发 `replace`、缺目标发 `add`；留存的 `replace` 若仅因本次删早期 `add`、编辑父级或重绑定而悬空，改为原位 `add`，保留其值及后续语句；留存的 `remove` 若因此已无目标，只删该冗余语句。其他合法原语句及非 Animator patch 顺序不变；既有合法 `add` 不为正规化而重写。baseline 本就有悬空 `replace/remove` 则报 `chart.writer.source_invalid`（cause=`chart.patch.target_missing`、原路径），不借修改自动修复。Writer 用同一存在性判定做结构验收，不需要外部 CXT I/O；执行验收须用同一组输入参数、CXT 和扩展能力将写出内容交 `ChartV4Resolver::resolve` 并核对解析后程序，不能只复读 Loader 的 `animators` 列表。

基础投影可能把 v4 参数引用变成占位、把模板/overrides 展开、在仅 Animator 的组件中注入 `cuexis.element`（`chart_v4_loader_projection.cpp`）。未改的占位、展开和注入字段不能回写成字面数据。按输入继承拓扑先应用模板编辑，再处理实例：字段相对 baseline 未编辑时保留原继承/patch，接受来自父级的可解释变化；显式实例编辑优先。对直接对象和根模板在原 DOM 修改实际编辑的叶子；对继承模板和实例，先从**编辑后的父模板/目标模板原 JSON** 开始，依序应用该 owner 已保留的合法 patch/overrides，得到未替换参数的有效 JSON 容器，再合并本次明确编辑的叶子。这个过程只解析继承和 patch，不用 typed 占位值、不执行参数替换或外部 CXT I/O。

输出 patch 必须遵守 `cuexis.chart.v4.schema.json::$defs.patch` 与现有 patch Reader 的白名单：向量的最细合法路径是 `/components/cuexis.transform/position`、`/rotation`、`/scale`，没有 `/position/0` 等数组索引路径。将明确编辑的单轴并入上述原 JSON 向量，保留其它轴的原字面值或完整参数引用，按该合法最小容器产生一条末尾 `replace`（有效目标不存在且白名单准许时 `add`）；同一容器多个轴编辑合成一条。非向量字段同样取白名单内最近的合法容器，禁止为方便而整组件覆盖未编辑引用或 Animator。若明确编辑的轴本身是参数引用，用该轴的新字面量替代；未编辑轴不得替代。不能恢复合法容器、继承/patch 不成立或新增/删除/重绑定不能保留其余 authoring 数据时，返回 `chart.writer.fidelity_unknown` 并定位，不回退旧 canonicalSource、不扩大白名单。

如需修改参数引用本身，编辑 v4 源 JSON 再进 Reader 取得新 typed baseline，不能篡改 `parameterUses`。结构验收重读 Loader 并比对引用；参数化运行验收必须对至少两个合法参数绑定调用 `ChartV4Resolver::resolve`，核对编辑轴和未编辑轴的最终值，不能只比较投影占位。理由可明确定性且能产出合法文本时才记录 `WriterNotFaithful`；首批对所有以上标为支持的字段期望无损。

规范输出复用 `serializeCanonical`：v3 原顺序策略，v4 按已有 v4 normal form，末尾单 LF。重读验证编辑已生效和未编辑结构/扩展仍在；无编辑样本保持现有 canonical golden、identity 和缓存命名。Writer 明细比对只在写入/显式迁移路径，不把验证性重读引入 Playback 帧或 canonical identity 的常规读取路径。

### 损失定位与迁移映射

`objectOrPath` 是 `$` 或 `$` 开头的 slash 路径，定位字段的父项，`field` 为单个未转义的 UTF-8 叶键（丢整项时为该项索引或带 `@` 的 ID 段）。每路径段用 `~ → ~0`、`/ → ~1`，其他字节不改。`objects/templates/behaviors/parameters/animationTemplateImports/animationClips/requiredExtensions` 具名数组紧随集合名的项段用 `@`+稳定 `id`（`@` 是这些集合的 selector，非任意对象键）；其余数组按原源模型的零基十进制位置，不以输出排序后的索引定位。数组整体丢失，定位拥有该数组的字段，禁止同时重复列其子项。`objectOrPath + "/" + escape(field)` 是全定位；路径上限 `ChartLimits` 的输入及路径组成字段预算，不做截断。`kind` 稳定 token `target_unrepresentable` / `writer_not_faithful`；`detail` 是下列 ASCII reason token（最多 32 bytes，无列表内拼装值或自由文案），用于稳定分类；人读原因通过诊断 message 和路径即时生成。

| `kind` | `detail` | 规则 |
| --- | --- | --- |
| `TargetUnrepresentable` | `field_not_in_target` | 目标 wire 对该源字段没有对等表示 |
| `TargetUnrepresentable` | `value_outside_target_domain` | 目标有字段但无法表示具体合法源值；若找不到明确合法替代则硬失败 |
| `WriterNotFaithful` | `field_omitted` / `value_changed` | 目标能表达，但实现产物实际删/改了该字段（含回读差异） |
| `WriterNotFaithful` | `authoring_data_discarded` | Migrator 丢了目标可表达的命名、扩展、未绑定声明或引用信息，且没有合同许可的等价表示 |

先对同一次迁移的实际转换输入/输出对比，再拿 Writer 结果；两阶段分别收集 `ChartWriteLoss`，按全定位、`kind` 数值、`detail` 字节序排序，精确去重。同一 `migrateToV4` 中的 v1/v2→v3 子迁移若失败，携带该诊断退出，不继续 lift。`ChartMigrationReport::discardedFields` 继续为 `vector<string>`，只在成功返回的 `artifact.report` 上可读；现有成功报告字段、v4 string array 与 v3 report golden 保持，成功无损时该损失列表为空。`ChartMigrationResult` 继续仅含 `artifact` 与 `diagnostics`，不新增失败 report 或 sidecar；`artifact == nullopt` 时没有可取的 `report`、`discardedFields`、`chartJson` 或 `reportJson`，不承诺另有内存失败报告。

每个合并后的损失只经失败结果的 `diagnostics` 可观测：发 `chart.migration.data_loss` severity `Error`、`fieldPath=全定位`、context `kind`、`detail`；来源为 `conversion` / `writer`，两边完全相同的损失合成一条且来源为 `conversion,writer`。失败验收把这些诊断的 `fieldPath` 按字节序排序并去重核对完整损失路径；不同 kind/detail 同路径可有多条诊断，路径集合只计一次。任何损失即 `artifact == nullopt`，原 Chart/输出/报表字节不变，CLI 先通过现有 Result 门禁才可进入双文件事务。超过 `Diagnostics` 限额拒绝无成品，保留首个可写入的诊断并标现有 overflow；限额失败不伪称诊断已完整、不输出假空损失报告。

转换对照：v1/v2 Reader 已拒绝非空 tempo/stop（`canonical_chart_loader_behaviors.cpp`）；去掉转换时无条件 `clear`，保留已接收的数据，不放松旧 Reader。v3→v4 对原 v3 JSON lift，不经 v3 typed 写回，非空 tempo/stop/templates 原值保留。旧 v1/v2→v3 的合同允许等价模板展开、首键改初值、事件等价生成并在原计数中记账；仅在引用/初始与有序运行行为可证明等价、没有独立未用作者数据时算无损。未绑定有真实内容的声明/metadata 或模板 extension 若被移除，分别定位声明或其字段；不得因运行效果相同就可丢弃，例如 `$/templates/@019b0000-0000-7abc-8def-000000000020/extensions`。旧 `unbound_single_key` 等无法等价的原错误保留，禁止再走 Writer。合法 lift/规范化不输出损失；身份计算仅在确认无损后进行。

### 调用点与可见性

| 基线入口/调用方 | 新去向 |
| --- | --- |
| `ChartWriter::write(ChartDocument)`，`chart_migrator.cpp::migrateToV3` | `write(migrated,3,limits)`。先检查转换，再检查 Writer；两旧纯文本入口同批删，无转发。 |
| `ChartWriter::writeV4`：`chart_migrator.cpp::finishV4Migration`、`behavior_candidate.cpp`、`tests/chart/{chart_writer_tests,chart_v4_c2_closure_tests,parse_characterization_tests}.cpp` | `write(source,4,limits)`；迁移、identity、closure/golden 样本仅在成功且无损后用 `text`。身份路径有损发 `chart.writer.identity_loss`，带首损失全定位及原因，无 identity。 |
| `chart_writer.cpp::chartDocumentValue` | 完整 v3 反向编码器；输出 JSON DOM 继续只在 Chart 内部。 |
| `detail::writeV4Value`（旧 Writer、`chart_v4_resolver.cpp`） | 改为 `detail::serializeV4CanonicalValue(json::Value,const ChartLimits&)`；Writer 传编辑且核对的文档树，Resolver 继续传已解析原树，normal form 不变。移除旧 helper 名。 |
| `detail::writeCanonicalValue`（`writeCanonicalJson`、`chart_v4_loader.cpp`） | 保留，作为全量 JSON 正规化，无 typed 源损失语义；删 `chart_v4_resolver_patching.cpp` / `chart_v4_resolver_program.cpp` 无使用的 writer-internal include。 |
| `ChartWriter::writeCanonicalJson`：Migrator、`playback_session_prepare_stages.cpp`、`playback_identity_tests.cpp`、`parse_characterization_tests.cpp` | 保留 API、字节和调用目的；不得改走保存重载。 |
| `ChartWriter::writeAnimationTemplate`：`chart_v4_resolver_program.cpp`、Writer/CXT 及解析特征测试 | 保留唯一 CXT 序列化及 golden。v4 loader 本身仍只读。 |
| `cuexis_chart_migrator`、Runtime/Playback migration equivalence、`chart_migrator_tests.cpp` | 仍走 `ChartMigrator`；CLI 消费 artifact/report 或输出诊断。主体迁移参数保留，`--help` 按工具外部行为修订为正常帮助退出 0。Packed Writer 与 CXC 编码不在本批写域。 |

仓内 authoring 工具可直接调用新重载，并依据 loss 决定是否交给用户确认；发布安装 SDK 无这些签名、DTO 或 DOM。第三方编辑器当前可用具名 CLI 迁移，不承诺进程内 Writer。未来另需 D-19 具名编辑器试用、同版无损基线、版本化窄合同及安装消费者验证，届时单独批准入口；本批不预装 authoring 组件。保存是否覆盖、另存、取消或自动保存仍归编辑器，Writer 不触盘。

### 验收向量

| 向量 | 精确断言 |
| --- | --- |
| P5-01 | v3 使用 tempo `{startBeat:0/1,durationBeats:1/1,startBpm:120,endBpm:150,startSlope:0,endSlope:0}`、Stop `{beat:2/1,durationMs:125}`，加根模板、基于它的继承模板和有可见覆盖的实例；按 typed 值重读均等、保留关系、损失空。v3 请求 4、v4 请求 3/5 均 `target_unsupported`、无文本。 |
| P5-02 | v4 取 `fixtures/chart_format_update/valid/chart_v4_cxt_template_binding.json`，改 `legacyProjection.timing.defaultBpm` 至 `150`、选现有 import 的 `source` 替成 `templates/alternate.cxt`；无解析资源的 Writer 重读保留两项编辑及其余原 author 数据。把另一个用例的 opaque `org.example` 下 `rationalLike={numerator:2,denominator:4}` 原样保留，绝不改成 `1/2`。无编辑的 committed Chart/CXT golden 字节不变。 |
| P5-03 | 对 v3 有效输入对象 `.name="edited"`，用本节 WriterTestAccess/afterEncode 在重读比较前从输出 DOM 遗漏 `name`，预期该对象 `@id` 父位置、`field=name`、`WriterNotFaithful/field_omitted`，文本仍合法；Migrator 或成品调用拒用。路径转义测试原键 `a~/b` → `a~0~1b`；两个阶段同一损失应只出现一条 `data_loss` 诊断且来源为 `conversion,writer`，失败 `artifact==nullopt`；只从 diagnostics 的去重 `fieldPath` 集合核对该位置，不读失败 report。 |
| P5-04 | 现有 v1/v2 等价展开 fixture 计数与 report golden 不变；加入被转换删除的独立作者扩展，期望 `chart.migration.data_loss`、上述模板 extensions 全定位，`chartJson` 与 `reportJson` 无 artifact；旧输入/已存在两输出目标哨兵字节逐个保持。相同 v3 输入中的未改 tempo/template 经 v4 lift 保留。在三个 checkpoint 分别注入 bad_alloc、length_error、其他异常，对应 allocation_failed、limit_exceeded、unexpected_failure，均无文本；真实输出字节/损失条数预算另跑本节限额向量，hook 不改变预算或比较器，解除后正常写回成功。 |
| P5-05 | 基于 `valid/chart_v4_template_animator.json`，取原 Animator 复制组件且唯一 layer ID 分别设 `layer.parent`、`layer.child`、`layer.instance`；对象改用新 child template。源父无 Animator、child 无 patch 时，给 child 或对象单独新增分别产生 `patch[0].op=add`、`overrides[0].op=add`，Resolver 正好解析该对象一层且 ID 为各自期望；同一写入给父加 `layer.parent`、再给 child 和对象新增，child 和对象均为 `replace`，Resolver 只解析 `layer.instance`。另造父无 Animator、child 原 patch=`[add(layer.child),replace(layer.parent)]`，删第一条 typed record 后写出只余 `add(layer.parent)`，Resolver 对对象恰一层 `layer.parent`，损失空。保留 `replace` 的负对照须由 Resolver 报 `chart.patch.target_missing`，Loader 重读成功也不算通过。 |
| P5-06 | 迁移 CLI `--help` 正常帮助退出 0；缺必需输入/输出、读入失败、路径冲突或提交失败沿既有分类返回 2，迁移无结果（含损失拒交）返回 1，成功返回 0。诊断保留既有码和字段路径；损失失败发生在提交前，已有输出/报告哨兵不变。新报告 rename 失败，旧双目标都有备份时应恢复两份原字节；原本无输出时撤新输出。另注入恢复失败，退出 2、保留实际备份和目标/失败阶段诊断，不声称原字节均恢复。两次均验证输入不变。 T-16 以实际迁移子进程检查退出码、stdout/stderr 及双目标字节；T-13 以三个具名工具的子进程分别检查单独帮助、原成功命令和坏参数。 |
| P5-07 | 构造有效 v4 父模板 `position=[0,{"parameter":{"domain":"chart-parameter","id":"y"}},0]`、实例继承且无 overrides，`y` 为 number。Loader 后只把实例 typed X 改为 5；Writer 追加 `replace /components/cuexis.transform/position`，value 必须为 `[5,param(y),0]`，父模板和参数声明不变、losses 空。分别以 `y=2` 与 `y=9` 运行真实 Resolver，实例为 `(5,2,0)` 与 `(5,9,0)`；typed 占位 Y 不得进输出。已有 overrides 的对照先把 Z 设 3，再改 X；新增向量必须保留 Z=3 与 param(y)。另取无 overrides 的独立夹具，同次改父 Z=4 且实例只改 X，得到 `[5,param(y),4]`；有原 Z=3 override 时再改父 Z=4，实例仍为 `[5,param(y),3]`。负对照写 `/position/0` 由原 patch Reader 拒绝；把整个向量从 typed 占位写成 `[5,0,0]` 虽可重读，两个 Resolver 对照均失败。 |


## 拆分与门禁

R5 按纯布局/GL 状态、帧消费/兼容证据两卡；以现行旧谱和内存构造的规约表验证，T-05/T-06 不依赖 T-11/T-13 的实现、wire、target 或 schema 已生成。T-11 负责作者字段接到 Prepared；T-13 负责后继成品接同一表。Player 先跑 T-14 共用无窗控制，T-15 接外壳、键位、窗口策略和 `--judge`；Writer 单卡。P4/P5 保持 `NEXT`，各自等待采用登记。

以下仅是未来实施 CTest 注册名，当前目标未创建、命令未跑。使用既有测试可执行文件扩展相应 fixtures；GL 用现有可创建上下文的测试组合，不在无窗 preset 伪造像素通过。

| 任务 | 命令 | 向量 |
| --- | --- | --- |
| 布局/借用目标 | `ctest --preset debug --no-tests=error -R '^rr_frame_layout$'` | R5-01–04、R5-06 内存比例、R5-09、R5-10 纯几何/逆算、R5-11 |
| 帧桥/迁移 | `ctest --preset candidate-debug --no-tests=error -R '^rr_laid_out_frame$'` | R5-05、R5-07–08、R5-10 相机/retained、旧入口探针；作者和 wire 向量在格式卡执行 |
| R5 双 flavor 链接与头 | [T-06 的双 shared 门禁](../tasks/T-06.md#任务级验收) | 两套实际 Player、旧谱摘要、符号/头边界与生命周期 |
| 会话控制 | `ctest --preset candidate-debug --no-tests=error -R '^rr_session_control$'` | P4-03–05、P4-07、P4-11–13 |
| Player 外壳 | `ctest --preset candidate-debug --no-tests=error -R '^rr_player_(startup|commands)$'` | P4-01–02、P4-06、P4-08–10 |
| 内部写回库 | `ctest --test-dir out/build/headless-debug --no-tests=error -R '^rr_chart_write$'` | T-16：P5-01–05、P5-07 |
| 迁移 CLI | `ctest --test-dir out/build/candidate-headless-debug --no-tests=error -R '^rr_chart_migrator_cli$'` | T-16：P5-06 迁移分支，先构建 `cuexis_chart_migrator` |
| 编译/打包 CLI | `ctest --test-dir out/build/candidate-headless-debug --no-tests=error -R '^rr_compile_pack_cli$'` | T-13：P5-06 三工具分支，先构建 `cuexis_gameplay_assemble cuexis_chart_candidate cuexis_cxc_pack` |
