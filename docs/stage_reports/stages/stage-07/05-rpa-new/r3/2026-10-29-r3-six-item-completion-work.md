# R3 六项优化实施与验证账本

状态：进行中；候选尚未全部验证，不是 R3/R4 退出

日期：2026-10-29

返回 [RPA 报告索引](../README.md)。当前阶段状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。owner 要求连续完成桌面接手文档 P1–P6，
不再按单个局部候选结束工作；原预算、错误语义、身份字节和历史证据保持原要求。

## 当前工作与完成条件

| 项目 | 本批实施/调查 | 尚需完成的核验 |
| --- | --- | --- |
| P1 Packed / re-prepare | 调用期单条 owning Pattern 编译复用，完整声明精确相等且预算固定；保留完整 assembler / Packed 验证和 semantic compare | 普通固定 setup 对照、拒绝首错及完整 wire/身份差异、全套回归 |
| P2 准备热点 | assembler 去掉 compiler 已执行的重复树形检查，复用相同 Pattern 成功产品；各 requirement 的 measure、containment、timing、resource 校验仍执行 | 复用前后成本/内存，异构输入与预算反例；不能把未单独归因的 timer/resource 宣布消除 |
| P3 update / Snapshot | 带当前实体校验的标量 resolver 索引；只保存索引，不跨帧保存借用值 | 已有固定 CPU 对照，尚需语义回归和真实 GPU 最终版本核验；extract 仍有成本 |
| P4 编码与真实内存 | 撤回输出偏移缓存；候选改为第二次实际使用才建立独立编码副本，保留公共输出可编辑合同 | 编码字节/错误、普通分配器时间、实际 new/delete/CRT live/peak/最终释放；不能把 cap 当峰值 |
| P5 端到端 Seek | 内置 Host 可转发既有 checkpoint span；新增 33/129 历史、early/full/partial、native/external、无/有/不可信 hint 的 36 组合 | 独立外部 origin snapshot 全字节、目标 tick/context/receipt、原尾延迟及重复性；worker 替换并非当前 Host 能力 |
| P6 维护/工具/矩阵 | metrics TLS 改为函数内每线程对象，消费者同步；全仓格式债处理 | MinGW 双 TU 红绿已通过；fresh STATIC/SHARED、Debug/Release/MinGW、安装消费者与完整 CTest 待完成 |

这些是候选及核验账目，不将“已写代码”记成完成，也不以 R3 技术工作代替设备/生产或 owner 接受。
40K 历史 Packed reference 缺失、B0 caps 的规模适用性与原 R4 固定窗口保持原证据边界。

## 已取得的独立证据

统一根为 `out/verification-20261029/r3-completion/`，起点 `4e9cea6`。

- `cpu-reset-measure/`：固定 36 新进程 / 3600 帧，normal allocator、metrics OFF、mask 4、BelowNormal，
  实际 update / reusable extract，无 GPU。少做重复 reset 的候选改善动画约 18%，但 10K static
  中位数上升 11.26%，不满足预设 guard，已撤回；原值不替换、不追加。
- `cpu-hint-measure/`：新的独立 36 新进程 / 3600 帧，同一明确 CPU 边界；40K static update
  中位数 5.401459 → 2.851241 ms，animated 50.117857 → 44.217372 ms。
  前一组真实 GPU 使用全部允许 CPU，本组是 mask 4 的 CPU 诊断，不能拼接成同一连续基线。
- `metrics-tls/`：真实 MinGW 两个 translation units，旧 inline 非平凡 TLS link 退出 1，原 multiple
  definition 日志保留；新实现 link / run 均退出 0，验证同线程跨 TU 同址、不同线程独立存储。
- `format/`：按 CMake 同一范围扫描 643 个 cpp/hpp/hpp.in，7 文件需要整理，前后源码/哈希和完整
  dry-run 日志保留。后续新文件与新改动仍要在最终门重查，不以此前通过替代最终源码检查。

偏移缓存的静态审阅发现既有 `rpa_b3_tests.cpp` 明确要求 Writer 输出可 clear / edit 后继续编码，
因此仅靠注释约束 append-only 不够；该版本已撤回并保存源码，不能削弱原测试来保留候选。

## 构建、版本与原门限

所有新构建串行、BelowNormal、affinity 3，由既有 Windows Job 包装器约束后代。
旧绑定构建/二进制只读，采用新的独立构建目录。Release 首次配置因 PowerShell 对未引用 suffix
拆分失败，原目录与错误保留；有效新构建是 `release-static-v2`，参数由 Python argv 传递。
当前 SDK 仍为 0.9.1，未添加公开安装字段或改变 wire / codec 版本。

原适用 Seek ≤20 ms、request/ready/GPU p99≤4.167 ms、初次 load≤2000 ms、15% 重复性和正式
配对置信区间要求均未放宽。未完成、失败、不可比与缺外部证据的项必须分别记录，不追加到通过。
