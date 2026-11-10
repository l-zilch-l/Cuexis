# R4 语义整改原始证据

状态：dated evidence；两轮有界诊断，不是正式性能或内存接受

日期：2026-11-05

返回[语义整改报告](../2026-11-05-semantic-memory-remediation.md)。

本目录保存 [raw.zip](raw.zip) 的原始文件字节和[逐文件清单](manifest.json)。
ZIP 中保持捕获的换行、stdout/stderr、无效首轮、修复后的失败第二轮，以及新旧源码的
build/dependency/source/binary 绑定、契约和普通回归记录；不包含编译产物本体。
ZIP 是取证归档文件，不是 Cuexis CXC 包。

| 目录/文件 | 内容 |
| --- | --- |
| owner-authorization.json | owner A 批授权，最多两轮、各单进程120秒，禁止40K/P8 |
| build-v1、build-v2、build-v3 | 三次 fresh observer 构建、完整输入绑定和原始输出；v1 未运行容量诊断 |
| contracts-v2、contracts-v3 | 非容量的独立 allocator/semantic 正反契约；v3 有实际 MSVC 内部对齐与越界反例 |
| round-1 | 原工具无效运行、21 assertions；保留 process-result、tool-invalid 和预登记 |
| round-2 | 21 assertions、19 coherent checkpoints、active World 下界358241B超原327680B，保留完整分析 |
| normal-debug-build、normal-debug-checks | 关闭全部插桩的fresh普通构建与World/Runtime/Playback共268 cases回归 |
| prior-c22-audit | 旧候选hosted原始快照，校正两行完整、一行CTest通过但未完成的本地记录 |
| independent-review.json | 两轴只读复核，原始日志独立复算与结果边界 |
| final-batch-disposition.json | 两轮已用尽；正式/P8额度未用；memory/P8停止，不更改O1 |
| static-checks | Python、格式、版本、架构和文档检查原始日志 |

运行[只读核验器](verify.py)：

~~~powershell
python -B <本目录绝对路径>/verify.py
~~~

该核验器检查 ZIP 与每个成员的大小/hash，直接从归档 stdout 独立重算唯一 allocation 和
payload span union、全局 exact/range、实际 phase、最终释放与停止额度；不解包、不构建、
不运行 Cuexis，不消耗诊断、正式或 P8 额度。

新诊断源码为 5fd1bd3c；后继报告 SHA 不能自动继承完整矩阵资格。所有通过、失败、
跳过与缺证据均须按原报告范围使用，不将小规模最终释放推广为设备或完整语义接受。
