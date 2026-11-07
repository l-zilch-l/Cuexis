# R3 legacy parse-once：机制已在产品中，补正式读数与记录更正（2026-11-03）

状态：local measurement；test-only。本记录不采用任何新候选、不改判据、不关闭 R3、不替代 R4 /
设备 / 生产接受。

日期：2026-11-03　源码：`a422e527`（含 `466ff76a` 的树复用实现）

返回 [RPA 报告索引](../../README.md)。口径见
[重复性口径裁定](../2026-11-03-r3-repeatability-scope-ruling.md)。

## 0. 结论

1. **不需要重启**：`legacyCanonicalText` 与 `loadDocumentStage` 的消费分支已经在产品里，
   由 `466ff76a`（2026-10-31，"Own the parsed Chart tree once in legacy prepare"）引入。
   归档的 `legacy-parse-once-measure-v1` 记的 `selected: false` 指的是更早那一版原型；它被重做过
   （writer 早于 caller 诊断、输出参数由 `has_value` 判断两处限制已消除）之后才进入产品。
   所以本轮没有任何补丁要移植，本轮要补的是**读数与记录**。
2. **读数**：同一份源码、同一个探针，只把 A 侧改成"由第二个解析产生身份字节"（即 `466ff76a`
   之前的形状），三次配对 × 三个规模：

   | 规模 | A 侧（两次解析） | B 侧（产品，一次解析） | 比值 [95%] | 变化 |
   | ---: | ---: | ---: | --- | ---: |
   | 1000 | 56.257 ms | 40.127 ms | 0.7139 [0.6574, 0.7753] | **−28.61%** |
   | 10000 | 698.395 ms | 469.161 ms | 0.6747 [0.6504, 0.7000] | **−32.53%** |
   | 40000 | 2 682.526 ms | 1 856.129 ms | 0.6949 [0.6660, 0.7250] | **−30.51%** |

   三个区间上界最大 0.7753，离 1.10 有 1.4 倍以上余量；也就是说"产品不比两次解析慢"这件事
   在这次测量里是被正向证明的，不只是没有超界。
3. **等价**：18 个进程里每个规模的 6 个读数（两臂各 3）`identity` 与 `frame_digest` **完全相同**；
   既有 `tests/chart/parse_characterization_tests.cpp` 另有一条断言保着
   "dispatch 给出的身份字节等于 writer 的输出"。
4. **一次方法论修正（保留）**：第一组 A 侧只切断"消费"（`loadDocumentStage` 忽略字节、回落到文本入口），
   却仍让 dispatch 产出字节，于是 A 侧多付一次序列化。那一组给出 −34.4%/−37.4%/−37.0%，
   **高估了收益**。改成切断"生产"之后才是 `466ff76a` 之前的形状，得到上表的 −28.6%/−32.5%/−30.5%。
   第一组原样归档在 [`tools/superseded-consume-only/`](tools/superseded-consume-only/)，不删数字。

## 1. 为什么这不是"重启既有未选用候选"

`2026-10-31-startup-evidence/legacy-parse-once-measure-v1/decision.json` 的
`numeric_gate: true`、`repeatability_screen: false`、`selected: false` 描述的是
`unselected-legacy-parse-once-v1.patch`：typed reader 借用树、`writeLegacyValue` 把字节回填到
输出参数。那份原型带着两处已知限制（writer 早于 caller 诊断、输出参数不能由 `hasValue` 推断）。

`466ff76a` 是重做版：dispatch 结果携带 `legacyCanonicalText`，`loadDocumentStage` 显式判断
`has_value()`，取不到字节时回落文本入口并原样报 writer 错误。本次核对的当前代码即为该版本：
`engine/chart/src/chart_v4_loader.cpp` 第 259–272 行与
`engine/playback/src/playback_session_prepare_stages.cpp` 第 85–101 行。

因此"重启"这个动作没有对象；把机制重新做一遍只会得到同一个实现。真正缺的是：
(a) 记录仍在按"未选用"口径描述它；(b) 没有按修正后统计量给出的、当前源码上的读数。

## 2. 测量设计

- **仪器**：`cuexis_pd_host_probe` 里的隐藏用例 `CPU legacy preparation fixed diagnostic`
  （`tests/pd/json_prepare_probe.cpp`），
  每进程一次计时同步 `prepareLoad`，夹具在计时前生成、commit/update/extract/digest 在计时后。
  夹具为合成的 legacy Chart v1（N 个对象 + 两个资源），与归档组同一生成器形状。
