# 中立路径 CPU 杠杆附件

状态：局部选用证据。返回 [报告](../2026-11-01-r3-neutral-path-cpu-lever.md)。

- [选用、边界与门结果](selection.json)
- [原始字节与来源清单](manifest.json)
- [预注册计划与门](neutral-lever-measure-v1/plan.json)
- [组绑定：HEAD、双臂、仪器补丁](neutral-lever-measure-v1/binding.json)
- [十四个进程的逐行读数](neutral-lever-measure-v1/results.json)
- [逐格中位数、变化与判定](neutral-lever-measure-v1/decision.json)
- [组的独立复核](neutral-lever-measure-v1/verification.json)
- [基线臂清单](arms/A/arm.json) / [候选臂清单](arms/B/arm.json)
- [候选补丁](code/neutral-path-lever.patch)
- [控制器](instruments/run_neutral_lever_v1.py.txt) / [复核器](instruments/verify_neutral_lever_v1.py.txt)
- [共享测量模板](instruments/gpu_runner.py.txt)
- [Release 全量 CTest 日志（1418 个测试块，本地化时间戳按不透明字节归档）](logs/full-ctest-release.journal.log.gz)
- [CTest 控制台末段：最终计数、总耗时与退出码](logs/full-ctest-release.console-tail.log.txt)
- [格式目标转录](logs/format-check.log.txt) / [该目标覆盖的 651 个文件](logs/format-file-list.txt)

## 目录结构

`neutral-lever-measure-v1/<序号>-<臂>-<实体数>-<模式>/` 保存每个进程的 `registration.json`、
`results.json` 和探针原始日志（确定性 gzip）。序号即实际启动顺序，ABBA 交错可直接从目录名读出。

`superseded/aborted-setup/` 保存首次启动即退出的那次尝试（未启动任何进程）。它不产生任何时间读数，
保留它是为了不让本组看起来像一次成功。

`arms/` 只保存两臂清单；可执行文件与依赖库按清单中的 sha256 绑定，字节本身不重复入库。

`logs/` 保存组外的检查转录。CTest 控制台流在作业缓冲里从第 335 个测试之后才有留存，因此控制台
文件是**中段起始**的原样记录，未做任何补写；完整的 1418 个测试块在 journal 里（时间戳是本地码页
字节，故按不透明字节做确定性 gzip）。

## 读法

本组是同一台物理 GPU 上的同一仪器、两份二进制的中立路径对照：两臂都开启逐帧像素回读、关闭 GPU timer，
因此绝对时间含回读成本，不是容量前沿点，也不与任何历史阶梯或矩阵行相加、相减或拼接。
40000 实体一对进程为诊断用途，只说明杠杆在压力规模上仍然生效，不构成任何门槛判定。
