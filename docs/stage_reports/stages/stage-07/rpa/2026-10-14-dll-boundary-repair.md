# RPA DLL 边界方案 A 修复与验证

证据日期：2026-10-14。状态：本次局部修复和验证完成；按 owner 要求停止，不继续其它 RPA 任务。

## 范围与身份

owner 拒绝私有导出例外，选择方案 A：线程/设备/应用编排外置，只补齐 Playback 的同步
能力边界，修复验证后停止。入口 HEAD 为 `7d636c24591c9672d9e72f035a2ff63dd642a5d8`；
修复保留在未提交工作树。`out/rpa-dll-fix-20261014/boundary-summary.json` 记录实际 DLL、
二进制 SHA256、源码文件身份、命令/日志与最终验证汇总；入口 HEAD 不单独代表修复源码。
原有 `tools/__pycache__/` 和旧 `out/` 证据保留。SDK 仍为 0.8.0 preview，构建为 26.10.14-1。

本次不接受生产阈值、不执行实体键盘/听音、不修复独立 CXC 聚合验证问题，不启动 PD、
hosted CI、PR、合并或发行，也不宣布 RPA/F1–F4 或 Stage 7 退出。

## 根因与实现

`d7a20d9` 将宿主实现编译进 Playback，又由 Player 和独立测试直接调用私有类，
通过 `CUEXIS_PLAYBACK_API` 形成私有跨 DLL 依赖。基础源码列表、Session 宿主状态与
桥方法没有完整 candidate 隔离，故 OFF 也导出。初始实测 OFF 为 88 个、ON 为 159 个
含 Playback detail 标识的导出名称；ADR 0033 的严格门禁正确拒绝。

修复分工如下：

| 归属 | 实现 |
| --- | --- |
| 非安装宿主支持 | `app/execution_host/` 的固定 STATIC target；worker、消息槽、capture/audio/renderer owner、宿主/步骤/表现回执 ledger、组合控制；Player 与测试共同链接，不复制实现 |
| Playback DLL 内部 | 软件会话的 accepted control、generation、fence、执行/表现/恢复状态；不接收外部 ledger、worker、协调器对象 |
| 公共同步能力 | Prepared owning 候选物化；candidate 的 owner 状态/软件副作用查询、控制绑定/fence/提交/确认、受控退休/Pause/Resume/Seek、时钟/Replay cut/表现采样与反馈 |
| 公共 owning 值 | 通用 presentation candidate；candidate-only 请求/回执、状态副本与不可自行构造有效值的 ExecutionFence；没有内部执行图、设备对象或线程 |

SDK 自身不创建宿主线程；原 P-B 部署、单 owner、Apply/Cancel/终态重传、资源退休与有向
Close 保留。宿主 ledger 保留步骤裁决和预留，DLL 保留软件 mutation 权限；没有第二套
可变的软件会话状态。Resume 在 DLL 内重新校验 route/trace identity，Seek 校验 archive/cut，
软件退休要求 capture cut 和 audio stop/isolation 证据。renderer 的旧同 owner prepare
也改用公开 owning 候选，不再包含 Playback 私有宿主头。

candidate OFF 不构建宿主支持、不安装 execution control，不保存 RPA control/fence 状态。
纯 SDK（无 Player/测试）也不构建内置宿主支持。新 target 在统一 warning 配置前登记，
实际 compile_commands 确认 MSVC `/W4`、Release `/WX`，以及 MinGW 的 warning/hidden flags。
没有新依赖或新 DLL。字段与调用合同见 [宿主边界 §7](../../../../api/realtime-host-boundary.md)。

ADR 0033 的 detail 禁止项保留。白名单只补登记已安装的 RealtimeClock 和公共 reconciliation
identity 函数。没有允许私有类、没有使用 WINDOWS_EXPORT_ALL_SYMBOLS。

## 验证

fresh configure 与清理构建后的适用门禁、完整受影响 executable 如下；这是局部修复矩阵，
不是最终 F1 未过滤全量矩阵。

| 配置 | 适用门禁 | 完整受影响 executable |
| --- | --- | --- |
| MSVC candidate ON shared Debug | 19/19 | Host 35、线程 Host 26、Playback 154、RPA 52、renderer 2 cases |
| MSVC candidate OFF shared Debug | 18/18 | Playback 121、RPA 52、renderer 2 cases；不运行旧残留的 Host binary |
| MSVC candidate ON static Release `/WX` | 17/17 | Host 35、线程 Host 26、Playback 154、RPA 52、renderer 2 cases |
| MSVC candidate OFF static Debug | 16/16 | Playback 121、RPA 52、renderer 2 cases |
| MinGW candidate ON shared Debug | 19/19 | Host 35、线程 Host 26、Playback 154、RPA 53（含 native128 differential）、renderer 2 cases |

门禁包含 architecture、诊断、版本/合同、适用导出表、独立 installed Playback/Gameplay
consumer 与导入检查。新的 installed consumer 实际调用 owner 状态、无效 fence/未绑定
Pause 和空 Prepared 物化拒绝；新增公共边界测试覆盖 foreign owner、线程拒绝、opaque
token、未确认 proof 和越界 participant/phase。三个 shared 配置各保留 1 项私有 parse-count
probe Skip，按共享库既有限制记录，不改写为通过。线程测试的 assertion 数随轮询次数变化，
不以其总数证明生产延迟。

第一次 MinGW 导出门禁揭示旧检查将 PE 当作 ELF，用 `nm -D` 读取而得到 no symbols。
修正为 `objdump -p` 的 export name table，支持当前与旧 Binutils 格式；required symbols
也只取 export table，不能以 import/debug 名称满足。Linux 的 nm 路径和 MSVC dumpbin 路径
保留。实际 DLL 正例以及 private export、未批准 export、缺少 required export、private import
反例都获得预期结果，已登记为 `cuexis_pe_export_gate_negatives`。

旧矩阵的原始私有导出失败日志保持原位；本次 MinGW 首次失败的命令/退出码/hash
保留在 initial-result 元数据，最终 gates.log 是复验日志，不作为首跑通过。新增宿主
target 的 warning 配置登记位置修正后，再验证
Debug 和严格 Release 的宿主/线程回归。OFF 验证按配置 target 选择 executable，目录里的
历史 candidate binary 不作为 OFF 证据。使用明确版本 DLL 路径，不按目录首个文件推断。

最终实测：MSVC OFF DLL 95 个导出名称、ON 202 个、MinGW ON 184 个，其中 Playback
detail 名称均为 **0**；新宿主测试 executable 的 Playback 私有导入也为 **0**。
格式检查、installed header ASCII、文档、版本一致性和 diff whitespace 检查通过。

## 停止位置

局部 DLL 冲突已修复并验证；改动未提交，owner 可审阅工作树与证据。
实体 B5、生产参数、预算、最终 F1/F2、F4 接受及独立问题继续保持未退出。
按本次明确指令，本轮完成报告后停止，不自动继续这些工作。
