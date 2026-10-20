# PD-4 按需交付、真实回收与紧凑观察记录

日期：2026-10-19。状态：PD-4 内部实施、适用本地门禁与新的无上下文独立复审完成；物理设备、正式性能与发行接受保持独立。
当前状态唯一见 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。

## 1. 授权、起点与版本

owner 明确回复“我接受这三项，你开始实施PD-4”，接受 [ADR 0047](../../../../adr/0047-pd-feedback-lifetime-and-delivery-retirement.md)。
分支 stage-7，HEAD `d344088236ae589c0b49a4994ef0fbddd6567ce2`。PD-2/PD-3-a/b/c 的既有未提交内容保留；
本批差异按 `out/pd4-verification-20261019/pre-pd4-workspace.zip` 与 `pre-pd4-state.json` 界定，不能把整个 git diff 当作 PD-4。

SDK 0.8.1 → 0.9.0；日期构建由版本工具更新为 26.10.19-1，vcpkg 同步。
`FeedbackObservation::expiry` 为 `optional<GameplayPresentationTick>`：有值是有限 exclusive end，无值是 UntilReset；
有限 INT64_MAX 与无期限可区分。消费者检查分支、读取 `expiry->value` 并干净重建；旧 minor 拒绝，不保留安装双版本状态机。
FrameSnapshot、HostContext、Gameplay/Score/Replay identity 和 wire 不变。迁移规则见 [VERSIONING](../../../../guides/VERSIONING.md)。

## 2. 实施责任与寿命

| 对象 | 持有者与结束证明 |
| --- | --- |
| 完整 Snapshot、resource 引用、membership | 不可变 FramePayload；最后 CPU 读者析构数组/字符串后，独立账户无分配返还额度 |
| 每次 delivery/attempt 描述 | 独立 attempt lease；复用旧 payload 也要准入；请求历史与 journal 不持该 lease |
| Ready 响应 | 每消费者一个同步槽；take/cancel/deadline 同锁裁决，移交前登记 Worker FrameId 和 dispatch token |
| 去重与执行许可 | Worker 的有界 metadata 请求史和登记前沿；Renderer owner 的独立 ConsumerId/frameId 前沿，journal 淘汰不能复位 |
| present 证据与反馈 | owner 独写紧凑 journal；只留键、source/context、成员、状态、时钟、结构化错误和受限缺失区间 |
| GPU/设备对象 | 继续由原 Renderer owner 的 generation、retire/fence/Close 管理；CPU 归还、Unknown 或 TTL 都不充当 GPU 完成证明 |

FrameStorage 的账户不含 Worker、Session、SDL 或 GL 指针，可跨 Close 存活。注销先释放该消费者的未来预留，
外部仍持有的真实 CPU/attempt 占用转入 retired 实占，继续限制全局准入。没有新增回收线程或 owner。

Registering/Active/Revoking/Retired 经原可靠生命周期通道与呈现 owner 确认；撤销先到也关闭未来激活。
确认时重验当前目标，旧注册/普通帧/激活/撤销不能激活或注销新身份。一个 Renderer，多个独立 CPU capture；
后者使用 ExactCapture，frameId=0、不产生 present attempt，不更改 Renderer 绑定。

SessionUpdatePresentation 独立于 SessionRequestFrame，先完成逻辑更新与激活/到期观察，再准入并按需构帧。
可靠输入/推进/控制保持 FIFO，排在交付物化之前；每轮最多一个物化任务与有界观察维护。
新路径回复仅含 metadata，完整帧通过独立 take 槽移出；旧同步路径只作新 session 边界下的测试对照。
Player 成功更新后的时间前沿独立推进，跳过交付不会重复累计 simulation delta。

present 实际成功后先由呈现 owner 记 last-success，再向有界观察 inbox 提交回执，不等待 Worker ACK。
先到回执依赖同步 dispatch token；迟到 Unknown 不覆盖确定终态；确定冲突保留原证据。
结构化错误深拷贝为受限 owning 链，完整比较顺序 context/cause；字节账覆盖节点、控制块、容器容量与字符串。

