# B5 线程部署与自动验证记录

日期：2026-10-13。状态：非实体设备实施与本地自动验证完成（testOnly）；不是 B5 退出或 F1–F4 接受记录。

## 范围与实现

owner 要求先由子代理复核 B4，再完成无需其参与的 B5 部分；随后要求停止继续委派，
本轮剩余实现、排错和验证由根代理完成，未再派子代理。B4 复核修复三个问题，完整 Host
32 cases / 823 assertions 通过，详见 [B4 报告](2026-10-12-b4-host-and-resources.md)。

AudioWorker 在线程内创建、解码、服务、隔离 callback、释放 clip store 并销毁设备。
SessionWorker 共置 D2/D3，PreparedPlayback、PlaybackSession、软件资源和 Runtime 留在 owner；
候选、frame、archive 和阶段回执为 CPU owning 数据。主线程 ExecutionHost 负责 D1、GPU 和协调。
SDK facade 不隐式创建线程。维护性采用单个在途控制及可查询原 job ID 的明确状态，
控制等待泵主线程事件；worker 不等待主线程清理，GPU 退休后仅 join 已退出 worker。

OwnerChannel 预留控制/终态回执槽直到 ACK；退出先封准入，再处理已接受记录。
主线程及 worker 采样时刻不同，因此 Advance 的 H 在 owner 实际处理时派生并缓存，
重复 ID 不重算 H，异值拒绝。Pause 在 owner inspection 后重读时钟，post K1 后不泵事件即执行 D1 cut。
Seek 使用真实 P=H+A 与 T=H+V，实际全域 readiness/fence 后提交。
故障 Close 导出未确认 pending、tentative Tick 成员和 fault，不制造 ACK 或已准入输入。
部署的 D1 数据通道另留两个真实 cut 消息槽；同线程 B4 默认行为保持不变。

显式 EOF tail 保持同一 kernel/run，并换 clock segment。主线程和 worker 共用实际 source
Pause/Resume 单调时间锚点，避免把回执到达时间当 source 动作时间；暂停不累计尾段时间。
新 run 的 terminal H 至少覆盖实际 control drain，输入 deadline 不变；不能在 terminal
证明 F(H)<B 的配置拒绝。完整 terminal 结果与 Replay 求值比较通过。
Player 接入 Pause/Resume、实际 archive cut Seek、RestartAtZero Reload、focus Pause、
有界事件队列、viewport resize、EOF tail 与定向 Close。入口为 --execution-test-only；
EOF 需显式 --execution-tail-input-deadline。生产容量、观察误差和预算缺失时不转正。

Reference Host 增加只使用已安装 SDK 的显式 fixture 时钟入口：prefix、EOF tail、完整 Replay
比较、unload 后 owning archive 均实际执行；它不模拟物理听音证据或隐式创建 SDK worker。

## 本地证据

日志位于 out/b5-*.log。最终源码身份和配置见 out/b5-source-manifest.json，
机器可读 CPU 测量见 out/b5-local-measurements.json；HEAD 单独不能代表未提交工作树。
最终非 Markdown 修改/新增源码 95 项，摘要为
`0603e4e0340485805be7cc72fd9d5415d4ddb94961f99db64f9e01ae5608e09d`。
76 项 C++ 修改文件 clang-format dry-run、117 个 public header ASCII 和新文件命名检查通过。

| 检查 | 实际范围 |
| --- | --- |
| B5 线程/故障/尾段 | MinGW Debug、Release、shared Debug 与 MSVC Debug 完整 executable，各 24 cases，通过 |
| B4 回归 | 最终 Host 32 cases / 823 assertions，通过 |
| Playback / Judgement | 完整受影响 executable；Playback 153、Judgement 207 cases；shared 的私有 parse probe 按限制 Skip |
| MSVC | 干净编译 Player、设备 probe、线程/Host/Playback/renderer；运行 CPU 测试，不运行设备 probe |
| Release 未过滤套件 | 中间快照 1025 项，943.35 秒，1 项 Reference Host 新源码调用不存在 clear 方法失败，1 项 filesystem symlink Skip；修为 unload 后 staging/installed gates 复验通过。最终新增/修复测试与受影响 executable 全跑；未将中间全套写成全绿，也未宣称最终注册集合全量重跑 |
| static/shared 与 candidate OFF | 架构、SDK installed Gameplay / Reference Host 消费和 OFF Playback 消费适用检查通过；OFF 全量构建通过 |
| 格式、ASCII、命名、版本、诊断、文档 | 逐项脚本复验；新生产文件不使用 rpa 前缀，版本仍为 26.10.12-2 / SDK 0.8.0 |

