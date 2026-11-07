# R2 公开 N1 归因与共用目标文件对照

状态：本机 HighQoS / 共用未变目标文件配置的局部收益及不退化门通过，提交 R2 退出评审核对；未据此记 owner 接受或 R4 正式通过。

日期：2026-10-26（Asia/Shanghai）。当前总状态见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)，
设计与接受边界见[整改计划](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md)。
承接[上一轮 HighQoS 对照](2026-10-26-r2-high-qos-local-benefit.md)。

## 1. 本轮问题与源码范围

用户要求继续 R2。本轮聚焦原 HighQoS 窗口 N1 的宽区间，不增加第四产品机制。
HEAD 仍为 `cccbd47eda8ecd65e018ec31063ec027c39bd04e`，九个主树 R2 输入与冻结 E 哈希一致。
没有改产品、公共 API、正式测试或构建输入；临时诊断全部位于 out/。

现有 public extractFrame 外层只做 owner/reentry/session/viewport/version 检查，
然后以 committed Runtime stamp 打开同步读取窗口，调用物化器；不会每次重新执行 Gameplay publication。
旧六进程 N1 的线程 CPU 成本与 wall 成本接近，thread cycles 也随慢进程上升，
不支持仅以 off-CPU 等待解释。它们不构成硬件频率或具体后台服务的根因证明。

## 2. 固定公开路径消融与一次短块停止

原 `out/r2-n1-public-ablation-20261026/` 预登记三进程消融，登记 SHA-256 为
`50cd17d15d174ca5cebd32eb28a23a7f81a7a6aee9b54a261bcf1d05ff4de9c7`。
首个进程的 outer-only 块仅 77252.7 us，低于原 100000 us 门，按事前规则立即停止；
该组没有有效完成进程，registration/claim/raw/stopped 均保留，部分块不加入后组。

另建 `out/r2-n1-public-ablation-v2-20261026/`，只将 synthetic outer-only 路径工作量固定增加四倍，
从 2097152 至 8388608 次；旧/新完整路径仍为 2097152 次，100 ms 门不变。
新登记 SHA-256 为 `b32f859dad11da432d2178a516ff9dc22efd96b715273a582620f391d5f393fa`。
这次恰好三个进程首次完成，72 块、786 项断言通过，最短块 317.508 ms。

该消融链接同一 E Runtime/Playback 库，提供一个临时物化器分派符号，旧 oracle、新物化器分开编译，
无 LTO。使用原公开 Gameplay N1 configuration/prepare/trace（64-step warmup 与 1024-step validated trace），
同一稳定会话、分开的 reusable owning DTO；八组块轮转旧/新/outer-only，字段与摘要验证在计时外。
每个进程先 30 秒热身、两条完整路径各 64 reads，mask=4、HighQoS、普通 allocator、正常优先级。

| 进程 | 旧路径 ns/read | 新路径 ns/read | synthetic outer-only ns/read | 新路径改善 |
| --- | --- | --- | --- | --- |
| 1 | 294.76 | 267.83 | 38.70 | 9.13% |
| 2 | 273.96 | 248.49 | 38.29 | 9.30% |
| 3 | 269.36 | 245.04 | 38.00 | 9.03% |

outer-only 保留公开前置检查与 Runtime 读取窗口，但故意跳过物化并留下先前填好的 DTO；
它只估计这一固定状态下的消融成本，不是合法 SDK 实现，也不能把两个中位数相减当作精确分层计时。
三次完整公开路径都支持当前物化器更快；该探针有分派与不同链接布局，不能替代原 B/E 接受。

## 3. 公开外层目标码核对

从原冻结 B/E 的 Playback 库各提取 playback_session.cpp.obj 的副本，原库字节不变。
dumpbin 逐函数比较共 12870 个函数；原文本因匿名 namespace 编译单元标记不同而有差异。
只把 B 的 `?A0x135a99c9@` 与 E 的 `?A0x1d505f69@` 映射为同一占位标记后，
12870 个函数的指令、字节、相对偏移及其余符号引用全部相同，其中公开读取相关 17 个函数相同。
raw comparison 与 normalized comparison 均保留，未以原文本逐字相同冒充归一化结果。

