# PD-5 40k Gameplay-host 准入阶段 A

状态：completed

更新日期：2026-10-21

快照日期：2026-10-21。本报告记录[阶段 A 计划](../../../../stage_plans/active/stage-07/pd5-gameplay-admission.md)
的定量证据；当前状态由 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 汇总。
[revision 4/hash 会计草案](../../../../proposals/implementation-input/stage-07/pd5-capsule-r4-accounting.md)
尚未接受，本轮不进入 B/C，不声称原样 40k 已获准入。

## 基线与范围

主树 F1 为 eed36648a692333e47f798aa45be36ca850d08de，工作区干净；新分支
codex/pd5-gameplay-40k-admission 位于 D:/Cuexis-pd5-40k-admission，1c7a145 开工留档，
2a564da 登记清单/工具，d6975ce 修正 visitor 适用范围和冻结 probe 选择。
产品源码、现行七项预算、SDK/版本、旧 golden、C2 与旧停止窗口均不修改。
证据根为 out/pd5-40k-admission-20261021-1c7a145。

## 工具资格与审查

第一版隔离构建因 GraphOutput 不具备审计字段失败，修正为仅 Output 可访问钩子；第二版在
Catch INFO 宏的条件表达式处编译失败，补括号后使用全新源码/构建树重建。audit、audit-r2、
原始 source-input.zip、instrumentation.diff、configure/build 日志全部保留，未覆盖失败证据。
这些都在六配置观察前发生，没有替换原生样本。

Standards 审查指出计划元数据枚举/日期问题，已修正并复核；Spec 审查指出 run 可从参数选择
未绑定 exe，现先验证构建目录、exe 路径/清单/摘要并仅执行绑定返回值，负例覆盖错目录和改字节。
两轴只读复核关闭发现，无其他未处置问题；实际构建/执行结果另列，审查不冒充运行验证。

## 执行结果

新建的 audit-r3/source 对应源码提交 083275df80500cbf79756096c127456124177785，诊断构建位于
out/build/pd5-40k-admission-20261021-release-r3。原始 Git archive 库存
0382ca7d140a25606da2647f04a713a200c77ce5a53637d29c2edafc3c47d074；插桩后库存
fe6f884d7f45df00b2ed5a788deb5009def0a0948decbf1fab1aa26a7889fedb。
原 fixture 的归档字节摘要为 edbf5fb3fbdb39004982e2ca0d19efa5a0fec4f43f823eadeff713d4b8a5e271。
原生成循环未改；实际计数保留 40k 实体、requirements、claims、原 namespace/competition。

Release STATIC 诊断构建通过，/WX、candidate ON，metrics/trace/lifetime observer OFF。
新 checked 算术测试 1 用例/7 断言，旧 Capsule 编解码/golden 20 用例/1874 断言，
冻结 Packed 预算 12 用例/179 断言全部通过；Python 62 项、架构与两份 C++ 支持文件格式检查通过。
另运行完整 chart 测试可执行文件，227 用例/42,425 断言通过，含既有 Foundation 静态容量与 golden；
其静态 40k 成功不覆盖本 Gameplay profile，未用它替代当前准入结论。
完整 Debug/Release CTest、shared/external consumer、Linux sanitizer/coverage、跨编译器 golden
本阶段未运行；没有 revision 4 产品实现可供这些后续 B/C 验收。

六个配置各执行一个独立原生进程、全部退出 0，无替换或补样。audit-r3/analysis.json 保存解析后的
完整记录，六份 .log/.process.json 保存原始数据、原生 PID/QPC 起点、退出码及 log SHA。
counts-summary.json / summarize.py 复算模型和贡献；前两次失败构建均未执行六配置观察。

N=1/64 的实际提议 payload 序列化长度与 sizing 一致；旧完整 Writer preimage 的长度、SHA
与 Header digest 均与计数/incremental SHA 一致。40k 的静态 sections 来自现有受预算约束的
encodeStatic 实际输出，四个 Gameplay sections 来自同一 atom visitor 的逐字段精确计数；
不是仅扣除旧文件若干 bytes 的估计。40k 未物化新 Gameplay payload 或新 Header，未实现新 Reader；
因此完整提议尺寸是已核算布局的 exact sizing，不是已获准入的新 wire artifact。

| N / 数值配置 | 提议 decoded section bytes | Header+directory+完整提议 file bytes | 原完整 semantic preimage bytes |
| --- | ---: | ---: | ---: |
| 1 low | 1,751 | 2,295 | 4,296 |
| 1 high | 1,751 | 2,295 | 4,296 |
| 64 low | 9,418 | 9,962 | 158,979 |
| 64 high | 9,410 | 9,954 | 158,979 |
| 40000 low | 6,817,880 | 6,818,424 | 99,588,531 |
| 40000 high | 6,746,120 | 6,746,664 | 99,588,531 |

两种数值配置均包含 40k entities / requirements / occupying claims，逻辑派生投影为 120,000。
新物理字典均为 40,048 strings / 13 refs / 549,885 UTF-8 bytes / 710,089 STR0 bytes；
原字典仍为 160,048 / 120,013 / 30,643,225 / 31,283,429，与前轮六配置调查逐项一致。
复算模型为新 UTF-8=`995+9*N+sumDigits(0..N-1)`，STR0=`1199+13*N+sumDigits`；
low/high 只改 timing.end 数值，不能称字符串复用率对照。