MSVC 栈溢出来自测试函数中大型控制证明临时对象累积；改为短生命周期 helper/heap owning
构造后原轨迹和断言通过，没有增大栈或放宽 timeout。头布局改变后采用 clean-first，
不使用增量旧对象读到 0xcdcd 的失败作业务结论。B1 GNU int128 oracle 的 MSVC 全目标限制仍保留。

最终 Debug 受影响回归：B4 32、Playback 153、Judgement 207、RPA 50 cases 全部通过。
Release 的相同受影响 executable 通过；MSVC B5 24、B4 32、Playback 153 通过。
shared B5 24/B4 32 通过，Playback 153 中 152 通过、1 项私有 probe Skip；OFF Playback
121 cases / 10615 assertions 通过，4 项架构/安装 Playback/Reference Host 门禁通过。
static 与 shared 的架构、installed Gameplay、Reference Host 各 3 项最终门禁通过。

## 测量与接受边界

预声明 CPU fixture 包括单 requirement 尾段与 256 requirement 同时到期的 testOnly 边界。
后者在声明的 maxRequirements=256 下验证 256 Miss、完整 Replay、D3/present 和 Close，
记录 prepare 时间、Packed 字节和 sample/present 时间；没有扩大生产限额。
H lag、frontier lag、sealed/folded cursor、请求与关闭/join 时间分别记录。
分位数仅描述当前样本，不能充当接受阈值；该边界不代表 Stage 8 的生产最大内容规模。

此前设备首跑保留为历史证据：真实音频 callback/隔离/EOF/Close 通过；RTX 4060 首次 RPA
Advance 当时失败，未到呈现。本轮按 owner 指令不重新运行实体设备，后续时钟修复不能
追溯将旧 GPU 结果改成通过。物理 DFJK/Hold、focus、真实听音、欠载/拔设备、刷新率与
实际 present 阻塞仍需实体设备验收。F3 生产参数/预算接受、F2 同 SHA hosted、F4 接受均独立待办。

## 构建产物清理

按 owner 授权盘点并删除 out/build 下 28 个旧目录，盘点文件大小约 91.0 GiB；
仅保留本轮 5 个配置。先保存旧日志/配置及链接清单，链接仅删除自身，不递归到目标。
清单为 out/build-cleanup-inventory-20261013.json，结果为 out/build-cleanup-result-20261013.json，
证据归档为 out/build-cleanup-evidence-20261013。注册 worktree 和全局 vcpkg 缓存保留。

## A 批真实设备基础复验

2026-10-13 owner 授权完成 A 批。本次实际运行 MSVC Debug 的
`cuexis_execution_device_probe.exe --reporter console --durations yes`，并编译同配置 Player。
证据目录为 out/b5-device-basics-20261013，首跑日志保留为 probe.stdout.log / probe.stderr.log。
最终源码 95 项摘要为
`6175b3bbae05c88d0997ac80b6d81553067324e6ca7c57ba14c0e409a58988d0`，
source-manifest.json 同时记录 probe 二进制 SHA256、配置和各次退出码。
先前 CPU 测量摘要和上述历史测试日志不重写为本次源码结果。

首跑音频通过，GPU 首次 Advance、Pause/Resume、present 及像素读取通过，但 Close
返回 host.rpa.context_stale。原因是 Advance 初次采样缓存 Tick，而 flush 发布前缀时
第二次采样已更晚；Close 引用较早缓存，低于 D1 已发布前缀。修复 ExecutionHost
使缓存记录实际成功发布的前缀 Tick，没有修改物理输入时间、放宽 cut 校验或制造 ACK。

