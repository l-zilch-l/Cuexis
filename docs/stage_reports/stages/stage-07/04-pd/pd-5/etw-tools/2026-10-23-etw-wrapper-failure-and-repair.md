# PD-5 ETW 启动包装器失败与独立修正版

状态：exit-code repair verified，cleanup review HOLD；elevated capture / PD-5 acceptance：not passed。

更新日期：2026-10-23，Asia/Shanghai。此前诊断见
[寿命与读取噪声报告](../memory-attribution/2026-10-23-memory-lifetime-and-read-noise-diagnostics.md)，当前摘要见
[CURRENT_STATUS](../../../../../../CURRENT_STATUS.md)。

## 实际失败与证据归属

owner 在管理员 PowerShell 执行第一版准备后，终端没有输出。只读核验确认原目录
`out/pd5-f1-noise-control-20261023-733ec8e/run-20261023T075318Z-1c28748529334603b99715aec1219127`
已在北京时间 15:53:38 写入 FAILED；没有 collector、native start 或 F1 计时样本。
WPR start 已尝试，但包装器读到 null exit code，随后专属 stop 超过原 20 秒 ceiling。
原 receipt 的 wpr_start_succeeded=false 由错误返回码判断产生，不能证明操作系统启动失败；
非管理员查询该 named instance 报告 WPR is not recording，但两个临时 ETL 随后仍持续增长，
证明这一非管理员结果不能用作管理员底层录制已清理的证明。原超时 stop 没有完成清理。
owner 随后在管理员终端手动对本次 unique instance 执行 stop，报告 trace successfully saved。
`after-failure.etl` 为 7,027,556,352 bytes，十秒间隔两次大小/修改时间一致，临时 ETL 已清空；
没有本次残留测量进程。原 trace 文件的变化属于该未停止录制及后续 merge，不声称全部原
15 文件 hash 保持不变。主代理未执行新的 start、stop、cancel 或自动提权。

原脚本的 FAILED 分支写 JSON 后直接 exit 1，没有打印失败原因，因而用户看不到结果。
主代理和无上下文 Luna 在只读 WPR status 中复现 Start-Process/HasExited/Refresh 返回 null；
PS 5.1 中立即缓存 Handle、使用 WaitForExit 后可取得 Int32 0。普通 cmd 的原包装器也可复现。
另发现管理员终端解析到系统 WPR，当前工具会话解析到 SDK WPR，路径此前没有固定。

## 分立修复与验证

原准备、失败日志与原 evidence ZIP 不改。修复草案入口为
`out/pd5-f1-noise-control-v2-20261023-2719f10/run_f1_noise_control.ps1`，单独 plan/binding：
缓存 WPR/collector process handle；有界 WaitForExit 后读取并检查退出码；超时终止及回收
CLI 子进程；预检、开始、每 10 秒心跳、停止及失败原因均向终端输出。
固定 SDK WPR 与 Python 的绝对路径和 hash，保留原 runtime asset/input 绑定。
三次 F1 测量、各自预热、mask、counts、20 秒 start/stop、420 秒采集上限在草案中均未变。

对实际 v2 helper 的 PS 5.1 测试确认 exit 0、exit 23、超时 kill/reap；语法及全部
artifact/input hashes 通过。完整普通权限预检一次 exit 2、显示 NOT RUN，WPR/native
attempt flags 均为 false，未创建 run 目录。Luna 另做成功/非零/超时与只读状态比较。
这些验证不证明实际 elevated start、collector 全链路或 stop 会通过。独立复审仍发现 collector
超时只强制终止、未等待回收就进入 stop 的竞态；实际 stop 超时还证明现有清理不能保证
底层录制在 ceiling 后结束。**v2 保持 HOLD，不交付下一次管理员采集命令。** 下一准备须
先修复 collector 回收和有权限的专属 ETW 清理/确认；非管理员 idle 不作全局退出证明。

证据目录 `out/pd5-etw-wrapper-repair-20261023-2719f10` 保存原 15 文件的当时 hash 清单、
复现与比较日志、v2 实际 helper 验证、权限拦截回执和后续手动清理/稳定性记录。
本轮没有任何新的 F1 测量或 40k 重跑，不是新性能候选、正式注册或接受结果。
本 trace 无 F1 measured PID，不能替代原噪声数据或正式测量。管理员下一次执行需另行完成
清理修复和新准备，只运行一次；当前不要求 owner 再执行采集命令。

产品源码、主树 F1、old golden、C2 与历史证据保持；适用预算、1.10/15% 与 reply deadline
30 秒未改，旧窗口 3、公开停止接口、merge/push 继续停止。
