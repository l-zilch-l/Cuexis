# R4 扩展 owning 覆盖、固定 CPU 组与平台核验

状态：candidate；完整 memory/P8、设备与最终接受仍未闭合

日期：2026-11-07

返回 [New RPA 报告索引](../README.md)。本报告承接
[缩页效果验证](2026-11-06-world-storage-effect.md)和
[R4 实施方案](../../../../../stage_plans/active/stage-07/05-rpa-new/r4/plan.md)。
原始输出、不可变登记、失败及只读复算见
[本轮证据](2026-11-07-owning-and-cpu-evidence/index.md)。

## 1. 授权、版本及候选

owner 要求继续剩余工作，并在下次推送前更新 version。本轮通过版本工具将日期构建
推进到 `26.11.07-1`，SDK API 保持 `0.9.1`，不以日期推进代替 SDK owner approval。
本轮实现候选 C 为 `24f0a914f306cbc20e05b1dccc9f82dac59a6024`，tree 为
`0bc59434103671b403ffd3bebc77a3df2fee493a`；已推送至 PR #32 的 `stage-7`
以及 `codex/r4-integration`。后继报告提交仅发布 C 的证据，不自动继承最终候选资格。
所有本机下载/远端命令使用 `127.0.0.1:7890` 代理。

本轮没有新增产品存储机制。已接受的七类非空组件 `page_size=128` 保留；新增实现仅在
extended observer 内扩大瞬时拥有观察，普通构建不执行 census，不改变 SDK 布局或公共语义。
O1 原数值及历史超界保留，正式 memory/P8 继续未准入，没有运行 40K。

## 2. 语义覆盖与去重结果

typed census 新增实际 Transform/Renderable/Behavior 等组件页、packed entity 范围、
World 本体、transform cache/children/order，以及当前 Appearance/Camera 字符串 heap 容量。
字符串按实际当前地址观察；SSO 不重复计费，字符串移出后不继续归给旧 root。
固定 caller buffer 和 collector 表在超界时失效，不通过动态分配扩大观察器量程。

预登记最多两个 64/high、64/low 诊断进程，各120秒；第二个须先确认首个有效且没有已证明超界。
两进程均 exit0、未超时、21 assertions 通过，19个精确 partition、无 observer overflow：

| active 唯一计费 | high | low |
| --- | ---: | ---: |
| stable registry allocations | 77,114 B / 22 | 77,114 B / 22 |
| 瞬时 owned spans allocation union | 42,420 B / 9 | 42,420 B / 9 |
| 下界合计 | **119,534 B / 31** | **119,534 B / 31** |
| 完整 backing requested / CRT storage，仅披露 | 119,806 / 122,038 B | 119,806 / 122,038 B |

15个 typed spans 的原始和为52,416 B，其中9,996 B已包含在全额计费的stable分配中，
不能再相加。相比旧103,738 B子集增加的15,796 B恰为 World本体448、cache14,560、
order520及新覆盖packed entities268；旧stable和三类页payload不变，属于覆盖扩展。
之前同一子集358,241→103,738 B的71.04%缩页效果仍是有效历史比较；119,534不能与
旧覆盖直接相减称为产品回归或新的优化幅度。

本工作流实际出现六类非空payload，无Hierarchy/非空children/长heap字符串；代码支持
不等于容量场景实际覆盖全部类别。长字符串交换由独立native契约测试覆盖。
观察器报告collector栈327,680 B，caller另约96 KiB；这些是观察开销，不算产品预算。

Close后跟踪root released，最终产品C++ new域requested/storage归零；probe.fixture仍保留
19,600 B，不能称为进程归零。完整World图、EnTT私有初始/sparse/page-directory、
transform scratch/transaction、ObjectEntityMap、其他语义域、shared retained/prepare峰值、
native/GPU/driver和observer扰动仍缺证据，故两结果均为 **unrated**。不因下界低于
327,680 B签发memory pass或提高预算；两容量诊断已用尽，不自动追加。

## 3. 契约、正确性与复核

- 四个native契约原进程均通过：positive为43 assertions/6 cases，三个拒绝进程各1 assertion。
  控制器曾错误假定Catch2用例顺序；原错误和日志保留，只读重新识别root/census与类型后
  对相同原始字节核验，未重新运行native进程。实际proof顺序为8192、12841、176、0、784。
