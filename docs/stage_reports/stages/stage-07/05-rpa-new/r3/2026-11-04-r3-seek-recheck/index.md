# R3 适用 Seek 机会性复核证据（2026-11-04）

状态：current evidence index；独立复核读数；test-only；不单独构成 R3 退出或生产接受。

本目录保存 2026-11-03 适用 Seek 余下三类读数的独立复核输出。原报告与原始 36 份日志保持原字节，
见[适用 Seek 余下三类](../2026-11-03-r3-seek-remaining-classes/index.md)；本轮只新增一次重放，
不改写原登记、判据或慢值。

## 身份

- 源码：`c085688a3a58bc99d531f66aefb1d90ed73e33b7`
- 独立工作树：`D:/Cuexis-r3seek-verify`
- 探针源差异：`tests/pd/pd5_host_probe.cpp`
- dirty diff SHA-256：`ca8ef3489e652defe43cdbf1cdbe36c2c0995c902d0979ade5d5e15a6547df18`
- 归档探针：`out/verification-20261103/r3-seek-classes-v1/cuexis_execution_device_probe.exe`
- 探针 SHA-256：`0ac53b9c03d19c078034da96859b5473024e2d99b10cd61b342980fbd9f0386b`
- 当前构建树中的同名探针已更新，不能替代上述归档二进制。

## 复核协议与结果

复核沿用原预注册：3 类 × 2 规模 × 6 个全新进程 × 每进程 16 次 Seek，共 36 个进程 / 576 次请求；
每次请求同时比较 Snapshot 与无提示 origin oracle 的编码字节。36/36 进程有效，576/576 oracle 校验一致，
六组均通过 `request_to_present_us <= 20,000`。本轮最差请求为 **1,249 µs**，余量约 16 倍。

| 类 | 记录数 | 有效进程 | Seek | 中位 | 最大值 | 判定 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| external | 129 | 6/6 | 96 | 540.5 µs | 966 µs | 通过 |
| external | 1024 | 6/6 | 96 | 528.5 µs | 1,249 µs | 通过 |
| natural | 129 | 6/6 | 96 | 538.0 µs | 933 µs | 通过 |
| natural | 1024 | 6/6 | 96 | 641.5 µs | 1,142 µs | 通过 |
| fallback | 129 | 6/6 | 96 | 505.5 µs | 757 µs | 通过 |
| fallback | 1024 | 6/6 | 96 | 490.0 µs | 1,132 µs | 通过 |

原始复算数据在 [`tools/results.json`](tools/results.json)，控制器为
[`tools/run_r3_seek_classes.py`](tools/run_r3_seek_classes.py)，运行日志为
[`tools/run.log.txt`](tools/run.log.txt)。逐进程日志在 [`runs/`](runs/)；[`manifest.json`](manifest.json)
重新登记本目录文件字节、SHA-256 与 `content_sha256`。

## 边界

- 本复核只支持“适用 Seek 五类各有独立通过读数”的本地登记，不产生设备、生产、hosted 或 R3 退出裁定。
- 原报告仍保留历史最差 1,880 µs；本复核的 1,249 µs 不回填原批次。
- 不把两次运行合成同一个统计量，也不删除原批次中的慢值或首轮作废夹具。
