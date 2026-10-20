# PD-3-a Runtime and World Software Transaction

日期：2026-10-18。状态：PD-3-a 内部实施、补正门禁与 GPT-6-Luna 复审通过。

2026-10-19 续接核对：98 项既有证据文件、31 项实现输入和记录的程序散列均未变化，
沿用本报告已有 C++ 实测结果；未重复运行构建或 CTest 矩阵。状态日期更新后重新执行文档、
版本、格式、diff 及证据保护检查。

## 1. 范围与对照点

用户在 PD-2 之后明确要求继续完成 PD-3-a。本批仅落实
[PD 计划](../../../../stage_plans/active/stage-07/plan-presentation-decoupling.md)的事务子批次，
依据[设计细则 §5](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md#5-软件事务与失败恢复pd-3)。
PD-3-b 状态修订/PresentationRead/物化器、PD-3-c 控制绑定以及 PD-4/5 未启动。
没有接受宿主候选合同、更新 SDK/构建版本、提交、push、PR、合并或发行。

基线仍为 `stage-7` / `d344088236ae589c0b49a4994ef0fbddd6567ce2`，工作树包含此前未提交的 PD-2。
`out/pd3a-verification-20261018/pre-pd3a-workspace.zip` 保存实施前 13 项未提交输入；
`pre-pd3a-state.json` 保存路径散列与 Git 状态，`pre-pd3a.diff` 保存 tracked 差异。
因此 PD-3-a 可与本次开始前的 PD-2 工作树单独比较，不把 HEAD 冒充新实现身份。
旧缓存、PD-0 冻结程序/输入/注册、历史慢样本与失败样本均保留。

最终实现指纹与构建绑定见本机 `out/pd3a-verification-20261018/implementation-build-binding.json`；
输入副本见同目录 `implementation-inputs.zip`。证据目录由 Git 忽略。
31 项实现/测试/构建输入的聚合 SHA-256 为
`2a822ae8f10695ecf80f9edab629ec3ecf4b4ed975176c4ae0b5371ccbc708e6`，
覆盖保留的 PD-2 与本批改动，不包含本报告和状态文字。

## 2. 实现与维护边界

World 新增非安装的 `TransformUpdate`，从原 `updateWorldTransforms` 提取 prepare/apply，
沿用原父子索引、拓扑排序、局部组合、dirty 传播及有限矩阵检查；没有第二套层级求值器或 World 克隆。
prepare 只读活动缓存并计算候选，记录已验证的局部/派生槽位。热路径复用 scratch，
安装只写受影响值与缓存索引。冷路径准备拓扑与仅 `WorldTransformComponent` 的 typed storage；
不在活动 registry 预插占位，不提前改变组件 presence。

冷安装交换 EnTT `basic_storage` 的 payload，保留 registry owner 与 signal 对象。
不能交换整个 signal mixin，否则会替换观察者连接。存在 construct/destroy 观察者或 EnTT group 时，
准备阶段以既有 `world.callback.exception` 类错误拒绝结构替换，不静默破坏 group。
单独的 `updateWorldTransforms` 沿共享 prepare 算法保留既有 signal 发布路径，
其外部回调不承诺事务回滚。World 私有持久计划复用准备空间，入口在复用前拒绝重入。

Runtime 在写入前准备 resolver 输出、Camera/Appearance 槽、材质字符串与 debug 记录。
共同 `installRuntimeState` 为 `noexcept`：安装 Transform/派生矩阵/缓存，直接赋值 Camera，
交换已准备材质，发布 resolver commit 历史，最后扣减/清理 lifetime。
正常 update、Gameplay update 与 base-property 重采样均使用同一边界；Runtime 不再依赖局部
Transform/Camera/Appearance rollback 清单恢复派生状态。既有 standalone PropertyResolver API 保留。
计划与表现槽引用在退出窗口时清理，失败不跨窗口保留可使用的裸槽引用。

reload 的候选采样显式传入目标 World，准备期间不换出活动 World；
分配或验证失败不能留下新 World 搭配旧 entity map/evaluation/lastFrame。
base-property 在候选 EvaluationState 更新基线，成功后交换，失败不发布 baseRevision。
普通 update 复用工作空间，没有为稳态路径新增整份 EvaluationState 拷贝；
Gameplay 原有候选状态拷贝和 base-property 冷路径拷贝仍存在，不声称消除全部复制。

Playback 在 Runtime 成功后连续调用私有 `noexcept finalize`，仅移动已准备投影/诊断并赋值
last frame/session state；该尾部无捕获、分配、外部回调或 stale 检查。
seal/Fold 仍是此前的独立成功点。Snapshot 提取继续位于安装后的只读交付面。
两个 extractFrame 入口现在把分配等异常转换为已有 preparation 错误族，标记
`operation=extract_frame`，不修改已安装状态。失败的 reusable destination 可能部分填充，
调用者必须丢弃失败输出；本批不承诺目的缓冲原样保留。
没有增加 owner、线程、历史 World、第二个 Fact 发布源或安装接口。

## 3. 验证

四个独立目录使用 MSVC/Ninja fresh configure，headless static Debug/Release、shared Debug
以及 static Debug candidate OFF。`VCPKG_ROOT=D:/vcpkg`，PD 编译插桩 OFF；
Debug warnings-as-errors OFF，Release ON。构建进程同时设置 ConsoleCP/ConsoleOutputCP 为 65001、
`VSLANG=2052`，退出恢复 codepage；核对实际 Ninja 依赖、源码/对象/程序时间及散列。
没有覆盖 PD-0/PD-2 程序。SDK 仍为 `0.8.0` preview、日期版本 `26.10.15-1`。

| 配置/检查 | 结果及限制 |
| --- | --- |
| `pd3a-debug` / candidate ON / static | fresh 构建通过；最终普通回归 1,067 项，0 失败、1 项符号链接条件 skip |
| `pd3a-release` / candidate ON / static / warnings-as-errors ON | fresh 构建通过；最终普通回归 1,067 项，0 失败、1 项同条件 skip |
| `pd3a-shared-debug` / candidate ON | fresh 构建通过；既有定向回归 17/17 通过；新事务用例单独实测，见下文 |
| `pd3a-default-off` / candidate OFF / static | fresh 构建通过；普通回归 951 项，0 失败、1 项同条件 skip |
| static 安装消费者 | find_package、find_package_playback、gameplay_installed_consumer，3/3 通过 |
| shared 安装消费者与导出边界 | find_package、find_package_playback、gameplay_installed_consumer、shared_export_surface、shared_consumer_imports、shared_playback_consumer_imports，6/6 通过 |
| 文档、版本、格式、diff、旧证据散列 | check_docs、update_version --check、全部改动 C++ 的 clang-format 检查、git diff --check、PD-0 保护散列与保留 PD-2 输入检查通过 |

World 的 5 项新用例：Debug/shared/OFF 各 234 条断言，Release 243 条。
Runtime 的 4 项新用例：Debug/shared/OFF 各 5,635 条断言，Release 6,195 条。
candidate ON 的 Playback `[pd][transaction]` 各 2 项、127 条断言通过，其中一项为保留的 PD-2 重试回归。
allocation suite 的 8 项在 static Debug 为 392 条、OFF 为 386 条、Release 为 334 条断言通过；
shared 为 6 项通过、2 项 allocator 边界显式 skip、221 条断言通过。
这些计数不是性能采样，也不以 Debug/Release 注入点数量不同推断行为差异。

四套目录实际 Ninja 依赖均非空、文件完整且不晚于对象。candidate ON 的 session、World plan、
resolver、Runtime、World test、Runtime test、allocation test、Playback 集成分别跟踪
147/74/75/106/118/135/101/113 项依赖；OFF 的 session/allocation test 为 125/95 项。
私有事务头和交付测试头均在相应依赖中。shared 增量链接可能只更新 DLL 而保留 consumer EXE，
绑定检查因此比较实际 Playback DLL 的时间和散列，并同时记录 EXE，不用 EXE 时间误判 DLL 过期。
Playback 的 12 个安装头 ASCII 检查通过，安装头 diff 为空。

新增测试全部位于 `tests/pd/`，编入既有 World、Runtime、Playback 与 allocation targets：

- World：首次派生组件、删除后重建、陈旧组件移除、父子矩阵、缓存有效位和完整拓扑对比；
  prepare/cancel、不分配的 apply、信号连接保留、真实 owning group 拒绝及连续热态零分配。
- Runtime：normal/Gameplay、首次/已热身、稳定/冷拓扑组合逐分配失败；
  比较活动局部/派生矩阵、完整缓存、Camera/Appearance、last frame、debug、baseRevision 与 token/lifetime。
  覆盖已准备拓扑后的缺相机槽拒绝、基线重采样溢出与同请求重试。
- reload：预先 prepare 更大候选，再向生产代码同一候选采样窗口注入分配失败，
  比较旧活动状态，重试候选并执行正常 public reload。测试桥非安装，不参与决策。
- Playback：有限局部 scale 的父子乘积溢出，验证 seal/Fold 已成功、失败阶段 Runtime、
  runtimeUpdated=false、旧投影 scope/source map 与 Snapshot 保留；用 HostOverride 修复后
  不再 submit/advance Kernel，复用已成功前缀，与干净控制、FrameDigest 和 Replay 精确比较。
- Delivery：更新成功后提帧失败不撤回已安装状态；同帧提取重试保持 digest，RemainingFrames 不重扣。

MSVC Debug 注入排除整个迭代器代理大小类，因该类可能在 STL 的 `noexcept` 构造函数内分配；
不能将 Debug 计数解释为完整分配点。Release 对上述事务操作及 reusable extraction 注入全部实际分配点。
Debug 的安装后交付失败通过非法 viewport 确定性触发；shared Playback DLL 不在测试 EXE allocator
拦截范围，相关用例明确 skip，不能作为 DLL 内分配故障证据。

普通回归明确排除安装消费者/host staging/performance/imports，适用安装门禁另跑。
其命令为 `ctest --test-dir <build> --output-on-failure --no-tests=error -j 2 -E`
`cuexis_external_consumer|cuexis_gameplay_installed_consumer|cuexis_reference_host_staging|cuexis_.*performance|cuexis_.*consumer_imports`。
这不是全部 CTest、Linux/MinGW hosted 或真实 GPU/音频/窗口矩阵。未重新生成 golden/fixture，
未重新运行 PD-0 正式测量窗口，不据本批作性能收益、预算或长期稳态结论。

## 4. 崩溃、审查和未关闭事项

用户截图中的 `cuexis_world_tests.exe abort()` 来自逐分配故障注入命中 MSVC Debug
`vector` 默认构造内部迭代器代理分配；该构造 `noexcept`，异常导致 terminate。
已取得符号调用栈并修正 Debug 注入范围，未关闭断言或更改生产错误语义。
原日志和栈为 `debug-world-original-abort.log`、`debug-world-abort-symbolic-stack.log`。
完整注入结果以 Release 为准，不将该崩溃改写成初轮全绿。

Release 提帧注入最初在 2 个分配点均发现 `bad_alloc` 越过 API 边界，
红日志为 `release-post-install-extraction-exception-red.log`。修复两个 extractFrame overload
的异常边界后，原分配失败与 digest/lifetime 检查通过；没有增加安装后的求值或回滚。
独立只读复核确认该处理只影响交付，且指出失败 destination 不具备强保留保证。

无上下文的只读子代理发现两项问题：reload 临时换 World 窗口缺少异常恢复；
standalone 每次新建 plan 导致稳态分配。分别改为候选 World 显式参数和 World 持久 scratch，
增加针对测试；子代理复核确认原反例已消除。静态审查不替代实测。

整条 reload 的最初宽扫还触发既有 Assets 问题：`ResourceScope` 构造函数声明 `noexcept`，
内部 `std::map` sentinel 仍会分配，OOM 时 terminate。栈保存在
`debug-runtime-reload-stack.log`。本批未修改 Assets 生命周期；该问题保持开放。
因此本批 reload 注入只证明新增候选采样边界，不证明资源加载及 public reload 全链路所有 OOM 点；
测试桥也未覆盖 public reload 外层 catch。失败前活动 World 从未换出这一性质由实现结构保证。

初期编译和测试夹具错误的日志原样保留：storage 迭代项类型、Runtime plan 成员位置、
测试初始化括号、交付测试 include、父引用 JSON 类型。最终成功结果另存，不以清日志消除失败历史。

## 5. 剩余边界

PD-3-a 内部实施及上述本地适用门禁完成。其结束仅覆盖内部软件安装事务及上述适用门禁。状态修订/PresentationRead/唯一物化器
仍归 PD-3-b，控制许可与保留画面身份仍归 PD-3-c，帧账本实际回收仍归 PD-4。
生产预算、尾延迟、实体设备/听音、跨平台 hosted 和 owner 最终接受无本批关闭证据。
Stage 7-PD、RPA 与整体 Stage 7 未关闭。


## 6. 2026-10-19 独立审核重新打开的门禁

用户要求不继承上下文的 GPT-6-Luna 审核，通过后才能进入 PD-3-b。代理独立读取合同、
源码和证据，给出 FAIL：§4 披露的 ResourceScope 构造 OOM 属于 §5 的候选准备范围，
不能用范围说明替代失败恢复。此前完成结论据此重新打开，PD-3-b 未实施。

修复把 ResourceScope 的 entry vector/map 归一个私有 Storage owner，构造允许分配失败，
noexcept move 仅移动 weak/unique owner，不再在安装窗口构造或移动 map sentinel。
增加公共 resource-backed reload 的逐分配失败恢复/重试，以及 populated scope move 零分配测试。
本批复验使用独立 `pd3a-fix-*` 目录，原 `pd3a-*` 程序与所有旧证据原样保留。
原工作树快照、审核 FAIL 和新证据位于 `out/pd3a-audit-fix-20261019/`。
新实现指纹为 `16e3d853f91f4d08fba117bd64e23bd50e4869b00f7d7850c41cef1b581627cf`。
Debug/Release 普通回归各 1,069 项、OFF 953 项均零失败、各 1 项条件 skip；
shared 定向 17/17、static 消费者 3/3、shared 消费者及导出 6/6 通过。
Runtime 事务组扩大为 6 项，Debug/OFF 9,426、Release 10,101、shared 9,380 条断言通过。
文档、版本、格式、diff、PD-0 保护散列和原 PD-2 六项输入保护检查通过。
OFF 初次并行构建出现未确定锁持有者的文件共享错误，原日志保留，未改源码的串行续建完成。
四套新构建的源码/对象/程序身份与完整实际依赖已绑定；shared allocator 限制仍与 §3 相同。

GPT-6-Luna 对修复和上述汇合证据给出明确 PASS，允许启动 PD-3-b。
§4 的 ResourceScope OOM 阻断由本节修复关闭，原失败、原程序及旧门禁记录不改写。
此结论不是通用物理内存耗尽恢复、跨平台、设备或 Stage 7 最终接受。
