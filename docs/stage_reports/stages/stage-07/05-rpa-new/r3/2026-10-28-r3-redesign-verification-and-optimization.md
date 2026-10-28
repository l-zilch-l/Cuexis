# R3 重构分支全量核验与后续优化登记

日期：2026-10-28。状态：进行中；R3、R4、设备、生产和 Stage 7 均未因此报告退出。

本报告记录 `D:/Cuexis-redesign`、分支 `Cuexis-redesign` 在提交
`d85c9984c366804224b1dddcfd13315a4c191840` 的全量测试与性能基线，并登记后续逐项取证和修复入口。
原始日志、JUnit、登记文件和探针结果保存在被 Git 忽略的 `out/verification-20261028/`；本报告不替代原始字节。

## 1. 工作区和证据边界

- 本批只在 `D:/Cuexis-redesign` 和 `Cuexis-redesign` 分支操作。
- 基线检查时工作树干净；登记的 1067 个源码文件没有漂移。
- `python -B tools/check_docs.py`、版本检查和 `git diff --check` 通过。
- 本地 MSVC 结果不能替代 Linux sanitizer/coverage、正式 ETW、设备音画接受、峰值内存或生产验收。
- 诊断点值用于归因；不能把单次本机结果写成正式预算通过。

## 2. 全量测试基线

| 配置 | 总数 | 通过 | 失败 | 跳过 |
| --- | ---: | ---: | ---: | ---: |
| 完整桌面候选 Release | 1393 | 1388 | 4 | 1 |
| Release STATIC | 1241 | 1236 | 4 | 1 |
| Debug STATIC | 1241 | 1236 | 4 | 1 |
| Observation STATIC | 1249 | 1244 | 4 | 1 |
| Release SHARED | 1224 | 1213 | 6 | 5 |

基线配置的四个共同失败是 `cuexis_external_consumer_find_package`、
`cuexis_external_consumer_find_package_playback`、
`cuexis_external_consumer_find_package_core` 和
`cuexis_external_consumer_find_package_audio_sdl`。保存的完整套件输出在许可证清单核验处报告了
`fnv-copyright` 未登记；之后的精确重跑又在嵌套消费者 configure 的 MSVC try-link 处遇到
`LNK1104 kernel32.lib`。在 VS Developer shell 中补齐 gate 清单后，verification-full 的四个
Release consumers 通过 4/4，redesign-shared 的四个 SHARED consumers 通过 4/4，两个 Shared
import checks 通过 2/2。许可证合同本身未改，修复的是验证清单与已安装文件不一致的问题。

SHARED 的两个 import 失败复用了这些消费者路径；清单修复后已通过 2/2，不再构成连带失败。
唯一的普遍跳过是测试环境没有可用目录 symlink；其余跳过和适用性见对应 JUnit。

Python 验证共尝试 28 个模块，26 个直接通过，共 299 个 unittest 用例，2 个显式跳过。
`check_gameplay_assembler_tests.py` 和 `check_gameplay_host_tests.py` 需要显式工具与 fixture 参数；
无参退出码 2 不计为产品失败，相关 CTest assembler CLI 门禁通过。

格式检查发现 55 处既有违规，集中在 `recovery.hpp`、`execution_kernel.cpp`、
`fold_recovery_tests.cpp` 和 `rpa_b3_tests.cpp`；这不是本轮 R3 代码变更导致的功能失败，后续另行修复。

Player 图形 smoke 和 audio smoke 均通过。Release 桌面构建成功，文档、版本和源码漂移检查通过。

## 3. Packed / prepare 性能基线

以下为 Release STATIC、正常分配器、Packed v4、CPU test renderer、source binding、无音频的独立诊断组。
阶段时间来自 `performance/phase-results.json`，不是正式设备验收：

| requirements | source prepare | Packed encode | content decode / re-prepare | Host load | setup 合计 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1,000 | 32.208 ms | 27.240 ms | 67.648 ms | 9.311 ms | 144.382 ms |
| 10,000 | 408.700 ms | 326.375 ms | 825.149 ms | 93.750 ms | 1.724 s |
| 40,000 | 1.806 s | 1.369 s | 3.564 s | 388.824 ms | 7.389 s |

完整 Packed 对照组的 40K setup 为约 7.44–7.47 s；10K 为约 1.72–1.74 s。
优化后的声明查找已消除此前的平方扫描；当前 40K 剩余成本主要集中在 decode/re-prepare、timer、identity/closure、
资源准备和图复制，不能通过删除校验来换取点值。

在 40K read/update 探针中，read 为约 428–431 ms / 64 次（约 6.7 ms/次），updated 为约
619–623 ms / 32 次（约 19.4 ms/次）。这些是下一轮细分的热点，尚未证明存在可安全删除的语义步骤。

### 3.1 Transform 增量路径取证与修复

