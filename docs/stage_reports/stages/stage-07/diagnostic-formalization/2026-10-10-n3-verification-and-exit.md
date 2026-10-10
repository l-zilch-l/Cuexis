# N3 Diagnostic Verification and Independent Exit

证据日期：2026-10-10。状态：N3独立退出；N1合同、N2迁移及全部本地硬门禁通过，冻结完整工作树快照，未提交/未发行。
范围仅为[诊断正式化N0–N3](../../../../stage_plans/active/stage-07/plan-diagnostic-formalization.md)。
本报告不实施RPA，不接受架构、生产预算或实体设备，不授权提交/推送/PR/保护修改/合并/发行。

## 合同、源码和版本

base HEAD `2ddbcafe22bd3facc63b4917cf99eeccc3d81cd1`，branch stage-7；实际验证对象包含未提交变更。
[N1合同与准入](2026-10-10-n1-contract-and-entry.md)及[分支映射](2026-10-10-n1-branch-mapping.json)
对应[Spec](../../../../formats/GAMEPLAY_V2_SPEC.md)、[ABI](../../../../api/GAMEPLAY_V2_ABI.md)和
[VERSIONING](../../../../guides/VERSIONING.md)的独立修订。SDK目标0.7.5不重开裁定；date build为26.10.10-1。
安装源头文件签名/布局/枚举无变化。Judgement模块不安装，其头仅增加测试友元；没有新增SDK入口。
完整sameError/sameKernelResult/sameSnapshot/sameReplayHistory源码与开工hash一致，没有忽略字段。
版本、安装元数据和公开差异核对在 `out/audit/diagnostic-formalization-20261010/n3-public-api-and-version-review.json`。

N0刷新无新增/删除：67阶段定义（63活动、3仅声明、1占位）另2 provisional。66改名、占位删除、grace细分、
legacy/query分开后共70唯一正式目标。十三项src-only不升级登记；未登记的operation-state错误已有
restoreGameplay→archiveGameplay公开Result承载，安装consumer验证该路径及全部metadata，不声称宿主不可观察。

## D01–D16定向证据

日志根：`out/audit/diagnostic-formalization-20261010/`。原始命令、退出码与实际case发现列表保留，
CTest按实际labels和名称选择。未以0例、SKIP、摘要相等或旧审批冒作通过。

| 项 | 执行证据及核验内容 | 状态 |
| --- | --- | --- |
| D01 | amount/timebase/CM-T10与Late参数优先级现有测试；安装consumer真实press/release同Tick负例；完整内部Error保持 | pass |
| D02 | Candidate explicit diagnostic projection逐一覆盖有限集合×九类别和未知类别；五公共input/timebase直返、其他透传、ordered sourceCode/重复字段；graph补字段 | pass |
| D03 | diagnostic session origins逐一验证两生命周期、legacy专属message；prepare/recover同时发布kernel/prepared、reset同时清空，不伪造query防御状态 | pass；不可达兜底为静态不变量证据 |
| D04 | grace conflict precedes forbidden chart supply，两故障同现仍先invalid_relation，单独禁供给为budget_exceeded，完整字段 | pass |
| D05 | diagnostic fault originals：beforeSeal/beforeFold，sealed/Fold/workTick/faultStage及完整DTO；现有Tick failure injection与公开Fold失败 | pass |
| D06 | 原owning DTO/Replay/Checkpoint及六旧文件不变；新Writer显式拒绝旧诊断；比较只在expected副本应用允许映射 | pass |
| D07 | 原有独立kernel/Fold start oracle、完整ReplayEvaluation、健康restore/certified Seek和fault同注入重放；无注入故障Replay按旧合同拒绝 | pass |
| D08 | 六真实原Writer revision1健康/kernel/Fold Replay/Snapshot全部version_unsupported；原manifest SHA256逐项核验 | pass |
| D09 | revision2读写；有效digest旧诊断污染record_invalid；仅改版本不消除kernel旧诊断；健康/Fold同body不承诺来源年代鉴别 | pass |
| D10 | W2原结构/预算测试及diagnostic envelope first errors：prepare/budget/magic/version/tag/length/section/digest/identity/count/record顺序 | pass |
| D11 | 健康原件与新Writer全字节比较，只有envelope u32 revision允许变化；body/digest、stateSchema1、eventCodec及全部身份保持 | pass |
| D12 | residual gate扫描真实生产者/动态suffix/有限投影/码表/工具；无整目录忽略；历史provenance等例外逐项限定 | pass |
| D13 | pollution检查early Replay record、512层cause、重复sourceCode；完整chain迭代；普通message/历史描述不误拒绝；预算仍优先 | pass |
| D14 | 直接restore/evaluate/checkpoint/Seek拒绝旧诊断；混合Checkpoint不适用忽略、失效凭证复验并从起点fallback；原archive不变 | pass |
| D15 | 现有prepare/reload/restore/Seek/submit/publication原子性；新Candidate diagnostic failure逐字段比kernel/公开返回Error/AdvanceReceipt/Error与evaluation，保留sealed前缀；仅安装头consumer真实Fold错误/AdvanceReceipt及完整Replay另验 | pass |
| D16 | CTest→CMake→Python真实正入口0；独立临时副本注入旧常量、旧动态prefix、漏投影、目标碰撞，四个负入口均exit8 | pass |

