# 中立绘制命令就地构造附件

状态：局部选用证据。返回[报告](../2026-10-31-r3-neutral-command-transfer.md)
与[上一批附件](../2026-10-31-draw-transfer-evidence/index.md)。

- [选用、源码绑定与边界](selection.json)
- [原始字节和来源清单](manifest.json)
- [24 种源码排列的摘要等价对照](wip-build-v1/check/decision.json)
- [普通 GPU 非劣性固定组](wip-group-v1/decision.json)

本次只改 `engine/presentation_renderer/src/draw_command.cpp` 的构造方式：
opaque 与 transparent 两遍各自预留最终 summary 量并就地 append，删除
`OrderedDraw` 临时数组与逐命令 move 转移。比较器、digest、`is_sorted` 提前退出未变。

对照关系固定为「已选用的 HEAD 版本 baseline」对「工作树就地构造版本 candidate」，
两个可执行文件都由同一次 Release 构建产出，逐进程校验设备身份、像素与能量控制。
40K render-only cycle 50.550 → 44.168 ms（-12.63%），40K static 61.865 → 57.748 ms
（-6.65%），1K 两点在噪声内（+0.32% / -3.79%）；四点均未超出 +5% 非劣性带，
进程相对范围最大 6.82%。

这不构成完整矩阵、容量或正式接受：40K render-only 仍约为 4.167 ms 帧预算的 10.6 倍，
六项剩余工作、同源码矩阵、原 load/Seek/内存门与 R3/R4、设备/生产接受继续开放。
