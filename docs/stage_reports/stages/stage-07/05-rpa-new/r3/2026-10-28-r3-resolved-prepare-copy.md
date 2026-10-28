# R3 已冻结输入的 prepare 复制消除

状态：local candidate retained；R3 未退出，R4 正式、设备与生产接受不变

日期：2026-10-28，Asia/Shanghai

## 1. 范围与起点

owner 在阅读桌面接手材料后要求接手 R3 重构优化。本轮只在 `D:/Cuexis-redesign`、
`Cuexis-redesign` 分支实施，起点 `b4b774639cfa5cbee9f22846e110d202b1c299be`，工作树干净。
使用 SDK 0.9.1 的 `out/verification-20261028/sdk-091/static-build/`，不是旧 0.9.0 性能 exe。
所有构建串行、`--parallel 1`、BelowNormal、affinity mask 3；测试、构建和性能进程不重叠。

工作范围为[整改计划](../../../../../stage_plans/active/stage-07/05-rpa-new/plan.md) §8.1
的准备重复工作；没有进入 R3c、正式测量、分域内存采数或设备接受。

## 2. 根因、实现与维护边界

`prepareResolvedGameplay` 原先先用 `request.assembly` 构造临时 `GameplayPrepareRequest`，
随后 `prepareStorage` 的 `auto assembly = request.assembly` 再复制一次。
两次都包含 sources 及嵌套 typed 文档；resolved 分支不运行 grace resolver，因此不需要这两个可变副本。

本轮只修改 `engine/judgement/src/gameplay_prepare.cpp`：

- 私有 `PreparationInputs` 借用原 assembly、grace 输入 spans 和预算，同步调用期间不保留该视图。
- resolved 输入直接进入同一 `assembleGameplay`；作者输入继续保留一份可变 assembly，进行 grace lowering。
- assembler 仍构造自有图并执行全部原校验。成功后立即把图中仅有的两个借用指针
  `timebase` / `latePolicy` 重绑到 `PreparedGameplayStorage` 的自有配置，再继续准备 requirement、resource 和 timer。
- storage 的 identity declarations 及 profile 字符串仍拥有；没有新增 cache、provenance 资格、执行路径或公共字段。

因此本轮没有跳过 Packed 外部校验、semantic compare、source closure 比较或 prepare 的任何语义门。
没有改 wire、hash、公共布局或 SDK 版本。维护责任仅是保持内部输入视图不逃逸，及成功结果立即重绑配置所有权。
若以后给 assembled graph 增加其他借用字段，须同步复核此处；不能直接把 caller 引用保存在返回的 prepared 中。

## 3. 功能、字节与源码核验

root 亲自检查差异并串行构建、执行以下 MSVC Release STATIC / metrics OFF 套件：

| 套件 | cases / assertions |
| --- | ---: |
| Judgement | 232 / 56,027 |
| Packed | 49 / 5,614 |
| Playback | 181 / 34,528 |
| Host | 61 / 43,339 |
| 架构门 | 1/1 |

Host assertion 数受循环进度影响，不作为固定 fixture 身份。新增一项 resolved prepare 生命周期回归：
prepare 后改写 profile 字符串 backing storage、late policy 和源 requirements，并销毁输入；
随后检查配置、chart/content/prepared identity、deadline、resource 索引和窗口查询。
既有作者 grace、resolved 拒绝与 publication 原子性用例仍在完整套件中执行。

独立诊断 `prepare_probe.cpp` 使用同一份源码，分别链接本轮 0.9.1 基线及候选库。
两组各通过 1 case / 141 assertions。四个成功场景包含普通输入、声明重排、多个 requirement 和执行 profile；
输出三类 identity 与完整 Packed bytes。四个拒绝场景包含负 frozen grace、重复声明、未知 capability
和错误 window phase，输出错误 code/message。共 20 份文件逐字节相同；成功输入还经过 decode、
semantic compare 和 identity 对照。该 20 份诊断不是此前 96 份 identity corpus，也不声称新增了独立 Recovery wire 全矩阵。

无上下文 Max `gpt-6-luna` 分别做只读调用链、取证工具审计及最终差异审阅；最终审阅未发现明确可行动问题。
子代理未构建或采样，其审阅结论不替代 root 的上述实测。

## 4. 新登记的普通性能对照

固定 1K / 10K / 40K，每规模三对，顺序 AB、BA、AB，共 18 个独立进程，全部有效。
每个进程用实际 revision 4 Packed 路径、33 records、owning source binding、CPU test renderer、无音频，
正常 allocator、metrics OFF、BelowNormal / HighQoS、测量 affinity mask 4。
每进程包含一次 setup 及首次/重复三次 Seek 到 CPU test present。没有预热或删除首次样本。