| 分项 | 本次设备证据 |
| --- | --- |
| 音频端点 | SDL 实际打开 48000 Hz / 2 channels / buffer 480 frames；静音 PCM，不是人工听音接受 |
| 音频独立服务 | 三次主线程阻塞 100 ms，service 增量均 37、presented 增量均 4800；阻塞 500 ms，service 增量 174/177/178、presented 增量均 24000 |
| 隔离、恢复、EOF、关闭 | 隔离后 25 ms callback 帧数不增；实际恢复和 EOF、Stop、再次隔离、retire、Close、owner 退出及 join 均通过 |
| OpenGL | NVIDIA GeForce RTX 4060 Laptop GPU；驱动 32.0.16.1664；实际 Advance、Pause/Resume、sample、present、绘制提交及 glReadPixels 通过 |
| GPU 读取范围 | presentationDrawn 为真，中心 RGBA 为 [14,16,18,255]；这是提交与读回证据，未据单个像素声称图形外观人工验收 |
| 有向关闭 | 三次 Host Close 到 closed、frame owning snapshot 跨 Close 保持有效；探针进程正常退出，末次检查无残留 probe/Player 进程 |

修复后连续三次各 2 cases 全部通过（475/474/481 assertions，退出码均 0）；
完整 MSVC B5 自动 executable 24 cases / 1689 assertions 通过，见 regression.log。
相关构建、格式检查、文档检查和 git diff --check 通过。
只改一项 C++ 源码；最终修复尚未在其他工具链重新跑矩阵，不把旧矩阵自动转记为本次通过。

A 批真实设备基础完成（testOnly）。B 批实体 DFJK/Hold、焦点、实际听音和 Player 操作，
C 批欠载/丢设备/压力，D 批同步及生产参数收敛仍未执行；B5 与 F1–F4 未退出。

## B 批谱面准备与未通过的 Player 预检

owner 授权使用程序生成素材创建谱面，随后指出已有桌面目录
C:/Users/Zilch/Desktop/Cuexis-S7A78-DeviceAcceptance-2026-10-09。核对旧包是
gameplay.execution.t4-k4.v1，使用 per-transition / per-render-frame 测试桥；
其四轨、Hold、合成音频和操作形式可复用，不能作为新实时路径的通过证据。旧目录未修改。

tools/make_execution_practice.py 生成 19 requirements、32 秒 PCM、组合键、13–17 秒 Hold、
31.5 秒 EOF 前 D 与 32.5 秒 EOF 后 J；本地包在 out/execution-practice，另存桌面
C:/Users/Zilch/Desktop/Cuexis-B5-DevicePractice-2026-10-13。mesh/material/texture/PCM
均在生成过程中构造，无外部素材。完整 Graph/Packed 的实际 prepare/commit 验证通过，
独立 ZIP/hash/entry、PCM 和关键 guide 时序检查通过。

新 profile 尚不属于既有作者 schema；新增工具的显式 --execution-test-only true
选项，在原作者 schema 校验后选择已有 RPA typed profile。仅允许 testOnly inline 与
匹配的完整 engine identity，不扩展 engine 作者 schema 或默认行为。14 个旧 CLI 回归通过，
未显式选择及 false 选择均拒绝且不发布。Player 接入四轨 display、Ready 及 debugScene
转交；这是测试显示适配，与谱面数据生成分开记录。私有呈现桥改动没有改变公共 SDK 合同。

当前源码摘要 395c84c755acfcb0a18a23358b882f298e57be5eaa6ccc60e05cbea863f02b27，
身份清单见 out/execution-practice/implementation.json。MSVC 干净构建通过，B5 24 cases /
1690 assertions、Player control 34 / 739、设备基础 2 / 671 均通过。
但实际 Player 新包预检在定向 Space 后失败为 host.rpa.clock_invalid；日志在
out/execution-practice/runs/20261013-191430-003。黑色 PrintWindow 截图不足以证明
Ready 引导外观正确。运行预检未通过，不能将测试包交付等同于可开始人工验收。
实体键盘、人工听音以及音频时钟失败的根因/修复仍待；README 已明确标记此限制。

owner 随后报告“没有任何显示”。定位到 ExecutionHost 加载请求未开启已有 DebugLine pass，
虽然 guideScene 已交给 renderer，active settings 仍丢弃引导。修复为按 renderer 的
debugPass capability 请求此通道，仅修改 app/player/src/execution_host.cpp。
MSVC 重建和 B5 完整 24 cases / 1696 assertions 通过。实际窗口 DPI-aware 屏幕截图
out/execution-practice/ready-display.png 已人工检查四轨、DFJK、判定线、Ready 提示均出现；
窗口正常关闭，无残留 Player。新源码身份见该目录 implementation.json，旧摘要不转记。
开始播放后的 clock_invalid / owner 本次日志中的 message_conflict 尚未定位修复；
未声称音符下落、Hold 或 B 批人工验收通过。桌面 README/身份/预览已同步。