journal 按 count/bytes/monotonic TTL 淘汰 attempt、feedback 和 tombstone；仍可能移交的 Ready 淘汰也记覆盖缺失。
有界缺失区间、可查询范围、缺失类别及饱和 lossEvents 可通过 owner 查询取得，范围外回答 HistoryUnavailable。
lossEvents 不是独立丢失反馈数，exactLossCount=false；新 scope/新 Fact 不继承旧缺失状态。
观察不足不使 Gameplay Suspended；原可靠输入/控制/Replay 的故障合同仍保留。

暂停同 viewport 共享 last-success backing；resize 独立准入。显式压力撤销或设备失效先撤销后续使用资格，
可靠通知 Worker 并释放自身引用，外部 CPU lease 与 GPU 责任不被强制释放。暂停缺图返回内部 NoRetainedFrame，
Player 显示诊断状态并等待合法 Resume/更新，不从新 World 补图或执行 frozen update。

## 3. 本地 testOnly 边界

实际 Player 明确为 execution-test-only。下列数值仅是可复现实验配置，不接受为生产预算：

| 责任域 | 本批声明 |
| --- | --- |
| 消费登记/CPU backing | 8 注册槽；全局 8 payload / 64 MiB；Renderer 最低 2 payload；单帧额度按准备 object/resource 数、256-byte identifier 上界与最多 4096 成员保守计算 |
| attempt 描述 | 全局 64、每消费者 8；每项保守计 `2*sizeof(FrameEnvelope)+sizeof(FrameAttemptLease)+sizeof(DispatchToken)+512` 请求存储，与 payload 预算分列 |
| journal | 常规 attempt/feedback 各 4096；总 8 MiB，tombstone 128，coverage 区间 8；保留 60 秒、receipt deadline 5 秒，均用共同单调时钟 |
| 观察 inbox | 128 项，每轮最多 16；结构化错误最多 16 层，文字/context 受 4096-byte 输入界约束，拒绝超界并记对应责任域缺失 |
| 强制回收 fixture | attempt/feedback 各 4、journal 16 KiB、tombstone/coverage 各 2，持续 5000 次实际宿主更新/呈现 |

计账验证针对 C++ 请求存储，不能当作 CRT 堆开销、进程 RSS 或生产峰值预算验收。

## 4. 验证与审查

四个独立 build tree 均先 fresh 配置并 clean-first；候选 Debug、Release、shared Debug 与 candidate OFF。
普通回归明确排除另行执行的 package/import/Reference Host 门禁和独立性能脚本；不是裸跑所有注册 CTest。

| 配置 | 普通回归 | 安装消费者/Reference Host |
| --- | --- | --- |
| candidate static Debug | 注册 1112：通过 1111、跳过 1、失败 0 | 基础包/Playback-only/Gameplay 3 项、Core-only/AudioSDL 2 项、Reference Host 1 项通过 |
| candidate static Release | 注册 1112：通过 1111、跳过 1、失败 0 | 基础包/Playback-only/Gameplay 3 项、Reference Host 1 项通过；Core-only/AudioSDL 独立项在 Debug/shared/OFF 验证 |
| candidate shared Debug | 注册 1097：通过 1092、跳过 5、失败 0 | 包/导出/导入 6 项、Core-only/AudioSDL 2 项、Reference Host 1 项通过 |
| default OFF static Debug | 注册 962：通过 961、跳过 1、失败 0 | 基础包 2 项、Core-only/AudioSDL 2 项、Reference Host 1 项通过 |

Debug/shared 沿基准 preset 使用 WX OFF，Release WX ON；Player Release WX ON 编译链接。
既有 Windows symlink、跨 DLL allocation/parse 观察限制按实际日志保留为 skip，不写成执行通过。本地范围为 Windows/MSVC；未运行 hosted Linux/MinGW 或 Linux sanitizer。

最终 C++ 输入 clang-format dry-run、版本一致性与 git diff --check 均通过；文档检查覆盖 459 个 Markdown 和 20 个候选 JSON/CXT，安装树中 545 个公开头文件副本全部为 ASCII。

