# PD-5 evidence correction and finite follow-up

日期：2026-10-21，Asia/Shanghai。角色：分批实施证据与交回核验材料。
当前状态以 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 为准；
[已接受协议](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md)不变。

## 起点与保护

基础 HEAD 为 2ab7b44，工作区干净；先做空提交留档 62ca993。SDK 0.9.0，构建 26.10.19-1。
证据根为 `out/pd5-followup-20261021-62ca993/`。旧窗口 3 不续跑，所有历史证据只读。
本轮只授权到第 5 批交回核验，不启动正式注册/窗口，不 push/PR/merge/release。

## 第 1 批：问题表与纠正

下表历史路径均相对于 `out/pd5-rootcause-20261020/`，正式两窗路径另注明。

| 问题 / 已核事实 | 原始证据 | 当前解释与尚缺实验 | 门禁影响 |
| --- | --- | --- | --- |
| 修复前 RPA read_long 两窗约 1.5362/1.5754 | performance-formal-20261020 注册窗口日志 | 真实局部成本增加；完整第三窗缺失 | 两窗超界，非全组接受 |
| F1 213.3→276.5，旧 B 338.0 ns | ladder-fix-F1.log | 同轮差额回收约 49%；缺 F1 后独立配对区间 | 诊断效果，不替代正式 |
| 无 1.10 不可达证明 | patch-Q*、prototype-V*、源码 | 变体有限，保语义空间未穷尽 | 保留原阈值 |
| IPO 全样本高波动 | ladder-fix-ipo*.log | 前段约 7 ns 是选段结果；缺受控对照 | 不作优化极限证据 |
| Q3 1.0988 点估计且丢保护 | local_measure.py、patch-Q3-direct-read.diff | 历史 PASS 非配对区间；不可作为候选 | 未正式通过 |
| 规模为修复前 A/B | scale-analysis.json、scale-probe-block.cpp.txt | 2 进程/侧、3 块/配置/进程，2048 读/块；缺 F1 长块 | 不接受 F1/尾延迟 |
| 差额拟合 71.6765+27.0590N ns | scale-analysis.json | N=Snapshot 对象；固定 owner 探针为 1 对象，camera 另计 | 不推导 2 对象下限 |
| total 终点不同 | analyze_host_ready.py、host-ready-phase-analysis.json | ACK 与 present-return 各自报告；缺共同完成端点 | 禁止 total 配对 |
| 46.55/57.30 µs | 同上 window-1 | 差 10.75 µs，帧预算 0.258%，数值增幅 23.1%；因端点不同不能作整帧收益 | 原 total 标签无效 |
| 公共 ready 仍可比较 | host_delivery_probe.cpp、两窗日志 | 同逻辑工作/请求→ready；额外往返是原因假设 | 不抹掉 ready 增幅 |
| 小中位数与 p99 不同 | measurement_method_tests.cpp | 块平均成本不能当操作尾部；需每进程足量样本 | 未判 p99 |
| 10% 与 15% 是不同统计量 | 已接受协议 §3/4 | 配对进程 CI、重复性分别判定 | 无放宽、无自动豁免 |

历史完整性新入口 `tools/verification/playback_performance/historical_integrity.py` 不要求旧 HEAD，
也不声称当前源码可登记。`history-before.json`：341/442 文件哈希和 301 个历史绑定构建文件通过。
原清理脚本保持原样，其 HEAD 断言不适用当前候选。

验证命令与日志：`python -B tools/check_docs.py` → `batch1-docs.log`；
`git diff --check` → `batch1-diff-check.log`。本批不要求 C++ 构建。


## 第 2 批：工具契约

新增有限诊断分析入口，复用 measurement.py 的进程配对 log-ratio CI 和原阈值；
不把诊断写入 formal manifest。身份、计数、工作量、端点、输出和进程完整性先校验，
不同完成终点拒绝。旧 require_comparable 补上双方都缺身份字段时的拒绝。
构建入口增加显式 --build-dir，默认行为兼容，并为新路径拒绝误覆盖。

旧行为例：local_measure.py 的 1.0988 打印 PASS；新行为仅返回 diagnostic_*，acceptance=false，
3 对不足/重复进程、短块、不同 ready/completion 端点会拒绝。相同 ready 则允许比较。
块统计不会升级为单操作 p99；样本不足仍保留 None。原阈值 1.10/15% 及绝对预算未改。