- fresh正常Release STATIC无插桩：World30 cases/632 assertions，Runtime54/11614，
  Playback185/34750，总269 cases/46996 assertions通过。完整依赖、编译命令和二进制绑定保留。
- architecture/version、全量format、40项Python工具拒绝/语义测试、版本一致性、文档及diff检查通过。
- Standards与Spec两轴独立只读复核均0项发现；独立memory复核从原始span和allocation复算一致，
  未运行额外容量工作负载。审查不代替实际验证。
- 原normal-release-v1构建误用了不存在的目标名cuexis_pd_owner_probe；保留失败，新的v2使用
  实际cuexis_pd_baseline_probe并完成构建。该修正不是产品修复，不消费正式组额度。

## 4. 正式 CPU 固定组

原pre-PD A为`d344088236ae589c0b49a4994ef0fbddd6567ce2`，匹配普通Release STATIC harness，
依赖与toolchain签名和B一致。四个pilot测量进程与独立30秒机器热身通过后，封存两个单元的
attempt-1、claim/seal；之后三窗、每窗三对、AB/BA/AB与BA/AB/BA交替，read-long固定
1,048,576次/块、每进程8块，各块至少100ms。正式36个测量和36个热身进程完整结束，
没有替样、重跑或环境检查失败；两个窗口间隔均至少600秒，源码与binary保持绑定身份。

| 单元/指标 | 全组B/A几何比 | 95%区间 | 正式结果 |
| --- | ---: | --- | --- |
| legacy validated_trace | 0.904180 | [0.837354, 0.976338] | inconclusive：A/B重复性均有超15% |
| legacy playback_only | 0.908936 | [0.865494, 0.954559] | inconclusive：B重复性超15% |
| legacy read_long | 1.140031 | [1.101273, 1.180153] | **over_budget**；A/B重复性通过 |
| RPA validated_trace | 0.664689 | [0.654805, 0.674723] | pass；A/B重复性通过 |
| RPA playback_only | 0.650015 | [0.641308, 0.658840] | pass；A/B重复性通过 |
| RPA read_long | 1.146032 | [1.128434, 1.163904] | **over_budget**；A/B重复性通过 |

legacy执行的A九进程noise为23.899%、B为18.844%；legacy playback的B为18.749%。
区间本身低于1.10不能覆盖重复性失败。read-long两单元的全组区间下界高于1.10，
所以是当前候选超界，不以绝对成本约0.25µs为由豁免。逐窗区间完整保留：legacy三个
窗口均跨线；RPA前两窗下界超1.10，第三窗跨线，尚不满足连续三窗持续超界的拆分触发。

本次读取夹具是原固定单对象提帧；历史R3的1800实体局部read门是不同场景，不能拼接
或据此认为R3历史结果失效。源码审计确认typed observer在普通OFF构建不执行，而当前
拥有窗口、版本校验、逐对象与camera读取路径相对pre-PD不同；尚无受控消融支持将
本次超界归因于128-slot、某一校验或具体模块，未在没有反例/成本证据时加入新优化。
从既有原始块离线计算，legacy读取每操作中位cycles约827→948，RPA约833→949；
cycle计数同样增加约14%，不能仅以墙钟抖动解释。cycles/wall不是独立测得CPU频率，
也不构成某函数的成本归因；本复算没有启动额外native进程。
保留第一正式结果及重开额度，任何新正式登记仍须实际可验证处置和新的binding。

本结果只覆盖固定CPU trace的相对成本及重复性；每进程8块不足p95/p99，不能认证
端点绝对预算、普通host、GPU/audio或P1–P7。只读核验器从751个归档成员重新解析
36份原始测量日志并复算六项结果一致，没有启动native工作负载。

## 5. F1/F2 平台与版本门

候选C=`24f0a914f306cbc20e05b1dccc9f82dac59a6024` / tree
`0bc59434103671b403ffd3bebc77a3df2fee493a` 的11行fresh矩阵全部`local_pass`；
总计13,652项测试注册、0失败、26项显式skip。11项是环境条件下的symlink逃逸测试，
15项是SHARED/MinGW不适用或由target配置条件跳过的测试；逐项名称见矩阵manifest。
每行均独立configure、clean-first build、完整未过滤CTest，并对适用candidate ON行重复
5次控制预算正确性程序。source before/after一致。行级结果：

