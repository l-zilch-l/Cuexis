# R4 World 页存储整改效果验证

状态：scoped memory reduction verified；完整语义内存仍为 unrated；O1 未变

日期：2026-11-06

返回 [New RPA 报告索引](../README.md)。本报告承接
[128-slot 实施](2026-11-05-world-storage-remediation-implementation.md)和
[原语义内存失败](2026-11-05-semantic-memory-remediation.md)，原始输出和只读复算见
[本轮证据](2026-11-06-world-storage-effect-evidence/index.md)。

## 1. 授权、范围与候选

owner 授权先验证现有优化，效果不好则继续优化；若优化后仍不理想，先汇报，再由 owner
考虑是否提高预算。本轮未选择任何新预算数值。执行前登记最多两个 64/high 容量诊断进程，
每个进程最多 120 秒；第二个仅在第一轮不理想、有具体整改后另行登记。正式采样、40K/P8
未纳入本轮。旧的两轮诊断及失败原件保持不变，本轮单独记账。

新源码为 `e643cd5bf4f12d64752cdce7f33bf52a31be7da4`，历史对照为
`5fd1bd3c90ebba767e2b6ac54375d63f209c2504`。两者产品实现差异仅为七种非空 World
组件的类型级 `page_size=128`；其他差异是版本元数据、页边界测试与报告。
执行观察器、语义 bridge、计费工具及原固定 Snapshot/Seek/Replay 工作流均未改变。
本轮从干净提交新建构建树，依赖清单、编译命令、编译器和可执行文件 hash 均已绑定；
运行前后源码与二进制一致。后继报告提交不自动成为本轮受测候选。

两次诊断均为同一 MSVC 19.51.36256.0 x64、Release STATIC、candidate ON；观察开关、
依赖 baseline 与 workload 环境一致：N=64、reuse=high、capsule revision=3。新构建没有
使用旧目录增量覆盖。需要本机下载的命令统一使用 `127.0.0.1:7890` 代理。

## 2. 实测结果

| 同一 active World 拥有子集 | 历史 1024-slot | 当前 128-slot | 减少 |
| --- | --- | --- | --- |
| 稳定 registry 子集 | 145,249 B | 77,114 B | 68,135 B |
| 三个实际组件页 payload 区间并集 | 212,992 B | 26,624 B | 186,368 B |
| 唯一拥有下界合计 | **358,241 B** | **103,738 B** | **254,503 B（71.04%）** |

该比较基于真实 Playback active 转移后的同一 census，按 allocation ID 去重；实际组件页
只计 backing allocation 内的 payload 区间并集，不把 MSVC 对齐前后缀当作 payload。
组件页 payload 从 212,992 B 降到 26,624 B，恰为原来的八分之一；registry 子集也含受缩页
影响的纯值组件存储，不能把此前的 145,249 B 全称为不可变固定成本。

这是相同、已覆盖拥有子集的字节差额，不是完整 World、整个进程或峰值内存节省量。
原 `state.world` ceiling 仍为 327,680 B。新下界低于 ceiling，但它没有包含全部拥有对象，
两者之差不能当作完整 World 可用余量；验证器保留 `unrated`，不签发 memory pass。

## 3. 正确性、计量与释放

- 容量诊断只运行一个进程，0.0729709 秒正常结束，21 assertions / 1 case 通过；
  这个时长只是受观察器影响的进程墙钟，不是产品性能读数。
- 运行前四个独立 allocator 契约进程通过：正向计量、未知 root 拒绝、root 表耗尽拒绝、
  page span 越界拒绝。正向用例覆盖实际 MSVC 内部对齐地址和重叠别名去重。
- 解析器的 11 项 Python 用例通过。原始日志中 19 个 checkpoint 的 global/domain partition
  一致，未发生 sticky observer overflow；prepared、runtime-installed、active 的独立转移保留。
- Close 后已跟踪 registry 子集真实 released；原有合法内容、结果与 fixture 依次释放后，
  产品 C++ new requested/storage 归零。测试/观察器自身的其他存储独立披露，未伪称进程归零。
- 独立子代理仅从新旧原始日志复算唯一分配与 span union，确认相同降幅和释放边界；
  [只读核验器](2026-11-06-world-storage-effect-evidence/verify.py)再次复算一致，未启动额外容量进程。

本轮没有新增产品或计量实现修改，也没有运行第二个容量诊断进程。旧失败仍绑定旧源码，
不追改成通过；本轮是新候选的限定改善证据。

## 4. 处置与剩余工作

当前缩页对已证实的主要开销有明显效果，因此保留 128-slot 实现，不继续加入新的存储机制，
也不建议基于本轮结果提高预算。本轮剩余的第二个诊断进程未使用，不自动转作正式或 P8 额度。

完整 World 图（包括对象本体、ObjectEntityMap、嵌套字符串和 transform scratch）、其他
current/candidate/shared retained/prepare-only 拥有关系、跨域同时峰值、observer reference
扰动，以及 native/GPU/driver 与设备退休仍待闭合。未覆盖对象可能使完整 World 超出原预算。
128-slot 对 2048/40K 的分页次数与产品吞吐影响也未由本轮小规模内存诊断验证。
正式 memory/P8、完整 F1/F2 和最终接受不据此恢复或关闭。

如果后续完整 owning 证据仍超出原 cap，再按真实成本与容量需求提交继续优化或提高预算的
具体选项，不用当前下界反推新上限。

## 5. PR #32 平台预检跟进

owner 指定将工作推送至既有 PR #32；已将 `e643cd5b` 快进推送到其远端 head `stage-7`。
该 head 的 PR-event Windows MSVC `42054754029` 和 Windows MinGW `42054754229`
均成功；Linux Quality `42054754043` 第一次的两个失败均为 CMake 下载 HTTP 504，
本轮只重跑失败 job，第二次已成功。通过项未重跑，原始网络失败保留。

Version Gate `42054753911` 仍因 SDK owner approval 无效而失败：可信 base 的 SDK API
为 0.7.0、候选为 0.9.1，需 owner 针对所选候选提交精确授权记录，代理不能代签或绕过。
这些平台预检不替代完整 F1/F2，也不自动验证后继报告提交。报告与状态本轮仅含文档和证据变更。
