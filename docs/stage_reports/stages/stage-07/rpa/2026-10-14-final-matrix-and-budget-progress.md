# Stage 7-RPA 自动矩阵、预算测量与生产验收准备

证据日期：2026-10-14。状态：实施与验收推进记录；B5/F1–F4 尚未退出。

## 范围与源码身份

本轮按 owner 请求继续 [RPA 剩余验收](../../../../stage_plans/active/stage-07/plan-rpa.md)。
owner 随后明确本轮暂不配合实体操作，先推进自动矩阵和预算测量；因此实体键盘、人工
听音和设备切换观察保留待执行。本轮不实施 PD，不接受生产限额，不进行远端发布或关闭。

入口为 stage-7 / `d744fa78fa626053c31f68f2ada20819632235f6`。现场 Git 核对确认，
B3/B4/B5 实施已在 `d7a20d9` 存档，等待优化已在 `74cee7e` 提交；旧 dated 报告的
“尚未提交”描述仅是当时快照。当前修改用 HEAD、逐文件 SHA256 和工作树实现 digest
共同标识，不能只用入口 HEAD 代表新增改动。SDK 仍为 0.8.0；日期构建经标准更新器
更新到 26.10.14-1，版本文件与 vcpkg manifest 同步。

## 实施与问题处置

- RPA 独立 rational oracle 改用仅在测试目录内的四个 32-bit limb 有符号整数运算，
  不依赖生产时间桥，不使用浮点近似，不增加依赖。覆盖 INT64_MIN、宽乘积约分、
  取消、溢出、负数 ties-to-even 和 fake-clock 溢出；MinGW 另以 2,048 组固定种子
  样本对照 native 128-bit 运算。初步 MSVC B1 7 cases / 64 assertions，MinGW B1
  8 cases / 6,208 assertions 通过；后续矩阵另绑定最新源码。
- MSVC Release `/WX` 暴露 Snapshot restore 中同名局部变量遮蔽；仅重命名外层变量，
  不改变恢复算法、分配顺序或诊断，不关闭 warnings-as-errors。
- Release 也暴露测试导出路径的 `getenv` deprecation；测试共享环境读取 helper 在
  MSVC 使用 `getenv_s`，其余平台保留 owning `getenv` 读取。实际 public RPA 用例
  带导出环境变量运行通过，Packed 与 Reference Host 文件真实生成。
- Stage 6 characterization 仍在已演进的 entry/API 文档中要求旧 static profile token。
  检查改为在仍保存该兼容合同的 CXC_FORMAT 中核对旧 token，其余合同检查保留。
- 首轮 `cuexis_cxc_tools` 在非 canonical 包解包目录提交处失败，原错误
  `cxc.unpack.output_commit_failed` 保留；同命令定向重跑通过。原因尚未确定，
  不把重跑通过描述成已修复产品缺陷，不增加自动重试掩盖首次失败。

## 自动矩阵与证据

新增 `tools/run_rpa_acceptance_matrix.py`，每次创建唯一证据目录，记录 configure/build/
CTest 命令、退出码、日志 hash、CMakeCache、二进制 hash 和前后源码身份。
CTest 使用 `--test-dir` 且无 label/name filter，避免 candidate test preset 的默认筛选。
配置行覆盖 MSVC Debug/Release、candidate ON/OFF、static/shared、headless、MinGW。
测试中的设备权限 Skip、失败和缺少的最终 SHA/hosted/人工观察分别保留。

首轮证据：`out/rpa-final-20261014/matrix/20261014-201610-9596cd60/`。
candidate Debug fresh configure、clean-first build 通过；全量 CTest 为 1,185 项、2 项失败、
1 项 Windows symlink 权限 Skip，耗时 513.79 秒。两项失败为上述 CXC 工具和旧 token
检查。随后 Release 严格构建暴露变量遮蔽。本轮修正过程中终止过时的矩阵调度，
保留全部日志；不能将被中断的 shared/后续行或源码改变中的 Debug 复验计作最终通过。

