# PD-5 阶段 C：原样 40k R4 功能验收阻塞交回

状态：failed / stopped / owner verification pending

本轮原样 40000/low 在首次 `ExecutionHost::advance` 返回
`host.rpa.control_pending`（`Execution host operation is incomplete`，context 为空），
没有达到探针要求的 Executed。R4 编码、实际 Reader/内容构建和 Host load 已通过，
完整功能链路没有通过。按首个配置失败即停止的边界，40000/high 未启动；没有重试、补样、
更换 profile、放宽 deadline/预算或修补产品逻辑。

## 固定基线、改动与构建

开工复核分支 `codex/pd5-gameplay-40k-admission`、HEAD
`58f4e8e380929b4b4c7a0401676ceacc85a589d0`、干净工作树及实际 worktree 映射，
与 owner 指定基线一致；未 reset/clean。
已读 AGENTS、原实施方案第 8–11 节、[阶段 B](2026-10-22-gameplay-admission-stage-b.md)、
[R4 entries](2026-10-22-gameplay-admission-revision4-entries.md)、
[metadata order](2026-10-22-gameplay-admission-r4-metadata-order.md)及指定三个测试/fixture 文件。

固定实施源码提交：`c544e07873d29f4d39bd30c2e1c3a58f3ce77182`。只改五个测试/构建文件：
`tests/pd/CMakeLists.txt`、`pd5_capacity_probe.hpp`、新增 `pd5_capacity_r4_support.hpp`、
`gameplay_admission_evidence.py`、`test_gameplay_admission_evidence.py`。
保留容量构造段原始文本（SHA-256 `aed388b28dffec7394e8bd88c1b8d962722ac92030f4c7f688dac7659a724dfe`）：
40000 entities / requirements、claims、namespace、competition 和 low/high 的 timing.end 差异均未改。
共享 `tests/rpa/execution_fixture.hpp` 及其默认 revision 3 编码未改。

局部显式 `CUEXIS_PD5_CAPSULE_REVISION=4` 使用正式 Writer 委托的同一个 bounded 实现，
然后调用实际 `GameplayContent::fromPacked`；没有另写 codec、绕过 Reader 或直接构造 Host 内容。
探针在操作及断言真正返回后才记录 succeeded；Result 错误保留 code/message/context/cause。
必要的证据校验器拒绝缺步、错误身份、虚假成功和预算/摘要变化。

唯一新 Release 目录：`out/build/pd5-stage-c-release-20261022/`。
新证据目录：`out/pd5-stage-c-20261022-58f4e8e/`。
两目录使用前均核对不存在；未复用历史目录。
全新 Release fresh/clean-first，STATIC、warnings-as-errors、Chart-v5 candidate；
真实 allocation lifetime observer 开启，PD metrics/trace harness 关闭。
使用已核验的 MSVC showIncludes override；实际探针 object 的依赖数为 225，状态 VALID。
编译身份在源码 SHA 固定后配置，实际运行 exe 内嵌该 SHA，并与运行环境相互核对。

实际 exe SHA-256：`b9341483f72f5720c31bdf728dfea1dba80047061706913c713d373951e3062a`。
源码 2045 个文件、构建 34 个载荷和配置在验收前封存；验收期间不修改源码/构建输入。
交回提交只增加本报告并更新 PD 索引与 CURRENT_STATUS，不代表重新构建或新的被测源码。

## 唯一一次 40k 运行与真实到达状态

实际测试名由 Catch 发现结果确认：`PD5 full content Snapshot Seek Replay boundary`。
`ctest -N` 原始发现记录保存，架构测试名也由实际发现核对。
第一次查询误用了当前 Catch 不支持的 `--list-test-names-only`，保留失败输出；
依据该二进制帮助改为 `--list-tests '*' --verbosity quiet`。该查询没有执行测试，
不构成 40k 重试。构建没有编译期探针失败。