## B 批尝试回退

owner 指出轨道/文字应由谱面实体表达，随后明确要求回退 Player 及相关代码。
本次仅撤回该测试尝试新增内容：Player guide 接入/Ready/sample/present 改动、guide 的
showScore 参数、ExecutionHost 的 debugScene 及 DebugLine 请求、私有 frame_observation
桥参数、assembler 显式 realtime 选项与工具 context 字段、tools/make_execution_practice.py。
既有旧 guide 文件恢复原样，没有删除历史 Player 引导设施。

回退后 95 个非 Markdown 源码文件逐项 SHA256 与 A 批 source-manifest.json 一致，
摘要恢复为 6175b3bbae05c88d0997ac80b6d81553067324e6ca7c57ba14c0e409a58988d0；
B3/B4/B5 及 A 批前缀 Tick 关闭修复保留。回退前 10 个源码备份与核对脚本在
out/execution-practice/rollback-backup。新桌面/本地临时包启动入口禁用，原始产物和
运行证据保留，原 2026-10-09 桌面目录未修改。上节编译、Ready 和失败结果均为已撤回
尝试的历史证据，不能当作当前能力或 B 批退出。未继续重做谱面、修复播放时钟或提交代码。
回退后的 MSVC clean-first 构建通过（Player、assembler、设备 probe、B5 与 Player control）；
B5 24 cases / 1692 assertions、Player control 34 / 739 回归通过，日志为
rollback-build.log、rollback-host-tests.log、rollback-player-tests.log。文档和差异检查通过。
本次未重新启动实体设备 probe，不新增设备验收结论。

## 谱面实体呈现版

owner 随后要求先改当前测试谱面。按 Chart v4 编写指南，用
tools/make_entity_practice_chart.py 生成自包含呈现预览：40 个具名 Object、21 个
behavior.event v1、14 个程序生成资源。标题/说明/DFJK 是字形 Mesh 实体，轨道、
判定线、短音符、Hold 头/体/尾均由包内 Transform/Renderable 定义；显隐、位置及缩放
在 Chart 中声明。无需 gameplay-guide 或专用 Player 绘制。谱面在 CXC 内的
assets/charts/practice.cuexis.chart.json；桌面目录提供相同 source 文件便于检查。

当前 Gameplay v5 candidate author 编译入口的 CanonicalComponent 只有静态 Transform /
Renderable / Camera，不能直接承载 v4 Behavior。因此本包明确作为 Chart v4 呈现预览，
不含 Gameplay 判定，不伪称接入 B5 实体输入或新时钟。PCM 为 32 秒提示音 + 3 秒静音；
实际 EOF 是 35 秒，不把 32.5 秒静音区音符显示当成 B5 EOF tail 通过。

实际未改 Player/engine/core，A 批 95 项源码 SHA256 逐项仍一致，只新增生成脚本。
Player 默认 CXC/Audio 路径实际 prepare、设备打开和绘制通过，日志确认 objects=40、
behaviors=21；actual window 截图检查轨道、字形、下落音符与 Hold 通过。首轮发现作者
浮点算术产生超预算 Beat 分母和 camera 不支持字段，修正作者数据后加载通过；又调整
出现窗口和 Hold 缩放以避免标题区域遮挡。运行后正常关闭，无残留 Player。
独立 ZIP/hash/资源引用/实体数/PCM 静音尾部检查、两次生成包 SHA256 一致通过，摘要
b837afe0f5cac9699aeffcd5d8bf9a4c7b6ef317b791d75dc044dd645bdc27c3。

产物在 out/entity-practice；更新现有桌面 Cuexis-B5-DevicePractice-2026-10-13 的
practice.cxc、source、README、启动脚本及预览图，旧临时 guide 材料归档为
previous-guide-attempt-20261013，旧 2026-10-09 独立目录未改。此为实体呈现数据验证，
B5 Gameplay 判定、物理 DFJK/Hold、真实听音及 EOF tail 验收仍未完成。

## v5 实体测试与作者指南更新