- **两臂**：`candidate` = 当前产品；`baseline` = 同一源码 + [`a-side.patch`](tools/a-side.patch)
  （legacy dispatch 不再产出身份字节）。A 侧只有这一处改动，源码在写注册前已恢复。
- **协议**：三个规模各三对，顺序 AB / BA / AB，每进程全新、`CREATE_NO_WINDOW`、
  affinity=4、priority=BelowNormal；执行节流（power throttling）本轮未控制，已在登记里写明。
  一次计时调用/进程；不合格进程保留、不补样。
- **预注册**：[`tools/registration.json`](tools/registration.json) 由
  [`tools/run_prepare_once.py`](tools/run_prepare_once.py) 在第一个进程启动前写出。
  裁决量写死为"三对的配对对数比值 + Student-t 区间，上界 >1.10 记为回归"。
- **规模**：18 个进程全部有效（exit 0、`All tests passed`、恰好一条 `PREPARE_RESULT` 且
  `entities` 相符）。原始读数 [`tools/results.json`](tools/results.json)，逐进程日志
  [`tools/logs/`](tools/logs/)。

## 3. 读数与逐对构成

| 规模 | 对 0（AB） | 对 1（BA） | 对 2（AB） | A 侧极差 | B 侧极差 |
| ---: | --- | --- | --- | ---: | ---: |
| 1000 | 56.257 → 40.127 ms（0.713） | 57.091 → 39.445 ms（0.691） | 55.681 → 41.112 ms（0.738） | 2.51% | 4.15% |
| 10000 | 696.508 → 469.161 ms（0.674） | 698.395 → 478.599 ms（0.685） | 704.356 → 468.678 ms（0.665） | 1.12% | 2.11% |
| 40000 | 2 694.089 → 1 844.967 ms（0.685） | 2 682.526 → 1 856.129 ms（0.692） | 2 676.356 → 1 894.995 ms（0.708） | 0.66% | 2.70% |

两侧的相对极差都在 15% 以内，且方向一致（每一对 B 侧都更快），不是靠某一对拉出来的。

## 4. 等价性

- `identity`：1000 → `90b8e06b…`、10000 → `5bd4ceda…`、40000 → `e07412ce…`，每规模 6 个进程完全一致。
- `frame_digest`：同规模 6 个进程完全一致（1K 3531761707033946743；其余见
  [`tools/results.json`](tools/results.json)）。
- 既有用例 `parse_characterization_tests.cpp` 断言 v4 路径**没有** `legacyCanonicalText`、
  legacy 路径有且等于 writer 输出字节——本轮改动没有触碰这条合同。

## 5. 被取代的第一组与教训

[`tools/superseded-consume-only/`](tools/superseded-consume-only/) 保留完整的 18 个进程读数、
登记与 A 侧补丁。它的问题写在 §0.4：切断"消费"而保留"生产"，A 侧因此比历史形状多一次序列化，
收益被高估约 5–7 个百分点。

教训是这类 A/B 要问清楚"被删掉的是哪一步"：树复用在 466ff76a 里同时搬走了**第二次解析**和
**第二次序列化**，只切一半就测不准。该组不再被引用。

## 6. 边界

- 单台机器、单个构建（`out/build/rpa-candidate-release`，Release、STATIC、candidate ON）、单次会话；
  一个窗口、三对，**不是**计划 §9.2 的九对正式单元，也不构成 load/设备/生产接受。
- 夹具是合成 legacy Chart，不是产品 40K 文档；两组夹具的绝对值不可互换，也不与
  `466ff76a` 当日的 `prepareLoad` 读数（3 788/3 981 → 2 739/2 878 ms，13 MB 真实文档）相加或相减。
- 本轮不改产品源码：`a-side.patch` 只用于造 A 侧，应用后已 `git checkout` 还原。
- 探针是新增的 test-only 仪器，只在 candidate 预设下编译；隐藏用例不进入 CTest。
- 探针源码在测量之后跑过一次 `clang-format`（仅空白）。登记里的 `probe.source_sha256` 记的是
  **运行当时**的字节，提交后的文件与之只差格式；两份二进制都由同一份逻辑源码构建。
- 36 份进程日志（本组 18 + 被取代组 18）按捕获字节提交，末尾带 Catch2 汇总后的空行，因此
  `git diff --check` 在这 36 个文件上各报一处 "new blank line at EOF"。与 2026-11-01 附件的同类情况一致：
  这是证据保存的结果，不是源码洁净问题，未为通过该检查裁掉字节。
