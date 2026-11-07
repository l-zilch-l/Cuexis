# R4 World 页存储效果原始证据

状态：dated evidence；一轮 64/high 限定诊断，非完整内存接受

日期：2026-11-06

返回[效果验证报告](../2026-11-06-world-storage-effect.md)。

[raw.zip](raw.zip) 保存捕获的原始文件字节，[manifest.json](manifest.json) 绑定每个成员的
大小和 SHA256；不含编译产物本体。归档包含新批授权与预登记、fresh 构建的 source/dependency/
compiler/binary 绑定、四组 allocator 契约、唯一容量进程的 stdout/stderr、计费分析、比较、
停止处置，以及从旧批原件复制的 baseline 日志与绑定。ZIP 是取证归档，不是 CXC 包。

独立只读核验：

~~~powershell
python -B <本目录绝对路径>/verify.py
~~~

核验器不解包、不构建、不运行 Cuexis。它核对归档 hash、逐文件字节、预算/源码/构建身份、
19 个 checkpoint 的 partition、唯一分配和 payload span union、真实 active phase、Close
及最终合法释放，再独立复算 358,241 B → 103,738 B 的限定改善。低于 cap 仍是 `unrated`。

新证据绑定 `e643cd5b`；历史 baseline 为 `5fd1bd3c`，二者的旧失败和新改善分开保留。
不代表 2048/40K 性能、完整拥有图、native/GPU/driver、正式内存/P8 或 R4 退出通过。