这排除了该翻译单元新增外层指令的解释，但没有比较最终链接地址、运行时布局、cache 或物理频率，
不能据此宣布 ASLR、匿名标记、调度或 IDEA 为根因。对象比较在 v2 的 object-comparison/。

## 4. 新共用未变目标文件的独立 B/E 窗口

基于上述构建差异另建 `D:/Cuexis-worktree/out/r2-common-objects-20261026/`。
两边共用一个按原 Release 命令编译的、未修改的 measurement_method_tests.cpp 驱动对象和全部 E 公共库；
仅显式替换各自原归档的 presentation_materializer.cpp.obj、presentation_read.cpp.obj。
B 两个对象来自原 B 库，E 两个来自原 E 库；没有重编产品源码，没有临时分派或 outer-only 逻辑。
原四个产品源/头的身份继续绑定，公共源除这四处本已一致；map 验证公开调用取共同 PlaybackSession、
物化与 Runtime 读取取指定原对象。两侧四规模 fixture 门与原身份完全一致。

登记 SHA-256：`fb6be68714dce2053083a1ed9549df50a02ae760146dea12f2d42e1be8f2bac9`，formal_measurement=false。
恰好 B-E / E-B / B-E 三对读取进程，每个前独立 30 秒热身，共十二 native 进程；
N1/N8/N64/N256、每块 2097152/1048576/262144/65536 reads、每规模八块、100 ms 门、
mask=4、1280×720、HighQoS/正常优先级、普通 allocator 和无产品插桩均保持。
无连续 PDH/ETW 观察器，不声称硬件频率恒定；没有改整机电源或停止用户进程。

六读取与六热身首次 exit=0，192 块均有效，最短 540.342 ms。
主代理从 raw 独立重算八块中位数、三对 log 比与 t(2)=4.302653 的 95% 区间；
全部十二个 QoS 设置、输出、源/产物/传递依赖和对象来源复核通过。
普通 CRT 外无第三方动态 DLL 依赖；本地 PATH 不提供额外产品 DLL。

| E/B | 几何比 | 95% 区间 | 最大块内范围/中位数 | 最大进程间范围/中位数 | 本配置结论 |
| --- | --- | --- | --- | --- | --- |
| N1 | 0.9082 | [0.8567, 0.9627] | 5.39% | 4.49% | 收益确认 |
| N8 | 0.8333 | [0.8177, 0.8492] | 4.72% | 2.01% | 收益确认 |
| N64 | 0.8197 | [0.7772, 0.8646] | 3.79% | 2.75% | 收益确认 |
| N256 | 0.8197 | [0.8057, 0.8339] | 3.46% | 2.73% | 收益确认 |

## 5. 结论与后续边界

本机 HighQoS / 共用未变目标文件配置的局部收益及不退化门通过，提交 R2 退出评审核对。本轮在新身份下检验构建可比性，并保留原独立构建窗口 N1 inconclusive；
没有追加原窗口样本、跨窗口合并、剔除慢进程、频率归一化或改变 15% / 1.10 / 95% 判据。
新构建配置的结果不能单独证明历史波动的物理根因，也不代表所有构建布局/默认系统 QoS 下的性能。

五配置正确性、包/消费者、布局、回滚与汇编边界复用已绑定的第三候选证据，
本轮新增四规模 fixture 和上述诊断产物核验，没有宣称又完整运行五配置或消费者。
R0/Q4 历史残余与独立构建 N1 的复现性限制继续列在退出材料；R4 仍需独立授权及原正式矩阵，
不能与旧 A/Q4 比值拼乘；R3、40k、ETW、设备/生产或 Stage 7 关闭未执行。