owner 明确要求将测试谱面改为 v5，并更新原 v4 作者指南。当前包不是将 v4 version 字段替换：
完整 practice.v5.json 使用已注册的 v5 inline author dialect，40 个 generated identity
实体含 Transform/Renderable/整数 alpha；离线 fixture helper 复用既有 author reader /
typed lowering、candidate assembly、Packed Writer/Reader。输出 compiled/chart.packed
承载 v5 Canonical Semantic Chart，真实启动明确选择该 entry；空 v4 fallback 仅满足旧
CXC 默认 Project closure，不是测试内容。作者 JSON 在包旁，当前容器不允许任意追加
未列入受支持闭包的作者文件；追加尝试被 cxc.entry.unlisted 拒绝，没有绕过校验。

原 v4 动画版本归档在桌面 previous-v4-entity-preview-20261013。当前 v5 只有静态布局；
窗口标为 STATIC V5。不把缺少动态 CanonicalComponent 的现有路径伪装成 v5 Behavior /
Animator 已实现，也不恢复 Player guide。当前无 Gameplay Requirement、无 B5 判定；
旧 v4 Behavior 声明保留为迁移证据，不写进当前可执行 v5 Packed 段。

验证：40 实体 / 14 资源、完整 typed semantic preimage round-trip、包 manifest/hash /
entry计数/35秒PCM、三项负例（未知 Behavior、fractional alpha、重复 identity）拒绝且不
发布、重复生成包身份一致；实际 Player 明确 entry 加载与截图通过，日志记录 objects=40、
behaviors=0、opaque draws=40，实际 SDL Audio 打开，正常关闭且无残留进程。
最终 CXC SHA256 为 1e3f4d757dfd7d8a25ff4d9fcd2ef917941fa098afd7c5ffc70b18d98e9565f5。
证据在 out/entity-v5；桌面包、v5 作者源、实体索引、启动脚本和预览已同步。

现行 [v5 作者指南](../../../../guides/CHART_V5_AUTHORING.md)区分当前候选编写形态、
已选择的动态合同、实际实现限制与迁移要求；原 v4 指南全文归档，旧路径继续兼容。
导航和 V5-F 文档边界按 owner 此次授权更新，不据指南或静态包宣称 V5-F / Stage 8 退出。
本轮新增 tools/compile_entity_practice.cpp、Windows 构建 driver 并更新生成脚本；
95 项 A 批源码逐项 SHA256 仍一致，Player/engine/core 未改。未提交或运行新的 hosted。

工作树未提交，Stage 7 保持 active。当前状态由 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 单独维护。


## v5 Behavior 呈现修复与桌面入口复验

owner 指出新窗口仍显示 STATIC V5、音符堆叠与 Hold 不动。原因是此前交付只生成了静态
Packed 实体，后续代码编译完成时又未同步桌面包/入口。此次增加独立注册的
candidate.behavior-v2-v1 source-json 路径，保持原静态 Packed Behavior 拒绝门禁。
Chart 编译器校验 version=5、Behavior event version=2、整数 alpha，并复用既有 typed
Object/Behavior validation 与 Runtime evaluator。CXC factory 编译一次、核对精确 bytes
与独立语义身份和实体计数、持有 owning runtime；Playback prepare 不重新解析源。
能力预检新增 Behavior v2，显隐要求现有 visibility capability。参数/Template/Clip/
Animator/CXT import 不在此 profile 范围，禁止以未知字段静默替代。

生成器现在将40个对象与21个Behavior写入 compiled/chart.v5.json：短音符下降及显隐、
Hold体段位置/伸缩、尾段下降及结束隐藏。文字/轨道仍为实体资源；Player仅把通用调试坐标轴
改为显式 --debug-axes，默认关闭，没有谱面专用绘制。当前零 Gameplay Requirement，
未接入B5判定；动态呈现通过不证明物理DFJK/Hold、听音/同步、真实EOF tail或预算接受。
本轮有通用 chart/CXC/Playback 及 Player 调试选项源码变更，旧95项A源码一致只属于修复前证据。

实际 SDL Audio / OpenGL 窗口截图对比确认同一D音符下降、Hold长体与缩短后的体段，
17秒后Hold消失。证据为 out/entity-v5/dynamic 的 preview-note-upper/lower、
preview-hold-growing/shrinking/gone、qa-stdout/qa-stderr；没有合成按键判定验收。
MSVC相关目标构建通过；完整受影响测试可执行文件：Chart 227、Playback 154、
Player control 34、execution host 24，共439项全部通过，架构CTest额外1项通过。
新测试覆盖source拥有权、随时间与discontinuity求值、缺失能力拒绝、语义身份变更、
旧Behavior版本/小数alpha/未知属性/畸形Renderable/Animator拒绝与对象预算。
格式检查、git diff --check及文档检查通过；不将此受影响矩阵称为仓库无过滤全量。
外部 Python jsonschema 校验未执行：环境未安装该库；入口已通过实际 CXC factory/
Playback 校验与仓库文档检查，未以外部Schema检查代替运行证据。

