# R2 单行借用候选续做与父代理核验

日期：2026-10-25（Asia/Shanghai）。状态：第二候选已实施，父代理完成正确性/包边界与固定局部诊断核验；全部收益区间仍 inconclusive，候选未接受，R2/R4 未退出。

前一候选和失败窗口保留在[首版报告](2026-10-25-r2-borrowed-presentation.md)；当前范围与门禁由[整改计划](../../../../stage_plans/active/stage-07/plan-rpa-performance-remediation.md)持有。本报告不改写前一候选的 25.3% 单对象退化或未确认的 N64 收益。

## 1. 本次决定与授权

用户要求继续 R2，允许借助子代理，但核验必须由主代理亲自完成。此前“设计和规划由主代理完成”继续适用。主代理选择同一中性同步读取合同内的单行 typed visitor 回退，并写出 `parent-design.md`；无上下文、Max Luna 子代理仅按指定方案编辑五个文件，没有设计、规划、构建、测试或测量。主代理亲自核对全部增量，并执行本报告中的验证。

主文档树仍为 `D:/Cuexis-worktree` / `stage-7` / `0157486800f8f40a4121ef0d5ab26bd041a08304`；实施树仍为 `D:/Cuexis-pd5-40k-admission` / `codex/pd5-gameplay-40k-admission` / `3e5837f63f76397429c9f7603fd7450d3ce9855d`。开始时分别 20、77 条既有 dirty 状态。主代理核对前批代码和四文档的结束哈希后，再保存本次存在/缺失、原始字节和哈希。证据根为实施树 `out/rpa-r2-single-row-20261025/`。

本次不扩大到 R3/R4、正式窗口、40k、ETW、设备或提交/推送/合并；不重跑前一窗口、不向前批追加样本。

## 2. 第二候选

首版编译结果已证明每次进入物化都预构造 32 行、gather 再对实际行置值；它不能解释全部已测退化，但明确包含可删工作。第二候选直接移除批量机制，采用 `visitSnapshotObject(index, context, consume)`：Runtime 产生一个函数局部中性借用行，Playback 同步消费后再访问下一对象。

实现只有 Runtime 的 `presentation_read.hpp/.cpp`、Playback 的 `presentation_materializer.hpp/.cpp` 和 Runtime 借用测试五个文件发生本批变化。主代理补了 first-outside/SIZE_MAX 与旧 owning 读取的错误码/消息对照及实际布局断言。原 `readObject`/Gameplay 发布保留；旧块接口、容量常量、索引数组和行数组从活动产品路径删除。没有手工对象生命周期、裸 registry 产品接口、跨帧缓存或池。

同一外层 owner/stamp/window/serial、const World、mutation/reentry 和 RAII 合同保持。Runtime 先检查可用性和索引，再只读 gather；行中的矩阵仍指向真实 WorldTransform。Playback 保留逐行字段、资源、错误及部分写入顺序；失败不访问下一对象，成功后才恢复最终对象数，再处理相机。每个对象仍通过 Result&lt;void&gt; 传播消费者错误，不能称为删除了全部逐对象 Result 开销。

## 3. 布局与亲自核验

主代理使用当前材料化目标的实际编译选项，分别编译并执行 Debug/Release 类型探针：MSVC 19.51.36256.0、STL 145 / update 202604、x64、C++20，iterator debug level 分别为 2/0。

| 实际类型/逻辑存储 | sizeof / 合计 | alignof |
| --- | --- | --- |
| ObjectPresentation | 128 B | 8 B |
| Result&lt;ObjectPresentation&gt; | 144 B | 8 B |
| Runtime 局部 ObjectVisualRead | 72 B | 8 B |
| 实际 SnapshotReadScratch 消费上下文 | 88 B | 8 B |
| 两个实际对象同时存活的显式逻辑存储 | 160 B | 分项如上 |

源码对实际类型和 160 B 合计设置当前目标断言，并将内部逻辑存储上界设为 512 B。合计排除编译器 frame、返回值、gather closure 状态、相机与错误构造，不是整个调用链栈峰值；没有造替身 aggregate 代算。首版 2648 B 仍是其历史真实 aggregate，不因第二候选变化回填。

