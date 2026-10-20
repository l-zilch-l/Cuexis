# PD-5 measurement readiness inspection

日期：2026-10-19。状态：**就绪检查及阈值提案已完成；正式采样未就绪、未启动**。
owner 本次授权“PD-5 测量就绪检查和阈值方案”。未接受新增性能阈值、生产 profile 或 owner 拆分。
本报告拥有现场证据；未来协议/候选阈值见[测量方案](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md)，
当前任务见 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。

## 1. 已现场核验

工作目录 `D:/Cuexis-worktree`，分支 `stage-7`，HEAD `d344088236ae589c0b49a4994ef0fbddd6567ce2`。
存在 PD-2/3/4 未提交修改；HEAD 单独不足以标识当前实现。本轮不修改产品代码，不重建或覆盖历史程序。

| 检查 | 现场结果 | 证据边界 |
| --- | --- | --- |
| PD-4 源码绑定 | 81 项 live hash 与冻结输入一致，指纹 `59e304b8f34afc4a124f22f5b156f5c4df20db3b90b984e2aebeaac9de787fdb` | PD-4 已选输入范围；不是 PD-5 完整源码/工具 inventory |
| PD-4 构建产物 | 29 项绑定产物 hash 全部匹配 | 不据此声称本次重新运行回归 |
| 旧实现 | 冻结目录 8 个旧正式/观察 EXE hash 全部匹配 | 只有 metrics 编译 OFF 的正式程序可作无插桩时间参考 |
| 物理测试归档 | 4 项清单 hash 匹配，physical-runs.zip CRC 通过 | 历史 1 轮音乐/2 轮键盘的有限观察，不是正式性能窗口 |
| Release 测量程序 | pd4-release STATIC/candidate ON/metrics OFF/WX ON，compile_commands 无 metrics 宏 | baseline/host 源码→对象→程序时序有效；依赖分别 200/199 |
| Debug 测量程序 | pd4-debug STATIC/candidate ON/metrics OFF/WX OFF，compile_commands 无 metrics 宏 | 同上依赖 200/199；Debug 与 Release 分开 |
| 实体 Player | SDK 0.9.0、26.10.19-1；SHA-256 `72594dfebc658e6767d672aadb12aa8afc3305bbf92156633a34652946f2eca1` | Player 构建不含测试 probe 是正常配置；正式新 harness 尚未注册 |
| 当前环境快照 | Ryzen 7 8845H，8 核/16 线程，交流电；WPR 未录制，无 Player 进程 | 只代表盘点时刻，不代表正式窗口环境已冻结或系统后台已隔离 |
| 当前显示配置 | CIM 报告 NVIDIA 4060 Laptop、驱动 32.0.16.1664、2560×1600/240 Hz，Windows build 26300 | 原生显示配置不等于测试窗口 viewport；实际 renderer/交换与音频端点在登记时再验证 |

检查入口和机器可读输出：`out/pd5-readiness-20261019/audit_readiness.py`、`readiness.json`；
四份 `pd4-{debug,release}-{baseline,host}-deps.txt` 保存原始 Ninja 依赖查询。
`environment-snapshot.json` 保存电源/拓扑/WPR 查询，`device-snapshot.json` 保存 CIM 设备配置；
候选核心策略仍为拓扑优先的 mask 4，mask 1 仅作原有对照，不根据新计时结果挑选。
本轮仅作只读库存和 hash/依赖检查；没有运行 pilot、性能样本、C++ 回归或 GPU/audio 采集。

## 2. 阻塞与可复用部分

| 项目 | 状态 | 需要补齐 |
| --- | --- | --- |
| 正确性/寿命既有证据 | 可作为输入 | PD-4 本地矩阵、独立审核、长 host/Replay 证据保留；新 harness 另验证等价性 |
| 无插桩固定 owner workload | 可复用边界 | P0 固定 trace/Playback-only/read-long 仍可实现配对；新登记不得指向旧构建目录覆盖旧证据 |
| formal_baseline.py | 不能直接作为 PD-5 runner | 固定 pd0-formal 路径、旧 FIELDS/host ACK、旧输入验证；不改后直接运行会混用阶段 |
| 宿主 collector | 阻塞 | 旧 collector 显式拒绝 asynchronous；B 输出 pd4.present-return.v1，不能套旧 request→ACK 数值 |
| A/B 可比性 | 部分具备 | 冻结 A 的源码/程序在，但没有 PD-5 配对 runner/等价边界注册；需要共同端点的新 harness 或明确 B-only |
| 长稳态 A/B | 不可整体比较 | A 已知 ledger 容量缺陷；其失败保留，B 单独作回收验证，不能靠周期 Reset 保持 A 不失败 |
| 控制耗时 | 阻塞 | 现日志没有命令发出→确认→首帧完整 QPC 序列；平均 FPS 无法给 Reset/Pause/Resume 延迟 |
| GPU 时间 | 阻塞 | 本轮检查 render_opengl/player/PD 源码未找到 timer-query 计时路径；CPU present 返回不等于 GPU 完成 |
| profile/预算 | 阻塞 | 尚无本轮 accepted 延迟/回归/生产容量预算；需要有限 profile 全部字段和静态 content/input/hash |
| 设备/环境登记 | 待新登记 | 先前用户提供安静环境不是永久状态；刷新率、实际端点、驱动/电源/拓扑须采样前后冻结核验 |

旧 collector 的拒绝是正确保护，不应删除拒绝来让旧结果“通过”。建议独立 `tests/pd/` PD-5
collector/runner，显式 schema 与 A/B 端点映射，保留旧脚本的原用途和旧注册结果。
新增观测优先放测试 harness/可选测量路径，不增加常驻 owner，不以测量需求更改 SDK 公共 API。

## 3. 判据与结论

15% 是进程重复性的原门槛，不是回归容忍线。当前方案另外提出本机 testOnly 的 10% CPU 回归容忍、
240 Hz 参考下的分段/请求/GPU 预算、50 ms 控制和 1 秒 Reset 等交互目标；均待 owner 接受。
具体端点、样本限制、三窗口协议、区间算法、内存完整性和拆分规则只在[方案](../../../../stage_plans/active/stage-07/pd5-measurement-proposal.md)维护。

就绪结论：**可以进入测量工具与 profile 准备；现在不能直接启动具有接受效力的正式窗口。**
方案接受后仍须关闭工具/边界/身份/profile 阻塞；此后才可生成正式 registration。
如阈值不被接受，允许记录诊断/基线，但 PD-5 判定必须写“证据不足”，不能自动进入多 owner 架构。

现有用户反馈 FPS 约 240→130→240，持续时间未知，暂不能据此判断预算是否超界或定位模块。
underruns=2 继续保留用户“切换 Codex 打字导致，与程序无关”的说明，不新增程序故障结论。

本轮文档及准备证据归档于 `out/pd5-readiness-20261019/`。原 PD-0/PD-4 及物理验收 evidence 未改写。
`preparation-records.zip` 打包本次文档、只读审计脚本、环境/库存/依赖与验证记录，`evidence-manifest.json`
记录本次产物 SHA-256；不是旧二进制的再次打包，旧产物仍由各自冻结清单绑定。
PD-5 尚未完成；RPA、生产性能接受、实体设备完整验收、Stage 7 关闭和发行各自保持原门禁。
