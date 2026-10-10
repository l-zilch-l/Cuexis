# N1 Diagnostic Contract and Implementation Entry

证据日期：2026-10-10。base HEAD：`2ddbcafe22bd3facc63b4917cf99eeccc3d81cd1`，branch stage-7。
当前实现状态归[CURRENT_STATUS](../../../../CURRENT_STATUS.md)。本报告记录N1准入，不声称N2/N3通过。

## N0差异与工作树保护

刷新扫描67项阶段定义与N0相同：无增删，63活动/3仅声明/1无调用占位；另两项provisional保持。
完整开工status、binary diff、所有tracked/nonignored文件hash及原未提交文件副本位于
`out/audit/diagnostic-formalization-20261010/baseline-*`。未删除或reset已有改动；历史N0报告与ZIP保持原件。
[逐分支映射](2026-10-10-n1-branch-mapping.json)含真实生产条件/上下文及70个名称处置行；
拆分grace后共70个唯一正式目标，不与现行公共或内部目标碰撞。占位删除不是新增能力。
provisional生命周期与selected-profile生命周期仍不同来源，均不改变类别和检查顺序。
prepare成功才同时发布kernel/prepared；recover同样，reset清除二者；故query防御兜底当前不可达。
legacy无参入口仅Judgement内部，安装SDK不暴露这些动词，保留其内部透传策略，不登记公共码。
原S7A2/3内部集合采用category投影，五公共input/timebase直返，S7A4直返；未知code不提升能力。

## 权威合同与允许差异

[Spec](../../../../formats/GAMEPLAY_V2_SPEC.md)与[ABI](../../../../api/GAMEPLAY_V2_ABI.md)已落定正式命名、
有限投影、两类准确message、Recovery revision2、旧值/首错/Checkpoint fallback及完整比较合同。
原首错、九类别/severity/faulted、ordered context/cause和会话原子性保持。
跨版本允许差异仅code/sourceCode映射、两类message、envelope u32版本；其余字段严格比较。
code历史年代只按有限旧定义识别，不扫描普通message，cause迭代遍历，无递归深度新限制。
复用codec.record_invalid拒绝Writer/Reader诊断污染；直接恢复/执行复用judgement.execution.relation_invalid。
版本拒绝仍codec.version_unsupported；Reader先prepare及预算、envelope/structure，再digest/identity/count。
Snapshot stateSchema=1、eventCodec、Prepared/Judgement/Fact identity不变。

## 固化旧格式与回退

六份真实revision1原件及hash在`tests/fixtures/diagnostic_revision1/manifest.json`；
MSVC fresh baseline、原源码Writer/Reader运行81断言通过。健康样本由公共会话产生；
kernel/Fold故障Replay经同注入evaluate完整比较；Faulted Snapshot由内部DTO组装且原Reader/restore验证，
公共Faulted snapshot仍拒绝。人工污染负例须另标注，不能冒作这些旧原件。
源码回退输入为base HEAD与开工binary diff/新增文件副本、对应旧工具链/0.7.1包和旧载体；
不重写原ZIP或降级新revision2产物，不只回退单层。

## SDK版本材料

目标0.7.5已裁定；[VERSIONING](../../../../guides/VERSIONING.md)记录候选Recovery拒绝的限定breaking patch例外。
production签名/布局/枚举不变，现有错误承载的code/sourceCode可观察变化单列；candidate wire不兼容。
N2.4更新SDK及candidate消费者最低请求0.7.5，生成头/install metadata必须同值。
显示日期经updater独立同步26.10.10-1；fresh/clean-first验证，不声称发行或共享二进制兼容。

## 命令级硬矩阵

源码身份：本次所有本地硬项允许冻结工作树快照（base HEAD、binary diff、新增文件、全量hash、配置）。
hosted/精确SHA审批仅对外发布/后续RPA最终门禁适用，本工作包本地退出不要求新提交；无自动提交授权。
所有构建隔离输出；必须发现非零case，Playback Gameplay按实际名称补齐，不靠judgement label。

