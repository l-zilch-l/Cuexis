# R3 setup 波动与首次 Seek 的轻量探针归因

状态：diagnostic；定位热点，未完成两个历史现象的唯一根因裁定；R3 未退出

日期：2026-10-28，Asia/Shanghai

## 1. 结论与适用范围

本轮按 owner 要求调查[准备复制消除](2026-10-28-r3-resolved-prepare-copy.md)中
1K setup 极差/中位数超过 15%、40K 首次 Seek 中位数上升两个现象。保留该候选，未修改产品或测试源码。
唯一树仍为 `D:/Cuexis-redesign` / `Cuexis-redesign`，HEAD `b4b7746`，入口的未提交工作全部保留。

**40K 的主成本已定位到恢复后重新投影 Fact，而非内核恢复、Runtime Transform 或 GPU。**
原两映像差值在 `GameplayPublicationAccess::publish` 中复现；继续拆分后，昂贵部分是
`PresentationProjectionAdapter::project`。本用例实际为 40K requirements，投影 source map 为空，
却仍读取 40,000 条 Fact、构造 40,000 个拥有型 `ProjectionSourceKey`，最终匹配行为零。
新 leaf 样本首次投影的循环段为 14.126–18.426 ms，与整个投影段近乎重合。

**尚不能把原约 11% 上升裁定为消除 prepare 复制导致的算法退化。**
同一映像切换原两次复制与当前借用时，差距大幅缩小且配对方向不稳定；同一映像、同一借用模式的 A/A
也出现了高点。代码布局、分配/缓存历史及运行态敏感性仍需进一步区分，不能据此断言某种代码对齐或特定后台程序就是根因。

**1K 原始 164.212 ms 高点没有足够历史计数可追溯。** 新组没有稳定复现该高点，
同代码 A/A 的 1K setup 仍有 124.468–137.495 ms 差异。新探针能定位新样本的慢段，不能重建旧进程的 CPU、
调度、缓存和缺页现场；原 16.72% 及不满足原门的结论保留，不宣布问题已解决。

## 2. 方法、扰动与固定组

证据根为 `out/verification-20261028/r3-seek-noise-probe-v1/`。没有覆盖、追加或重新解释旧性能组。
所有构建串行、BelowNormal、affinity mask 3；采样串行，build 与产品进程不重叠。

使用当前 SDK 0.9.1 的 Release STATIC / metrics OFF 对象和库。探针仅写在 `out` 中的源码副本与
独立链接产物；普通 build tree 的源文件、对象和 exe 不修改。两个审计子代理及一次独立探针解释审阅均为
无上下文、Max `gpt-6-luna`，未运行构建或采样；root 亲自构建、采集、读取原始事件并复算。

- PD metrics 与 RPA observation 宏都保持 OFF，避免大型线程存储、校准及额外 worker 往返改变冷路径。
- 独立探针固定缓冲约 272 KiB，在计时前触碰；记录 QPC、当前执行线程 cycles / CPU 时间，
  选择性记录进程 CPU、进程级 PageFaultCount、private commit 和 working set。完成后统一导出。
- 每进程 32 次空重型 scope 校准均值为 2.62–3.04 us。此校准不证明所有嵌套探针的总扰动为零；
  不把诊断映像点值冒充普通产品性能。
- 观测到 Windows CPU 时间以 15.625 ms 台阶变化，不能用它精确解释一次约 15 ms Seek 的微秒级等待差值。
  cycles 与粗阶段 CPU 仅作诊断；PageFaultCount 同时包含软/硬缺页，且属于整个进程，不能直接判 disk-cold、
  硬盘 I/O、单线程缺页或分域内存预算。
- 子进程实际读取 affinity=4、BelowNormal、execution-speed throttling disabled；记录 OS、处理器、AC 状态和
  平衡电源计划。固定真实 Packed revision 4、33 records、owning source、CPU test renderer、无音频，正常 allocator。
- 每规模固定 AB / BA / AB；两轮组的 round 0 是 observer 初始化后的新进程首个 fixture，round 1 是该进程内
  销毁第一轮后新建 fixture/Host 的对照。不能称为 disk-cold，也不将两轮池化成独立进程样本。

| 组 | 固定计划与用途 | 结果 |
| --- | --- | --- |
| `phase-v1` | 1K / 40K，各三对、每进程两轮；两种准备实现、两个映像，定位 setup / Seek 大段 | 12/12 有效进程 |
| `same-image-switch-v2` | 同规模、顺序和轮次；一个 exe，通过 copy / borrow 模式切换原两次复制与当前实现 | 12/12 有效进程 |
| `same-image-aa-v2` | 1K / 40K，各三对、每进程一轮；两标签均为同一 exe、同一 borrow 模式 | 12/12 有效进程 |
| `projection-leaf-v3` | 原计划 40K 三对；叶子计数首次导出未执行 | 第一个进程功能断言通过，collector 判无效，立即停止；未补样 |
| `projection-leaf-v4` | 修正导出方式后的新组；40K 三对、每进程一轮，同一映像 copy / borrow | 6/6 有效进程 |

