# R3 零绑定投影优化与通用匹配撤回

状态：本地候选核验；保留零绑定修复，撤回通用匹配试验；R3 未退出

日期：2026-10-29

返回 [RPA 报告索引](../README.md)。当前接受状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)，前置归因见
[setup 与首次 Seek 轻量归因](2026-10-28-r3-seek-noise-attribution.md)。

## 1. 决策与恢复点

owner 要求按第一性原理多方向探索、先提交留档、达不到预期则更换方向。
本轮已先提交 `2706bb32df40c14ccff9d8b1e4143cdca33cbb9e`，保留已验证的 prepare
复制消除、其所有权回归和前轮归因。唯一实施树是 `D:/Cuexis-redesign`，分支
`Cuexis-redesign`；没有切换其他工作树、推送、合并或发行。

当前真正需要解决的是：没有任何 presentation source binding 时，为什么仍为每个已验证 Fact
复制五个身份字符串和 emission path？prepared source 索引为空，匹配集合必为空，
但 PublicationRead、Fact 内容校验、游标推进、已有状态清理和事务发布仍有职责。

| 方向 | 实现与证据 | 处置 |
| --- | --- | --- |
| A 原投影 | 恢复点上的 owning key 和匹配排序 | 固定普通对照基线 |
| B 零绑定 | 每个 Fact 完整校验后，若 sources 为空，跳过 key 构造和查询 | 保留，40K 首次请求到 CPU 测试呈现中位数下降 84.5% |
| C 通用借用 key / 有序合并 | prepared 索引仍拥有 key；查询同步借用字符串/path；合并 Any 和 timing 行，保留重复及次序 | 微基准改善，但 10K Host 首次延迟三对均上升，撤回并保留源副本 |
| D 批量免重复读校验 | 需要重新证明 epoch、重入、会话销毁与借用生命周期合同 | 本轮不实施；不是已测得无收益 |
| E prepare pattern 复用 | 审阅发现可考虑调用期成功编译结果复用，但深相等成本、预算和第三次 assembly 编译仍需单独调查 | 仅探索结论，未实现、未测量 |

C 没有以微基准收益替代端到端守门，也没有追加样本试图翻转结论。
本轮最终产品差异仅为 B 的两行分支及语义回归；不引入公共 API、依赖、跨调用缓存或新生命周期。

## 2. 最小修复保持的合同

分支位于 `PresentationProjectionAdapter::project()` 的 phaseOutcome 类型、requirement、phase、
outcome 存在性及枚举合法性检查之后。stray / consumeEmpty 仍沿原路径处理。
每个 ordinal 仍调用实际 PublicationRead，保留 weak owner、线程、epoch 和边界检查。
错误仍在原顺序返回；candidate 构造与提交方式、源状态不变保证不变。

这不是函数级提前返回：cursor 仍逐项推进，循环之后的 future / active、group、target、diagnostics、
lastTick 与写入生成仍执行。空索引不授权跳过 publication prefix / scope / Tick 检查，
也不授权信任任意 Fact。非空索引继续使用原 owning key 与排序算法。

新回归覆盖零绑定的有效前缀和六类非法 Fact 与有绑定路径的同首错，验证失败不修改输入状态；
另保留通用试验补充的完整路径谓词、Any / timing 的规范次序和重复行、嵌入 NUL / UTF-8 /
长字符串以及 UINT64_MAX repeat index 对照。这些测试约束行为，不要求采用撤回的算法。

## 3. 固定普通 Host 对照：A 与 B

证据根为 `out/verification-20261028/projection-matching-v1/`。
`performance-registration.json` 在运行前固定 1K / 10K / 40K，每规模三对独立进程，顺序
AB / BA / AB。18 个进程全部有效，每进程三次请求，共 54 次；不替换、不追加。
SDK 0.9.1、Release STATIC、普通分配器、PD / RPA 观测关闭，Packed 完整入口、33 条历史、
source identity、CPU test renderer、无音频。每个测试进程在放行前核验 affinity 4、BelowNormal、
HighQoS（执行节流关闭）；没有同时构建或采另一组性能。

| requirements | A 首次值（µs） | B 首次值（µs） | 中位数变化 |
| --- | --- | --- | --- |
| 1K | 756 / 697 / 731 | 518 / 388 / 454 | 731 → 454，下降 37.9% |
| 10K | 4378 / 4169 / 3889 | 855 / 1030 / 858 | 4169 → 858，下降 79.4% |
| 40K | 16531 / 16899 / 15970 | 2562 / 3418 / 2245 | 16531 → 2562，下降 84.5% |

九对首次请求全部改善，满足本轮预设的 40K 至少下降 30% 及小规模改善目标。
40K 全部九次请求最大值由 21261 µs 降为 3418 µs，原慢点仍保留。
setup 中位数变化仅为 -1.2% / +0.7% / -0.6%，不是本修复收益来源。
原始记录为 `performance-results.json`，首次与所有请求见 `empty-seek-summary.json`；
`performance-summary.json` 单独汇总 setup，不能当成 Seek 数据。

B 的 40K 首次极差 / 中位数约 45.8%，相对重复性仍超过 15%。本组证明有限 CPU 宿主场景的
局部收益，不证明正式重复性、所有 presentation 布局、设备、音频、内存或 R3/R4 接受。
前轮 1K 历史高点、首次 Seek 原约 11% 差值的唯一运行态机制仍未裁定，不能改写旧窗口。

## 4. 通用方案：微基准通过、Host 守门撤回