运行命令：`cuexis_pd_host_probe.exe "PD5 full content Snapshot Seek Replay boundary" --reporter compact`。
进程 PID `2424`，明确环境 `CUEXIS_PD5_ENTITIES=40000`、`CUEXIS_PD5_REUSE=low`、
`CUEXIS_PD5_CAPSULE_REVISION=4`、`CUEXIS_PD5_EXPECTED_SOURCE_SHA=c544e07873d29f4d39bd30c2e1c3a58f3ce77182`。
原件输出和冻结参考目录均显式绑定；没有继承其他 PD5 运行配置。
开始 `2026-10-22T22:42:45.990114+08:00`，进程被终止并由 runner 收回于 `2026-10-22T23:50:31.543552+08:00`，
OS 退出码 `4294967295`（强制终止码，不是原始功能错误码）。
首次观测失败记录在 `failure-observed.json`；正常退出等待超过 25 分钟仍未结束后，
再次核对 PID、exe 路径、完整命令行、失败 stage 和 high 未启动，保存终止前原始日志快照，
然后终止这一个已失败的独立探针进程。`forced-stop.json` 保存终止原因、时间、PID/命令及
快照哈希。没有将等待或终止包装成第二次运行，也没有重新调用 advance。
完整保存截至终止的原始输出；Catch 正常结束汇总与析构清理完成记录没有产生，
不能声称正常退出、显式 close、after-close checkpoint 或生命周期通过。

| 操作 | 40000/low 实际状态 |
| --- | --- |
| 固定身份、原样 fixture、before-load memory | 通过，真实 checkpoint 已输出 |
| R4 encode、Header/预算/冻结原件核对 | 通过 |
| GameplayContent::fromPacked | 通过实际 Reader、内容重建与 prepare |
| frame budget、Host create、Host load | 通过 |
| 首次 Host advance | 失败：host.rpa.control_pending；未取得 Executed |
| query、Snapshot/encode/decode | 未到达，未写成功 |
| archive、evaluateReplay、sameResult | 未到达 |
| pause、replayCut、seek、sample、present、seek 后 sameResult | 未到达 |
| active checkpoint、显式 close/reset、after-close checkpoint | 未到达，不以析构清理替代 |
| 40000/high 全链路 | low 失败后未启动 |

探针保留原有 `replyDeadline=30s`。该错误码可由多个控制/等待路径返回；当前 stage 证据没有
内部 job/子步骤诊断，不能只据错误码证明具体超时路径、主要热点或完成状态。
本轮不追加产品诊断或修复。需要 owner 核验的具体阻塞是：原样 R4 low 在既有 Host 条件下
首次 advance 不成功，因此无法进行要求的后续功能验收。

## 已生成 low 原件、身份和预算核对

low 原件完整 SHA-256：`f03936693c13d0517cb8dd7c91e13977b996f6f37457da2cadb1eed6a627a301`。
语义摘要：`78c23507481084ba7aa900efd56f72065992d8ad2409f657fcf194784252038d`。
Header revision=4，entities=requirements=40000；文件与前轮已核验 R4 原件逐字节一致，
完整语义摘要、各 section 的 bytes/records 与冻结 Stage A 一致；Header 和全部 section CRC 有效。
独立只读 `audit_wire.py` 不执行 Reader/Host，也不重跑配置。

| 七项物理预算 | low 实际值 | 原上限 |
| --- | ---: | ---: |
| file bytes | 6818424 | 16777216 |
| decoded bytes | 6817880 | 16777216 |
| 最大 section bytes | 3725942 | 16777216 |
| strings | 40048 | 100000 |
| references | 13 | 100000 |
| entities | 40000 | 40000 |
| requirements | 40000 | 40000 |

Writer 本次观测 semantic bytes=99588531、scratch peak=7145909、SHA=1、
dynamicLiveBytes=0、exportedBytes=0；受限核心 budgetAccepted 为真。
四维 accepted ceilings 保持 `134217728 / 1000000 / 4096 / 8388608`。
Ix/key 由原 bounded 核心按既有 accepted profile 强制检查；本次未新增实际 Ix/key 高水位字段，
不把上限冒充实测值。实际 Reader 已通过原有有界入口，本轮没有独立记录它的 scratch 高水位；
阶段 B 的 Reader scratch/单次 SHA 证据保持原报告，不能冒充本轮新增观测。
high 的本轮原件和统计不存在，不用冻结的 high 预期值替代实跑。

## 生命周期观测与验证范围