独立复审代理未继承对话上下文，按起点 ZIP 界定本批差异并复算 81 项输入/ZIP：
`59e304b8f34afc4a124f22f5b156f5c4df20db3b90b984e2aebeaac9de787fdb`。
它独立运行 25 个用例 / 151088 assertions，全 exit 0：协议 21/120992、实际宿主 3/30088、精确分配 1/8。
实际 5000 次更新/呈现覆盖至少 35 秒逻辑源，payload peak=2，journal live/peak=5872 bytes，Replay 等价；
另有 10000 次小容量交付、领取/取消竞争、极值寿命、旧登记/旧 scope、TTL、NoRetained 与跨 Close 读者验证。
16 层 owning 错误链实际新增请求存储 4720 bytes，计费 5920 bytes。

复用冻结公开 probe：13 个 Snapshot 与 FrameDigest v3 逐字节一致，7878 bytes，SHA-256
`f4303f95f2b2de54993946f3f5ba6a3a91e28b7a8bb333427bc43029ea1c60c0`。
冻结 PD-3-c 的 259 项 evidence hash 现场核验未改动。

真实 NVIDIA RTX 4060 Laptop / OpenGL 3.3 普通 Player smoke 完整 6 帧 exit 0，包含最小化/恢复、失败 reload、
失败 prepare、legacy 非 portable 负例、竞争候选拒绝及成功 reload。它不经过 RPA ExecutionHost，不能替代实体键盘/听音验收。
首轮 GPU smoke 失败：旧 negative probe 仍期待已移除的 adapter 旧错误；仅补正 smoke 的 fixture 非 portable 断言与
当前 owning materialization 的 context_stale 拒绝，保留 active-cache 不变要求，未放宽 Renderer 生产合同。
原始失败日志与新二进制/exit0 日志分开保存；首轮进程退出码未被捕获，不从错误日志臆造数值。

开发期问题及首轮失败保留在 evidence：optional typed tick 比较、旧测试消费者/视图迁移、MSVC COMDAT、
未完成增量时启动的中间矩阵、三个旧 consumer minimum minor、Reference Host 负例 escaped regex 与 smoke 探针编译问题。COMDAT 对象和结构检查单独归档；attempt lease 的工厂集中到交付组件，
后续完整构建通过。它们均不算正式测量窗口，不靠重试改变性能判定。

历史 host collector 的 ACK 指标不能套用新异步路径。当前 probe 发出 `pd4.present-return.v1`，统计 present return/receipt enqueue；
旧 collector 明确拒绝该 schema。保留原 PD-0 失败/慢态样本、15% 门槛与历史二进制，不在本批重跑正式性能窗口。

## 5. 证据与退出边界

本批证据根为 `out/pd4-verification-20261019/`：起点 ZIP、实施 ZIP/指纹、本批 delta、每次构建/回归/消费者原始日志、
snapshot-parity、错误链分配、GPU 两轮日志及原始失败对象分别保留；verification-summary.json、implementation-build-binding.json 和 independent-review-final.json 记录最终汇总、46 个选定对象/29 项产物及独立审查。
冻结真实 0.8.1 包拒绝 0.9.0 请求；新包拒绝 0.8.1、过高 patch 与其他 minor/major。Reference Host 四配置均通过迁移后的 41 个命令用例及拒绝门禁。没有提交、推送、PR、合并或发行。

本批不接受 PD-5 owner 拆分、正式性能阈值、生产 profile、多 Renderer、录制/导出产品、跨资源代精确暂停恢复。
后续新二进制实体 DFJK 完整课程、Pause/Reset/Resume 与真实音乐听音已取得受限观察，见 [PD-4 实体报告](../rpa/device/2026-10-19-pd4-physical-music-and-keyboard-observations.md)；控制附近 FPS 下降后恢复待量化，设备失效/恢复仍未验收。
此前 PD-3-c 首轮实体 capacity_exceeded/退出 1 保持失败；不能以本批自动化覆盖或改写。
RPA、正式性能与 Stage 7 关闭仍各有独立接受门禁。