本轮将 Runtime 的 Transform prepare 分为两条明确路径：默认 `prepare()` 保留完整缓存结构、
层级和本地变换验证；只有 Runtime 在连续帧中确认缓存仍由自身拥有时才传入 `cacheTrusted=true`，
并只传播写入节点及其后代。Runtime 的可变 World 回调会使缓存失效，下一帧自动回到冷缓存重建。
脏节点按缓存中的拓扑序提交，避免实体 ID 顺序与父子顺序不一致；新增反向实体创建顺序回归用例。

验证结果：World 25 cases / 421 assertions、Runtime 53 cases / 10,768 assertions 全部通过；此前
World 的 `PD3a World allocation failures and derived overflow preserve active state` 回归已恢复。
Playback 先前同一优化批次的 130 cases / 17,185 assertions 全部通过。

同一 40K Release MinGW 静态候选探针（64 read、32 updated、分段计时）输出保存在
`out/verification-20261028/transform-followup-20261028-mingw-release.txt`：read 提取
282.952 ms / 64（4.421 ms/次）；updated 的 update 为 360.041 ms / 32（11.251 ms/次），
extract 为 153.705 ms / 32（4.803 ms/次），合计 513.746 ms / 32（16.055 ms/次）。相对本报告
基线区间，read 总提取约下降 34%，updated 合计约下降 17%；这是本机诊断结果，不是 R4 或设备接受。

## 4. CPU / GPU 诊断结果

- CPU stream 1200 样本：request-to-ready p99 0.352 ms，request-to-present-return p99 0.377 ms。
- GPU hidden smoke（vsync 开启，未按正式窗口采样）：GPU render p99 0.585 ms，CPU submit p99 3.848 ms，
  present-return p99 5.082 ms，最大 27.2 ms。这只用于诊断。
- GPU capture-1/2/4 原先以 `host.rpa.context_stale: Frame delivery identity is not active` 结束。
  已确认测试夹具给 renderer 沿用了 CPU consumer 的单 payload / 单 attempt 配额，低于生产注册要求的各两份。
  仅修正 GPU renderer 的测试配额后，独立复核的 capture-1/2/4 均通过：每项 8 frames、2 warmup、
  GPU timer 开启，分别为 169 / 229 / 349 assertions，GPU render 各 8 次，关闭后 payload / attempt
  live count 均为零。bounded attempt 边界另通过 13 assertions；生产配额校验未放宽。
  原始日志、构建和二进制哈希见 `out/verification-20261028/performance/gpu-profile-review-20261028T172524/`。
  主代理另行运行 capture-4（349 assertions）与 attempt 边界（13 assertions）复核，均退出 0；
  日志为同级 `gpu-profile-parent-review-capture-4.log` 和 `gpu-profile-parent-review-boundary.log`。
  此结果仅为有限功能回归，不是正式设备性能接受。
- 原设备测试在 Close 后直接检查平铺 `frame->snapshot` 得到空对象；同一帧的 `snapshotView()` 保留 1 个对象。
  已将断言切到 owning view，并在 NVIDIA GeForce RTX 4060 Laptop GPU 上运行精确用例
  `B5 physical OpenGL device submits reads pixels and directed closes`：1 case / 37 assertions 通过。
  现有 retained-control 与 RPA B5 用例也验证 payload-backed snapshot；根因是测试访问过时字段。
- 40K Capacity/Snapshot/Replay 探针因缺少匹配身份的 R4 历史 Packed 参考文件，在编码前停止，不能据此判定解码失败。

## 5. 后续取证和修复顺序

1. 已修复许可证白名单遗漏；四个 Release consumers、四个 SHARED consumers 与两个 Shared import checks
   的精确重跑通过，详见第 2 节。
2. 已将设备探针的过时平铺快照断言改为 `snapshotView()`；上述精确实体设备用例通过。
3. GPU capture 的测试配额修复和独立有限功能复核已完成，详见第 4 节；正式设备性能接受仍独立。
4. 为 40K Capacity/Snapshot/Replay 寻找已登记且身份匹配的参考 Packed 文件；找不到则保留为未判定。
5. Transform updated 热点已完成一轮增量取证和修复；继续细分 40K decode/re-prepare 与剩余提帧成本，任何优化必须保持
   Packed 字节、FrameDigest、Replay/Snapshot wire、错误顺序和资源所有权不变，并用独立提交保存恢复点。

本报告只记录当前证据和下一步，不宣布 R3、设备或生产性能接受。

### 5.1 Packed 分段取证登记

恢复点为 `bb923378cb9164d70dde3c12fee3fba95d98bc3b`。本轮保留全部原始证据，
MinGW Release STATIC 诊断构建 `out/build/packed-stages-20261028/` 在链接时遇到 inline thread-local
metrics 的 TLS init 多重定义，未取得有效采样。实际有效诊断使用 MSVC Release STATIC 构建
`out/build/packed-stages-msvc-20261028/`，启用 `CUEXIS_PD_METRICS_TEST_ONLY=ON`，
复用已安装依赖且禁止安装依赖。这是诊断构建；宏关闭的普通构建另行验证。

