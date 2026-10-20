# Stage 7-PD PD-3-b 状态修订、只读桥与按需物化

日期：2026-10-19。范围：[PD 计划 PD-3-b](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)
及[细则 §4.1–4.3、§5、§12.2](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md)。
状态：内部实施及本地适用门禁完成，GPT-6-Luna 独立复审 PASS，PD-3-b 可退出。
当前状态唯一见 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。

## 1. 进入条件与对照点

按用户要求，GPT-6-Luna 以不继承父对话历史的子代理审核 PD-3-a。首次审核发现
`ResourceScope` 在 noexcept 构造期间分配的真实终止风险；补正及完整重新验证后复审 PASS，
才开始本批。[PD-3-a 报告 §6](2026-10-18-pd3a-software-transaction.md)保存处理和证据。

本批从 `stage-7` / HEAD `d344088236ae589c0b49a4994ef0fbddd6567ce2` 的未提交 PD-2、PD-3-a
工作区继续。`out/pd3b-verification-20261019/pre-pd3b-workspace.zip` 与
`pre-pd3b-state.json` 保存进入本批时的 38 个修改/新增输入；不覆盖已冻结 PD-0/1/2/3-a 证据。
本批没有提交、推送、PR、合并或发布。

## 2. 最终实现

- Runtime/World 继续拥有唯一当前属性状态。`presentation_revision.hpp` 为普通更新、Gameplay
  更新、override acquire/release、base property、reload、commit/unload 和受控可变 World 回调
  提供同一 checked reserve/publish guard。安装前失败不发布版本；已进入可变回调后即使返回错误或
  抛异常，也保守发布实际状态。Reset/恢复与同步 mutation 不计普通 update ordinal。
- Playback 保存一个轻量当前描述：session/result/content/layout/world 修订、真实来源种类、
  可缺省的 frame/T/Fact/control cursor/work tick/Fold 和 projection 激活修订。激活表仍由 projection
  唯一拥有；不在每次更新复制 membership，也不添加完整属性数组、历史 World 或结果队列。
- Runtime 非安装 `PresentationRead` 按稳定索引读取 Object/Camera 的中性有类型字段与 presence。
  开始时核对 owner/layout/version；借用只在同 owner 的不可重入操作内有效。weak window 与 serial
  防止离开操作或进入后续窗口后复活旧 view，window 不延长 World 寿命。真正 const Runtime/World
  访问避免只读操作误选可变 `withWorld` 重载。
- Playback `presentation_materializer.cpp` 的 `fillObjects`、`fillCamera` 与唯一
  `materializeSnapshot` 直接填 owning/reusable Snapshot；两种公共 `extractFrame` 共用该入口。
  Playback 不再遍历 registry 或解释 World 组件。prepare 时固定的相机选择次序保持旧规则；
  资源闭包、缺字段默认、矩阵计算、错误顺序与公开 Snapshot 格式保持既有语义。
- 内部 ExactVersion 仅支持当前提交状态；旧版本返回 `playback.presentation.version_unavailable`，
  不重算/Seek。已持有的 owning Snapshot 保留原值。纯查询、提帧、viewport 变化、receipt 和纯控制
  不改变来源/画面修订；GameplayOnly 不制造 World 或判定来源字段。

安装后的描述发布只做标量/optional 赋值和已有 projection/diagnostic 的 noexcept 转移；
Runtime 成功至 finalize 之间不添加分配、stale 复查、外部回调或设备动作。
只读窗口首次使用可分配一个固定 gate；热态读取和原热态提帧仍由分配测试验证。

没有新增 SDK 公共方法、字段或版本；安装 Playback 头只增加 private friend/forward declaration，
不暴露 Runtime、World、registry 或新内部描述。原同步 FrameEnvelope、控制/fence/receipt/Close
许可路径保持，PD-3-c/4 的来源绑定与额度交付本批未实施。

## 3. 审核发现与修正

Luna 对 PD-3-b 的第一轮复核发现中性桥的 catch-all 会把物化 OOM 改成 Runtime 错误。
已恢复 `bad_alloc` 传播至原 Playback 分配错误边界，新增实际 string payload 分配注入，
断言 `playback.prepare.budget_exceeded` 与 `operation=extract_frame`，随后同一 destination 可重试。
桥关闭窗口的 RAII 在异常路径仍执行；安装状态不回退。

最终设计不变量复核还补齐投影目标读取的布局绑定：`HasTarget` 的预期 owner/layout 来自保存稳定
索引的 `SnapshotLayout`，version 来自当前 Runtime 状态。即使将来出现同 ID 的内容重建，旧布局
也在读取 callback 前由统一桥拒绝。补正前的全绿矩阵保存在证据根 `pre-layout-guard/`，最终
门禁和源码绑定在补正后重新执行，不混用这组较早程序。