## 完整提议文件的逐 section 算账

Header=96 bytes，directory=14×32=448 bytes。无省略的未知 remainder；下表与 decoded 合计相等。

| Section | records | 40k low bytes | 40k high bytes |
| --- | ---: | ---: | ---: |
| ARCH | 1 | 52 | 52 |
| CNS0 | 0 | 0 | 0 |
| ENT0 | 40000 | 80000 | 80000 |
| IDN0 | 40000 | 680012 | 680012 |
| META | 1 | 94 | 94 |
| REF0 | 13 | 50 | 50 |
| REQ0 | 0 | 2 | 2 |
| STR0 | 40048 | 710089 | 710089 |
| TIME | 1 | 27 | 27 |
| TRN0 | 40000 | 143488 | 143488 |
| GPH0 | 1 | 113 | 113 |
| GPR0 | 40000 | 3725942 | 3654182 |
| GPD0 | 40006 | 487275 | 487275 |
| GRC0 | 40004 | 990736 | 990736 |

所有七项提议物理计数/尺寸在原冻结上限内；这仅证明该布局下本 fixture 的物理需求可容纳。
正式 Writer 的检查完全未改：N≤64 旧 wire 成功，40k 两次仍返回 packed.budget.strings。
proposed_wire_accepted=false、host_reached=false、performance_acceptance=false 始终分别登记。

## Hash、重复展开、派生引用与 scratch

40k 的原 preimage 分拆两种配置相同：static prefix 2,920,218 bytes、global 894、requirements
75,111,124、definitions 1,909,192、coordination 19,647,103，总计 99,588,531，超过旧 16 MiB
检查 82,811,315 bytes。此处由完整原 revision 3 语义 visitor 计数并向增量 SHA 输出；
hash 输出 vector 始终 0 bytes。没有用 streaming 改变正式 Writer 的旧拒绝行为。

五类共享定义在 Requirement 展开中的贡献：pattern 1,240,000 bytes、measure 1,000,000、domain
3,520,000、solver 8,760,000、binding 2,320,000，共 16,840,000，各类访问 40,000 次。
每类仅一个共享定义，超过一次嵌入的重复量为 16,839,579 bytes，不能因物理去重按一次计费。
完整 hash 的 Ix 展开访问数为 400,001（本 fixture `10*N+1`）；projection 文本访问 280,010
（`7*N+10`），累计文本 48,151,678 bytes。逻辑派生数 120,000 与物理 REF0 的 13 行是不同维度。
Claim 展开及其内部 E/resource-key 的 bytes 是嵌套包含关系，不把所有 expansions.bytes 直接求和
冒充不重叠的 preimage；分阶段 bytes 才是可加分拆。

最大单 projection 为 274 bytes，对应 binary framing 134 bytes；N=1/64 为 266/268 bytes。
实测 prefix size 2,920,218，vector capacity 5,120,256 bytes。按 expected/actual 两个 key 文本
同时存活推得 prefix capacity+两 key 最低 buffer bytes 为 5,120,806，即
5,120,256+2×(274+1)，每个 key 的 +1 是字符串终止符；这不是完整 scratch 上界。
allocator slack、Pattern traversal、Model、旧字典观察集合与计数 map 开销未计入该数，不将它
冒充进程峰值或未来流式实现的 heap 承诺。旧大字典仅为阶段 A 观察保留；新 Reader 不得仿造此表。

因此本轮可给必要需求：preimage ≥99,588,531 bytes、expanded reference visits ≥400,001、
maxProjectionKeyBytes ≥274；这些是该输入的必要值，不是已接受的上限。Hash 专用 scratch
完整上界需由后续明确分配/计费设计核验，不能把这次已知 buffer 下界当成允许值。

## 交回、未知与准确恢复点

[会计草案](../../../../proposals/implementation-input/stage-07/pd5-capsule-r4-accounting.md)列出四维
会计、checked/memoized counting、单 key streaming、长 token/path 与共享大定义攻击放大模型、
明确 wire/semantic codec/metadata 映射，以及每维边界/溢出/入口绕过负例。具体上限仍未接受。
还有新会计通过何种 API/内部 profile 载体传入、完整 scratch 上界和诊断 path 待核验；
不擅自修改 installed 头、公共 API/SDK 版本，也不将这些未决事项伪装成现行合同。

阶段 A 定量交付完成；接下来先核验/接受四维工作量 envelope、字段差异与入口载体，再实施 B 的
权威合同/Reader/Writer/metadata，之后才执行 C 的原样 40k Host→Snapshot→Replay→Seek→present→close。
当前没有这些功能成功证据，准入修复尚未完成。没有正式性能登记，旧窗口 3 继续停止，C2 不合入。

handoff-manifest.json 保存最终分支/HEAD、源码/工具/fixture/构建身份、提交列表与历史校验；
review.diff 是相对 F1 的完整可审阅差异。evidence.zip、evidence-index.json、receipt.json 与
verify_packet.py 提供原始证据和只读完整性核验，构建树按 build-binding.json 的绝对路径保留。
新的 B/C 尚未进入；准确恢复点为本报告对应的最终提交，其父系保留 1c7a145 开工点和
083275d 实测工具源码；可按提交逐批撤回新增工具/文档，主树 F1 产品和旧证据无需回退。