本轮依次处理：Packed 完整 content 创建归因、证据支持的最大热点优化及回归、
GPU capture stale context 生命周期取证、身份匹配的 R4 历史参考文件缺项。
后两项没有完成证据时保持未判定。

计时树为 `gameplay_content_create -> packed_decode -> packed_link_validate /
packed_identity / packed_reconstruct`，其中 reconstruct 下再分
`packed_shared_prepare` 和 `packed_semantic_compare`。外层 inclusive 仅用于总耗时；
逐段 exclusive 用于归因，不累加嵌套 inclusive。解析、owner 构造和析构等未单独分段的
操作保留在对应父阶段 exclusive 中。

40K `[followup]` 夹具使用 Chart v1 presentation source，只能验证 read/update；
Packed 成本须通过 `[redesign-host-perf]` 的真实 Packed Gameplay 路径取证。
本登记不把 presentation-only 测量当作 Packed decode 样本。

有效组 `packed-prepare-stages-clean-clean` 的 1K / 10K / 40K 均退出 0。40K content 创建 inclusive
为 3513.460 ms，其中 packed shared prepare 为 1724.810 ms，packed identity 为 562.633 ms，
semantic compare 为 276.666 ms。prepare 子阶段记录了 source prepare 和 decoded re-prepare 两次调用；
assembly 的 1555.659 ms 是这两次调用的合计，不能写成单次 prepare 成本。

assembly 八段探针已完成 MSVC 串行构建和 40K 有效采样，组名 `packed-assembly-stages-v1`。
source prepare / decoded re-prepare 的 assembly inclusive 分别为 896.580 / 773.779 ms；其中身份生成
290.157 / 271.328 ms、闭包 188.279 / 186.856 ms、稳定化 203.179 / 103.248 ms、图构建
99.334 / 100.053 ms。此次 content 创建 inclusive 为 3692.030 ms，shared prepare 为 1857.310 ms。
与前组波动保留，不以不同组差值宣称优化。下一步针对 content-only closure 的整图深拷贝验证优化。

四路 clean build 因占用过多用户机器资源而停止，
该中断构建及日志保留，不视为有效测量。后续构建统一单路、低优先级，限制在两个逻辑处理器上，
禁止子代理或其他配置同时构建。已清理中断编译的进程；后续恢复采用增量构建。

### 5.2 Content-only closure 无复制取证与优化

`validateClosures` 原先为排除 ruleset / presentation contributions 而深拷贝整张
`CanonicalGameplayGraph`。现改为私有 closure collector 显式接收 contribution 集合；公开入口仍包含
原 contributions，content completeness 入口仅传空集合，不复制 graph。内容遍历、revision 精确比较、
排序和去重保留；全 unknown、全 disabled、capability completeness、feature completeness 的首错顺序不变。
新增两个用例、六个 sections，覆盖贡献与内容重叠 / 重复、错误 revision 和多错误优先级，并检查输入不变。

普通 MSVC Release STATIC（metrics OFF）四个相关测试可执行文件全部通过：Judgement
231 cases / 55,999 assertions，Packed 49 / 5,614，Playback 181 / 34,528，Host 61 / 41,125。
独立身份诊断在优化前、后分别验证 24 个场景 / 1,898 assertions；chart / content / prepared / runtime
共 96 份原始字节文件逐文件完全相同。证据为 `performance/identity-before/`、
`identity-after-closure/`、`identity-after-closure-comparison.json`；不是仅比较两个新实现入口。

同一 MSVC Release STATIC、正常分配器、metrics ON、单逻辑处理器 affinity 4、BelowNormal / HighQoS、
40K requirements / 33 records 的诊断组对照如下；每组只有一次 setup，不能代替重复性或正式性能接受：

| 阶段 | assembly-stages-v1 | closure-stages-v1 |
| --- | ---: | ---: |
| source prepare 的 closure | 188.279 ms | 38.072 ms |
| decoded re-prepare 的 closure | 186.856 ms | 38.946 ms |
| decoded re-prepare 的 assembly | 773.779 ms | 630.821 ms |
| packed shared prepare | 1857.310 ms | 1697.230 ms |
| content 创建 | 3692.030 ms | 3553.320 ms |
| setup 合计 | 7722.368 ms | 7453.577 ms |

closure 的对应段约下降 79%；shared prepare 的对应点值约下降 8.6%。外层其余成本和运行波动仍保留，
不将局部下降外推成完整路径同比例改善。原始 registration / results / JSONL / logs 在
`out/verification-20261028/performance/packed-{assembly,closure}-stages-v1-*`。
测试失败的 assembly 不再产生虚假的 requirements 子阶段样本；成功路径计时口径不变。