新增测试曾发现测试夹具使用无 ResourceManager 的 renderable Runtime、错误的公开 scale 枚举，
以及把 RPA advance decision 当作旧 advance receipt 查询；均依据现有接口修正测试，未改变产品
语义或放宽门禁。最终验证以完成后的输入绑定为准。

## 4. 等价与验证证据

证据根：`out/pd3b-verification-20261019/`。使用四个新目录 `out/build/pd3b-debug`、
`pd3b-release`、`pd3b-shared-debug`、`pd3b-default-off`；metrics 为 OFF，Release 为 WX ON。
42 个实施/测试输入的最终指纹为
`58ae77f33014e35e472ed588f0d4b83eb2897dc480421fa25256130474b3a89d`。
`implementation-build-binding.json` 与 `implementation-inputs.zip` 保存源码、Ninja 实际依赖、
对象和链接产物 hash/时间及配置。每个关键对象的依赖均非空，所有输入早于其对象，链接产物
晚于对应对象；新 read/materializer/state test 均包含在实际构建依赖中。

| 最终配置 | 普通 CTest 选中 | 失败 | 明确跳过 |
| --- | ---: | ---: | ---: |
| candidate static Debug | 1080 | 0 | 1 |
| candidate static Release / WX ON | 1080 | 0 | 1 |
| candidate OFF static Debug | 961 | 0 | 1 |
| candidate shared Debug | 1065 | 0 | 5 |

普通矩阵排除另行执行的 package/import 门禁及独立性能脚本，不等于完整裸跑所有注册 CTest。
static 的 1 项为 Windows symlink 环境 skip；shared 另有 1 项内部 parse 计数不可用和 3 项
跨 DLL allocator 注入不可用的明确 skip。shared 私有 descriptor 测试不编译入程序，
public materializer 两例为 43 assertions，Runtime neutral read 仍完整执行。

四配置 Runtime neutral read 均为 4 cases / 165 assertions，包含热态读取零分配；
static candidate descriptor/public Snapshot 为 6 / 189，OFF 为 3 / 92。
static 分配错误码专项为 1 / 8；原分配测试全集 Debug 9 / 400、Release 9 / 342、OFF 9 / 394，
shared 为 6 pass、3 skip。原 Runtime 事务 6 cases 的 assertions 分别为 Debug/OFF 9426、
Release 10101、shared 9380；原 World 和 Gameplay finalize/失败重试测试也全部通过。
Debug allocator 仍保留 PD-3-a 所述 MSVC iterator-proxy size class 限制，Release 不排除该类分配。

static 包消费者 3/3、shared 包消费者与 export/import 门禁 6/6 全部通过。
文档检查验证 454 Markdown、20 candidate JSON/CXT；版本、全部 42 输入中的 C++ 格式检查及
`git diff --check` 通过。受保护 PD-0 注册/closure、原 PD-2 六项输入、PD-3-a 原证据 manifest
所列文件全部核对未改动。

`verification-summary.json` 保存各组统计、skip 与命令；`evidence-hashes.json` 保存证据 hash。
所有最终测试日志均晚于最终对应链接产物，避免引用布局补正之前的全绿结果。
Luna 对最终指纹独立复核代码与当时 manifest 的 124 个证据文件散列，给出
“PASS：PD-3-b 可退出”；完整裁决保存于证据根 `luna-final-audit.md`。裁决后的文档收口单独
重跑 docs/diff 检查，未改实施输入、测试日志或旧基线。

独立公开消费者 `snapshot_probe.cpp` 分别链接已冻结 PD-3-a installed package 和当前实现。
13 个同源 Snapshot 覆盖初始帧、父子行为矩阵、两个 viewport、mesh/material/相机、visible/opacity/
tint override、RemainingFrames 到期与 reload。逐字段序列化全部公开 Snapshot 值并附 FrameDigest v3，
两边均为 7,878 字节，SHA-256 均为
`f4303f95f2b2de54993946f3f5ba6a3a91e28b7a8bb333427bc43029ea1c60c0`。
这不是重新生成 golden 或正式 PD-0 性能采样；旧 PD-0 注册/失败窗口保持原样。

## 5. 验收边界

本批只完成状态读取、修订与物化边界，不完成 PD-3-c、PD-4/5、跨 owner 交付、retained payload、
ledger retire/reclaim、多 Renderer、真实 GPU/设备丢失、生产阈值、owner 验收或 Stage 7 关闭。
同源字节等价与热态零分配不声明 Release 性能收益；后续正式测量必须保留原 15% 门禁和历史样本。
