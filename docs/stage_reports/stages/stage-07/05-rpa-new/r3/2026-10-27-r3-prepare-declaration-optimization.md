# R3 prepare 声明查找细分与优化

日期：2026-10-27。状态：已实施并完成本地核验；R3 未退出。

承接用户继续细分和优化 prepare 的授权，以及[上一轮 Packed 索引优化](2026-10-27-r3-packed-index-optimization.md)。
唯一工作区为 `D:/Cuexis-redesign`，唯一分支为 `Cuexis-redesign`，起点为
`572931a47deee8bff6f498f32b5bdda6b65f3e9b`；未操作其他分支。

## 1. 精确归因

在 `out/prepare-probe-20261027/` 生成 prepare/assembler 诊断副本，产品源码保持起点字节。
独立三个新进程覆盖 1K/10K/40K，记录 prepare、assembly 各阶段和声明查找的实际扫描量。
原查找计数从最终命中位置计算，不在每个线性比较中递增；调用级计时存在开销，
诊断点值只用于归因，不混入普通性能组。

40K 每次 prepare 中：

| 路径 | 调用数 | 原线性扫描项数 | 首次 prepare 内计时 s |
| --- | --- | --- | --- |
| mergedNamespace 回填 | 40,003 | 800,140,006 | 4.844055 |
| findDeclarationById | 80,000 | 1,600,280,000 | 9.002278 |

其中 ID 查找主要用于 Requirement 声明验证和 relation members 验证；合并回填按每条原始
input 重新扫描整张排序表。对 N 个 Requirement 的该固定夹具，前者扫描量为
`N*N + 7*N`，后者为 `(N+3)*(N+4)/2`，在 1K/10K/40K 的两次 prepare 中均精确成立。
完整 Packed 的 source prepare 和 Reader re-prepare 各执行一次，因此成本重复发生。

40K 首次诊断的 assembly merge/validate 分别约 4.895/9.103 s，资源准备约 0.218 s、
Requirement 编译/验证约 0.203 s、timer 阶段约 0.591 s。声明重复扫描是该轮首先应处理的主因。
这些带探针计时不能与普通组时间直接相减。

## 2. 产品改动与排序依据

仅修改 assembler（`engine/judgement/src/gameplay_assembler_*.cpp`）：

- `findDeclarationById` 从线性扫描改为按完整 `StableDeclarationId` 二分，缺失项继续返回空指针。
- 合并回填同样按完整 stable ID 二分定位，然后填写 references、required features/capabilities。
  不新增缓存或索引容器，也不改变输入处理顺序、错误优先级或内容规范化。

排序前置在唯一调用链真实成立：merge 先插入所有输入，按 stable ID 排序并拒绝重复 ID，
然后才解析引用/回填。`canonicalCompare(MergedDeclaration)` 的首要键就是 stable ID。
回填只改变非键字段；后续 stabilization 保持该声明顺序，content validation 和 relation
validation 才调用 ID 查找。回填输入已全部插入且通过重复检查，二分结果必然存在。
未改变 prepare 入口、任何校验/哈希/预算、公共 API、ABI、wire 或依赖。

优化后的独立诊断位于 `out/prepare-detail-20261027/`，继续测相同阶段。
40K 每次 prepare 的 ID 查找为 1,228,940 次二分比较，回填为 614,517 次比较；
各规模均满足每次查找不超过 `ceil(log2(N+4))` 次比较。
诊断 JSON 的 `scanned` 在这两条新路径表示 comparator 次数，旧路径表示线性扫描项数，
二者不能当作同一种访问字节量。

## 3. 普通完整 Packed 对照

改产品前先登记 `out/prepare-baseline-20261027/`，改后另登记 `out/prepare-opt-20261027/`。
两组各六个新进程，固定顺序为 33 条历史的 1K/10K/40K，再 129 条历史的三规模；
每进程首次和两次重复，各 18 行全部有效、exit 0，sameResult 和目标呈现检查通过。
没有覆盖、重试或替换失败样本。

两组同为 Release STATIC、正常分配器、observation OFF、mask=4、HighQoS、source binding、
Packed v4、单对象 CPU 测试 renderer、无音频。Host 回复期限 5 s、外部保护 600 s 保持原值。
完整准备包括内存 source prepare→encode→fromPacked decode→Host load；不包含磁盘/CXC IO、
真实 GPU/audio，不使用 typed 捷径。构建及后续回归在普通计时组之外执行。

| requirements / 历史 | 本轮基线准备 s | 优化后准备 s | 准备减少 | 首次请求到呈现 ms | 两次重复 ms |
| --- | --- | --- | --- | --- | --- |
| 1K / 33 | 0.177066 | 0.144766 | 18.2% | 0.718 | 0.484 / 0.374 |
| 10K / 33 | 3.047706 | 1.751449 | 42.5% | 4.650 | 3.975 / 3.937 |
| 40K / 33 | 27.844833 | 7.443408 | 73.3% | 15.476 | 15.225 / 14.882 |
| 1K / 129 | 0.169504 | 0.149556 | 11.8% | 0.767 | 0.528 / 0.492 |
| 10K / 129 | 3.157952 | 1.738260 | 45.0% | 4.167 | 4.032 / 4.170 |
| 40K / 129 | 27.625113 | 7.634911 | 72.4% | 16.448 | 15.380 / 14.848 |