本次 40k 只有 before-load checkpoint：live_count=1000311，requested_cpp_bytes=130920139，
CRT storage=194940043，exceeded=false。没有 active/after-close checkpoint，也没有完整生命周期
通过结论。原观察器覆盖 linked C++ new/delete（含 aligned/array/nothrow），不覆盖直接 malloc、
SDL、driver 或 OS heap；该过程内存观测不是 hash-work 8 MiB scratch。

失败前的小规模 plumbing 仅运行 1/low 和 64/high，两次均完成实际 R4/Host/Snapshot/Replay/
Seek/present/close 链路和三个真实 checkpoint。它们不替代 40k。
这些 after-close checkpoint 仍保留 fixture、packed、content、expected、snapshots、replay、
evaluated/cut 和 renderer；仅 frame 清空、Host close/reset，不证明所有内容已释放或零泄漏。
40k 未到 Seek，因此没有本轮 40k seek 耗时。小规模已有时间戳仅为功能观察，未设阈值或
作为性能/门禁通过结论。

| Release 默认非隐藏回归 | 用例 | 断言 | 结果 |
| --- | ---: | ---: | --- |
| cuexis_gameplay_packed_tests | 45 | 5476 | 通过 |
| cuexis_playback_tests | 174 | 28011 | 通过 |
| cuexis_chart_tests | 227 | 42425 | 通过 |
| cuexis_judgement_tests | 216 | 35624 | 通过 |
| cuexis_cxc_tests | 64 | 975 | 通过 |
| cuexis_rpa_tests | 52 | 20801 | 通过 |
| cuexis_host_protocol_tests | 59 | 121916 | 通过 |
| cuexis_execution_host_tests | 33 | 36079 | 通过 |
| cuexis_pd_host_probe | 7 | 3546 | 通过 |
| 合计 | 877 | 294853 | 通过 |

以上回归在固定源码/构建上于 40k 前完成；PD Host 默认用例含真实 lifetime-observer 所有权测试。
Python 新证据方法 4、既有 feasibility 方法 5、测量方法 13，共 22 项通过。
架构 CTest 1/1、全仓 cuexis_format_check 通过；报告/索引/当前状态写入后的 docs 与 diff 检查通过，
实际输出封存在 `docs-final.log` / `diff-check-final.log`。
独立静态两轴审阅：Standards 0、Spec 0；不将静态审阅当作运行验收。

本轮没有运行全仓 CTest、Debug/shared/外部 consumer/Linux 矩阵，也没有重跑六配置 wire-only
专项；产品 Reader/解码顺序未改。本轮的两个小规模完整 plumbing 和一个失败的 40k low
分别如实登记，不能宣称全矩阵或 40k high 已通过。

## 封存、完整性与停止

原始材料含 `acceptance/low/process.log`、新 `input.r4.packed`、启动环境/PID/退出状态、
失败首次观察、强制停止记录和终止前日志快照、实现 diff、原样构造证明、Release/方法/架构/格式/文档日志、静态审阅、
`source.zip` 与 raw-byte inventory、`build-payload.zip` 与实际二进制绑定、冻结清单及独立审计脚本。
最终 `receipt.json`、`packet-index.json` 和 `packet.zip` 绑定交回提交与全部载荷；
`verify_packet.py` 只读复核，不执行测试或改写证据。

封存前后核对 11 个既有历史证据根目录的 6695 个文件，保持原件不变；主树 F1
`eed36648a692333e47f798aa45be36ca850d08de` 与独立 C2
`747ef18dc8ba3933f0e9b68cee7cbf8a2ee6e2c6` 均保持干净、不变。
产品源码、旧 golden/fixture、共享默认 R3、七项物理预算、四维 ceilings、SDK/API/版本未改。
最终固定源码与构建载荷复核、docs-only 交回差异及原始文件哈希见 provenance audit/receipt；
最终只读核验须通过后交回。

阶段 C 本轮失败并停止交回 owner。不能声明“原样 40k R4 profile 功能链路通过”，
更不代表性能接受、PD-5 完成、RPA/生产接受或 Stage 7 关闭。
未做性能优化/正式测量、preparation/registration、其他候选、主树合并/推送或 SDK 升级；
旧窗口 3 继续停止，原 1.10/15% 与全部生命周期规则保持，等待核验后另行决定后续。
