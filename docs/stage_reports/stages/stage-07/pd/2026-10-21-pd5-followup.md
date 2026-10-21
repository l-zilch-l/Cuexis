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