C 在独立 `general-static` 目录构建。`general-binding.json` / `general-bin/` 保存二进制绑定，
`general-presentation_projection.cpp` / `.hpp` 和 `general-projection_adapter_tests.cpp`
保存撤回前源字节；最终树恢复 B，不修改这些归档。

独立 frozen oracle 从 `2706bb3` 读取旧投影主体，仅改 include / 类名，并使用当前私有数据布局。
它不是旧二进制。217 assertions / 1 case 逐项对照完整投影状态、writes、诊断、回调顺序、错误与输入不变性。
微基准同一 exe、同一个 prepared 索引，对照 frozen 和 C；每次进程五次调用均值，
三对独立进程、两个规模、三种布局，共 36 个有效进程。Fact reader 为合成借用回调，
没有真实 PublicationRead 校验，因此不能与 Host 延迟直接混算。

| 布局 | 1K 中位数（µs），frozen → C | 40K 中位数（µs），frozen → C |
| --- | --- | --- |
| 空索引 | 76.58 → 6.12 | 4132.70 → 233.62 |
| 有绑定但不匹配 | 205.58 → 119.22（下降 42.0%） | 15343.80 → 10966.86（下降 28.5%） |
| 全匹配 | 660.40 → 546.46（下降 17.3%） | 47013.66 → 40712.56（下降 13.4%） |

18 对均改善，达到预设 unmatched 至少下降 20%、matched 不回退目标；部分单元重复性仍超 15%。
全匹配 adapter-only 仍约 40.7 ms，不是所有布局满足 20 ms 的证据。
登记、全部值和摘要见 `adapter-comparison-v1/`；oracle helper 位于
`out/verification-20261028/projection-oracle-agent/` 和 `oracle-build/`。

随后另建 `general-host-comparison-v1/`，不复用或覆盖 A/B 登记：B 对 C，固定同样的 18 个进程和 54 次请求。
其端到端首次请求到 CPU 测试呈现如下：

| requirements | B 首次值（µs） | C 首次值（µs） | 中位数变化 |
| --- | --- | --- | --- |
| 1K | 425 / 377 / 629 | 436 / 364 / 344 | 425 → 364，下降 14.4% |
| 10K | 854 / 875 / 914 | 1418 / 1021 / 968 | 875 → 1021，上升 16.7% |
| 40K | 3111 / 2537 / 2437 | 2462 / 2398 / 2273 | 2537 → 2398，下降 5.5% |

10K 三对均上升，C 首次极差 / 中位数约 44.1%。本组没有证明具体硬件或代码布局原因，
也没有证明所有非空布局端到端回退；但不足以支持 C 无回退的保留决定，故撤回。
终止尚在进行的 C SHARED 构建并保留日志，最终 B 单独 clean 构建和回归。

## 5. 验证与实验完整性

最终 B 的验证如下，日志均位于证据根的 `final-*`：

| 配置 | Playback | Host | 定向门 |
| --- | --- | --- | --- |
| Release STATIC（在已验证的 B 构建目录增量编译新增测试） | 184 cases / 34747 assertions 通过 | 61 cases / 42122 assertions 通过 | architecture 通过 |
| Release SHARED（clean，166 步） | 176 cases 中 175 通过、1 跳过；28220 assertions 通过 | 61 cases / 42132 assertions 通过 | architecture、shared export surface 均通过 |

SHARED 私有测试编排与 STATIC 不同，不能把 STATIC 的 184 项移记到 SHARED。
Host assertions 随异步观察次数变化。C 撤回前的 STATIC 回归另存为 `general-*`，不冒充最终 B 验证。
最终 engine / app / cmake 源字节与被测 B 的 `candidate-binding.json` 逐项相符，私有头已恢复原字节；
新增测试没有改运行路径。本轮未重跑全量矩阵、安装消费者、设备测试或正式性能接受，公共 API 未改变。
文档检查、修改 C++ 的 clang-format 检查及 `git diff --check` 通过。
最终 B 另行链接 frozen oracle，217 assertions / 1 case 通过，见 `final-b-oracle-build/`；
首次 helper 启动因既有构建目录未导出 compile_commands 而停止，开启导出后另存 attempt2 日志，未运行性能。
最终源副本、各配置源 / 库 / 二进制及日志摘要封存在 `final-manifest.json`。

实验中的失败保留且不计作成功：B 首次测试编译因 Tick 可选值比较失败，
误启动的旧 exe 测试及快照均隔离为 `incomplete-*`，修复构建后才生成有效 B 绑定；
C 首次 configure 的未整体引用 suffix 参数失败、oracle helper 首次类型别名编译失败均另存日志，
没有影响正式产品 API。

所有构建均 `--parallel 1`、BelowNormal。C 首轮 STATIC fresh build 发现子编译进程未继承父 affinity 3，
初期实际为 65535；不能声称该轮全程限制在两个逻辑 CPU。后段通过 Windows Job 修正，
后续构建使用 `limit_build.py run` 从 launcher 起强制所有后代 affinity 3 / BelowNormal，
实际进程读回核验。未启动 ETW；Job 不带 kill-on-close。性能进程在独立控制器下使用 affinity 4，
不放进限制 affinity 3 的构建 Job。构建限制修正不授权改判历史测量。

## 6. 剩余范围

非空绑定投影、完整 Packed / prepare 及高 requirements 的其他成本继续跟踪，
pattern 编译复用只是待单独设门的候选。原 15% / 20 ms 门限、自然恢复锚点合同、R3c 进入条件和
R4 正式 / 设备 / 生产接受均不变。本轮仅交付具有直接证据的 B，不宣布 R3 完成。
