# S7A-3/4 limited functional acceptance

状态：受限功能验收完成，容量整体证明未完成；S7A-9 保持 INCOMPLETE GATE

日期：2026-10-05

## 1. 范围和输入锚点

依据 [A–G 实施交接](../../../archive/stage-07-planning/s7a-3-4-implementation-handoff.md)。
本次消费已选定的 execution / author profile，实现 S7A-3 剩余静态与离线 author 工作，以及
S7A-4 的 ingress、contact、T4/K4、timer、Fact 和事务 kernel。
逐行合同证据见 [E1 / D–F 审计](2026-10-05-s7a-3-4-e1-audit.md)；
[暂停报告](2026-10-05-s7a-3-4-implementation-progress.md) 保留为此前快照。

分支 `stage-7`；基线 HEAD `6b11d102f6dcac4d9782cd03d0bae15add54af28`。
本报告首次验收锚定提交前工作区指纹；包含本报告的实际 Git commit 作为 owner 后续授权推送的锚点。基线 SHA 不代表新增行为通过 hosted。
最终 65 个变更实现、测试、Schema、CMake 输入的 path+SHA256 manifest 为
`out/s7a-3-4-behavior-inputs-final.json`；指纹
`6556ae0e3aca01389fb722f5837febdfffc899d8ede210f2f2e9364aeef0efda`。
算法是按 path 排序的 `{path,sha256}` 数组、ASCII compact JSON、sorted keys、末尾 LF 的 SHA256。
原暂停报告的 57 输入指纹只描述暂停时快照，本次使用独立 final 文件。

上述功能矩阵验收时版本为 `26.10.04-1`，SDK API `0.7.0`。保留原有设计文档。功能收口后按新增授权实施 CI 优化，见 [独立报告](2026-10-05-ci-runtime-optimization.md)。

## 2. 实现与合同证据

| 卡 | 交付 | 验证入口 |
| --- | --- | --- |
| A | 完整 execution fields、两路 identity、immutable compiled program、Capsule candidate revision3、稳定诊断 | execution prepare/identity mutation、Capsule round-trip 与 hostile bytes、旧 Writer/Reader revision 拒绝 |
| B | json_support owning DTO、严格私有离线 grammar/Schema、真正 CXT frozen Binding expand、affine mixed-radix、common assembler | 2×3 双路 canonical 等价、prototype、source permutation、unused definitions、多/零 requirement、zero Repeat、原子失败 |
| C | Capsule §11 八组与 §12 增量逐项 IV/IU/M/LB 台账 | E1 报告逐条给出消费点、具体 case 与断言；容量另记 LB |
| D | ingress owning journal、reserve-before-publish、L1 late routing、独立整数 L2 oracle | O/C/F/H、负 Tick/i64 两端、duplicate precedence、原/dispatch Tick collision、整批 admission 回滚 |
| E | owning prepared/config/query、physical contact、Pattern prefix、T4 phases、K4 groups、owner release、Free/Held/Terminal | 半开 window、head late/repeat、body/tail/zero grace/no-tail、held 不抢占、priority/tieRank、independent group、observe/stray/consumeEmpty |
| F | immutable timers/index、独立 S1 source rescan、tagged Fact/ordinal/IDs、t+1 signal、TickDraft seal | timer-before-input、逐 prefix 结构对照、固定 seed traces、one-shot/segmented、四失败点、MIN Tick、ID MAX、error/signal overflow |
| G | 完整构建/CTest、static/shared external consumers、四编译器、golden、格式/文档/架构 | 见 §3；工作区指纹与基线 commit 分开 |

本次修复的配置诊断区分 incomplete 和 unsupported；不完整 mandatory profile 走
`identity_closure_incomplete`，非空未知 token 走 `capability_disabled`。
诊断表仍为九类，R19 carrier 保持；只允许 Ruleset/kernel transaction failure fault session。
五份独立临时负表（重复、category、kernel faulted、foreign faulted、carrier）都以 exit1 被拒绝。

两项 author 合同缺口的最小反例与修订记录在
[author profile](../../../formats/gameplay-v2-author-profile.md)：现有基础 Reader 不支持 v5 inline JSON，
故补显式私有离线 grammar；Repeat 生成实例身份使用稳定 join，解决模板 localId 与实例声明名冲突。
这不发行 Chart v5/CXT v2 生产 Reader。