新版本的严格构建预检查与稳定源码矩阵正在执行；最终结果只消费新的 manifest，
不以本段初步运行替代 F1 退出。F1 还需绑定最终行为 SHA 和完整问题处置。

严格 Release 修正后增量预检查通过；完整 RPA executable 为 52 cases / 20,801 assertions，
完整 owner host executable 为 26 cases / 2,079 assertions，全通过。预检查日志为
`out/rpa-final-20261014/release-preflight-recheck.log`、`release-rpa-preflight.log` 和
`release-host-preflight.log`；初次 fresh/clean-first 失败另留原文件，不写成首次即通过。

2026-10-14 现场远端核对：最近 Linux Quality、Windows MSVC、Windows MinGW 的绿色
运行仍绑定 `2ddbcafe22bd3facc63b4917cf99eeccc3d81cd1`。它们不覆盖当前 RPA，F2 未退出。

## 五类预算测量

新增 `[measurement][budget]` 工作负载：1/256 个 Hold、每 Requirement 三个 phase，
一次到期 timer burst 加 32 次空推进，随后 Snapshot encode/decode/recover、Replay
archive/encode/decode/evaluate 和 exact Seek。每次均核对完整 kernel result；记录
prepare、timer burst、稳态 32 样本 p50/p95/max、wire bytes、恢复/Replay/Seek 耗时、
Fact 数和 Replay record 数。既有实际 owner host 测量扩展为 1/256 个 Tap，保留
Packed bytes、准备与 sample/present 成本及完整 Replay 比较。

首轮 Debug 数据揭示：256 个 Hold 产生 768 个 Fact，33 个 advance record 下 Snapshot
为 16,921,746 bytes，Replay 为 16,427,167 bytes。该字节计数不依赖计时负载；首次
恢复和 Replay 求值各约 4 秒，后续并发构建期间更慢。初步时间不是隔离 Release 预算。
修正后 Release executable 首测对应恢复约 357 ms、Replay 求值约 346 ms、kernel Seek
约 128 ms；这仍是首测而非生产 latency target，重复采样与最坏内容证据独立保留。
这是声明的合成规模边界，不能称为生产最坏内容，也不能将 256 定为生产最大 Requirement 数。

| 预算域 | 本轮机器可读测量 | 仍需输入 |
| --- | --- | --- |
| 内容 | Requirement/phase/Fact、准备耗时、host owning Packed 字节 | 实际代表/最坏内容、activity/Pattern 分布、prepare heap peak |
| 稳态 | timer burst、32 次空推进、分位数、owner 请求与 H/frontier/Fold cursor | 长时间历史增长、输入/控制突发、资源/旧 handle 保留峰值 |
| Snapshot/Seek | wire bytes、encode/decode/recover、kernel exact Seek、完整结果核对 | 端到端宿主/设备 Seek、checkpoint 对照、生产 maxSeekLatency、峰值内存 |
| Replay | record/Fact、wire bytes、encode/decode/evaluate、完整结果核对 | 长输入/控制历史、fault prefix、生产 record/element/byte 限额 |
| Packed | host 工作负载 Packed bytes 与 construction/prepare 成本 | Writer/Reader 分项峰值、真实内容与 Stage 8 独立发行 profile |

五域实测值、建议阈值、硬/软属性与超限行为必须分列；建议数值和生产接受当前均为空。
不放宽已有 codec/内容限额，不将 testOnly 容量、观察有效期或尾段期限转正。
现有生产启动对未接受 profile 的拒绝仍保持。M-TIME/M-TAIL 依赖实体输出/校准，
M-SPLIT/M-CAP 还需代表压力与 retained handle 测量；F3 未退出。

## 交接与退出边界

本轮交接输入见 [生产验收交接草稿](../handoffs/2026-10-14-rpa-production-acceptance-handoff.md)。
它包含 7A 既有证据、v4/v5 输入映射、identity/golden/capability、五域预算、迁移/回退、
未选 7B+ 项和问题处置入口。有效 N3、实体 B5、最终 F1/F2/F3 与 owner acceptance
仍是 F4 门禁。实体证据延期不代表通过或取消；PD 也不替代这些门禁。