父代理聚焦结果：Runtime 4 cases / 100 assertions，Playback 5 cases / 6434 assertions 通过；后者继续使用被冻结旧物化器，逐字段、完整错误/context 和部分写入对照，覆盖原 0/1/31/32/33/65 对象布局。预热成功路径零 C++ 分配、owner/逃逸/旧 serial/重入/mutation、真实矩阵地址、缺组件、callback 错误/异常/bad_alloc 继续覆盖。

父代理完成五个独立 Debug/Release observation ON/OFF 与 Release SHARED OFF 构建及 47 条适用回归/架构检查，均通过且前后源码哈希一致；STATIC staged consumers 3/3、SHARED 6/6 通过。SHARED 原一项私有 parse probe 和三项可执行文件分配注入跳过保留。实际安装头 ASCII、八个本批 C++ 路径格式门通过。证据为 matrix-test.json、integration-results.json 与 parent-assembly-check.json；不是子代理代跑。父代理核对 Release 对象确认 32 行构造器消失、单行 visitor 调用存在。

## 4. 诊断和回滚边界

新建独立 B/D 树及构建；B 从前次冻结 B 的实际源字节复制后重新构建，D 为本次单行候选，两侧使用完全相同且已验证的原诊断夹具。A/Q4 不重跑，旧 inconclusive 保留；新结果只回答 D/B 局部变化，不能拼乘旧 A 对照得到 R4 接受。

执行的一次新身份固定窗口为：B-D / D-B / B-D，原 1/8/64/256 规模和读取数，原 mask=4、普通 allocator、每进程 30 s 机器热身、每规模 fresh-session 64-step 预热、8 blocks 且各 ≥100 ms。身份/退出/短块/输出错误停止；块内和进程间范围仍以 15% 报告，所有样本保留，不加轮追测。

本批五路径增量在 `rollback/code/manifest.json`，主代理已于隔离目录验证 start → 正向 check/apply → end → 反向 check/apply → 原始字节及存在/缺失状态恢复；现场字节未作反向尝试。前一候选补丁和原始证据不变。完整九路径 R2 累积补丁在 `rollback/cumulative-code/manifest.json`，四文档最终补丁在 `rollback/final-documents/manifest.json`；父代理均按真实起点完成正反应用和字节恢复，不冒称整树回滚。较早 documents/ 只绑定执行中报告，已有排除标记。


## 5. 固定诊断结果与处置

六个测量进程均首次 exit=0、无超时、无替换，全部身份/输出及 ≥100 ms 读取块检查通过。`diagnostic/registration.json` 绑定新 B/D 树、工具、编译器、库/DLL、普通 allocator 与插桩关闭；原 A/B/Q4/C 窗口未重跑或回填。

| D/B 对照 | 几何比 | 固定 95% 区间 | 结论 |
| --- | --- | --- | --- |
| N1 | 1.1047 | [0.7616, 1.6025] | inconclusive，噪声超线 |
| N8 | 1.0095 | [0.6586, 1.5475] | inconclusive，噪声超线 |
| N64 | 0.9356 | [0.7109, 1.2313] | 点估计改善 6.4%，收益未确认，噪声超线 |
| N256 | 0.9727 | [0.7056, 1.3409] | inconclusive，噪声超线 |

所有原始块、进程中位数、块内与进程间噪声范围均保留在 diagnostic/results.json。没有增加样本，没有用第三轮较快的 D 进程替换其他进程。当前数据既未证明消除了小规模退化，也未确认主点收益；R2 收益门仍未完成。

源码/编译结果进一步显示当前实现有逐对象函数指针消费回调、上下文读取和 Result<void> 传播层；这些是旧 getter 调用形态之外的附加工作，但不是对其 ns 成本的量化。主代理随后选择更接近旧物化调用结构的第三候选：标准 Result 返回借用记录，在原循环内立即物化，保留所有检查和公共合同；新的机制与身份另行登记，旧窗口不加轮。本报告只登记第二候选，不把后续未测实现算为已验证。