## 3. 本地验证矩阵

Debug 已执行 fresh configure 和完整 build；Release 已执行 fresh configure / clean-first 完整 build。
工具链实测 MSVC19.51.36256.0、MinGW GCC16.1.0、Linux GCC15.2.0、Clang21.1.8；
元数据保存在 `out/s7a-final-toolchains.json`。
本轮 Clang 将预安装 Linux 依赖复制到 `/tmp/cuexis-stage7-clang-deps`，避免 GCC clean consumer
删除其 `ec/as` 目录时影响 Clang；独立 TMPDIR 和 LF 脚本在 `out/s7a-clang-resumed.sh`。
GCC 使用 `out/s7a-gcc-resumed.sh`。两个 Linux 配置三组 case 名称已与 Windows 清单逐个核对一致，
见 `out/s7a-final-linux-label-manifest.json`。
完整 CTest 使用 VS Developer 环境、`--no-tests=error --output-on-failure -j1`，各配置独立 TEMP/TMP。
最后测试源冻结后再编译受影响 targets，并补最终全部三 label 复跑。
四个 Windows 配置三组 case 名称集合逐个相同，清单存于 `out/s7a-final-label-manifest.json`。
shared 的三个 skip 是 Windows symlink、Playback parse-once instrumentation 和 owning-copy allocation instrumentation；
静态 Debug/Release 的后两项实际运行通过，不将 shared skip 记为已执行。

| 配置 / 检查 | 结果 | 本地日志 |
| --- | --- | --- |
| Windows MSVC Debug 完整 CTest | 943/943，0 failed；1 Windows symlink skip；995.69 s | `out/s7a-acceptance-debug-final.log` |
| Windows MSVC Release 完整 CTest | 943/943，0 failed；1 Windows symlink skip；789.28 s | `out/s7a-acceptance-release-final.log` |
| Windows MSVC Debug 最终三 label | 187/187，33.90 s | `out/s7a-final-focused-debug.log` / `.xml` |
| Windows MSVC Release 最终三 label | 187/187，11.19 s | `out/s7a-final-focused-release.log` / `.xml` |
| Windows MSVC headless shared Debug | 858/858，0 failed；3 documented skips；684.47 s | `out/s7a-acceptance-shared-final.log` / `.xml` |
| Windows MinGW GCC headless Debug | 855/855，0 failed；1 Windows symlink skip；1044.47 s | `out/s7a-acceptance-mingw-final.log` / `.xml` |
| Linux WSL GCC Debug headless | 恢复后完整853/853，0 failed/skip；3393.81 s | `out/s7a-acceptance-gcc-resumed.log` / `.xml` |
| Linux WSL Clang Debug headless | 恢复后完整853/853，0 failed/skip；2090.93 s | `out/s7a-acceptance-clang-resumed.log` / `.xml` |
| Linux GCC / Clang 最终三 label | 各187/187，0 failed/skip；17.41 /17.18 s | `out/s7a-final-focused-gcc.log` / `.xml`、`out/s7a-final-focused-clang.log` / `.xml` |
| labels 各自 -N / 实际运行 | judgement=164（含诊断脚本），gameplay_packed=16，gameplay_author=7 | focused XML 和 CTest 清单 |
| 最终 execution，seed740304 | 14,344 assertions / 39 cases | `out/s7a-final-freeze-execution.log` |
| 最终 author，seed740304 | 300 assertions / 7 cases | `out/s7a-final-freeze-author.log` |
| 最终 Capsule，seed740304 | 1,711 assertions / 16 cases | `out/s7a-final-freeze-packed.log` |
| S2 source failures，seed740304 | 141 assertions / 1 case | `out/s7a-final-freeze-s2.log` |
| format / docs / status / target / version | format exit0；362 Markdown /20 examples；status4/4、target2/2；version consistent | `out/s7a-final-format.log`；最终 docs、status、target、version 复验均通过 |
| whitespace / conflict / architecture | diff-check 和含 untracked 的 96 路径扫描 0 问题；architecture 完整矩阵通过 | full CTest 与显式扫描；9 个变更公共头全 ASCII |

### 3.1 同输入字节与 preimage golden