Judgement定向15 cases/15277 assertions；Playback诊断3 cases/14295 assertions，含本次补足的完整回执。
Linux ASan/UBSan实际生产编译flag、定向Judgement15/15277与公共Gameplay27/15747另留日志，不用普通构建代验。
MSVC各ON配置普通公共Gameplay27 cases/15747 assertions；隐藏Candidate public installed-consumer fixture
不计为普通用例，它由安装consumer显式生成/消费。实际工具/Reference Host按矩阵日志单列。
旧基线Judgement全套200 cases/20048 assertions；六原件固化81 assertions。不是以新字符串替回旧名制造历史证据。

类别投影原来不保留cause，本次保持该既有行为，测试同时核对原owning cause未变；不是忽略cause来过检查。
Query防御兜底当前不可达、三仅声明源码不声称运行覆盖；它们的静态/政策证据单列。
Replay header Fact count在dialect前，最终event count仍在逐条执行后；Snapshot两个header count都在dialect前。

## 配置、安装与平台硬矩阵

配置和必须执行项已在N1先行确定，未在运行失败后改成不适用。
N2版本变更后每行首次fresh configure/clean-first；暂停后的M5/L2只继续未结束的同配置构建，原中断日志保留。
新增D15仅为测试，对已通过配置增量重建Playback并执行公共Gameplay，生产源码/配置未变，未无故重复完整矩阵。

| 行 | 实际配置/工具链 | 运行证据 | 状态 |
| --- | --- | --- | --- |
| M1 | MSVC Debug STATIC ON，Player/tools启用 | 首次完整选择集124项：123通过，#21 cuexis_gameplay_installed_consumer失败；修正同Tick press/release输入后单项复跑1/1通过；其余123项沿用首次通过结果。另有final-windows-run Judgement210/210、Gameplay27/27及3脚本gate通过；未在修复后重跑全部124项 | pass |
| M2 | MSVC Release STATIC ON，warnings-as-errors | m2-run完整332；final-windows-run Gameplay27/gates；consumer-complete-rerun | pass |
| M3 | MSVC Debug SHARED ON，headless/tools | m3-run完整290；final-windows-run Gameplay27/gates及shared installed consumer | pass |
| M4 | MSVC Debug STATIC OFF，headless | m4-run完整248；package/ASCII/泄露/能力隔离，无调用不存在的candidate API | pass |
| M5 | MSVC Release STATIC OFF，headless | m5-resume-run续跑完整242 | pass |
| M6 | MinGW UCRT GCC Debug STATIC ON，headless/tools，x64-mingw-static | m6-resume-run完整282；实际GCC，缓存依赖复用、独立build/staging；最终安装完整回执另验 | pass |
| L1 | WSL Ubuntu GCC Release SHARED ON，headless/tools，x64-linux | l1-resume-run完整282；dependency prefix供给子configure；final-linux-run Gameplay27/诊断15，最终安装完整回执另验 | pass |
| L2 | WSL Ubuntu Clang Debug STATIC ON，ASan/UBSan，headless/tools | 同一Clang Debug STATIC ASan/UBSan配置续跑完整CTest选择集282/282、0失败/无skip；其中#14 cuexis_gameplay_installed_consumer通过(513.48秒)，#16 Reference Host通过(547.56秒)。实际生产编译flag已核对；定向Judgement15及公共Gameplay27通过 | pass |

