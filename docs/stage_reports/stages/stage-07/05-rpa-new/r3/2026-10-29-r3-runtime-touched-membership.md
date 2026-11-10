# Runtime 本帧触及集合的平方扫描消除

状态：保留局部优化；R3、正式与设备接受保持独立

日期：2026-10-29

返回 [RPA 报告索引](../README.md)。当前状态以
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md) 为准。前一保留恢复点为 `3a44249`，
见[外观查询优化](2026-10-29-r3-runtime-appearance-lookup.md)。

## 成本与设计

本轮继续 P3。全量 Chart v1 动画夹具对每个实体写一个 transform.position.z；
`PropertyResolver::markTouched` 对每条写入线性查找之前触及的实体。40K 个不同实体每帧需比较
`40000 × 39999 / 2 = 799980000` 个索引，成本随实体数平方增长。
前轮临时分段将 resolver reset / layers / overrides 定位为主要成本；该动画归因组原观察器
误用 static 的移动数量校验，其原 invalid verdict 保留，不能替代本轮普通对照。

保留方向使用 Entry 自带的本帧 membership 标记：首次触及先追加原有列表，再置位；
`resetEntry` 随已存在的帧重置清位。保留首次触及顺序、层合成、重复写入拒绝、finalize、
collectCommitEntries、组件校验与事务提交。没有全规模额外位图清零、哈希容器或跨帧借用缓存。

有序 `ensureEntry` 插入新基线时，同时调整 touchedEntries、committedEntries、thisCommit
中受影响的索引，避免 Entry 的标记移动后与旧索引脱节。维护成本为一个内部标记及三份索引的
插入同步；复制/移动由同一 owning Entry 和索引容器共同承担，失败重置继续消费原列表。
同步只在插入新实体时执行，稳态帧不新增该扫描。

MSVC Release 独立编译探针确认，前后 `sizeof(Entry)` 均为 3096 bytes、resolver 均为
136 bytes，新标记使用已有填充空间。拥有 resolver 的 World / Runtime translation units 均重新
编译；公开 Playback 安装接口、SDK 0.9.1、wire / 身份、World 缓存失效及 Snapshot 合同不变。

## 撤回的 C 与新 D

原 C 只有标记，独立审阅发现晚插入较小 entity 后的回归：旧索引会重置错误对象，
被移动 Entry 的 true 标记又阻止后续入队。旧索引移动问题此前已存在，但新标记会使恢复机会丢失，
因此 C 不予保留，不以 GPU fixture 正常为理由忽略接口允许的顺序。

证据 `out/verification-20261029/runtime-touched-v1/`：原固定 18 进程组在 15 个完整进程后停止，
第 16 个 baseline 进程被明确中断，其原日志和 invalid 结果保留，余下两个未启动。
反例在 C 上为 1 case / 18 assertions，2 failures：下一帧 opacity 残留 0.25，期望 1.0。
原源码副本、二进制、登记、原始样本、`stopped.json` 和 red 日志全部保留。

修正插入同步后的 D 使用独立 `runtime-touched-v2/` 目录、源码/库/二进制绑定和新登记，
没有补完旧组、替换原值或在原组追加。新测试同时覆盖 prepared / installed 的插入、
复制与移动、多个属性/层、base 修改、失败后帧重置，以及 commit 后晚插入再直接 rollback。
审阅发现的 thisCommit 直接消费覆盖缺口已补测，最终 World 为 29 cases / 567 assertions。

## 普通分配器真实 GPU 对照

仍使用 revision 3 的真实可见实体夹具、1920×1080、RTX 4060 Laptop、VSync OFF，
普通分配器、metrics / GPU timestamp OFF；全部实体中心在裁剪空间内，每帧命令数完整，
全部动画实体发生位移，末帧橙色材质像素符合要求。
测量默认 CPU mask 65535、BelowNormal、执行节流 OFF，warmup 至少 8 帧且 2 秒。
构建继续单任务、affinity 3 / BelowNormal，旧已封存构建和证据只读。

B 为 `3a44249` 的批量外观查询实现，D 只增加本轮 membership / 索引同步。
事先固定 1K / 10K / 40K，各三对 BD、DB、BD，18 个新进程，每个 60 测量帧。
表中是各进程平均值再取三进程中位数，不是合并全部帧的 p99。

| 动画实体 | B update | D update | update 变化 | B 完整 cycle | D 完整 cycle | cycle 变化 |
| --- | --- | --- | --- | --- | --- | --- |
| 1K | 1.169258 ms | 1.134470 ms | -2.98% | 2.794010 ms | 2.711770 ms | -2.94% |
| 10K | 16.924275 ms | 10.821130 ms | -36.06% | 32.366782 ms | 25.661280 ms | -20.72% |
| 40K | 147.932880 ms | 47.343008 ms | -68.00% | 209.572743 ms | 109.652125 ms | -47.68% |

D update 的三进程相对极差分别为 6.16% / 6.92% / 1.85%。1K 三对并未逐对均改善，
小规模收益仍不能判为稳定；本轮局部保留依据是跨规模中位数和大规模明确改善，并另行检查静态路径。
原正式重复性、配对置信区间和固定窗口要求保持原合同，不由本表替代。
40K D 完整 cycle p99 为 111.2–124.5 ms，仍远超 4.167 ms，本轮不更新稳定 240 FPS 容量点。

静态路径独立固定六个新进程，各 120 帧，40K 以 BD、DB、BD 检查未改变的静态工作量。
静态 update 中位数为 5.513420 → 5.501576 ms，完整 cycle 为 67.950220 → 67.888544 ms，
没有观察到对应中位数退化。两个新组共 24 个有效进程 / 1800 测量帧；原始结果及统计见
`static-guard-results.json`、`static-guard-summary.json`，最终审计通过，因此保留 D。

## 回归、取证与剩余成本

| MSVC Release STATIC 定向套件 | cases / assertions |
| --- | --- |
| World 最终测试集 | 29 / 567 |
| Runtime | 53 / 11,591 |
| Playback | 184 / 34,747 |
| Host | 61 / 42,188 |

构建方式沿用前轮独立 replacement objects 与不变 SDK 0.9.1 库；World / Runtime 测试重新编译，
Playback / Host 使用前轮不变测试对象。最后只新增 rollback 测试时，仅重编该测试 TU 并运行完整
World 套件，没有无理由重测已通过的不变套件。不作为 fresh 全矩阵、SHARED、MinGW 或安装门。

`audit.py` 重算全部普通样本、帧序号、移动/可见/像素证明、日志与二进制哈希、统计和旧 GPU
构建/证据哈希；绑定、编译与 link 命令、layout、red / green、审阅处置及门禁均在证据目录。
最终源码和证据清单由 `final-manifest.json` / `commit-receipt.json` 固定。

P3 仍有 resolver 全量状态重置、变换准备/验证、extract 和正常 OpenGL submit 成本。
本轮不改变 P1/P2/P4/P5/P6 的未完成项、原 15% / 尾部预算、40K 内存与 R3/R4/设备/生产接受边界。
