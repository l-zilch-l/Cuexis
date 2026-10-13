# Stage 7 B4 宿主与资源分离实施记录

日期：2026-10-12，最终核对延续至 2026-10-13；状态：B4.1–B4.5 本地实施与适用验证完成（testOnly）；Stage 7 仍 active。
源码起点 `stage-7 / 8f18394`，SDK API `0.8.0`，日期构建 `26.10.12-2`。
B3 与 B4 同在未提交工作树；没有 commit、push、PR、merge 或发行行为。

## 实施范围

按 [B4 计划](../../../../stage_plans/active/stage-07/plan-rpa.md) 顺序，先采用同线程显式 owner 驱动。
真实线程创建、callback 隔离实现、SDL/实体设备接入及性能容量验收属于 B5/F1–F4。
这里的 audio callback 隔离来自显式注入的证明接口，不能从 stop 成功推断设备 callback 已静默。

| 批次 | 本地实现与可检查入口 |
| --- | --- |
| B4.1 | 非安装 `host_protocol`：完整步骤目标、固定容量记录、Apply/Cancel/start/终态查询、退休 tombstone、各 owner 预留清理槽；同 ID 异载荷撤销许可 |
| B4.2 | `host_candidate` 导出 owning capabilities/manifest/resources/encoded audio；Prepared 留在 Playback owner；Test/OpenGL renderer 新增 prepareOwned；AudioOwner 分离 active 与 pending 租约 |
| B4.3 | CPU prepare generation 重验与 commit fence；prepare/fence 全域证明；commitOnce 保留真实 SoftwareApplied/Unknown 数值效果；普通 mutation 被 fence 阻挡；失败清理与新 Close 独立退休 fence |
| B4.4 | 实际 K1/Pause/R2 facade 连接，软件步骤去重；ResumeTrace owning 身份、最终 route 与 nextContext 绑定；A1 保留 cut/未准入输入且永久终止直接续跑；设备 owner 独立记录部分效果与隔离回执 |
| B4.5 | owning FrameEnvelope、完整 latest 帧、实际 submit/present 回执、scope ABA 拒绝、反馈 expiry/Unknown/迟到历史确认；有向 Close 逐域证明，CPU owning 结果独立保活 |

新增生产文件按职责命名，`rpa_` 前缀只用于 tests 文件。
B4 测试独立目标为 `cuexis_host_protocol_tests`，不依赖 B1 GNU `__int128_t` oracle。
公共 SDK facade 仍是单 owner；没有默认启动 worker 或 audio thread。

## 复核修正

子代理 Standards/Spec/联合审计复核后修正：旧 epoch ack 误删新 epoch 消息；原步骤终态及
完整载荷冲突；nextContext 晋升；重复分配故障诊断；active/pending 音频 Reload；已激活 renderer
的 Pause/Resume；已暂停 capture 的 cleanup cut；整个 cut 的容量预留；各域独立 generation；
真实部分失败后 fence 清理；正常记录耗尽后预留清理；原协议故障的 software retire 与 Close 循环。
OpenGL 存在保留资源却不能恢复 context 时 rebuild 明确失败，不再宣称 referencesReleased。
最终复核补齐动态 ResumeTrace 的最终 cut/hash 绑定、正常 Advance 消耗后的控制 commandOrdinal、
renderer retry 的实际 finalizedContext，以及软件 Pause/Resume 首次执行必须持有匹配 control fence。
软件发布异常记录 allocation fault 并返回错误；不能把该故障槽误写为已经发布 Unknown 终态。

## 本地验证

日志保存在 `out/b4-*.log`。最终非 Markdown 修改/新增源码清单为 66 项，
历史快照 `out/b4-pre-independent-source-manifest.json` 的摘要为
`8ecf8c4e879ab86526ffe572a71baca409d659df7971a6e7422c684d9feb2621`；
包含 B3/B4 未提交源码，不能用 HEAD 单独代表它。`out/b4-source.diff` 保留 tracked patch。