共 42 个有效诊断进程，198 次 Seek / CPU test present；另有一个 v3 无效观察进程原样保留。
全部有效组均执行真实性能用例的成功、计分和 Seek 完整结果一致性断言；不是只检查进程退出码。
v3 的未使用 inline 导出对象没有触发退出导出，v4 显式注册退出回调；两版源码、二进制、登记及失败日志各自保留，
不把无效组时间混入有效组。更早一次 builder 的重复锚点断言失败发生于生成探针阶段，没有产品样本。

## 3. 40K：发布成本与模式干预

以下均取 round 0、attempt 0，每侧三个进程，不池化重复请求。

| 组 | 基线标签首请求→present 中位数 | 候选标签首请求→present 中位数 | 本次比较的含义 |
| --- | ---: | ---: | --- |
| 原普通组 | 14.761 ms | 16.417 ms | 两个原普通 exe 的描述性差值约 +11.22% |
| phase-v1 | 15.013 ms | 16.371 ms | 两个轻量诊断 exe 中差值仍存在 |
| same-image-switch-v2 | 15.165 ms | 15.359 ms | 同 exe 的 copy / borrow 差值约 +1.28%，三对方向不一致 |
| same-image-aa-v2 | 15.147 ms | 14.981 ms | 同 exe、同 borrow，两标签中位数约 -1.10% |
| projection-leaf-v4 | 15.204 ms | 15.222 ms | 新叶子探针下 copy / borrow 的描述性中位数接近 |

phase-v1 的首次阶段中位数：

| 阶段 | 基线 | 候选 |
| --- | ---: | ---: |
| Playback fork（含内核验证/恢复） | 0.215 ms | 0.195 ms |
| native state copy | 0.021 ms | 0.020 ms |
| native fault reserve | 0.015 ms | 0.014 ms |
| suffix | 0.142 ms | 0.125 ms |
| Playback publish | 14.292 ms | 15.699 ms |
| publish 所在线程 cycles 中位数 | 54.008 M | 58.702 M |

两侧每次均经过 `kernel.native.*`，随后为 `kernel.suffix.from32`；不存在一侧退回原点或多执行大量历史记录的证据。
上表包含嵌套 scope，不能相加作为互斥总量。Host worker 在一次 Host 生命周期内复用，per-seek 新建的是
Judgement 候选，而非整个 worker。

同映像干预在一个实现文件中保留旧 helper：copy 路径仍从原 HEAD 提取旧 `prepareStorage`，
并构造 `GameplayPrepareRequest{request.assembly,...}`，因此保留两次深复制；borrow 路径使用当前候选。
MAP 确认解析入口来自 `gameplay_prepare-switch.obj`，没有拉入库中的旧 prepare 对象。
这验证运行时复制语义，不是复现旧映像机器码布局。

**慢值完整保留。** A/A 同算法一侧的首请求出现 19.394 ms；switch-v2 的 borrow 路径 round 0 / attempt 2
出现 20.591 ms，其中 `publication.project` 为 19.9995 ms、75.109 M cycles，进程 fault delta 为 0，
Runtime update 仅 0.0293 ms。不能把所有诊断点写成低于 20 ms，也不能只用中位数宣布预算通过。
该例说明慢点可发生在重复请求的 Fact 投影计算中，不要求首次缺页或更多后缀重放；没有证明具体 allocator/cache/调度根因。

## 4. 叶子探针确认的具体工作

执行位置：`engine/playback/src/presentation_projection.cpp` 的 `PresentationProjectionAdapter::project`。
恢复以 fresh projection scope 开始，cursor 从 0 消费到 publishableCursor。

v4 六个进程的首次 Seek 均观测到：

| 计数 | 每个进程 |
| --- | ---: |
| `prepared.sources.size()` | 0 |
| 读取 Fact | 40,000 |
| 构造 `ProjectionSourceKey` | 40,000 |
| matched rows | 0 |
| RDTSCP 叶子样本 | 40（ordinal 每 1024 项抽一项） |

读取路径 `PublicationRead::fact` 返回带借用 `RequirementIdentity*` 的小型值，不是每条 Fact 的身份深复制。
它仍逐项检查 owner、read epoch 与 cursor。之后 `sourceKey` 复制五段字符串及 emission path，
构造拥有型 tuple，即使 source map 为空也先构造 key，再查找空表，最后释放临时字符串/vector。
这里是下一步可以评估的具体重复工作；不能误报成 R2 的全量 Snapshot 物化或建议重写 R2 表示。

