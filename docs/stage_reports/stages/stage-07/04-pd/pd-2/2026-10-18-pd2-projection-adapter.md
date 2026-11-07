# PD-2 Prepared Binding Index and Projection Adapter

日期：2026-10-18。状态：PD-2 内部实施及本地适用门禁完成。

## 1. 范围与源码绑定

本次用户明确要求完成 PD-2。[Desktop 接手摘要](2026-10-18-pd2-continuation-handoff.md)
用于理解背景，不据其后续路线启动 PD-3 至 PD-5。
范围归 [PD 计划](../../../../../stage_plans/active/stage-07/04-pd/plan.md) PD-2
及[设计细则 §3](../../../../../stage_plans/active/stage-07/04-pd/design.md#3-projection-的确定性处理pd-2)。
既有聚合、来源身份、错误和恢复语义继续由 Gameplay Spec、ABI、Playback lifecycle 与 RPA 合同拥有。

实施前基线为 `stage-7` / `d344088236ae589c0b49a4994ef0fbddd6567ce2`。
本批工作尚未提交；该 SHA 是代码对照点，不是新实现的验证身份。没有 push、PR、合并或发行。
既存 Python 缓存未删除、未纳入实施材料。PD-0 冻结输入、程序、正式注册和历史失败样本未覆盖。

本机证据目录为 `out/pd2-verification-20261018/`。9 项实现、测试和门禁输入的 SHA-256
映射、实际依赖及程序身份记录于 `implementation-build-binding.json`，输入副本在
`implementation-inputs.zip`。聚合指纹为
`fe971c4c2f5c68d741cc15706a6c5e67121303cb5f5f15483d8f4399fc7ee065`，不含本报告和状态文字。
证据目录被 Git 忽略；本报告保留复现条件和结论。

## 2. 实现与消费边界

- 新增非安装的 `presentation_projection.hpp/.cpp`，由 Playback 拥有。
  两个 prepare 入口均在候选内建立索引，再随 PreparedPlayback 提交。
  来源键完整比较 requirement 六部分身份（含 emission path）、phase、outcome、timing；
  使用有序树查找，不依赖哈希或散列碰撞。另有 target→布局槽、bindingId→来源成员槽、
  规范化成员集合→组槽。私有支持表明确两个现有 capability ID/revision 和绑定闭集；
  内容工厂原有 capability、别名、组闭包及相反值冲突准入继续有效。
- Adapter 只通过 PD-1 PublicationRead 的读取回调消费新增 ordinal。
  只跳过 stray/consumeEmpty；未知必需 tag/phase/outcome 返回原类错误，不安装候选。
  Fact 顺序保持，匹配行按稳定定义顺序处理。完全重复行折叠，但不同 target、不同 lifetime
  的合法 aliases 保留；token 去重仍为 scope 内 `(FactId,targetId)`，未增加判定身份。
- transient state 分为 cursor、active/waiting effects、future queue、已见成员、组匹配计数和
  rejected groups。future 以 effective T、Fact ordinal、规范化行序索引。
  到期只移除活动效果，成员记忆保留；没有新增 Fact 时仍处理到期、组状态和未来激活。
  groupCommit 同次有效/缺失成员产生原 partial-group，保留成功成员并拒绝后续新成员；
  全缺失不伪造 partial-group。运行时冲突仍拒绝，不以遍历次序选择相反值。
- 命中目标槽后仍核对当前布局 ID/mesh，并在当前 Runtime/World 中核对实体与 Renderable。
  target 缺失只走原纯表现诊断。Runtime 成功前，cursor、published、future、组计数、拒绝标记、
  T 和诊断均留在候选内；失败可重新读取同一成功前缀，不重提已接纳输入。
- query/feedback 引用 prepare 定义，活动状态不再复制整行 binding。
  候选处理顺序与观察代表行选择分别有明确规则：安装前恢复原 Fact/声明顺序，保持重叠同值
  aliases 的公开来源窗口选择。Seek/restore/reset/内容替换仍整体重建 transient scope。
  GameplayOnly、公共 owning query、Replay/Snapshot、同步 FrameEnvelope、控制/fence/设备协议保留。

本批没有更改 Judgement Kernel、Gameplay identity、生产 Spec/ABI、安装头或 SDK 版本。
SDK 仍为 `0.8.0` preview，日期构建为 `26.10.15-1`。
Runtime/World 的完整派生状态事务证明仍归 PD-3-a，不把现有局部回滚当作已证明的完整事务。

维护成本限于一个 Playback 私有适配器和 prepare 索引；没有增加 owner、线程、历史 World 或
第二个 Fact 发布源。每次候选仍复制当前紧凑 transient state，不能据此声称已消除全部复制或取得性能增益。

## 3. 验证

使用 MSVC、Ninja 和 `VCPKG_ROOT=D:/vcpkg`，四个独立目录 fresh configure，构建默认目标。
构建进程内同时设置 ConsoleCP/ConsoleOutputCP 为 65001、`VSLANG=2052`，退出恢复 codepage。
PD 编译插桩 OFF，Debug 沿用 warnings-as-errors OFF，Release 保持 ON。
新目录为 `pd2-debug`、`pd2-shared-debug`、`pd2-release`、`pd2-default-off`；没有覆盖旧正式测量目录。

| 配置/检查 | 结果及限制 |
| --- | --- |
| `pd2-debug` / headless-debug / candidate ON | 完整构建通过；最终普通回归选择 1,056 项，1,055 通过、0 失败、1 项符号链接条件测试跳过；首轮失败另记于 §4 |
| `pd2-release` / headless-release / candidate ON / warnings-as-errors ON | 构建经 §4 的文件共享冲突处置后完成；普通回归选择 1,056 项，1,055 通过、0 失败、1 项符号链接条件测试跳过 |
| `pd2-shared-debug` / headless-shared-debug / candidate ON | 完整构建通过，定向回归 17/17 通过 |
| `pd2-default-off` / headless-debug / candidate OFF | 完整构建通过；普通回归选择 941 项，940 通过、0 失败、1 项符号链接条件测试跳过 |
| PD-2 专属测试 | static Debug、static Release、shared Debug 各 8 项、461 条断言通过 |
| 修正后的 B5 捕获夹具 | static Debug 单项 34 条断言通过，并随最终 Debug/Release 回归通过 |
| static 安装消费者 | find_package、find_package_playback、gameplay_installed_consumer，3/3 通过 |
| shared 安装消费者和导出边界 | 上述三项及 shared_export_surface、shared_consumer_imports、shared_playback_consumer_imports，6/6 通过 |
| 文档、版本、格式及 diff | `check_docs.py`、`update_version.py --check`、全部改动 C++ 的 clang-format 检查及 `git diff --check` 通过 |

专属用例为 8 项、461 条断言，
用例包括完整来源与 timing、原 ordinal、future/expiry、空增量、组成员记忆、partial/全缺失、
稳定拒绝、scope/前缀拒绝、未知必需枚举、相反值、真实 Runtime 失败后表现重试、恢复重建、
来源窗口兼容及既有 prepare 冲突拒绝。损坏枚举通过私有 adapter 读取回调注入；
这不是修改实际 KernelProjection 的故障注入，也不声称 PD-1 原先已有该项证据。

普通回归使用 `ctest --test-dir <build> --output-on-failure --no-tests=error -j 2`，排除
`cuexis_external_consumer|cuexis_gameplay_installed_consumer|cuexis_reference_host_staging|cuexis_.*performance|cuexis_.*consumer_imports`；
适用包门禁单独运行。这是有明确排除项的 headless 本地回归，不是全部 CTest、设备或跨平台矩阵。
既有 golden 和 fixtures 未重新生成。PD-0 注册的正式窗口未再次执行。

四套目录的 Ninja 依赖均非空、完整、实际存在且不晚于对象，测试程序不早于相关对象。
三个 candidate ON 目录中，session/adapter/adapter unit/public integration/B5 fixture/test adapter copy
分别有 147/54/119/113/134/54 项依赖；candidate OFF 的 session 为 125 项。
session、adapter 和直接单测均跟踪新私有头；公开集成及 B5 夹具消费 facade，无需包含私有头。
12 个 Playback 安装头 ASCII 检查通过，安装头 diff 为空。详细对象与程序散列见上述构建绑定文件。

## 4. 失败处置与审查

新增 Runtime/retry 用例初次在 restore 后请求不存在的历史 archive，1/7 失败；
按原恢复合同在 restore 前取得并核验 archive，再比较恢复后的完整结果，没有改生产语义。
失败日志为 `debug-projection-archive-fixture-failure.log`。

只读实现审查发现：合法重叠同值 aliases 的规范行序会改变 source-map 首行选择，
例如声明 `[0,20)` 后 `[0,10)`、T=5，旧查询取 end=20，新候选原先会取 end=10。
现已保留 declarationOrder，并测试两个声明顺序及短窗口到期后的切换。
实际 mutation 暂去声明序保护时，8 项中 1 项在 10/20 比较失败；恢复后 8 项、461 断言通过。
红绿日志为 `alias-mutation-red.log` 和最终 `pd2-debug-projection.log`；审查复核已关闭该问题。

首轮 Debug 普通回归选择 1,056 项，出现一个 B5 宿主测试失败：`captures > 0` 实为 0。
原测试依赖 awaitSession 进入等待泵分支，但回复已就绪时合法地直接返回。
仅修正测试夹具：在已有的“SessionInspect 后、控制 cut 前”的 ClockSource 回调显式执行
同一捕获泵；保留原 captures/cut 断言，并新增该固定注入点恰好执行一次的断言。
修正后单项 34 条断言通过；生产宿主等待代码没有改变。这证明检查后的输入进入 cut，
不据此声称覆盖了等待泵分支。原失败完整回归日志保留为
`debug-regression-before-deterministic-host-fixture.log`；最终回归结果另记，不能改写成首轮全绿。

只读审查和定向复核未发现其他阻断问题；审查本身不替代实测。

Release 首次默认并行构建在 `cuexis_gameplay_packed_tests` 链接后的命令处报告 Windows 文件
共享冲突。检查时没有仍在运行的同路径进程；未确定冲突文件及锁持有者，不能据此断言是防病毒
软件或产品缺陷。原日志保存为 `release-build-file-sharing-failure.log`。未更改源码或构建配置，
以 `cmake --build out/build/pd2-release --parallel 1` 完成剩余 11 个构建步骤，随后专属测试通过。

## 5. 结论与下一批

PD-2 内部实施及上述本地适用门禁完成。当前状态、计划清单和剩余待办已同步；
下一批为 PD-3-a，本次未启动该批及其后续工作。
帧账本实际回收仍归 PD-4；生产预算、尾延迟、长期稳态、真实设备、听音同步、Player/GPU、
Linux/MinGW hosted 及 owner 最终接受均无本批完成证据。Stage 7-PD、RPA 和整体 Stage 7 未关闭。