| 验证 | 结果与范围 |
| --- | --- |
| 最终 MinGW Debug | Host 28 cases/719 assertions、Playback 153/26893、Judgement 207/35292、RPA 50/20658、renderer 2/87、Runtime 39/1012 全部通过；架构检查通过 |
| Debug 未过滤 CTest | 中间快照 999 项跑完（751.21 秒），无失败，1 项实体 symlink 检查 Skip；最终注册 1001 项未重新跑完整 Debug，最终受影响 executable 另行全跑 |
| Release 未过滤 CTest | 中间快照 1001 项跑完，1 项软件新增测试状态预期错误失败、1 项实体 symlink 检查 Skip，660.19 秒；修正后最终重建，完整受影响 executable 各 479 cases 全通过；显式 B4/架构/安装 Playback 与 Gameplay consumers 32 项全通过。最终未重新跑完整 1001 项，不将首跑写成全绿 |
| MSVC Debug | clean-first 编译 Player/B4/Playback/renderer/OpenGL；B4 29 项、Playback 153、renderer 2、OpenGL 22 全通过；最终异常出口改动重建后 B4 29 项与 Playback 153 项再次全通过 |
| shared | 34 项架构/安装消费/Reference Host/B4 首跑仅软件新增测试的状态预期错误失败；最终重建后显式 B4 29 项、完整 Host 28 项全通过；完整 Playback 153 项中 152 通过，1 项私有 parse probe 按共享库限制 Skip；首跑其他门禁通过 |
| candidate OFF | 静态 Debug 全量编译；架构、安装 Playback consumer、Reference Host staging/parser 4 项通过；完整 Playback 121 cases/10615 assertions 通过；最终重建通过 |
| docs / diagnostic / version / format | 文档检查、诊断 self-test、26.10.12-2 一致性检查通过；66 项源码中 C++ 的 clang-format dry-run、全部 public header ASCII、新文件命名和 diff whitespace 检查通过；最终文档复验通过 |

MSVC 首次增量验证发生 AudioOwner 旧对象与新协议头布局不一致；对象时间早于头文件更新。
clean-first 后所有上述音频故障消失，不能把之前失败作为生产逻辑缺陷，也不能省略干净重建证据。
MSVC 全量测试目标仍受 B1 GNU `__int128_t` oracle 限制，未改写既有独立 oracle；
B4 已拆成独立目标，不能据此宣称整个 MSVC 测试目标集合可编译。
初次被聊天中断的构建/测试不作为完成证据。OpenGL 22 项使用 dummy 视频驱动的可控路径。

R19–R23、R32–R41 的本地证据采用完整阶段回执及真实调用结果；不是仅凭 score/digest 推断。
`tests/rpa/rpa_b4_tests.cpp` 覆盖 owning、generation、取消/退休、容量/分配故障、部分失败、
联合 Load/Close、失去软件发布回执、呈现观测及真实 TestRenderer/FakeAudioTransport 调用。
`tests/playback/playback_gameplay_tests.cpp` 的 B4 case 覆盖实际 K1 取消等待、Pause drain、R2 once、
post-cut 时钟故障的 A1 与未准入输入保留。
RemainingFrames 仍由既有 Runtime 成功更新消耗，present bridge 没有调用 Gameplay/Runtime update。

## 2026-10-13 独立复核补充

按 owner 要求，在 B5 实施前由三个子代理分别复核合同、代码规范和证据。
合同复核发现并修复三个问题：部分 Audio Load 失败后的不确定设备状态仍需实际 Stop；
pending Audio 候选的退休必须持有原 activation 的终态取消证明；控制 commandOrdinal 的
最大值必须在准入前拒绝，不能溢出。代码规范复核未报告阻断项。

修复后的独立源码快照为 `out/b4-independent-source-manifest.json`，66 项，摘要
`068c5e3ae195c90b91cba6936b75164ec0f4b79787fb2ed0721402268f15c621`。
根代理重新运行完整 B4 Host executable，32 cases / 823 assertions 全部通过，日志为
`out/b4-root-independent-confirm.log`。上表的原始矩阵仍绑定此前历史快照，
不冒作这三个修复后重新跑完的全部矩阵，也不覆盖后续 B5 修改。

## 证据边界与交接

TestPresentationRenderer 与 FakeAudioTransport 不是实体 GPU/声卡验收。
OpenGL 编译和线程检查不等于真实听音、输入、刷新率或失去设备恢复验证。
callback proof 与 join 的真实跨线程实现、设备失败/阻塞复测、生产容量与预算仍由 B5/F1–F4 承担。
记录槽及历史 scope 均显式有界；没有把测试容量默认转正为生产预算。
B3 历史源码清单只固定 B3 当时快照，不能作为新增 B4 源码的身份。
当前状态由 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 单独维护。
