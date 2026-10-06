# S7A-5/6 Limited Functional Acceptance

状态：verification

证据日期：2026-10-06；J0–J7本轮受限功能验收完成，十行本地证据回填完成；新SHA hosted及后续预算/设备证据待回填，非阶段关闭。

## 基线与证据口径

实际开工HEAD `9f6803f1752650dffb576742ff4592b8f59c0b4b`，stage-7，clean。
保留后续历史；未reset。日期版本26.10.06-1，SDK0.7.0。本轮源树指纹及最终提交另列。
选择与决策归[计划§3.3](../../../../stage_plans/active/stage-07/plan.md#33-s7a-56-本轮实施记录2026-10-06)，
字段归[Spec](../../../../formats/GAMEPLAY_V2_SPEC.md)与[ABI](../../../../api/GAMEPLAY_V2_ABI.md)。
桌面接手只作输入，当前合同按U01–U13重审落定。
IV=已实现且有本轮验证；IU=已实现待证据；M=测量；LB=环境限制。

## 十行功能证据

测试文件 `tests/judgement/fold_recovery_tests.cpp`；所有旧Judgement测试继续回归。

| 目标 | 本轮正例、负例与独立证据 | 状态 |
| --- | --- | --- |
| 5.1 | finite registry/build/manifest、显式Loadout；package/Life/非法scope/缺mapping拒绝 | IV |
| 5.2 | 规范Fact逐Tick Fold；自然checked故障及三Register注入；kernel seal保留、旧Fold逐成员不变 | IV |
| 5.3 | 手算MAX,+1,-1 checked/clamp；有/无表、重复grade区间、缺口；真实body break -50和timer Miss absent；Combo峰值 | IV |
| 5.4 | 真实Fold bonus在t+1消费，空Fact due消费且不发链；consumer故障回滚；kernel/shared/step明确拒绝；max/bitOr代数及重复贡献 | IV |
| 5.5 | live/Replay/restore/Seek全量统计比较；逐phase/outcome/grade含absent、stray/consumeEmpty；独立Fold oracle；owning query/reset | IV |
| 6.1 | prepared实际graph/config/registry身份重算；Loadout与Ruleset投影；不同配置恢复拒绝 | IV |
| 6.2 | 人工逐字节LE canonical golden；Replay roundtrip、version/truncation/digest/budget；伪计数/重复sequence经Reader拒绝 | IV |
| 6.3 | 原session销毁后fullDTO恢复；pending/activation/rank/grade/Fold/coverage/ID篡改拒绝；faulted new recovery只查询 | IV |
| 6.4 | exact H/F(H)、negative Tick、完整/partial cut、多checkpoint与从起点结果一致；错误cut不因checkpoint变可接受 | IV |
| 6.5 | future100随SeekH10保留，lastObservedTick拒绝live10；query/空submit不fork；首次accepted或durable control分支，旧archive有效 | IV |

人工golden先约束oracle；真实同Tick body+tail用MAX,+1,-1验证checked中间失败与clamp末值MAX−1。独立Fold oracle不调用production reducer/排序/checkedScore。
联合kernel oracle为既有独立S1 scanReference，比较Facts/phases/resources/contacts/ownership/receipts/observers。
ReplayEvaluation比较全部Kernel/Fold/error/revision/faultStage，统计或bonus偏差即使Facts相同仍拒绝。
故障注入覆盖未seal、seal后Fold、due消费、Register非法写、录制预留失败；故障不会变成新fault Fact。

## 本地矩阵与原始输出

最终新Judgement源码固定为199个case、20,048次断言；其中36个本轮case、1,549次断言。
聚焦CTest使用 `-L 'judgement|gameplay_packed|gameplay_author' --no-tests=error -j2`，
非零枚举为Judgement 200项（含诊断Schema）、Packed 16项、author 7项，共223项。
所有旧T4/K4、L1/L2、S1/S2、author双路/affine、Capsule revision 2/3保留回归。
全量初跑后源码仍有修订的环境，用最终重编译与223项补跑闭合本轮变化，不冒充一次最终全量运行。

| 环境 | 本轮全量与最终补跑 | 状态 |
| --- | --- | --- |
| MSVC 14.51 Debug/static | fresh/clean-first；全量970项遍历，分发门禁初败后单独通过；最终979项可枚举，199-case与223项最终补跑通过 | IV |
| MSVC 14.51 Release/static | fresh/clean-first、warnings-as-errors ON；全量972项0失败，1项平台skip；最终979项可枚举，199-case与223项最终补跑通过 | IV |
| MSVC headless/shared | tools ON、fresh/clean-first；全量894项，CXC工具初败后单独通过，3项skip；最终199-case与223项补跑通过 | IV |
| MinGW GCC 16.1.0 headless/static | tools ON；全量889项0失败，1项skip；最终891项可枚举，199-case与223项最终补跑通过 | IV |
| Ubuntu GCC 15.2.0 Debug/static | 最终重编译；889项遍历1188.82秒，1项AudioSDL初败后原始门禁复验通过；199-case通过，Judgement/Packed/author共223项在全量内通过 | IV |
| Ubuntu Clang 21.1.8 Debug/static | 最终重编译；889项遍历905.39秒，2项AudioSDL初败后原始门禁逐项复验通过；199-case通过，Judgement/Packed/author共223项在全量内通过 | IV |
| hosted Linux Quality/MSVC/MinGW | 本报告所在提交推送后的实际SHA；不等待CI | IU |

Windows命令为 `cmake --preset <preset> --fresh`、
`cmake --build --preset <preset> --clean-first`、`ctest --preset <preset> --no-tests=error -j2`；
实际preset是debug、release、headless-shared-debug、mingw-headless-debug。
headless/shared与MinGW显式开启developer tools以包含真实author测试。
最终受影响目标重编译为cuexis_judgement_tests、cuexis_gameplay_author_tests、cuexis_gameplay_packed_tests。
Linux使用out/build/wsl-gcc-direct和wsl-clang-direct，Ninja、Debug、tools ON，禁用Player/SDL/AudioSDL/OpenGL。
CMake prefix及VCPKG_INSTALLED_DIR指向既有out/build/wsl-gcc/vcpkg_installed，triplet x64-linux；
最终CTest同时导出CMAKE_PREFIX_PATH，使独立消费者继承实际依赖路径。AudioSDL初跑误从Windows PATH取得SDL3.dll；
将既有/mnt/d/vcpkg/packages/sdl3_x64-linux原生包补入Linux依赖prefix后复验，无新增第三方依赖。
Linux因Windows worktree .git绝对路径不可用，显式CUEXIS_IMPLEMENTATION_SHA=9f6803f1752650dffb576742ff4592b8f59c0b4b仅作基线标识，
实际被测源树由下列输入指纹绑定，不声称该基线SHA包含新实现。

原始输出位于out/s7a56-*，本地留存、未入仓：

- Windows全量：debug-ctest、release-ctest、shared-clean-ctest、mingw-tools-ctest。
- Windows最终：debug/release/shared/mingw-closure-build、-closure-judgement、-closure-focused。
- Linux最终：gcc/clang-closure-build、-closure-judgement、-persistent-ctest、-audio-add-rerun、-audio-package-rerun；复跑直接执行CTest JSON中的原始command，成功以EXACT CTEST BODY EXIT 0记录；早先last-ctest是已中止的慢缓存运行，closure-ctest含缺失/tmp失败，均不作为完整通过证据。
- 失败复跑：debug-distribution-rerun、shared-cxc-rerun。
- 输入清单与测量：s7a56-inputs.json、s7a56-measure.json、s7a56-measure-final.json及测量stdout/stderr。
- 严格诊断：gcc/clang-werror.log；格式/文档：closure-format、check_docs与状态/目标/版本门禁。

## 故障证据的处置

MSVC缓存showIncludes前缀乱码，头依赖未正确跟踪。仅有2052语言资源，VSLANG=1033未能切换英语。
早先在tools OFF→ON的增量构建复用了10月5日author对象，新旧布局混用导致崩溃；
已固定vcvars之后VCPKG_ROOT=D:/vcpkg并fresh/clean-first，重新编译author与所有Judgement对象。
该失败不记为产品语义通过，不修改全局工具或CI策略。
Debug全量分发门禁因同build目录Ninja并发报deps log Permission denied，构建结束后单独复跑1项通过。
shared全量CXC工具一次报unpack.output_commit_failed；同源码单独复跑1项通过，未改CXC实现。
Linux首轮编译发生在最后源码修订期间，混合布局崩溃与消费者缺依赖prefix均被明确废弃；
最终重新编译全部33个受影响cpp、继承实际Linuxprefix后复验。Linux独立add_subdirectory消费者在Windows目录耗时319.55秒；
后续将各build的ec临时目录链接到WSL原生缓存、原输出保留为ec.s7a56-p9-backup，
CMAKE_BUILD_PARALLEL_LEVEL=4。初次/tmp目录在后续WSL调用中不存在，8个消费者目录创建失败，
该次不计完整通过；改为/home/zilch/.cache持久目录后完整复跑889项，未修改CI或验证脚本。
进程终止仅针对本轮CTest子树。
Release shadow warning改名，保留/WX；
KernelProjection和旧author的新增optional字段补齐显式初始化，避免Linux -Werror漏门禁。
Linux AudioSDL消费者的首次SEGFAULT由链接命令定位为Windows SDL3.dll进入ELF，与新增Judgement无关；
Clang installed AudioSDL门禁在SDL版权文件拷入前配置，漏装sdl3-copyright.txt，亦需修正prefix后复验。
既有SDL3 Linux包补入prefix，消费者须复验通过才退出该失败，未修改AudioSDL实现。
独立末轮审查未发现P1/P2。最终kernel与author文件在GCC/Clang均以-Werror -fsyntax-only额外通过。clangd kernel检查遇到unsupported code-action/tweak，不能把该输出当编译诊断通过；
编译与执行矩阵作为本轮主要实现证据，hosted sanitizer/coverage仍待回填。

## 人工golden、字节与测量

W2人工canonical key golden为53个明确LE字节，测试逐字节比较，不由production Writer生成期望值。
另有手算三Fact/Fold与同Tick MAX,+1,-1极值；Fold oracle不调用production reducer、排序或checkedScore。
本轮固定fixture的Replay为6,727字节，Snapshot为7,334字节；不同工具链artifact不是数值摘要替代物。
四套Windows配置与Ubuntu GCC/Clang共六套输出逐字节一致，全部199-case /20,048断言通过。

| artifact | SHA256 |
| --- | --- |
| Replay | 4a0ebb85fc4f49d722a513f153088a609d987a74ace45503588cdc33f07b44e1 |
| Snapshot | ef9e3c2d7b6a4ae200514b8e45616067435e3d6e4100de774589c69cdc2bba1c |

最终输入指纹SHA256为 `b4328fa6a3d50f70dc268ba1cbc8ba6b27b6b01c0087778cbcab7c0a835289f1`，共26个修改/新增engine、tests、tools、cmake与vcpkg输入；path/bytes/SHA256排序后组成canonical JSON。
文档不进该指纹，避免报告自引用。最终提交以本报告所在提交为准，不复制历史SHA的hosted结论。
36-case功能组一次测得wall 1.0955028秒；这是包含fixture/prepare/Fold/restore/Seek的进程总时间，
不是单独prepare或每Tick/Seek延迟承诺。该次退出后PeakWorkingSet不可取得，不填造内存数值；
随后在进程存活时采样：wall 1.2266625秒、观测CPU 0.84375秒、观测PeakWorkingSet 13,320,192字节（约12.7MiB），36 case /1,549断言通过。该测量仍是功能fixture进程，非稳态产品profile。
独立分项延迟、真实容量、稳态与maxSeekLatency仍归9，未接受阈值。

## 边界与待回填

文档检查377 Markdown/20 JSON-CXT、状态合同4项、目标合同2项、版本门禁20项（2项skip）、版本一致性、全仓格式与git diff --check均通过；7份变动/新增public header逐字节ASCII检查通过。

十行IV仅指本轮受限功能及已列正负例；不接受生产容量/state-budget/steady thresholds，仍为INCOMPLETE GATE归S7A-9。
有限codec字节bounds和testOnly数字不是生产预算接受。GPU/真实设备/音频本轮LB；7/8/9产品集成未展开。
U05极值分两种实际配置：close=3时H=MAX保留finalization gap与pending timer；close=0时真实工作MAX，
无Hook输出成功、有输出checked(t+1)失败且kernel seal/旧Fold双前缀保留。不存在用数学helper冒充真实Hook路径。
u64 bonus实际消费者允许MIN+UINT64_MAX=MAX，checked/clamp比较数学结果，不先窄化或有符号溢出。
Snapshot用sealed kernel前缀重执行拒绝不可达matcher/contact/receipt状态；验证H使用close间隔而非watermark，
near future input8/H10正例保留pending。orphan800后的repeat900没有head证据，实跑撤销了错误缺陷猜测。

按owner授权提交并推送，不等待CI。新提交同SHA hosted Linux Quality（含ASan/UBSan/coverage）、Windows MSVC/MinGW
与设备/生产规模证据待回填；Stage7A保持active，不升SDK、不建PR、不merge/release、不关闭阶段。
