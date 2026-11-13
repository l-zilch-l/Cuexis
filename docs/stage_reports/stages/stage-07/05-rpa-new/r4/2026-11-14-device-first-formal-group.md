# R4 设备后端首个正式组（2026-11-14）

状态：completed fixed group，保留逐 metric 结论；不是设备、GPU 时间或生产整体接受。

返回 [New RPA 报告索引](../README.md)。授权与准入边界见 [11/13 收尾裁定](2026-11-13-closeout-decisions.md)及 [工具实施报告](2026-11-13-closeout-implementation.md)。

测量候选为 `ce2a9a6952fd4a9a4d82949544f7ef3482e204e6`，tree 为 `f2ec992cd94aa32c838d5ee863bdbca0b37033d2`。独立 P3 与 capture-1/2/4 分别消费首次 device claim；三个完整窗口各执行四单元、每单元三个 B 进程，共 36 个 sample 进程，全部 exit 0。CPU 历史单元与额度保持独立，设备结果不替代 CPU 结论。

P3 的 request_to_ready、close、load 绝对门分别为 9/9 pass；重复性 request_to_ready、close、load_host 为 pass，cpu_submit、present_call、request_to_present_return 为 inconclusive。capture-1/2 的 capture_delivery 重复性为 pass，capture-4 为 inconclusive；capture 未新增绝对门。inconclusive 保持未定，不改为 fail 或 pass。

证据根为 `D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/device-group-ce2a9a69/`。主结果 `run/result.json` 为 19,326 B，SHA-256 `ecffa82d400b9fd00ae08c7bf7f0e73b46ab2f62df12ae77f59a050c8a850a38`，与 `D:/Cuexis-worktree/out/evidence/r4/closeout-20261113/device-complete-observation.json` 的 result_binding 匹配，四单元的 metrics 亦一致。

preparation.json 的 SHA-256 为 `aefbcd6d7126580ca005b48b3a657d30354c6856e0111f704bf05d168383d4f5`，与 seal.json 匹配；seal 的 SHA-256 为 `8223ec0d007da218bf4efac9660210b3b69629f387415689d0584c763651e618`，与 running 记录匹配。preparation 的原字段 `formal_registration=false` 保留；四份实际 claim、marker 与 seal 的哈希及 preparation 引用均匹配，不追改原字段。

| 单元 | Stable protocol ID | Claim |
| --- | --- | --- |
| p3-renderer | p3-renderer-device-release | attempt-1 |
| p4-capture-1 | p4-capture-1-device-release | attempt-1 |
| p4-capture-2 | p4-capture-2-device-release | attempt-1 |
| p4-capture-4 | p4-capture-4-device-release | attempt-1 |

四份 claim 均为 `rpa.execution.claim.v1`、同一 source SHA、B-only；各文件位于 `D:/Cuexis-worktree/out/evidence/r4/attempt-ledger/<stable-protocol-id>/attempt-1.json`。claim/marker 身份不因本报告发布而重置，也不自动开启第二次 claim。

四单元均为 Release / normal / B-only，实际使用设备 renderer；metrics_compile_enabled=false、allocator=normal、audio=false，P3 gpu_timer=false。没有 A 侧，不产生 A/B 相对回归结论。四份绑定 pilot 均 passed=true、source SHA 一致，字节数和哈希与 preparation 匹配，candidate binary SHA-256 均为 `b97b39c09f395d64f42e8a621ef1a9e9a8167324037b2a94684a702aed41e56d`。

预登记并由 runner 核对的结构身份如下；每单元均为 frames=14,400、warmup_frames=2,400、source_step_ticks=7,000、source_tick_rate=1,000,000。核心语义与期望计数沿该 runner 的已完成校验结论，保持原 workload、比例和计数。

| 单元 | Profile / endpoint | 每进程结构计数 |
| --- | --- | --- |
| P3 | gpu-stream-v1 / request-to-present-return | request_to_ready、request_to_present_return、cpu_submit、present_call 各 14,400；load_host、close 各 1；另有 load，登记 requests_per_process=57,603 |
| capture-1 | presentation-capture-1-v1 / delivery-ready | SDK update 14,400；materialize 28,800；capture delivery 14,400；capture consumers 1 |
| capture-2 | presentation-capture-2-v1 / delivery-ready | SDK update 14,400；materialize 43,200；capture delivery 28,800；capture consumers 2 |
| capture-4 | presentation-capture-4-v1 / delivery-ready | SDK update 14,400；materialize 72,000；capture delivery 57,600；capture consumers 4 |

三个 capture profile 均为 renderer consumers=1，renderer_delivery、cpu_submit、present_call 各 14,400，reloads=0、delivery_generations=1、payload_charge_per_frame=1,048,576 B；payload_live_after_close 与 attempt_live_after_close 均要求为 0。其登记 requests_per_process=14,400 与按 consumer 计数的 capture delivery 不混为同一口径；这些结构身份不把计时 inconclusive 改成 pass。

运行窗口均按 UTC 记录，对应 Asia/Shanghai 日期为 2026-11-14。每窗依次执行 P3、capture-1/2/4。间隔按完整前窗 end 至下一窗 start 计算，均满足至少 600 秒；全组首窗 start 至末窗 end 为 5,294.376848 秒。

| 窗口 | Start UTC | End UTC | 窗口时长（秒） | 前窗结束至本窗开始（秒） |
| --- | --- | --- | ---: | ---: |
| 1 | 2026-11-13T17:35:27.260122+00:00 | 2026-11-13T17:58:10.949931+00:00 | 1363.689809 | — |
| 2 | 2026-11-13T18:08:10.950571+00:00 | 2026-11-13T18:30:54.653205+00:00 | 1363.702634 | 600.000640 |
| 3 | 2026-11-13T18:40:54.653618+00:00 | 2026-11-13T19:03:41.636970+00:00 | 1366.983352 | 600.000413 |

