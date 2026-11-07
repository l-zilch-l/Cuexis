# R4 F4 技术门账与 PD-5 移交准备

状态：prepared；不是退出接受记录

日期：2026-11-07

## 1. 候选与证据规则

本轮实现候选 C 为 24f0a914f306cbc20e05b1dccc9f82dac59a6024，tree 为
0bc59434103671b403ffd3bebc77a3df2fee493a；日期版本 26.11.07-1，SDK 0.9.1。
后继报告提交仅发布 C 的证据，不能自动成为已验证最终合并候选。
本表按 R4 计划批次及原 P/F 单元定位，不重定义原合同的判据 ID。

## 2. 逐项门账

| 计划单元 | 本轮证据 | 未闭合范围/恢复条件 | 责任 |
| --- | --- | --- | --- |
| R4-0 起点 | 干净候选、source/tree、独立构建、依赖与二进制 binding | 最终接受须明确选择的候选 | 代理准备；owner 最终选定 |
| R4-1/2 B3/B4/B5 | 既有确定性机制与线程反例；本轮正常 Release World/Runtime/Playback 269 cases / 46,996 assertions 通过 | 最终候选完整矩阵另列；历史 capture_gap/重复 Pause/CXC 持有者不因测试通过而视为已定位 | 代理 |
| R4-3 O1 | 2026-11-05 方案 B 的已接受数值/hash保持 | reference 设计值不是完整堆上界或生产参数；没有新数字接受 | owner 已裁定 reference；代理验证适用项 |
| R4-4 工具 | typed owned spans、无分配 census、实际字符串归属交换/拒绝门、40 项 Python 测试通过 | 所有其他协议专用 adapters、observer 扰动及内存全图仍须准入 | 代理 |
| R4-5 P0 legacy/RPA | 首个三窗组已完成：两read_long超界，legacy另两项因重复性inconclusive | 只认证固定 CPU trace 相对成本/重复性；不代替端点 p99、P0 host 或 P1–P7 | 代理 |
| R4-5 P1/P2/P3/P4/P6 | 未启动本候选正式单元 | 原内容/比例/consumer 数、60s/10000updates/10倍 retention、端点与 observer pilot 后封存；普通 512-update host 夹具不能代验 | 代理 |
| R4-5 P5 | 未启动本候选正式单元 | 按固定请求数，绑定真正 present-return/join；状态确认或 ACK 不能代替端点；保留控制空窗 | 代理 |
| R4-5 五类 Seek | 历史 R3 适用类结果保留，本候选未重验 | 逐类16请求、cold每进程1请求；新 candidate/匹配原 A/协议专用工具准入后正式固定组 | 代理 |
| R4-5 memory | high/low N64 各一诊断进程：119,534 B active 唯一下界；两者 unrated | 需完整拥有关系、prepare/decoded峰值、current/candidate/shared retained、native/GPU/driver及observer扰动；两诊断已用尽，不自行加跑 | 代理提出具体处置；必要时owner裁定 |
| R4-5 P8 | 未准入，未运行40K | 完整内存及固定40K原工作流工具准入；维持low/high各一次独立额度，不能借CPU或小场景通过恢复 | 同上 |
| R4-6 F1 | 候选C的11行fresh/clean-first未过滤矩阵已完成 | 候选C 11/11 local_pass、13,652项测试注册/0失败；26项skip逐项保留，不能以此替代正式单元 | 代理 |
| R4-6 F2 | 候选C已推送PR#32与R4分支；六个push与三个PR平台run均通过，Version Gate因SDK owner审批无效失败 | 核实实际run SHA和结论；网络失败原样保留；Version Gate单独保留 | 代理 |
| R4-7 O2/P7 | 实体清单已准备，历史PD-4证据保留 | 本候选实体键盘/Hold、听音、设备故障恢复、精确AV/仪器 | owner实体参与；代理准备/处理 |
| R4-7 F3/生产 | 当前宿主仍testOnly | 校准与具体生产参数接受；reference通过不能删除testOnly条件 | 代理提案；owner接受 |
| R4-7 O3/PD退出 | 本表提供移交定位；PD-5仍“未通过但定案” | 明确未完成范围、剩余责任和恢复条件；owner接受具体移交后才改变退出状态 | owner |
| 条件 Version Gate | 受测候选C日期版本26.11.07-1；报告提交E前已推进工作树版本至26.11.07-2 | SDK 0.7.0→0.9.1仍需owner针对最终候选的精确批准；按owner要求暂不修、不代签 | owner |

## 3. PD-5 待接受移交

| 历史未完成项 | R4承接位置 | 当前接受状态 |
| --- | --- | --- |
| 相对CPU与正式重复性 | P0原共同A、九对三窗；其他工作单元各自登记 | 本候选CPU组已完成，读取正式门超界；其余工作单元未取证 |
| 读取/Seek/Close及控制端点 | P0-read、五类Seek、P5 | 历史局部门保留，本候选整组未完成 |
| 分域内存、prepare/decoded与最终释放 | memory及P8 | 所有下界/原失败保留；完整图未判定 |
| 真实设备及生产预算 | P7/O2/F3 | 人工/仪器与具体生产值未接受 |

以上为技术准备，不构成PD-5、RPA、Stage7A或整个Stage7关闭，也不代表owner已接受带限制移交。