以上是固定本机夹具的独立进程点值，不提供置信区间、p99、正式性能或设备接受结论。

## 4. 优化后阶段及剩余成本

本轮 `phase-registration.json` 另登记三个新进程，仅在宿主副本增加外层阶段时间戳，
链接当前产品库，无核心函数插桩；外部保护 300 s、Host 期限 5 s，数据与普通组分开保存。

| requirements | source prepare s | Packed encode s | content decode s | Host load s |
| --- | --- | --- | --- | --- |
| 1K | 0.033368 | 0.025650 | 0.066929 | 0.009274 |
| 10K | 0.426342 | 0.333790 | 0.839311 | 0.089199 |
| 40K | 1.800410 | 1.371361 | 3.480151 | 0.391887 |

细探针的 40K 两次 prepare 中，assembly merge 已分别约 48/46 ms，ID 查找总时间约
66/44 ms，回填约 20/19 ms，原平方扫描主成本已消除。
该诊断里的剩余 timer 阶段约 437/454 ms、identity 阶段约 270/265 ms、closure 验证约
188/166 ms、资源准备约 153/156 ms，另有图复制、稳定化及编译/验证等成本。
后续可继续细分 timer 生成与 identity/closure 处理，但不能只凭这些点值删除重复验证。
本批未修改这些路径，也未证明全部剩余成本已最优。

## 5. 兼容与回归

- 新增三文档、稀疏 ordinal、同 ordinal 跨文档、前后向引用、元数据回填及重排测试。
  覆盖首项之前、间隙、末项之后和文档边界缺失；在旧实现先通过 1 case / 19,390 assertions，
  再随新实现完整回归。该测试锁定行为，不以计时阈值作为单元测试。
- 同一独立 driver 分别链接保存的旧 assembler 目标文件和当前 library，运行 96 组成功/
  拒绝场景，包含重排、重复 ID/名称、跨文档缺失、错误 kind、Requirement/relation 缺失及
  pattern 名缺失。Chart/Content/Prepared identity、声明/引用元数据、prepare 结果和错误 code/
  message/全部 context 的序列完全一致：280,814 B，SHA-256 为
  `0a7651f15ea8f36857b54b3c14fd079640dd126a1f6ecfd54105d1335ba5c9fd`。
- 1K/10K/40K 的 Packed 大小与 SHA-256 在改前/后探针完全一致；40K 为 4,779,779 B，
  SHA-256 `fb07ae17cceba38e69d76149eea141d5ac1b53b21b32ea4f95baf8b510aca5fc`。
  Replay/Snapshot wire 对照仍与来源绑定实验一致，1 case / 44 assertions 通过。
- Release STATIC、Debug STATIC、Release SHARED、Release observation STATIC 均完成全目标
  增量构建及 GameplayPacked、Judgement、RPA、Playback、ExecutionHost、HostProtocol 六套相关回归。
  Judgement 为 229 cases / 55,887 assertions；SHARED Playback 为 172 passed / 1 skipped，
  跳过原因为私有 test-only parse probe 对各 DLL 隔离；其余适用用例通过。
- STATIC 架构/已安装 Gameplay 消费者 2/2 通过，SHARED 加导出面检查 3/3 通过。
  本批未运行全仓 CTest、Linux sanitizer、真实设备、prepare-only 峰值堆或 decoded semantic heap。
- 既有审阅代理独立只读检查全部三个 ID 查找调用点、排序/必命中依据、错误顺序和新增测试，
  未发现重要问题；其未重跑构建或测量。主代理已亲自检查差异并执行上述验证。

## 6. 证据和一次产物覆盖

`out/prepare-probe-20261027/` 保存起点源快照、基线细探针及三进程 raw logs/登记；
`out/prepare-baseline-20261027/` 保存普通旧实现组；`out/prepare-detail-20261027/` 保存新实现细探针；
`out/prepare-opt-20261027/` 保存普通新实现组、外层阶段组、oracle、四配置回归和安装/导出门。
`audit_results.py` 核验全部 raw log 哈希、样本数、源码身份、当前二进制、扫描公式、比较上界、
Packed/wire/oracle 一致性；`audit.json` / `final-source.zip` / `final-manifest.json` /
`commit-receipt.json` 保存最终证据身份。

本轮曾误调用上一轮 `link_phase.py`，覆盖 `out/packed-opt-20261027/packed_phase.exe`。
该旧 exe 原字节已不可用，不声称精确恢复；原登记、raw logs、结果及哈希清单均保留且再次核验。
`previous-artifact-overwrite.json` 保存原/现哈希和发生原因；对上一轮 manifest 的检查确认唯一
变化文件为该 exe。本轮阶段组已使用独立目录、独立登记及输出路径断言，未以覆盖后的 exe
冒充上一轮二进制。普通基线 exe 原哈希已登记，未另保存其副本；旧 assembler 对象独立保存。

本批保持全部接受门。准备成本进一步下降不宣布 R3c、R4、设备、内存、生产接受或 Stage 7 关闭。