性能脚本在启动前固定全部样本、源码/库/二进制/SDK/配置身份和停止条件，清除继承的 metrics 环境变量，
通过 stdin barrier 在设置 affinity / QoS 后放行；首个无效样本即停止，不补样或扩展组。

| requirements | 基线 setup 三次（ms） | 候选 setup 三次（ms） | 基线 / 候选中位数（ms） | 中位数变化 |
| --- | --- | --- | --- | ---: |
| 1K | 164.212 / 140.457 / 142.066 | 132.837 / 131.233 / 129.069 | 142.066 / 131.233 | -7.63% |
| 10K | 1652.550 / 1643.121 / 1630.043 | 1512.458 / 1530.867 / 1543.371 | 1643.121 / 1530.867 | -6.83% |
| 40K | 7286.086 / 7113.921 / 7187.428 | 6740.460 / 6773.312 / 6592.659 | 7187.428 / 6740.460 | -6.22% |

9/9 对 setup 方向一致改善；本轮保留复制消除候选，但不宣布正式统计通过。
尤其 1K 基线的 `(max-min)/median` 为 16.72%，超过原 15% 重复性门；原值完整保留，
不解释成可豁免样本，不追加样本去改变本组结论。其他五个配置/规模分组的该比值为 1.37%–2.87%。
本组不是 R4 的三窗口九对矩阵，不提供其 Student-t 置信区间接受结论。

**Seek 没有被本轮证明改善。** 40K 三个首次 request-to-CPU-test-present 点值为：
基线 15.228 / 14.761 / 14.551 ms，候选 16.417 / 16.604 / 15.007 ms。
首次中位数上升约 11.2%，原因未归因；若合并每组全部九次请求，中位数为 14.739 / 14.885 ms。
这些点值不能混用成首次/尾部改善，也不因均低于 20 ms 就宣布适用 Seek 整体通过。
setup 包含 fixture source prepare、Packed encode、decode/re-prepare 与 Host load，不是进程启动到首画面的初次 load 边界。

独立 probe 的首次运行在 runner 中解析 Ninja 路径失败；随后手工链接缺失 console subsystem，
修正后基线及候选 probe 均通过。失败日志保留；它们发生于性能登记之前，不计作产品失败或替换的性能样本。

## 5. 证据与后续

全部新产物在 `out/verification-20261028/r3-p1-resolved-copy-v1/`：

| 入口 | 用途 |
| --- | --- |
| `entry.json`、`baseline-binding.json`、`candidate-binding.json` | 起点、每侧 2213 个 tracked 输入文件、库、四套 exe、生成版本头与 CMake cache 身份 |
| `baseline-build.log`、`candidate-build.log` | 串行构建原始输出 |
| `baseline-bin/`、`candidate-bin/` | 各自独立保留的 0.9.1 四套测试 exe |
| `prepare_probe.cpp`、`probe_build.py`、`baseline-probe/`、`candidate-probe/`、`byte-comparison.json` | 同源独立诊断、实际编译/链接命令、失败及通过日志、原始 bytes 和逐文件对照 |
| `experiment.py`、`performance-registration.json`、`performance-results.json`、`performance-summary.json` | 固定计划、控制器哈希、18 个完整进程及汇总 |
| `perf-*.log`、`test-exits.json`、`cuexis_*_tests.log`、`architecture.log` | 全部普通样本、功能和架构日志 |
| `final-manifest.json`、`final.patch` | 完成文档同步后的文件/证据绑定及本轮可撤差异 |

旧 SDK 0.9.0、旧失败、identity writer 撤销试验和原始组均未覆盖。
本轮未执行完整 CTest、Debug / SHARED / MinGW 矩阵、安装消费者重验、真实设备、40K 分域内存或正式接受。

后续先解释 40K 首次 Seek 变化，并继续依据既有分段证据检查重复 pattern 编译、timer finalize
和 typed 重建成本。它们没有混入本轮候选。外部来源、无可信锚点、early/full/partial cut、fault
及首个目标帧的完整覆盖和统计仍待完成；R3c 继续受单独设计门约束。
回退只撤销本候选及其对应文档差异，保留所有证据；不切分支或重置整个工作区。

后续[轻量探针归因](2026-10-28-r3-seek-noise-attribution.md)已将 40K 主成本定位到恢复后的完整 Fact 投影；
同映像对照未稳定复现原约 11% 差值，1K 旧高点仍不能唯一归因。本轮仅按原始审计 JSON 将上述全部九次
candidate 中位数的转录错误 14.845 ms 修正为 14.885 ms，原始组及首次请求数值保持不变。