桌面包及脚本实际同步，旧静态产物归档 previous-static-v5-20261013；设置/日志保留，
2026-10-09原目录未改。使用桌面 start_test.ps1 复验的 command.json 选中
compiled/chart.v5.json，日志 objects=40、behaviors=21、resources=13，正常有向关闭；
证据目录 runs/20261013-205750-524。重复生成包SHA一致，最终CXC SHA256：
221d4e20b077b410d0d3f215fc6df87c6060a798b9e4b5e3204d3d4a458f14bf。
新源码快照 out/entity-v5/dynamic/source-manifest.json，digest 5141d800c782d7f992ddc52b05f56936847974f5aa4a00d2fa89590266944b7f。
作者指南、entry profile文档和Schema、CURRENT_STATUS已同步；工作树未提交，
未运行新的hosted矩阵，B5/Stage 8/V5-F仍未退出。


## R 从零重播修正与复验

owner 报告暂停正常，按 R 不重置随后窗口无响应。确定的输入映射缺陷是 R 使用
KeepChartTime，与测试谱面 R RESTART 提示不符。本次将 Player 输入入口的 Reload
策略改为 RestartAtZero；显式控制 API 的 KeepChartTime 继续保留。暂停时 R 回零并
保留暂停，Space 继续。增加重启后首帧阶段日志，用于区分音频/timeline、Runtime、
extractFrame、renderer submit/present 阻塞，正常帧不重复记录这些日志。

修正前自动测试播放中、暂停后、较晚时刻与有/无 FPS 共27次重载，未复现整个窗口无响应。
这不是对 owner 现象的否认，也不是窗口死锁已定位的证据。修正后 MSVC相关目标构建
及34项Player控制测试通过；实际暂停后 R 的帧记录 chartTimeMs=0，三个连续 R 的
首帧均完成并可响应窗口消息。证据在 out/restart-investigation 的 paused/repeated
帧记录、日志与 player-tests.log。抓栈工具已验证可读取本地PDB，但没有捕获旧版卡死栈。

owner 使用更新后二进制复验，明确回复“新版 R 已正常”。对应桌面运行
runs/20261013-213950-859：两次 R 后 renderer present/首帧完成，后续 Space 输入
正常处理并有正常关闭记录。R问题的修正与当前复验成立，不将旧版无响应原因写成
已证实的音频/渲染死锁；不把此局部操作复验当作 B5判定、实体键盘或整体阶段退出。

## 2026-10-14 Player 清理与 v5 实体谱面判定接入

按 owner 指令移除 `player_gameplay_guide.cpp/.hpp`、guide 解析/数据、固定 DFJK 检查、
`appendPlayerGuide` 绘制分支、旧 `--gameplay-guide` 参数及专用窗口启动器。
旧参数按未知参数拒绝；离线 S7A-7/8 生成器不再输出 guide，检查器直接核对作者字段与
FactBinding。它仍是历史逻辑步夹具，不冒充当前 B5 实体交互谱面。

清理前后清单位于 `out/content-cleanup-20261014/`：首批 47 项 / 298 文件 / 83,696,718 bytes；
当前判定包发布时另清理桌面的旧解包副本、旧 verification 与 5 个旧动态预览，并写入
`desktop-final-inventory.json`。保留 settings/runs、构建、CPU/设备/重启诊断及失败证据，
不修改 `Cuexis-S7A78-DeviceAcceptance-2026-10-09`。清理是 owner 授权的具体产物范围，
不是回退 B3/B4/B5、A 批成果或删除其他未提交源码。

当前桌面目录为 `Cuexis-B5-DevicePractice-2026-10-13`。`practice.cxc` 同时携带
`compiled/chart.v5.json` 动态 source entry 和 `compiled/gameplay.packed` Gameplay V2 capsule。
前者 104 Object / 22 Behavior / 37 资源；后者作者模型包含这些显式实体和 19 个逻辑生成实体，
持有 18 Tap + 1 Hold 的 19 Requirement、21 阶段和 42 FactBinding。
视觉音符头和 Requirement owner 分开记录，不能把视觉 UUID 当作逻辑生成身份。
独立作者源、配置、course、实体索引及完整可编辑 Object/Behavior 源放在包旁；不在 Playback
运行时执行生成脚本或解析 course。包旁 Packed 与包内载荷逐字节一致。

