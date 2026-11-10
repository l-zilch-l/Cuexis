# R3a recovery closure and preparation verification

状态：local implementation verified / R3 in progress，2026-10-27（Asia/Shanghai）。
本报告不登记 R3 整体退出、R4 正式性能、40k、设备或生产接受；R3b 尚未实施，入口裁定待 owner。
返回 [New RPA 报告索引](../README.md)。[实施登记](../../../../../stage_plans/active/stage-07/05-rpa-new/r3/plan.md)。

## 1. 范围与身份

用户授权本次接手完成 R3，后续明确继续推进且不处理文件拆分。唯一实施树 `D:/Cuexis-worktree`，
分支 `stage-7`，HEAD 仍为 `c4bc5d4456ffe4778a90f9bae410638c0016a8eb`；本批产品/测试差异未提交。
新测试绑定 dirty source，不能把该 HEAD 单独当作最终行为身份。原 R2 接受/技术收尾未提交差异保留。

入口 `verify_handoff.py` 通过：两棵树、75 个绑定文件、146 个归档缺失路径相符。
起始 2186 路径清单、精确字节归档和旧 dirty patch 在 `out/rpa-r3-20261026/entry*`。
历史 PD-5/R2 证据和只读历史树未重跑、替换或重建。

## 2. 最小产品变更与反例

| 变更 | 已核实问题/目的 | 证据 |
| --- | --- | --- |
| captureState 从 logTail 取实际应用历史 | 完整源含未来记录时，早期 complete cut/partialH 的 Snapshot 混入源 suffix，恢复失败 | `early-cut-red.log` 四种 cut 8 failures；最新三项 R3 链 1320 assertions 通过 |
| 私有 archive provenance | 外部保留 mutable ReplayData alias 后，旧指针证书会跳过已变更前缀 | `provenance-red.log` 原点拒绝而带证书错误接受；新增直接/多跳、非法/完整合法替换反例通过 |
| RPA restore 复用初始候选 | 原先两次 prepare，首个候选不参与重放 | 真实 prepare/index/初始 reserve/配置副本由 2 套减为 1 套；普通 allocator 对照见 §4 |
| Playback 通过 forkForSeek 创建独立候选 | 删除 makeGameplaySession 产生的多余空候选，保留当前 identity 拒绝门和事务替换 | 全状态/坏 suffix/新 scope/旧 owning、原会话不变及 Prepared 生命周期反例通过；Faulted Seek 仍拒绝 |
| finalization fault 恢复 | RPA restore 原先只重建带 failedTick 的故障，遗漏 control-stage finalization | pre-seal/Fold/finalization 故障完整 DTO、错误与后续拒绝对照通过 |
| 区分瞬时分配失败和确定性 admission 拒绝 | 瞬时 OOM 被记入可重放拒绝后，健康 Snapshot 不能恢复 | 独立探针先 19 assertions/2 failures；修复后 direct/decode 均成功，19 assertions 通过 |
| 确定性拒绝形成新 journal 分支 | Seek 后发布真实拒绝记录须清除旧 seekSource；重复/late/conflict 等语义拒绝继续保留 | 确定性 duplicate 的 branch、Replay、Snapshot 恢复对照通过 |

分配失败修复只将 `std::bad_alloc` 标为未完成 admission 的操作失败，保持原 Error、active/pending/
journal/source 不变并释放临时 reservedLog。其他确定性拒绝不归入该标志；没有新增公开诊断码或 wire 字段。
依据为 execution profile §7.3 的 submit allocation failure `session_unaffected` 与 RPA 可复算 admission 合同。

provenance 字段在不安装的 Judgement 类型中，不改变 installed Playback SDK 布局。
它只证明 owner 封存；本报告绑定的 R3a 构建尚未启用 RPA checkpoint 快恢复，外部来源继续原点验证。
Snapshot/Replay wire revision、canonical SHA-256、Fold/故障 reserve、线程拓扑均未更换。

## 3. 新观测的口径与用途

复用原 `CUEXIS_RPA_OBSERVATION_TEST_ONLY`，新增 16 个 uint64 统计字段：prepare 调用/成功/时间、
派生索引次数/时间、初始 reserve/配置副本、RPA restore-validation 包络、四种动态复制、四类尾块元素。
采集为原 owner 的定长聚合，计时外导出；无逐对象打印、新锁或新线程。

`rpaRestoreValidationUs` 包含重放、完整 DTO 物化及比较，排除 prepare；index 时间嵌套在 prepare 时间内，
不相加。早期旧探针的 `replay_us` 还包含第二次 prepare，保留原日志但不拿它作前后相位时间比较。
当前宿主真实 prepare 为 3 次：1 个目标候选，另 2 次来自未改动的 canonical 编码，不误报整个 Seek 只准备一次。
copy 计数放在实际复制成功后；元素数不是嵌套字符串 bytes，旧 configValues/公开 DTO 字节缺口不填零。

宏关闭的三份 Release 库 RPA observation 符号均为 0；宏开启对应为 Judgement 30、Playback 31、Host 2986。
独立同轨迹 ON/OFF 的 11 行 R0EQ transcript 完全匹配，digest 为
`b4608fbfd7a89706d5c982fa8e57132e4dbb12abde7fa019cc762bbf6c1a7bf2`。
这些证明编译隔离及该轨迹等价，不证明观测零扰动；宏 ON 的额外存储/时钟/尾块遍历不承担正式测时。