Linux首次configure无法读取Windows worktree .git指针，随后显式提供base HEAD给旧F3参数。
该值仅满足已有参数，不把它冒作本次行为提交SHA；本次身份是完整冻结工作树快照。
工具链、显式ON/OFF、library、命令、case发现/选择及退出码见各run/discovery/selected JSON和raw logs。
性能/capacity、Linux coverage/clang-tidy按N1为not_applicable，命名与wire回归未新增判定算法，定向sanitizer仍为硬项。
GPU/window/真实键盘/audio/听音、生产预算、RPA最终同SHA hosted及发布审批本次not_applicable或not_run；均未宣称接受。

## 失败、修复和暂停记录

初次安装consumer使用两个相同press，现有规范去重使其成功；改成同Tick真实press/release验证碰撞，原m1失败日志保留。修复后仅单项复跑consumer通过，M1其余123项保留初跑通过结果；未声称修复后全量124项重跑。
Linux两个通用消费者第一次缺tl-expected依赖prefix；仅修正验证环境供给，原l1失败日志保留。
M5/L2因owner暂停终止，不记产品失败或pass；本次按新授权继续同一诊断工作。
新增D15测试首次误把KernelProjection当有kernelView方法，编译失败后按实际DTO修正；保留n3-full-receipt-build原日志。
M1矩阵pass按逐项证据合并判定：首次完整124项除#21外均通过，#21修复后单项复跑通过；final增量检查另通过。修复后没有全量重跑124项。L2初次构建曾因owner暂停中断，恢复后在原同一配置完成构建及282项完整CTest，无skip；#14安装consumer包含在该282项中，不是矩阵外单独复跑。同时链接/执行M1测试遇LNK1168文件占用；后续顺序运行通过，first-LNK1168日志保留。没有以跳过用例修复失败。

## 残留例外、证据身份和回退

有限legacyDiagnosticCodes仅用于拒绝，包含69准确旧字符串；不是旧code生产层、兼容alias或新公共registry。
例外精确到policy旧表、fold_recovery单个旧kernel负例、Playback单个未定义prefix透传负例、
immutable fixture manifest、码表rulingProvenance历史字段。普通注释历史讨论不作活动code；无目录级豁免。

退出包为本目录新文件 `2026-10-10-n3-verification-evidence.zip`，完整manifest/原始日志/源码快照位于 `out/audit/diagnostic-formalization-20261010/`。
source-manifest记录base HEAD、binary diff、新增文件、全量SHA256和源码指纹；不只用HEAD标识验证对象。
全量新源码快照与exact closing hashes匹配；派生证据ZIP本身单列SHA256以避免自引用。

回退包opening-source-snapshot包含1770开工文件，其中1769匹配开工原始字节；
唯一clean tracked tools/check_docs.py从base Git blob复原，原混合换行字节不可恢复，语义与base一致。
此限制在opening-snapshot-reconstruction逐项标注，保留开工原hash，不伪称1770全部exact-byte。
所有原dirty文件使用开工副本；baseline-status/diff记录已存在的删除，不擅自恢复历史ZIP。
回退恢复整套匹配源码/合同/SDK0.7.1/工具链与六revision1原件，重新构建旧包；
不重写revision2产物降级、不单独回退码表或版本、旧发行0.7.0不变。
旧N0报告/ZIP及无关原RPA材料开工hash保持；本次额外RPA材料已按owner要求回退，不纳入诊断交付。

## 独立退出与未执行项

N3独立退出：N1预定八行本地硬矩阵均pass；D01–D16及适用安装消费者通过；完整源码身份、diff、新增文件、配置、原始日志和新证据包已冻结并核验。
最终format、文档414/20、status/section/target 4/5/2、version updater和diff门禁通过，封存检查见n3-final-checks-run。
输出只供后续依赖核对；不启动任何RPA工作、不沿用旧0.7.1审批接受0.7.5、不自动发布。