| 行 | 注册测试 | 失败 | skip |
| --- | ---: | ---: | ---: |
| MSVC candidate ON Debug | 1,420 | 0 | 1 |
| MSVC candidate ON Release | 1,420 | 0 | 1 |
| MSVC candidate ON SHARED Debug | 1,403 | 0 | 6 |
| MSVC candidate OFF Debug | 1,212 | 0 | 1 |
| MSVC candidate OFF headless Debug | 1,067 | 0 | 1 |
| MSVC candidate OFF Release | 1,212 | 0 | 1 |
| MSVC candidate OFF headless Release | 1,067 | 0 | 1 |
| MSVC candidate OFF SHARED Debug | 1,065 | 0 | 6 |
| MinGW candidate ON Debug | 1,268 | 0 | 1 |
| MinGW candidate ON SHARED Debug | 1,250 | 0 | 6 |
| MinGW candidate ON Release | 1,268 | 0 | 1 |

同一候选SHA的Linux Quality、Windows MSVC、Windows MinGW三平台push runs
`42338703031`、`42338703012`、`42338703030`均success；PR #32的PR-event
`42338711734`、`42338711746`、`42338711678`亦success。R4分支push的三平台
`42338703190`、`42338703227`、`42338703142`success。
PR #32 Version Gate run `42338711727`失败，受信checker理由为
`version.sdk_api.approval_invalid: owner/date/unedited record required`；base SDK为0.7.0，
候选为0.9.1。日期构建26.11.07-1已在本候选首次推送前更新；按owner要求不修改SDK批准记录、
不豁免。后继报告提交/版本提交E若被选作最终候选须另行冻结验证，C的F1/F2不自动外推。

只读运行清单、各job状态和实际head SHA在证据包保存。F1/F2此处只裁定候选C。

## 6. 读取整改诊断处置

针对legacy与RPA两项read-long正式超界，隔离一个合同内局部候选：只在现有read窗口中
比较layout owner/layout，避免先复制完整PresentationState再取stamp两个字段；保留原
available()线程/window/serial检查、外层完整版本stamp、错误码/消息、首错和DTO写入行为。
两轴独立只读复核为0发现；Release正常allocator重建成功，Runtime54 cases/11,625 assertions、
Playback185/34,750 assertions通过；对象码确认调用点由`state()`替换为`validateLayout()`。

预登记本地单变量两轮筛选，每轮4测量+4热身上限，未改变正式attempt。首轮每profile仅一对：
legacy prototype/control=**1.01715**，RPA=**1.05562**；read块noise分别0.40–0.64%
及0.87–1.17%，短组内波动低但候选更慢。未达到预先要求两profile都至少快5%，
按停止规则不运行第二轮，不采用该局部代码，不重开正式样本；原固定CPU超界保留。
第一次控制器尝试在native进程启动前因foreground-policy字段缺失停止，0测量/0热身进程；
保留原registration/stopped记录，用新控制器和独立目录重登记。正式额度消耗仍为0。

诊断只有一个进程/臂，不估算置信区间，不证明完整根因。原始日志、环境、binary/source bindings、
对象码以及两次registration均在附件；只读打包器重解析36正式样本，复算六项P0判定，
重算两个内存high/low原始span、矩阵11行，并校验10个远端run SHA。

## 7. O2、F4 与剩余门

## 8. 未完成项与接受边界

O2实体操作清单与F4/PD移交门账随本轮准备；它们不是owner接受记录。
P0普通host、P1–P7、五类Seek仍需各协议真实端点、原负载、稳态时长、observer/pilot及不可变登记；
当前普通runner不能自动处理renderer/capture/controls/Seek/device，不能以同一个common-host夹具
替代各单元。完整内存及P8、生产参数、精确AV/实体故障恢复和O3仍未完成。
历史capture_gap、重复Pause、控制附近FPS波动和CXC现场持有者保持原未定位状态。

Version Gate仍需owner针对所选候选提供有效SDK批准；按owner指示暂不修、不代签、不绕过。
本轮技术进展不关闭PD-5、RPA、Stage7A或整个Stage7，也不授权merge/release。