| candidate revision | bytes / SHA256 | structural preimage / SHA256 |
| --- | --- | --- |
| 2 | 3264 / `471e5cefc7cd2b1376163ad87cf1b1d7557f624252fe2cef707b7af1a9774e7c` | 4528 / `82765a403f7fe3c0b820aae1e3ca699793fe206223278d7a2c33606121b7c9ff` |
| 3 | 3370 / `ff1ff218925f2f8c684505651b9170c46ce2f386091fd3da34a8bdfe99e18bb4` | 4793 / `8c5c41467cc986974d038eb66d5836198918ffe245adf51d8e575f156bf7a8ca` |

这些是 Catch2 中固定 bytes/hash 的断言，不将不同工具链各自产生 hash 再互相比对冒充 golden。
revision1 Reader 拒绝 2/3；2/3 Reader/Writer 的上下文互拒也纳入同套测试。

### 3.2 首次失败和处置

- 完整静态外部消费者首次失败发现 manifest consumer 改变 feature 后共用 producer 安装目录，
  会卸载 SDL 并使并行/后续 staging 找不到 license。修复
  `cmake/VerifyExternalConsumer.cmake`：每个 clean manifest consumer 用独立 vcpkg install root。
  最终 Debug/Release static 与 shared consumer 检查复验；没有修改 CI。
- Windows 原有 ResourceFixture 的固定临时目录在并行 CTest 间冲突。最终用 serial CTest 与
  各配置独立 TEMP/TMP；这不是 kernel 错误，未改写资源模块。
- Capsule hostile-byte 新测试最初误将 table-count 当作第一行字段，capacity 断言读到 7。
  修正 fixture 的 GPD0/GRC0 游标；最终 Capsule 16/16、1,711 assertions 通过。
- MinGW 首次 audio-sdl find_package consumer 的 vcpkg registry HEAD 查询失败；最终同门禁重跑。
- WSL Git 不识别 Windows worktree 路径，CFU-F3 先前 configure 被阻塞。配置明确提供真实基线
  `CUEXIS_IMPLEMENTATION_SHA`，新行为另由上述 manifest 记录。
- Clang 直接配置的 external consumer 未继承本机依赖 prefix，最初 tl-expected 发现失败；
  补 prefix 后还发现 SDL3 被解析到 Windows MinGW 包（消费者 SegFault），以及直接配置没有提供
  license 安装根（package 缺 entt-copyright）。同一消费者换成 Linux SDL3 后运行通过，源码不变。
  停止该环境不完整的矩阵，最终配置显式提供 Linux prefix、VCPKG_INSTALLED_DIR / triplet、
  manifest OFF 与独立 TMPDIR，启动完整重跑；暂停时只完成前5项，不删 consumer gate。
  最终 Clang 是读取预安装 Linux vcpkg 依赖的直接 CMake 配置，GCC 是 vcpkg toolchain 配置。
- CXC tools 首次 unpack staging commit 一次失败；单独原门禁重跑通过，随后完整 Debug 943 项通过。
  本次未修改 CXC 产品实现。

## 4. 实际 fixture 尺寸与测量范围

- author golden：2×3 展开，6 个 owning 实例；多 requirement 和零 requirement 实体有专门 fixture。
- Hold fixture：9 个 immutable timers 的人工期望断言。
- 随机差分：seed740304，24 条场景 ×40 次输入提交，10 个 advance prefixes/场景；
  包括同资源竞争及不同 priority/tieRank。逐字段对照 phase/resource/contact/ownership/receipt/fact；
  mismatch 分支会删除最小化并输出可重现 trace。另有 L2 的 5000 整数表输入。
- 最终 Debug 187 个 focused CTest 的单 case elapsed：min0.054966 s、median0.068331 s、
  p95（排序索引 floor(0.95×186)）0.526686 s、max5.292680 s、总33.417883 s。
- MSVC Debug execution 39 cases（seed740304）三次独立进程墙钟 4.730955/3.791962/4.549680 s；
  采样 `PeakWorkingSet64` 为 13,221,888 /13,275,136 /13,291,520 bytes。
  原始值在 `out/s7a-execution-process-measurements.json`，含 Catch2、oracle、fixture 与失败注入成本。

这些数值描述当前测试工作负载与机器的观察值；不是每个 kernel Tick 延迟、内部状态峰值的
整体证明或 steady threshold。没有把 representability、既有 measured containment cutoff 或
UINT64_MAX 变成生产业务 cap。尚未测得的内部峰值与生产耗时分布归 S7A-9。

