# R4 O1 前核验原始证据

状态：candidate；普通正确性与工具预检证据，不是正式性能、内存或生产接受

快照日期：2026-11-05

返回[集成与预算审计报告](../2026-11-05-pre-o1-integration-and-budget-audit.md)。
本目录保存开工身份、构建/CTest、GPU smoke、真实布局、预算重算及音频启动竞态的
原始失败和修复后记录。raw/ 的字节按捕获值保存，Git 使用 binary 属性防止换行归一化。
日志末尾空行和原输出空白不裁剪。

已保存 101 份原始文件，共 9351982 B；[概要](summary.json)和[逐文件 SHA-256 清单](manifest.json)
记录起点、成功、失败与跳过，不包含最终候选 C、正式注册 claim/seal 或 owner 接受。

| 内容 | 路径与结论 |
| --- | --- |
| 开工保全 | raw/baseline/：5074 文件 hash、3048 保护条目、原 tracked patch/status |
| 初次 MSVC | raw/initial/：configure/571 步 clean build；错误环境 11 失败与修正后的 1418 项回归均保留 |
| 初次 MinGW | raw/initial-mingw/：原预检 1266 项，1 个真实 B5 失败、1 项权限跳过；source_unchanged=false |
| 音频反例 | raw/audio-startup-repro/：原用例 10 次未复现；确定性用例 red exit42、green exit0，不作为性能样本 |
| 修复后两平台 | raw/after-audio-fix/：MSVC 1419 项、MinGW 1267 项，均 0 失败，各 1 项权限跳过 |
| 工具与普通 GPU | raw/final-checks/：12 项 Python 测试、格式/文档/版本；raw/initial/smoke-v2-result.json 明确 exit0 |
| 旧 B0 | 原始 --check exit1；两个来源文件 CRLF 哈希与初始保全相同，LF 规范字节完整推导一致；不改冻结原件 |
| 交付预算 | raw/preparation-tools/ 与 raw/layout-published/：276 个布局输入 hash、24 域×3 规模、声明余量与独立设备口径 |
| 源码修复 | raw/product-and-runner.patch：内部真实槽位查询、Close 局部修复、确定性反例与构建目录隔离 |

[只读复算器](verify.py)可逐字节核验全部 raw 文件、重推 R4 预算与规范 LF 的完整 B0，
并核对两平台完整 CTest 日志 hash。运行 `python -B <本目录绝对路径>/verify.py`；
不编译、不访问设备，也不启动性能、内存或 P8 负载。