36 份 `run/<unit>/window-<n>/pair-<m>-B/sample.json` 的 command 均与 preparation 中各自 B 命令一致，exit_code 均为 0。各进程 warmup/sample runtime-environment 共 72 份，passed 全为 true、errors 全为空；每单元为 18/18。全组最大实际采样间隙为 1.0301475000014761 秒，低于登记 maximum_gap_seconds=2.0。

runtime monitor 为 sampled-windows-environment-v1、1 秒采样，范围是进程枚举、前台、AC 与电源方案；不包含 GPU/audio 证明，子间隔活动仍可能漏检。退出码和 sampled 环境通过不代签计时、语义外延或生产接受。

P3 绝对门逐进程使用已登记 statistic，没有以 median 或全组池化值替换。request_to_ready 用 p99_us；close/load 各仅一条，使用 max_us，p99_us 为空。单位均为微秒。

| P3 absolute metric | 每进程 n | Actual statistic | 九进程判定值范围（us） | limit_us | 结论 |
| --- | ---: | --- | ---: | ---: | --- |
| request_to_ready | 14,400 | p99_us | 295–323 | 4,166.666666666667 | 9/9 pass |
| close | 1 | max_us | 15,274–17,524 | 1,000,000 | 9/9 pass |
| load | 1 | max_us | 622,581.5–638,257.6 | 2,000,000 | 9/9 pass |

| P3 进程 | request_to_ready p99_us | close max_us | load max_us |
| --- | ---: | ---: | ---: |
| W1P1B | 318 | 17242 | 626083.0 |
| W1P2B | 320 | 15274 | 622581.5 |
| W1P3B | 313 | 17524 | 638257.6 |
| W2P1B | 315 | 16168 | 625606.1 |
| W2P2B | 300 | 15988 | 636174.9 |
| W2P3B | 321 | 17316 | 626925.7 |
| W3P1B | 323 | 16803 | 626795.9 |
| W3P2B | 295 | 16729 | 638007.9 |
| W3P3B | 317 | 17234 | 630287.6 |

W1P2B 的 request_to_ready max_us=6,479 原值保留，该门实际判定使用 p99_us=320。load_host 的重复性与 load 的 2 秒绝对门分列，没有为 load_host 新增绝对门。三个 capture 单元的九个 per_process_absolute_gates.gates 均为空；不将其重复性 pass 改称绝对门 pass。

重复性按每进程 median 的 `(max - min) / median` 计算各窗口、first six 与 all nine；noise_limit=0.15。下表以百分比显示，小数点后六位仅供呈现，原精度判定保持。九组均与 result 一致。

| Unit / metric | W1 noise | W2 noise | W3 noise | First six | All nine | 结论 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| P3 / request_to_ready | 11.675127% | 6.217617% | 7.106599% | 11.794872% | 11.675127% | pass |
| P3 / close | 13.049530% | 8.213756% | 3.005416% | 13.469021% | 13.390466% | pass |
| P3 / load_host | 0.266972% | 1.292163% | 4.007693% | 1.679445% | 4.096451% | pass |
| P3 / cpu_submit | 4956.967742% | 3.585398% | 3.552632% | 4989.483934% | 5010.886571% | inconclusive |
| P3 / present_call | 20.224719% | 4.044944% | 4.988662% | 20.224719% | 20.224719% | inconclusive |
| P3 / request_to_present_return | 1130.860534% | 6.402439% | 7.142857% | 1158.547655% | 1163.981763% | inconclusive |
| capture-1 / capture_delivery | 1.851852% | 1.851852% | 7.547170% | 1.851852% | 7.407407% | pass |
| capture-2 / capture_delivery | 2.173913% | 4.545455% | 6.818182% | 6.666667% | 6.666667% | pass |
| capture-4 / capture_delivery | 0.000000% | 23.529412% | 0.000000% | 23.529412% | 23.529412% | inconclusive |

首窗 P3 W1P3B 的 cpu_submit median=3,917.75 us、request_to_present_return median=4,140 us；其余进程分别为 74.4–77.5 us、310.5–337 us。present_call 首窗为 45.0、44.5、36.0 us，W2/W3 noise 虽低于 15%，不能丢掉 W1 再改判。capture-4 的 W2 median 为 1.6、1.7、2.0 us，绝对值小也不豁免 23.529412% 的重复性未定。原慢值、进程和三窗结果全部保留，不拼接其他轮次或推断 driver 根因。

所有单元 result.scope 仍为 relative_cost_and_repeatability_only，overall_acceptance 仍为 requires_all_registered_absolute_and_other_gates；完整执行不等于整个设备组已接受。CPU submit、present-call、present-return 均不是 GPU 执行时间，也不证明屏幕实际呈现时刻；audio=false，不外推音乐、物理输入或听音结论。独立 stream observer 使用其各自登记与准入证据，不替代此 normal 组，也不能由此组反向认证 GPU observer。

下一步集中处置 P3 三项及 capture-4 一项重复性 inconclusive；独立 observer 的完整结果见[观察器报告](2026-11-14-stage-and-gpu-observer.md)。限制尚未获接受，不自动改为 pass 或 N/A。设备默认额度不要求运行第二次，本报告不创建、推荐或预签第二次 claim。后续若需修复或改变受控输入，应保留本次结果并按原授权重新绑定受影响证据；最终候选仍须十一行 F1 与同 SHA 三平台 F2，不继承历史候选成功或把缺口作为人工豁免。

小记录的按字节副本及来源哈希见[本轮归档](2026-11-14-closeout-evidence/index.md)；原始完整进程文件与历史失败不替换。