## 4. 固定局部成本对照

源码/工具/构建/库/二进制绑定、固定顺序与停止规则在 `cost-registration.json`、`cost-claim.json`、
`baseline/binding.json`、`candidate/binding.json`。六进程次序 B-C / C-B / B-C，N=64、每进程 8 块、
每块 2048 次 owning Snapshot recover，普通 allocator、Release STATIC、mask=4、显式进程 HighQoS，产品观测宏 OFF。

| 指标 | 结果 |
| --- | --- |
| C/B 几何均值 | 0.631599 |
| 三进程对 log-ratio 的 95% t 区间 | [0.544472, 0.732669] |
| B 全组块 spread | 12.8354% |
| C 全组块 spread | 14.2181% |
| 原 15% 噪声门 | 通过 |

该组绑定的是 RPA restore 去重子候选，后续安全修复和 Playback 去重不换标进该局部性能身份。
边界为预热后 Snapshot recover 及该调用返回值释放；不是首次加载、端到端 Seek 或 R4 三窗口接受。
没有补样、频率归一化或替换旧慢值。

## 5. 新源码功能、兼容与构建门

| 配置 | Judgement | RPA | Playback | ExecutionHost | 架构 |
| --- | --- | --- | --- | --- | --- |
| STATIC Debug candidate ON | 224 | 55 | 179 | 61 | 通过 |
| STATIC Release candidate ON | 224 | 55 | 179 | 61 | 通过 |
| STATIC Release observation ON | 224 | 56 | 179 | 68 | 通过 |
| SHARED Debug candidate ON | 224 | 55 | 170 pass / 1 skip | 61 | 通过 |
| SHARED Release candidate ON | 224 | 55 | 170 pass / 1 skip | 61 | 通过 |
| STATIC Debug candidate OFF | 224 | 55 | 130 | 不适用 | 通过 |

数字为 case 数，不把随真实时钟循环变化的断言数当固定常量。SHARED 跳过的是既有 private parse-probe
characterization，不能记成执行通过。其余列均零失败。新 R3 定向三项为 1320 assertions。

STATIC clean Playback/Core/Gameplay installed consumers 为 3/3；SHARED 同三项为 3/3。
Release full build 与 `cuexis_format_check` 通过；文档检查使用新 candidate-OFF 的 target facts，通过 578 Markdown/
20 JSON/CXT（本报告新增前的检查数量，最终数量以新检查日志为准）。
Linux、MinGW、sanitizer、GPU、真实设备、40k、ETW/WPR、正式窗口未执行。

首次配置因 PowerShell 拆分未引用的 `exp.candidate` 失败，原错误保留。中文 MSVC 只有 2052 语言包，
`VSLANG=1033` 没有切到英文；CMake/Ninja showIncludes 前缀解码异常导致增量漏掉 header 依赖和旧布局崩溃。
本表使用独立全新构建/完整 clean rebuild，未以那些陈旧对象接受结果；尚未修改系统工具或工程配置来修复该环境问题。

## 6. 回滚、剩余范围与下一步

固定前缀诊断另在 `seek-profile-registration.json` 登记一次进程，N=64/256/2048、33 条正常 advance、
既有显式 checkpoint 固定在第 17 条 H=1119，目标第 33 条 H=1135；每规模 3 次 origin/one-hint。
普通 allocator/宏 OFF、mask=4/HighQoS，18 个样本和完整 Snapshot 对照均完成，192 assertions 通过。

| requirements | 原点 forkForSeek 样本范围 | 现有单 hint 样本范围 | 首次 checkpoint 工厂 |
| --- | --- | --- | --- |
| 64 | 13.73–14.18 ms | 15.16–16.28 ms | 9.30 ms |
| 256 | 57.14–60.48 ms | 64.42–70.01 ms | 40.21 ms |
| 2048 | 688.55–790.32 ms | 740.15–779.61 ms | 470.51 ms |

这是归因信号，不是正式性能区间，也不是端到端 Seek。它排除后续 Snapshot 物化、Host 协议、clock、
sample/present，不能接受 20 ms 门。当前 RPA 不使用 checkpoint 恢复，但仍扫描/比较 hint 的历史；
现有 hint 没有消除前缀执行，不能把加 hint 当优化已成立。R3b 的可信前缀机制及其首次/驻留成本需另测。

`r3a-product.patch` 与 `r3a-observation.patch` 已在隔离 Git workspace 反向/正向应用，恢复精确入口字节；
覆盖 11 个源/测试路径和当时 5 个 R3 文档路径，另 2172 个入口路径相符。该检查证明字节恢复，不宣称
独立 patch variant 已单独编译，也不自动覆盖随后新增的本报告/导航/审计附件。
不 reset/clean 主树，不将 R2-only patch 反套成 R3 回滚。

R3b [受控候选提案](../../../../../stage_plans/active/stage-07/05-rpa-new/r3/trusted-restore-candidate.md)
已由无上下文 max gpt-6-luna 审查。后续核对确认总计划 §8.2 已允许首版在最终 archive 工厂上生成，
此前额外确认请求已经撤回；当前 R3b 已实施并在独立构建核验，不将其结果回填到本报告的 R3a 身份。
R3c 条件门尚未进入，生产 checkpoint caps/间隔尚未登记。
R3a 当前选定准备去重通过本地门，不等于动态事务、tail payload、canonical 身份缓存等条件子项都已解决。