本次 `python -B -m unittest discover -s tests/pd -p 'pd5*_tests.py' -v` 47 项通过，
日志 batch2-tests.log；文档与 diff 检查分别记 batch2-docs.log、batch2-diff-check.log。


## 第 3 批：有限诊断预声明

假设 H1：F1 仍有每对象中性读取/复制成本；H2：真实更新→提取与未更新热读的相对表现不同；
H3：公共 request→ready 的延迟仍须独立于 ACK/present-return 解释。主指标为各规模 read 的
每进程块中位；updated 为 update-start→Snapshot-ready 的组合成本，不能相减得到子项归因。
不采纳任何子项 p99 推断，不增加插桩或 IPO。

新正常计时构建为 out/build/pd5-followup-62ca993-f1；A 从冻结 implementation-inputs.zip
在证据根 baseline-source 解包，另建 out/build/pd5-followup-62ca993-a，仅增加同一个
read_scaling_probe.hpp include，host 两侧固定 32 预热+1024 测量、capacity=2048。
原冻结 A 不改动；适配 A 不等于原二进制，其归档/补丁/库与工具链另登记。
实际 build/cache/source/exe SHA256 在采样前冻结于 plan.json 并提交其摘要；运行时重新复核。

每侧 3 独立进程，顺序 AB/BA/AB；规模每进程固定 1/8/64/256，各自 read 与 updated：
3 块，read 次数分别 2097152/1048576/262144/65536，updated 次数分别
262144/131072/32768/8192。每配置先 64 次 update+extract；仅首对象有真实移动 Behavior。
每进程前 30 秒机器预热；本次不作校准。短块小于 100 ms 停单元并保留失败，不补样。
scale 仍绑定 mask=4（沿既定核心策略），host/生产使用冻结默认允许集合；不按结果挑核心。
normal allocator、Release STATIC、candidate ON、metrics OFF，无 ETW/GPU/audio。
环境快照随日志记录；OEM/后台安静程度不能独立保证时明确列为限制，不借诊断接受正式环境。

内容 chart SHA256、资源字节、序列、viewport、U/F/R、Snapshot 对象与相机数均绑定。
规模 registry 为 N 对象+1 camera，无 Gameplay requirement；更新轨迹不 Reset，不追加历史；
每块检查输出 digest，updated 必须变化，A/F1 必须精确相等。宿主在固定合法容量内完整走
1056 帧并逐帧对 reference oracle；任何容量/EOF/身份/输出错误停止该单元，无事后截短。
ACK 与 present-return 分项，enqueue/drain/GPU 缺无侵入边界，保持 unavailable。
宿主每进程 1024 操作可报告 p99，scale 仅块均值，不能报告操作 p99。

生产单元 B-only 每种 low/high reuse 固定 3 进程，40k entities/requirements，使用已有
PD5 full content Snapshot Seek Replay boundary；它与原 RPA-F3 Packed probe 内容分别命名，
检查完整 Snapshot/Replay/Seek/Close。次数少只报成本/max 与正确性，不作生产或尾部接受。

运行入口 finite_followup.py freeze/run，先冻结再提交登记摘要再执行；原始日志、进程退出码、
哈希及全部样本输出到独立证据根。错误停该单元；噪声与超界保留固定全部进程，不追加替代。
工具错误只可新身份再登记，不能覆盖旧 log。有限诊断始终 acceptance=false。

预检全量 Release 构建发现原有 gameplay_author 同时编译 gameplay_entry.cpp 却缺少
Judgement 私有 include 路径（build-f1.log C1083）。补齐 PRIVATE 路径，不导出 internal 头；
重建日志 build-f1-r2.log，受影响完整构建/架构验证随本轮留下。


构建准备记录：F1 全量 Release 第一次因上述 PRIVATE include 失败；第二次暴露本轮探针误拷
move-only PlaybackSource，已在每模式独立建立并 move，第三次完整构建退出 0，失败日志不覆盖。
A 冻结 implementation archive 未收录完整 tools/，首次配置因开发工具 allowlist 缺 target 拒绝；
适配 A 配置仅关闭不参与计时的 CUEXIS_BUILD_DEVELOPER_TOOLS，正常计时库仍为同工具链
Release /O2 /Ob2 STATIC、candidate ON、metrics OFF；不补入 B 库或产品源码。
此构建差异记入 plan 与比较限制，原 A 不重建。本轮 host 扩展通过显式
CUEXIS_PD_HOST_DIAGNOSTIC_1024=1 启用，原 formal 默认仍为 512/1024。
