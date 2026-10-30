# R3 中立绘制命令转移与已有顺序检测

状态：保留局部实现；普通有限对照与相关回归通过，完整最终矩阵及 240 FPS 仍开放。

日期：2026-10-31。返回 [New RPA 索引](../README.md)。
当前接受状态只见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 改动与语义

前批[渲染归因](2026-10-31-r3-startup-and-render-attribution.md)发现 neutral builder
逐帧构造、排序、复制与 digest 成本显著。本次根树只修改
`engine/presentation_renderer/src/draw_command.cpp`，没有引入跨帧缓存：

- 用原来的完整比较器检测是否已有序；只有检测失败才执行原 `std::sort`。
- 临时 `OrderedDraw` 队列最终转入 owning `DrawSummary` 时移动 command，避免再分配字符串副本。

两份 summary 容量均在移动前 reserve。移动后不再读取临时 command；
输入仍为只读，输出继续拥有所有字段。opaque ID/index、transparent depth/ID/index
规则、逐对象验证、资源语义和 digest 算法/字节保持原实现。没有安装头或 API/wire 变化。

只读代码质量审查未发现正确性或生命周期阻塞。审查指出近乎有序、最后才出现逆序的数据
会多一次近完整比较扫描，因此另做固定 CPU 保护组，不用主组的有序输入推断无退化。

## 功能与反例

baseline/candidate 各通过 3 cases / 399 assertions，包含原中立渲染事务测试和新增隔离表征：
24 种源排列覆盖重复 ID、透明深度、长资源/对象字符串，清空输入后读取所有输出字段；
每种排列的完整记录与 digest 两版逐行一致。
原 OpenGL 配置与 CPU builder/late-error/摘要表征 22 cases / 140 assertions 通过。
测试探针留在隔离证据中，没有为简单 move/is_sorted 向产品新增镜像测试。

格式化后根 CPP 在 `draw-transfer-final-v1` 显式重编并重新链接上述两个测试程序，
3/399、22/140 再次通过。其余对象与库真实复用，不把 overlay 描述为 fresh 全仓矩阵。

## 普通物理 GPU 固定组

`draw-transfer-measure-v1` 预登记 1K/40K × render-only/static × 两对 AB/BA，
固定 16 新进程、960 计时帧，1080p、RTX 4060 Laptop、VSync OFF、普通分配器，
timer 与计时内像素读回关闭。预热至少 8 帧及 2 秒；初末像素证明在计时外。
实际 draw 数、入镜、像素、采样数及 mask 65535 / BelowNormal / 执行节流 OFF 逐进程核验。
baseline/candidate 唯一绘制差异为本次 neutral CPP。

双方使用相同 SAX v2 仅加速计时外启动，不含实例化/GL 状态缓存。
共同库来自 `product-sequence-build-v1`，两边都含独立分页活跃索引，夹具不执行 Gameplay；
这不表示这些其他候选已进入产品。所有 exe 都使用与头/import library 同树的 app-local
Release DLL，文件 hash 见 binding。

| 场景 | 完整 cycle 中位数 baseline → candidate | cycle 变化 | submit 变化 |
| --- | --- | --- | --- |
| 1K render-only | 1.248 → 1.166 ms | -6.56% | -6.87% |
| 1K static | 1.443 → 1.321 ms | -8.46% | -9.99% |
| 40K render-only | 53.705 → 48.156 ms | -10.33% | -10.35% |
| 40K static | 68.428 → 60.179 ms | -12.05% | -14.38% |

满足预设 40K render-only submit 至少改善 10%、其余完整 cycle 不退化超过 5%，
各场景双方进程相对范围都小于 15%。数值、保护和本地重复性 screen 通过。
这是局部两进程/variant 对照，不是原 R4 正式配对 CI 或长期容量接受；40K 仍远超 4.167 ms。

## 近乎有序与逆序保护

审查后的 `draw-transfer-guard-measure-v1` 独立预登记 4 新进程 ABBA，
每进程 1K/40K × 尾部逆序/全逆序；每种输入 3 次预热、8 次计时，
先取进程内中位数，再汇总进程中位数，digest 两版相同。
CPU mask 4、BelowNormal、节流 OFF，实际控件逐进程核验，无物理窗口。

| 输入 | baseline → candidate | 变化 |
| --- | --- | --- |
| 1K 尾部逆序 | 668.925 → 612.750 us | -8.40% |
| 1K 全逆序 | 709.675 → 639.725 us | -9.86% |
| 40K 尾部逆序 | 31.564 → 28.819 ms | -8.69% |
| 40K 全逆序 | 33.478 → 29.661 ms | -11.40% |

全部满足不退化超过 5% 与进程相对范围 15% 保护。未延长、替换或补样改判。
真实分配器故障下新 move 路径会减少可失败分配位置，不声称异常中间态的内存地址或计数恒等。
无缓存、额度或长期 owner 保留策略变化。

## 证据与后续

原目录 `out/verification-20261030/r3-next-v1/draw-transfer-*` 原位冻结。
[附件索引](2026-10-31-draw-transfer-evidence/index.md)、
[manifest](2026-10-31-draw-transfer-evidence/manifest.json) 与
[选用绑定](2026-10-31-draw-transfer-evidence/selection.json) 保存源码、命令、日志及结果。
本次局部变更选用；完整新源码矩阵、其余六项工作、CPU 命令摘要热点、原 load/Seek/内存门、
R3/R4、设备/生产与 Stage 7 接受继续开放。