六个首次循环段分别为 16.492 / 14.341 / 18.426 / 14.126 / 14.432 / 14.198 ms，
对应 `publication.project` 为 16.529 / 14.364 / 18.460 / 14.151 / 14.454 / 14.222 ms。
Runtime、最终提交和 sample 后的增量投影均远小于该循环。

叶子采样区分 readFact、key 构造、lookup 及行尾/临时值释放，样本未发生处理器迁移；
原始 `*_tsc` 和空时间戳开销保留。固定 stride 会与共享页边界对齐，且计时本身有开销，
因此不把抽样占比外推为整个 40K 循环的精确时间百分比。

后续候选可优先评估在 source map 为空时避免临时 owning key 的构造，但仍必须保持逐 Fact 的原有
合法性检查、错误优先序、cursor / active / future 状态、scope 生命周期及完整发布事务。
本轮没有加入该优化或删掉校验；不能从零绑定 fixture 推广到所有真实绑定布局。

## 5. 1K：能确认的波动与不能追溯的旧现场

原基线三值仍为 164.212 / 140.457 / 142.066 ms，原 `(max-min)/median=16.72%` 不变。
第一进程恰好是高点只支持提出启动/次序假设，不足以证明 disk cache、CPU 唤醒或某个后台程序。

phase-v1 的 baseline round 0 是 142.294 / 148.774 / 133.778 ms。最大与最小进程中，
`content.from_packed` 为 81.506 / 66.285 ms，执行线程 cycles 为 305.729 / 248.095 M；
source prepare 为 30.079 / 29.552 ms。新组的这次主要差异落在 Packed content 创建段，且伴随 cycles 增长，
不是只有 Host worker 启动等待增长。但不能把该新样本归因回旧 164.212 ms。

A/A 同一 borrow 实现六值为 127.591 / 137.495 / 134.500 / 126.581 / 135.018 / 124.468 ms。
两标签中位数 127.591 / 134.500 ms，相差约 5.42%，尽管二进制和算法相同。
其慢段也不完全一致：有的主要在 fromPacked，有的在 setup 的其他区段。
同进程第二轮也没有一致变快，因此“全部由进程第一次初始化造成”没有得到支持。

这些组为不同诊断问题分别预登记，不是追加旧样本到通过；均不承担原正式 15% / 置信区间门。
原 1K 超门仍保留，1K 唯一根因及原 40K 两映像固定差值的底层机制均未裁定。

## 6. 证据、修正与后续边界

根目录 `out/verification-20261028/r3-seek-noise-probe-v1/`：

| 文件 / 目录 | 内容 |
| --- | --- |
| `entry.patch`、`entry-prepare-copy-report.md` | 本轮开始前的 tracked 差异及原报告原文 |
| `light_probe.hpp`、`build_probes.py`、`sources/`、`objects/`、`build-commands*.json`、`build-manifest.json` | 轻量大段探针、具体编译/链接命令、源码副本及对象绑定 |
| `deep-build-v2/`、`build_deep.py`、`run_switch.py`、`run_aa.py` | 同映像 copy/borrow 与 A/A、MAP、实际链接对象及哈希 |
| `leaf_probe*.hpp`、`leaf-build-v3/`、`leaf-build-v4/`、`build_leaf*.py`、`run_leaf*.py` | 叶子探针的失效与修正版；两版各自保留 |
| 各组 `registration.json`、`results.json`、原始 `.log` | 全部计划、环境、readback、逐次结果、失败/停止及完整原始事件 |
| `analyze.py`、`analysis.json` | 独立日志哈希复核、QPC 包含关系、阶段中位数、工作计数和源码不变核验 |
| `final-manifest.json` | 最终文档及诊断证据绑定 |

同一 attempt 在 Seek 和后续 sample 各有一次 `publication.project`；分析依据 QPC 包含关系分配到
`request.seek` / `request.sample`，没有把约 2–7 us 的 sample 投影池化进 Seek 投影中位数。

原准备复制报告中全部九次 candidate request-to-present 中位数曾误写为 14.845 ms，
本轮按原 `independent-results-audit.json` 修正为 14.885 ms；首次 16.417 ms 和原始数据不变。
原报告原文快照保留，不更改旧原始登记、样本或接受结论。

本轮产品和测试源码与前轮已验证候选逐文件 SHA-256 一致。没有新产品修复、公开 API / wire 变化、
正式性能接受、设备/生产门或内存预算裁定；诊断峰值不改变原门限，R3/R4 均未退出。
