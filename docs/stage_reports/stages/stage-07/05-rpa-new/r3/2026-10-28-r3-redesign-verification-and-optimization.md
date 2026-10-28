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
- GPU capture-1/2/4 均以 `host.rpa.context_stale: Frame delivery identity is not active` 结束，
  仍需区分测试夹具生命周期问题和真实上下文问题。
- 原设备测试在 Close 后直接检查平铺 `frame->snapshot` 得到空对象；同一帧的 `snapshotView()` 保留 1 个对象。
  已将断言切到 owning view，并在 NVIDIA GeForce RTX 4060 Laptop GPU 上运行精确用例
  `B5 physical OpenGL device submits reads pixels and directed closes`：1 case / 37 assertions 通过。
  现有 retained-control 与 RPA B5 用例也验证 payload-backed snapshot；根因是测试访问过时字段。
- 40K Capacity/Snapshot/Replay 探针因缺少匹配身份的 R4 历史 Packed 参考文件，在编码前停止，不能据此判定解码失败。

## 5. 后续取证和修复顺序

1. 在 VS Developer 环境重跑四个 external consumer，确认许可证白名单是清单遗漏还是环境前置失败；必要时只改验证清单，
   不改变已记录的第三方许可证安装合同。
2. 已将设备探针的过时平铺快照断言改为 `snapshotView()`；上述精确实体设备用例通过。
3. 独立复现 GPU capture 的 stale context，记录 session、frame delivery 和 renderer 生命周期；没有证据前不绕过该门禁。
4. 为 40K Capacity/Snapshot/Replay 寻找已登记且身份匹配的参考 Packed 文件；找不到则保留为未判定。
5. Transform updated 热点已完成一轮增量取证和修复；继续细分 40K decode/re-prepare 与剩余提帧成本，任何优化必须保持
   Packed 字节、FrameDigest、Replay/Snapshot wire、错误顺序和资源所有权不变，并用独立提交保存恢复点。

本报告只记录当前证据和下一步，不宣布 R3、设备或生产性能接受。