Player 通过显式 `--gameplay-packed` 加载 owning GameplayContent，并使用已有
`prepareGameplayLoad` 附加到动态 source。此参数只在 execution test-only 模式使用；
配置选择 `gameplay.execution.t4-k4.rpa.v1` / `fact.semantic.rpa.v1` / `late.window.batch.v1`
和显式能力、预算、校准身份、键位与 tail 输入截止。未写入布局、UUID、DFJK 映射或音符时刻。
右侧 Hit/Miss 徽章由谱面 Behavior 初始隐藏，真正的阶段结果通过 FactBinding 显示；分数日志
来自 worker 的真实只读查询。FPS 仍是通用可选 `--show-fps`，默认关闭。

接入验证发现并修正四个通用问题：

- 输入泵在等待 owner 回复时使用协调器的时钟，会推进 publication cursor，随后旧观察被拒绝；
  捕获改用已有的独立 clock replica。
- Audio service 给不变的量化帧重新标记轮询时间，可能使外推 H 倒退。SDL 回调现在同时发布
  帧计数和单调时间，采用有限重试的 seqlock 读取，由实际回调锚定名义设备速率映射。
  映射只向更早的已观察锚点细化，避免未来样本和延迟回调导致回退；缓存观察不刷新时间。
  误差包括实际回调与映射的差值，超过显式 20 ms test-only 上界就拒绝，并未放宽检查。
- 有限音频源携带真实末帧，Playing 外推不越过源 EOF；EOF 后仍须显式 tail。
  帧 ledger 允许其他身份不变的向前 clock-only scope 切换，保留旧 scope 证据与拒绝门禁。
- 新载入后立即暂停（包含暂停后 R）缺少可冻结 Runtime 帧；现在在接收控制前完成首个
  advance/sample/present。暂停后 R 保留暂停，Space 恢复，播放中 R 保持播放。

验证证据：

| 项目 | 结果与证据 |
| --- | --- |
| 真 assembler / prepare | 19 Requirement、21 phase、42 binding，`actualPrepareValidated=true`；`out/entity-v5/judgement/gameplay-assemble.log` |
| 离线真实 RPA 与完整 Replay | 全命中 46 / 21 / 0；零输入 −21 / 0 / 21；Hold 提前松开 36 / 19 / 2；三组 Replay 相同，`gameplay-verification.log` |
| 实际 SDL/OpenGL 窗口 | 真实音频设备、Windows 注入 DFJK、score=46 / hits=21 / misses=0、绿色反馈实体及 EOF tail 完成；`window-current-hit.log` 与 `feedback-current-hit.png` |
| 控制 | Pause、暂停后 R、4 秒冻结、Resume、播放中 R、有向关闭通过，`window-controls.log`；暂停重载停在新载入的初始实际 cut，并非旧进度 |
| 相关单元回归 | Player 34、Execution host 26、协议 33、Chart 227、Playback 154、AudioSDL 19，共 493 项；`out/entity-v5/final-*-tests.log` |
| 架构 / 文档 | architecture CTest 通过；434 Markdown / 20 candidate JSON/CXT 与 diff whitespace 检查通过 |
| 历史离线夹具 | 清理后的生成器/检查器重新生成并通过 ZIP/hash/PCM、4 组 Host golden/Replay、2 组音频/ChartClock 拒绝；`out/device-acceptance-cleanup-check/` |
| 包确定性 | 两次生成 CXC SHA256 相同：`4d4a6bfd70bd288f3a6bc9a31fdac912b8d88c200847ee6a7eabcea705fb0bd3` |

失败的 PostMessage 输入没有生成 SDL 按键，不能算输入通过；此前真实 keybd_event 注入暴露的
clock 错误、暂停重载首帧错误和修复前截图保留在同一 evidence 目录。最终注入是自动窗口验证，
不是实体键盘手感或听音验收；也未运行新 hosted/full-release 矩阵或接受生产参数。
当前测试谱面已接入 B5 判定，B5、V5-F、Stage 8 和整体 Stage 7 均未退出。