## 5. 本次暂停点与恢复顺序

owner 再次要求先完成当前部分再停止。本次收尾以 GCC core find_package consumer 和 Clang
修正环境后的 add_subdirectory consumer 的通过结果为边界；之后开始的 GCC audio-sdl
find_package / Clang Playback add_subdirectory 检查被主动终止。退出15/143是 owner 请求的
暂停，不是这些检查已经得到功能失败结果；没有继续留后台矩阵进程。

此前暂停时 A/B/D/E/F、C 功能对账和 Windows 矩阵已完成，跨工具链 E1 行仍为 IU。
恢复后 GCC/Clang 完整矩阵各853项通过，revision2/3 固定 golden 在四工具链实际通过，
该行回填 IV。本次仅声明“受限功能验收完成，容量整体证明未完成”，Stage7A 继续 active。

以下清单保留恢复执行顺序；第1–4项现已完成，第5项按独立 CI 报告记录：

1. Linux GCC 编译最后修改的测试源，完整 CTest 和三 label 复跑；现有本轮前10项消费者结果
   保存为部分证据，不替代完整矩阵。当前旧 discovery 是851项，最新应重新生成清单。
2. 按 `out/s7a-clang-final-matrix.sh` 的 LF 脚本和明确 Linux dependency/license prefix
   重跑 Clang 完整矩阵；最终 discovery 当前853项。
3. 核对 GCC/Clang judgement、Capsule、author 各 label 非零且与最终187项清单一致，
   归档 revision2/3 同输入 byte/preimage golden、最终工作区指纹与文档检查。
4. 矩阵完成后回填 E1 唯一跨工具链 IU、最终报告、CURRENT_STATUS 与阶段计划；
   仅可使用交接规定的“受限功能验收完成，容量整体证明未完成”口径。

5. owner 于2026-10-05新增授权：S7A-3/4任务完成后、推送前优化远端CI耗时。
   先采集当前分支 job/step 的墙钟、依赖安装/编译/测试耗时与 cache 命中证据，
   再选择有测量依据的缓存、重复构建消除或并行度优化；保留现有功能、外部消费者、
   架构、诊断、格式、版本、sanitize/coverage 等验证要求。
   不直接恢复此前已回滚的CI实验；对本次改动进行针对 runner 的验证并比较前后耗时，
   缓存配置有效性和冷/热 cache 成绩分别记录。
   此授权增加后续CI优化范围，取代原交接对本次CI修改的禁止；优化在本次功能收口后开展，不构成推送授权。
   owner 后续已授权继续；本轮已完成 Linux 完整矩阵与最终187项三 label，功能收口后完成 CI 优化和本地验证。

## 6. 后续阶段门禁

S7A-9 完整 state-budget/capacity 证明、内部维度峰值和 steady thresholds 保持 **INCOMPLETE GATE**。
S7A-5 Ruleset/Fold、S7A-6/7 Snapshot/Replay/Playback/Player 产品工作尚未开展；本次内部 oracle、
signal 和故障注入不能替代这些批次。S7A-8 SDK / Stage6 handover 保持独立，未升 SDK。

hosted、GPU/真实设备、sanitize、coverage 本轮未运行；hosted 新 SHA 尚未验证，owner 未作 Stage7A 最终接受。
本报告只按交接允许的受限功能验收口径记录本地实施，不关闭 Stage7A、不发行 candidate 格式。

## 7. owner 授权提交/推送与日期复验

owner 在上述受限功能验收和 CI 优化完成后，明确授权提交并推送，并要求不等待远端 CI 跑完。
本轮日期 build 更新为26.10.05-1，SDK0.7.0保持；两份日期输入由工具同步修改。
日期更新后的 MSVC Debug/Release fresh / clean-first 复验单独记录，不覆盖§3旧日期矩阵。
Debug最终三label187项通过，37.52 s；`out/s7a-prepush-debug.log` / `-focused.xml`。
Release完整943项通过，0 failed、1 Windows symlink skip，667.02 s；`out/s7a-prepush-release.log` / `.xml`。
commit/push授权不等于容量证明、Stage7A关闭、merge/release或hosted通过。

日期更新后的72个行为、构建和CI输入记录于 `out/s7a-prepush-inputs-final.json`；指纹 `1eb7b36de82cc67dc4bd1fd52b636ead8cb53015df752223b2215d4fab21805d`。
