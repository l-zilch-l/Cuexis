# R4 独立 stream observer 取证（2026-11-14）

状态：completed observer qualification；不是 normal 三窗正式计时、GPU observer 或生产整体接受。

返回 [New RPA 报告索引](../README.md)。政策边界沿 [11/13 收尾裁定](2026-11-13-closeout-decisions.md)与 [工具实施报告](2026-11-13-closeout-implementation.md)：四阶段仅沿 2026-11-02 已接受的可开关归因、同负载、覆盖及开销披露例外；GPU 没有同类例外或已接受的新数值扰动门。

取证 source SHA 为 `ce2a9a6952fd4a9a4d82949544f7ef3482e204e6`。声明固定三轮，每轮依次运行 normal、gpu、stage、memory，共 12 个样本进程，每个另有一次 CPU warmup；frames=14,400、warmup_frames=2,400、maximum_processes=12、formal_sampling=false。首个 process/identity/environment/coverage 错误即停止且不替换的规则保持；本批有完整 summary，没有 stopped.json。

证据根为 `D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/stream-observers-ce2a9a69/`。`declaration.json` 为 1,806 B，SHA-256 `8118b9206e8005d9c8a071a18e4cb47403a66a526036a9f55fa9ba78d74d7682`；`summary.json` 为 8,192 B，SHA-256 `e1972d27b245d8b675d661abea1dec61b563acc5584545d25edb2f625ac30491`。每个 `pair-<1..3>-<kind>/` 保留 sample/warmup 元数据、环境摘要及原始输出；normal/gpu 共用 normal 构建绑定，stage 使用 stage-timing，memory 使用 memory 绑定，CPU warmup 单独绑定。

12 份 sample.json 与 12 份 warmup.json 均记 exit_code=0；24 份 sample/warmup-environment.json 均 passed=true、errors 为空，共 1,236 个环境采样点。全批最大实际采样间隙为 1.0250305000008666 秒，低于登记的 2 秒上限。

| 模式 | Sample exit 0 | Warmup exit 0 | Runtime passed | Sample 进程 elapsed 范围（秒） |
| --- | ---: | ---: | ---: | ---: |
| normal | 3/3 | 3/3 | 6/6 | 70.918908–70.938517 |
| gpu | 3/3 | 3/3 | 6/6 | 70.914362–71.544615 |
| stage | 3/3 | 3/3 | 6/6 | 71.610939–72.708814 |
| memory | 3/3 | 3/3 | 6/6 | 70.970321–70.979644 |

runtime monitor 为 sampled-windows-environment-v1、1 秒采样，范围限于进程枚举、前台、AC 与电源方案，不作 GPU/audio 结论，子间隔活动仍可能漏检。进程退出与 sampled 环境通过不代签 observer 扰动或正式性能接受。

summary.exact_output_equal=true，含义是汇总器规范化 identity 后的同负载、digest、计数与结果一致；它剔除已声明的 instrumentation 专属字段及 gpu_render 计数，不表示原始日志逐字节相同。GPU 每轮 gpu_render 为 14,400 个有效样本；汇总器要求数量等于登记 frames，包含最终 query drain 的覆盖检查。stage 的 judgement→d2、projection→d3、snapshot→materialize 每轮各 14,400 个，queue 每轮 72,000 个；queue 计数独立于帧数，不强行缩成 14,400。

四阶段 12 个数值门均按实际 p99_us 判定为 pass，observer_admitted.stage=true 仅来自既有例外，不是新增“扰动很低”的接受。下面单位为微秒，三轮仍是独立 observer 进程，未合成 normal 三窗重复性结论。

| Stage metric | 每轮 n | 第 1 / 2 / 3 轮 p99_us | 实际 statistic | limit_us | 数值门 |
| --- | ---: | --- | --- | ---: | --- |
| d2 | 14,400 | 21.5 / 20.6 / 20.0 | p99_us | 1,000 | 3/3 pass |
| d3 | 14,400 | 34.8 / 36.0 / 35.1 | p99_us | 2,000 | 3/3 pass |
| materialize | 14,400 | 7.0 / 6.8 / 6.8 | p99_us | 2,000 | 3/3 pass |
| queue | 72,000 | 22.0 / 20.1 / 20.5 | p99_us | 1,000 | 3/3 pass |

GPU 三轮的 gpu_render p99_us 均为 10.24、max_us 均为 24.576，median_us 分别为 9.216、8.192、9.216；三项 status 均为 observer_unvalidated，observer_admitted.gpu=false。summary 没有为这三项给出获准的 statistic/limit 判定；小数值、完整 query 覆盖及 drain 都不使其变为 pass。

开销表的 endpoint 比值为每轮 observed median / 同轮 normal median，wall 比值为 observed sample elapsed / 同轮 normal sample elapsed。以下均为倍数，保留六位小数；summary 的 pair=0/1/2 对应表中第 1/2/3 轮。九个 wall 比值与各 sample.json 的 elapsed_seconds 相符，数值原精度保留在 summary。

| Observer / 轮 | request_to_ready | request_to_present_return | cpu_submit | Process wall |
| --- | ---: | ---: | ---: | ---: |
| gpu / 1 | 0.828877 | 0.900641 | 1.073628 | 0.999936 |
| gpu / 2 | 0.765000 | 0.825959 | 1.022727 | 1.008544 |
| gpu / 3 | 0.884211 | 1.037855 | 0.992116 | 0.999812 |
| stage / 1 | 1.010695 | 13.246795 | 0.610442 | 1.025239 |
| stage / 2 | 0.885000 | 12.197640 | 49.418561 | 1.009509 |
| stage / 3 | 0.926316 | 13.053628 | 51.438896 | 1.009539 |
| memory / 1 | 1.122995 | 1.105769 | 1.045515 | 1.000725 |
| memory / 2 | 1.050000 | 1.023599 | 1.016414 | 1.000525 |
| memory / 3 | 1.052632 | 1.044164 | 1.014455 | 1.000639 |

stage 的外层 request-to-present-return median 比值为 12.197640–13.246795；cpu_submit 第 2/3 轮分别为 49.418561、51.438896，第 1 轮 0.610442 亦保持原值。wall 比值接近 1 受到固定 240 Hz sleep pacing 的影响，不能解释成 endpoint 扰动低、没有开销或观测器被普遍接受。低于 1 的单轮 endpoint 比值也不构成无扰动证明；本批没有额外建立因果或通用上界。

四阶段值保持 instrumented attribution 口径，不从 normal 时间中扣除，不替换独立 P3/capture normal 组。15% 是重复性阈值，不是 observer 扰动门。memory 的开销披露不证明完整 semantic ownership/lifetime 图、内存上界或 P8 准入；GPU query 时间也不代替进程 residency、CPU submit 或实际屏幕呈现时刻。

summary.formal_acceptance=false，scope 为 fixed observer qualification processes; not the normal three-window group。可登记的结论是 12/12 observer 样本完成、24/24 runtime 通过、同输出及登记覆盖成立、stage 的 12 个数值门沿既有例外 pass，而 GPU 三项继续 observer_unvalidated。GPU 准入政策与其他剩余缺口仍待明确处置，本报告不接受新阈值、追加次数或生产保证，也不替代最终候选的 F1/F2。

小记录的按字节副本及来源哈希见[本轮归档](2026-11-14-closeout-evidence/index.md)；原始完整进程文件与历史失败不替换。