| 行 | 配置命令（Windows先dot-source .buildenv.ps1） | library/compiler/triplet | 必须执行 |
| --- | --- | --- | --- |
| M1 | cmake --preset candidate-debug -B out/build/diagnostic-debug --fresh -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON | STATIC/MSVC/x64-windows | clean-first全build；judgement、Playback Gameplay、Player Gameplay、tools/Host、diagnostic/architecture/package/version/docs及安装consumer |
| M2 | cmake --preset candidate-debug -B out/build/diagnostic-release --fresh -DCMAKE_BUILD_TYPE=Release -DCUEXIS_WARNINGS_AS_ERRORS=ON -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON | STATIC/MSVC/x64-windows | 同M1非GPU用例 |
| M3 | cmake --preset candidate-headless-debug -B out/build/diagnostic-shared --fresh -DCUEXIS_LIBRARY_TYPE=SHARED -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON | SHARED/MSVC/x64-windows | clean-first全build；judgement/Playback/tools/Host、安装candidate及通用package拒绝 |
| M4 | cmake --preset headless-debug -B out/build/diagnostic-off-debug --fresh -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF | STATIC/MSVC/x64-windows | clean-first全build；架构/公共头泄露ASCII、通用安装consumer、无candidate表面 |
| M5 | cmake --preset headless-release -B out/build/diagnostic-off-release --fresh -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF | STATIC/MSVC/x64-windows | 同M4 |
| M6 | cmake --preset mingw-headless-debug -B out/build/diagnostic-mingw --fresh -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/c++.exe -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON -DCUEXIS_BUILD_DEVELOPER_TOOLS=ON | STATIC/MinGW/x64-mingw-static | clean-first全build；judgement/Playback/安装及脚本gate；环境缺失记not_run，仍是硬项 |
| L1 | WSL cmake -S /mnt/d/Cuexis-worktree -B /mnt/d/Cuexis-worktree/out/build/diagnostic-linux-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCUEXIS_LIBRARY_TYPE=SHARED -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=ON -DCUEXIS_BUILD_PLAYER=OFF -DCUEXIS_BUILD_SDL_ADAPTER=OFF -DCUEXIS_BUILD_AUDIO_SDL_ADAPTER=OFF -DCUEXIS_BUILD_OPENGL_ADAPTER=OFF -DCUEXIS_BUILD_DEVELOPER_TOOLS=ON -DCMAKE_PREFIX_PATH=/mnt/d/Cuexis-worktree/out/build/wsl-gcc/vcpkg_installed/x64-linux -DVCPKG_INSTALLED_DIR=/mnt/d/Cuexis-worktree/out/build/wsl-gcc/vcpkg_installed -DVCPKG_TARGET_TRIPLET=x64-linux | SHARED/GCC/x64-linux | fresh/clean-first全build；judgement/Playback/tools/Host/package consumer |
| L2 | 同L1，目录diagnostic-linux-sanitize、Debug、STATIC、/usr/bin/clang++、CUEXIS_ENABLE_SANITIZERS=ON | STATIC/Clang/x64-linux | ASan/UBSan judgement/Playback/gates；不能用MSVC代替 |

所有行安装staging/ec在各自binaryDir下，clean consumer；M1/M3/L1覆盖旧revision、新code/sourceCode及恢复操作。
Linux coverage/clang-tidy非本次命名/wire验收硬项（无新判定算法/控制流能力，定向ASan/UBSan覆盖）；
实体GPU/键盘/音频、生产容量与RPA F2 hosted矩阵不适用本诊断退出，未声称已验收。
Release shared由L1覆盖；Debug shared由M3覆盖。candidate OFF不调用不存在的API。

## N1门禁

N0差异、全量唯一目标/分支映射、显式投影及恢复/首错合同、0.7.5兼容材料、真实旧fixture、
命令级硬矩阵与工作树回退输入闭合。N1文档/合同自动检查通过后才能进入N2；
N3 D01–D16须另留真实运行证据，硬项缺失仍不退出，RPA新架构实施必须等待。

N1退出：413份Markdown/20份candidate数据通过；status/section/target为4/5/2通过，diff check通过。N2按本报告输入开始。

N1公开性追踪补明：operation-state内部错误已有restoreGameplay→archiveGameplay可观察路径；
不会把“未登记”误写为“宿主不可观察”。Spec/ABI保留其原Result承载和完整元数据，安装consumer补该负例；
仅legacy无参语义与query防御兜底不构成安装SDK新动词。此处没有新增载体或公共码登记。
