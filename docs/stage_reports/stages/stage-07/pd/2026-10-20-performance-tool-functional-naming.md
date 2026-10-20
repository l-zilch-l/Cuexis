# Playback 测量工具与内部观测实现的功能命名修正

日期：2026-10-20。范围：文件与目录命名、调用引用、文档导航及迁移验证。
当前实施进度仍以 [CURRENT_STATUS](../../../../CURRENT_STATUS.md) 为准；本次修正不表示测量准备或正式验收完成。

## 命名要求与结果

owner 指出非测试文件使用 PD 阶段号会增加后续维护成本。可复用源码、工具与配置按功能命名，
阶段信息留在测试、阶段文档和测量协议身份中；该规则已补入
[编码政策](../../../../guides/CODE_POLICY.md)。

工具统一位于 [tools/verification/playback_performance](../../../../../tools/verification/playback_performance/README.md)：

| 原名称 | 当前功能名称 |
| --- | --- |
| `tools/verification/pd5/` | `tools/verification/playback_performance/` |
| `pd5_build.py`、`pd5_prepare.py` | `build_profiles.py`、`prepare_profiles.py` |
| `pd5_measurement.py`、`pd5_windows.py` | `measurement.py`、`window_protocol.py` |
| `pd5_process.py`、`pd5_owner_runner.py` | `process_control.py`、`owner_runner.py` |
| `pd5_profile_runner.py`、`pd5_device_inventory.py` | `profile_runner.py`、`device_inventory.py` |
| `pd5_profiles.json` | `profiles.json` |
| `tools/verification/measure_pd0_method.py` | `tools/verification/playback_performance/baseline_method.py` |
| `tools/measure_pd0_method.py` | `tools/measure_playback_method.py` |
| `app/player/src/pd5_trace.hpp` | `app/player/src/scripted_playback_trace.hpp` |
| `engine/core/internal/cuexis_internal/pd_memory.hpp` | `engine/core/internal/cuexis_internal/allocation_observer.hpp` |
| OpenGL 的 `pd_gpu_timer.hpp` / `.cpp` | `gpu_timer.hpp` / `.cpp` |
| OpenGL 的 `pd_gpu_memory.hpp` | `gpu_storage_observer.hpp` |

新增加的 `observer.py` 与 `gpu_residency.py` 同样使用功能名。
`Pd5Trace` 改为 `ScriptedPlaybackTrace`；新 Player 与分配观测开关分别使用
`CUEXIS_PLAYBACK_TRACE_HARNESS`、`CUEXIS_ALLOCATION_LIFETIME_OBSERVER`。
CMake、include、Python 导入、原生 child 命令、源码绑定和文档目标及显示文字同步更新。

`tests/pd/` 的兼容入口仍留在测试目录。两个非测试目录的阶段命名基线入口已移除，
现有仓库调用方转向功能路径；旧命令对应关系见 [工具索引](../../../../../tools/README.md)。
独立审查发现直接启动 `process_control.py` 的 GPU 观察分支缺少包搜索根，已补齐脚本入口的
仓库根定位，并通过跨工作目录、隔离 Python 搜索路径的导入核验。

## 验证与证据边界

- 全部 `tests/pd/*_tests.py`：58 项通过，含既有基线支持、统计、窗口及进程树检查。
- 19 项有限调用检查通过：新 CLI 的绝对路径跨目录调用及模块调用、根入口、历史调用方导入、
  内部 child 包导入及无负载的亲和性 child 启动。没有启动 Player 或性能负载。
- `app/`、`engine/`、`tools/`、`cmake/`、`examples/` 的活动文件名未检出 PD 阶段号；生成缓存不属于源码检查范围。
- 迁移涉及的五份 C++ 实现/头文件通过 `clang-format --dry-run --Werror`；架构 CTest 通过。
- 最终 Release 增量构建通过，覆盖 Player、host/device/baseline probes 与 Chart 容量工具；
  host、baseline 的普通测试各 6 cases 通过。隐藏的性能与设备轨迹未执行。
- 新建无上下文子代理独立复查了命名、调用、根目录定位、identity hash 和文档路径；
  修复 child 导入问题后，没有报告未修复的命名迁移问题。它没有自行运行 C++ 构建。

证据目录为 `out/functional-naming-20261020/`，保存补充迁移前 ZIP、调用检查与原始日志、
Python 测试日志、Release 增量构建和格式检查记录。更早的命名迁移清单及迁移前 ZIP 保留在
`out/pd5-complete-preparation-20261019/`。本轮最终构建与文档检查状态见证据目录的 `validation-summary.json`，
`functional-naming-evidence.zip` 和 `evidence-manifest.json` 保存最终源码快照、迁移清单和验证记录。

所有冻结 ZIP、原始日志、manifest 和历史命令按其原时点保留。路径变化使旧 preparation
身份失效，后续测量必须重新 prepare；不能沿用旧 manifest 或改写历史绑定来消除差异。
本轮没有 pilot、正式注册、正式窗口或性能通过判定。
